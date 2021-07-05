/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-file.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-history.h>



enum
{
  ENCODING_CHANGED,
  EXTERNALLY_MODIFIED,
  LOCATION_CHANGED,
  READONLY_CHANGED,
  LAST_SIGNAL
};



/* GObject virtual functions */
static void     mousepad_file_finalize        (GObject           *object);

/* MousepadFile own functions */
static gboolean mousepad_file_set_monitor     (gpointer           data);
static void     mousepad_file_monitor_changed (GFileMonitor      *monitor,
                                               GFile             *location,
                                               GFile             *other_location,
                                               GFileMonitorEvent  event_type,
                                               MousepadFile      *file);
static void     mousepad_file_set_read_only   (MousepadFile      *file,
                                               gboolean           readonly);



struct _MousepadFileClass
{
  GObjectClass __parent__;
};

struct _MousepadFile
{
  GObject __parent__;

  /* the text buffer this file belongs to */
  GtkTextBuffer *buffer;

  /* location */
  GFile    *location;
  gboolean  temporary;

  /* file monitoring */
  GFileMonitor *monitor;
  gchar        *etag;
  gboolean      readonly;

  /* encoding of the file */
  MousepadEncoding encoding;

  /* line ending of the file */
  MousepadLineEnding line_ending;

  /* whether we write the bom at the start of the file */
  gboolean write_bom;

  /* whether the filetype has been set by user or we should guess it */
  gboolean user_set_language;

  /* autosave */
  GFile    *autosave_location;
  gboolean  autosave_scheduled;
};



static guint file_signals[LAST_SIGNAL];



G_DEFINE_TYPE (MousepadFile, mousepad_file, G_TYPE_OBJECT)



static void
mousepad_file_class_init (MousepadFileClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_file_finalize;

  file_signals[ENCODING_CHANGED] =
    g_signal_new (I_("encoding-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  file_signals[EXTERNALLY_MODIFIED] =
    g_signal_new (I_("externally-modified"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  file_signals[READONLY_CHANGED] =
    g_signal_new (I_("readonly-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  file_signals[LOCATION_CHANGED] =
    g_signal_new (I_("location-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, G_TYPE_FILE);
}



static void
mousepad_file_init (MousepadFile *file)
{
  /* initialize */
  file->location = NULL;
  file->temporary = FALSE;
  file->monitor = NULL;
  file->readonly = FALSE;
  file->encoding = mousepad_encoding_get_default ();
#ifdef G_OS_WIN32
  file->line_ending = MOUSEPAD_EOL_DOS;
#else
  file->line_ending = MOUSEPAD_EOL_UNIX;
#endif
  file->etag = NULL;
  file->write_bom = FALSE;
  file->user_set_language = FALSE;
  file->autosave_location = NULL;
  file->autosave_scheduled = FALSE;

  /* file monitoring */
  MOUSEPAD_SETTING_CONNECT_OBJECT (MONITOR_CHANGES, mousepad_file_set_monitor,
                                   file, G_CONNECT_SWAPPED);
}



static void
mousepad_file_finalize (GObject *object)
{
  MousepadFile *file = MOUSEPAD_FILE (object);

  /* cleanup */
  g_object_unref (file->buffer);
  g_free (file->etag);
  if (file->location != NULL)
    g_object_unref (file->location);

  if (file->monitor != NULL)
    g_object_unref (file->monitor);

  if (file->autosave_location != NULL)
    g_object_unref (file->autosave_location);

  (*G_OBJECT_CLASS (mousepad_file_parent_class)->finalize) (object);
}



MousepadFile *
mousepad_file_new (GtkTextBuffer *buffer)
{
  MousepadFile *file;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  file = g_object_new (MOUSEPAD_TYPE_FILE, NULL);

  /* set the buffer */
  file->buffer = GTK_TEXT_BUFFER (g_object_ref (buffer));

  return file;
}



static void
mousepad_file_monitor_changed (GFileMonitor      *monitor,
                               GFile             *location,
                               GFile             *other_location,
                               GFileMonitorEvent  event_type,
                               MousepadFile      *file)
{
  GFileInfo *fileinfo;

  /* update readonly status */
  if (event_type == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED
      && G_LIKELY (fileinfo = g_file_query_info (location, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                                                 G_FILE_QUERY_INFO_NONE, NULL, NULL)))
    {
      mousepad_file_set_read_only (file,
        ! g_file_info_get_attribute_boolean (fileinfo, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE));
      g_object_unref (fileinfo);
    }

  /* the file has been externally modified */
  if (event_type == G_FILE_MONITOR_EVENT_CHANGED || event_type == G_FILE_MONITOR_EVENT_CREATED
      || event_type == G_FILE_MONITOR_EVENT_MOVED_IN
      || (event_type == G_FILE_MONITOR_EVENT_RENAMED && g_file_equal (file->location, other_location)))
    g_signal_emit (file, file_signals[EXTERNALLY_MODIFIED], 0);
}



static gboolean
mousepad_file_set_monitor (gpointer data)
{
  MousepadFile *file = data;
  GFile        *location;
  GError       *error = NULL;
  gchar        *path;

  if (file->monitor != NULL)
    {
      g_object_unref (file->monitor);
      file->monitor = NULL;
    }

  if (file->location != NULL && MOUSEPAD_SETTING_GET_BOOLEAN (MONITOR_CHANGES))
    {
      /* monitor the final target in case of a symlink: see
       * https://gitlab.gnome.org/GNOME/glib/-/issues/2421 */
      if (g_file_query_file_type (file->location, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL)
            == G_FILE_TYPE_SYMBOLIC_LINK
          && (path = realpath (mousepad_util_get_path (file->location), NULL)) != NULL)
        {
          location = g_file_new_for_path (path);
          g_free (path);
        }
      else
        location = g_object_ref (file->location);

      file->monitor = g_file_monitor_file (location,
#if GLIB_CHECK_VERSION (2, 56, 2)
                                           G_FILE_MONITOR_WATCH_HARD_LINKS |
#endif
                                           G_FILE_MONITOR_WATCH_MOVES,
                                           NULL, &error);
      g_object_unref (location);

      /* inform the user */
      if (error != NULL)
        {
          g_message ("File monitoring is disabled for file '%s': %s",
                     mousepad_file_get_path (file), error->message);
          g_error_free (error);
        }
      /* watch file for changes */
      else
        g_signal_connect (file->monitor, "changed",
                          G_CALLBACK (mousepad_file_monitor_changed), file);
    }

  return FALSE;
}



void
mousepad_file_set_location (MousepadFile *file,
                            GFile        *location,
                            gboolean      real)
{
  g_return_if_fail (MOUSEPAD_IS_FILE (file));

  /* set location */
  if (file->location == NULL && location != NULL)
    {
      file->location = g_object_ref (location);

      /* mark the document as modified (and therefore savable) in case of a new, empty
       * but localized document */
      if (! g_file_query_exists (location, NULL))
        gtk_text_buffer_set_modified (file->buffer, TRUE);
    }
  /* reset location */
  else if (file->location != NULL && location == NULL)
    {
      g_object_unref (file->location);
      file->location = NULL;
    }
  /* update location */
  else if (file->location != NULL && location != NULL
           && ! g_file_equal (file->location, location))
    {
      g_object_unref (file->location);
      file->location = g_object_ref (location);
    }

  /* not a virtual change, such as when trying to save as */
  if (real)
    {
      /* this is a definitive location */
      file->temporary = FALSE;

      /* activate file monitoring with a delay, to not consider our own saving as
       * external modification after a save as */
      g_timeout_add (MOUSEPAD_SETTING_GET_UINT (MONITOR_DISABLING_TIMER),
                     mousepad_file_set_monitor, mousepad_util_source_autoremove (file));

      /* send a signal that the name has been changed */
      g_signal_emit (file, file_signals[LOCATION_CHANGED], 0, file->location);
    }
  /* toggle location state */
  else
    file->temporary = ! file->temporary;
}



GFile *
mousepad_file_get_location (MousepadFile *file)
{
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), NULL);

  return file->location;
}



gboolean
mousepad_file_location_is_set (MousepadFile *file)
{
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);

  return file->location != NULL;
}



const gchar *
mousepad_file_get_path (MousepadFile *file)
{
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), NULL);

  return mousepad_util_get_path (file->location);
}



gchar *
mousepad_file_get_uri (MousepadFile *file)
{
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), NULL);

  return g_file_get_uri (file->location);
}



static void
mousepad_file_set_read_only (MousepadFile *file,
                             gboolean      readonly)
{
  g_return_if_fail (MOUSEPAD_IS_FILE (file));

  if (G_LIKELY (file->readonly != readonly))
    {
      /* store new value */
      file->readonly = readonly;

      /* emit signal */
      g_signal_emit (file, file_signals[READONLY_CHANGED], 0, readonly);
    }
}



gboolean
mousepad_file_get_read_only (MousepadFile *file)
{
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);

  return file->readonly;
}



gboolean
mousepad_file_is_savable (MousepadFile *file)
{
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);

  return file->location == NULL || gtk_text_buffer_get_modified (file->buffer);
}



void
mousepad_file_set_encoding (MousepadFile     *file,
                            MousepadEncoding  encoding)
{
  g_return_if_fail (MOUSEPAD_IS_FILE (file));

  /* set new encoding */
  file->encoding = encoding;

  /* send a signal that the encoding has been changed */
  g_signal_emit (file, file_signals[ENCODING_CHANGED], 0, file->encoding);
}



MousepadEncoding
mousepad_file_get_encoding (MousepadFile *file)
{
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), MOUSEPAD_ENCODING_NONE);

  return file->encoding;
}



void
mousepad_file_set_write_bom (MousepadFile *file,
                             gboolean      write_bom)
{
  g_return_if_fail (MOUSEPAD_IS_FILE (file));

  /* set new value */
  file->write_bom = write_bom;

  /* set the encoding to UTF-8 if not already compatible */
  if (file->encoding != MOUSEPAD_ENCODING_UTF_7
      && file->encoding != MOUSEPAD_ENCODING_UTF_8
      && file->encoding != MOUSEPAD_ENCODING_UTF_16BE
      && file->encoding != MOUSEPAD_ENCODING_UTF_16LE
      && file->encoding != MOUSEPAD_ENCODING_UTF_32BE
      && file->encoding != MOUSEPAD_ENCODING_UTF_32LE)
    file->encoding = MOUSEPAD_ENCODING_UTF_8;
}



gboolean
mousepad_file_get_write_bom (MousepadFile *file)
{
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);

  return file->write_bom;
}



GtkTextBuffer *
mousepad_file_get_buffer (MousepadFile *file)
{
  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), NULL);

  return file->buffer;
}



void
mousepad_file_set_line_ending (MousepadFile       *file,
                               MousepadLineEnding  line_ending)
{
  g_return_if_fail (MOUSEPAD_IS_FILE (file));

  file->line_ending = line_ending;
}



MousepadLineEnding
mousepad_file_get_line_ending (MousepadFile *file)
{
  return file->line_ending;
}



void
mousepad_file_set_language (MousepadFile *file,
                            const gchar  *language_id)
{
  GtkSourceLanguage *language;
  GtkTextIter        start, end;
  gchar             *data = NULL, *content_type, *basename;
  gboolean           result_uncertain;

  /* the language is set by the user */
  if (language_id != NULL)
    {
      file->user_set_language = TRUE;
      language = gtk_source_language_manager_get_language (
                   gtk_source_language_manager_get_default (), language_id);
      gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (file->buffer), language);

      return;
    }
  /* exit if the user has already set the language during this session... */
  else if (file->user_set_language)
    return;

  /* ... or a previous one */
  mousepad_history_recent_get_language (file->location, &data);
  if (data != NULL)
    {
      file->user_set_language = TRUE;
      language = gtk_source_language_manager_get_language (
                   gtk_source_language_manager_get_default (), data);
      gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (file->buffer), language);
      g_free (data);

      return;
    }

  /* guess language */
  gtk_text_buffer_get_start_iter (file->buffer, &start);
  end = start;
  gtk_text_iter_forward_chars (&end, 255);
  data = gtk_text_buffer_get_text (file->buffer, &start, &end, TRUE);

  content_type = g_content_type_guess (mousepad_file_get_path (file), (const guchar *) data,
                                       strlen (data), &result_uncertain);
  basename = g_file_get_basename (file->location);
  language = gtk_source_language_manager_guess_language (
               gtk_source_language_manager_get_default (), basename,
               result_uncertain ? NULL : content_type);

  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (file->buffer), language);

  g_free (data);
  g_free (basename);
  g_free (content_type);
}



const gchar *
mousepad_file_get_language (MousepadFile *file)
{
  GtkSourceLanguage *language;

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (file->buffer));

  return language != NULL ? gtk_source_language_get_id (language) : MOUSEPAD_LANGUAGE_NONE;
}



gboolean
mousepad_file_get_user_set_language (MousepadFile *file)
{
  return file->user_set_language;
}



gint
mousepad_file_open (MousepadFile  *file,
                    gint           line,
                    gint           column,
                    gboolean       must_exist,
                    gboolean       ignore_bom,
                    gboolean       make_valid,
                    GError       **error)
{
  MousepadEncoding  bom_encoding;
  GtkTextIter       start, end;
  GFile            *location;
  GFileInfo        *fileinfo;
  const gchar      *autosave_uri, *charset, *bom_charset, *endc, *n, *m;
  gchar            *contents = NULL, *etag, *temp;
  gsize             file_size, written, bom_length;
  gint              retval = ERROR_READING_FAILED;

  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (file->buffer), FALSE);
  g_return_val_if_fail (file->location != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* autosave restore */
  if ((autosave_uri = mousepad_object_get_data (file->location, "autosave-uri")) != NULL)
    location = g_file_new_for_uri (autosave_uri);
  else
    location = g_object_ref (file->location);

  /* if the file does not exist and this is allowed, no problem */
  if (! g_file_load_contents (location, NULL, &contents, &file_size, &etag, error)
      && g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) && ! must_exist)
    {
      g_clear_error (error);
      g_object_unref (location);

      return 0;
    }
  /* the file was sucessfully loaded */
  else if (G_LIKELY (error == NULL || *error == NULL))
    {
      /* update etag */
      g_free (file->etag);
      file->etag = etag;

      /* make sure the buffer is empty, in particular for reloading */
      gtk_text_buffer_get_bounds (file->buffer, &start, &end);
      gtk_text_buffer_delete (file->buffer, &start, &end);

      if (G_LIKELY (file_size > 0))
        {
          /* get the encoding charset */
          charset = mousepad_encoding_get_charset (file->encoding);

          /* detect if there is a bom with the encoding type */
          if (! ignore_bom)
            {
              bom_encoding = mousepad_encoding_read_bom (contents, file_size, &bom_length);
              if (G_UNLIKELY (bom_encoding != MOUSEPAD_ENCODING_NONE))
                {
                  /* ask the user what to do if he has set an encoding different from default
                   * (including with respect to GSettings, i.e. UTF-8 only) */
                  bom_charset = mousepad_encoding_get_charset (bom_encoding);
                  if (file->encoding == MOUSEPAD_ENCODING_UTF_8 || file->encoding == bom_encoding
                      || mousepad_dialogs_confirm_encoding (bom_charset, charset) != GTK_RESPONSE_YES)
                    {
                      /* we've found a valid bom at the start of the contents */
                      file->write_bom = TRUE;

                      /* advance the contents offset and decrease size */
                      temp = g_strdup (contents + bom_length);
                      g_free (contents);
                      contents = temp;
                      file_size -= bom_length;

                      /* set the detected encoding */
                      file->encoding = bom_encoding;
                    }
                }
            }

          /* try to convert the contents if needed */
          if (file->encoding != MOUSEPAD_ENCODING_UTF_8)
            {
              temp = g_convert (contents, file_size, "UTF-8", charset, NULL, &written, error);

              /* check if the conversion succeed at least partially */
              if (temp == NULL)
                {
                  /* set return value */
                  retval = ERROR_CONVERTING_FAILED;

                  goto failed;
                }
              /* set new values */
              else
                {
                  file_size = written;
                  g_free (contents);
                  contents = temp;
                }
            }

          if (! g_utf8_validate (contents, file_size, &endc))
          {
            /* leave when the encoding is not valid... */
            if (! make_valid)
              {
                /* set return value */
                retval = ERROR_ENCODING_NOT_VALID;

                /* set an error */
                g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                             _("Invalid byte sequence in conversion input"));

                goto failed;
              }
            /* ... or make it valid and update location for end of valid data */
            else
              {
                temp = g_utf8_make_valid (contents, file_size);
                g_free (contents);
                contents = temp;
                g_utf8_validate (contents, -1, &endc);
              }
          }

          /* detect the line ending, based on the first eol we match */
          for (n = contents; n < endc; n = g_utf8_next_char (n))
            {
              if (G_LIKELY (*n == '\n'))
                {
                  /* set unix line ending */
                  file->line_ending = MOUSEPAD_EOL_UNIX;

                  break;
                }
              else if (*n == '\r')
                {
                  /* get next character */
                  n = g_utf8_next_char (n);

                  /* set dos or mac line ending */
                  file->line_ending = (*n == '\n') ? MOUSEPAD_EOL_DOS : MOUSEPAD_EOL_MAC;

                  break;
                }
            }

          /* get the iter at the beginning of the document */
          gtk_text_buffer_get_start_iter (file->buffer, &start);

          /* insert the file contents in the buffer (for documents with cr line ending) */
          for (n = m = contents; n < endc; n = g_utf8_next_char (n))
            {
              if (G_UNLIKELY (*n == '\r'))
                {
                  /* insert the text in the buffer */
                  if (G_LIKELY (n - m > 0))
                    gtk_text_buffer_insert (file->buffer, &start, m, n - m);

                  /* advance the offset */
                  m = g_utf8_next_char (n);

                  /* insert a new line when the document is not cr+lf */
                  if (*m != '\n')
                    gtk_text_buffer_insert (file->buffer, &start, "\n", 1);
                }
            }

          /* insert the remaining part, or everything for lf line ending */
          if (G_LIKELY (n - m > 0))
            gtk_text_buffer_insert (file->buffer, &start, m, n - m);

          /* place cursor at (line, column) */
          mousepad_util_place_cursor (file->buffer, line, column);
        }

      /* assume everything when file */
      retval = 0;

      /* store the file status */
      if (G_LIKELY (! file->temporary))
        if (G_LIKELY (fileinfo = g_file_query_info (location, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                                                    G_FILE_QUERY_INFO_NONE, NULL, error)))
          {
            mousepad_file_set_read_only (file,
              ! g_file_info_get_attribute_boolean (fileinfo, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE));
            g_object_unref (fileinfo);
          }
        /* set return value */
        else
          retval = ERROR_FILE_STATUS_FAILED;
      /* this is a new document with content from a template */
      else
        {
          g_free (file->etag);
          file->etag = NULL;
        }

      failed:

      /* make sure the buffer is empty if we did not succeed */
      if (G_UNLIKELY (retval != 0))
        {
          gtk_text_buffer_get_bounds (file->buffer, &start, &end);
          gtk_text_buffer_delete (file->buffer, &start, &end);
        }

      /* cleanup */
      g_object_unref (location);
      g_free (contents);

      /* guess and set the file's filetype/language */
      mousepad_file_set_language (file, NULL);

      /* this does not count as a modified buffer */
      gtk_text_buffer_set_modified (file->buffer, FALSE);
    }

  return retval;
}



static gboolean
mousepad_file_monitor_unblock (gpointer data)
{
  MousepadFile *file = data;

  g_signal_handlers_unblock_by_func (file->monitor, mousepad_file_monitor_changed, data);

  return FALSE;
}



static gboolean
mousepad_file_replace_contents (MousepadFile      *m_file,
                                GFile             *file,
                                const char        *contents,
                                gsize              length,
                                const char        *etag,
                                gboolean           make_backup,
                                GFileCreateFlags   flags,
                                char             **new_etag,
                                GCancellable      *cancellable,
                                GError           **error)
{
  gboolean succeed;

  /* suspend file monitoring */
  if (m_file->monitor != NULL)
    g_signal_handlers_block_by_func (m_file->monitor, mousepad_file_monitor_changed, m_file);

  /* replace contents */
  succeed = g_file_replace_contents (file, contents, length, etag, make_backup,
                                     flags, new_etag, cancellable, error);

  /* reactivate file monitoring with a delay, to not consider our own saving as
   * external modification */
  if (m_file->monitor != NULL)
    g_timeout_add (MOUSEPAD_SETTING_GET_UINT (MONITOR_DISABLING_TIMER),
                   mousepad_file_monitor_unblock, mousepad_util_source_autoremove (m_file));

  return succeed;
}



static gboolean
mousepad_file_prepare_save_contents (MousepadFile  *file,
                                     gchar        **out_contents,
                                     gsize         *out_length,
                                     gchar        **out_eol,
                                     GError       **error)
{
  GtkTextIter   start, end;
  gchar       **chunks;
  gchar        *contents, *p, *encoded;
  const gchar  *charset, *eol = NULL;
  gsize         length, written;

  /* get buffer contents */
  gtk_text_buffer_get_bounds (file->buffer, &start, &end);
  contents = gtk_text_buffer_get_slice (file->buffer, &start, &end, TRUE);
  length = strlen (contents);

  /* convert to the encoding if different from UTF-8 */
  if (file->encoding != MOUSEPAD_ENCODING_UTF_8)
    {
      /* get the charset */
      charset = mousepad_encoding_get_charset (file->encoding);
      if (G_UNLIKELY (charset == NULL))
        {
          g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
                       MOUSEPAD_MESSAGE_UNSUPPORTED_ENCODING);
          g_free (contents);

          return FALSE;
        }

      /* convert the content to the user encoding */
      encoded = g_convert (contents, length, charset, "UTF-8", NULL, &written, error);

      /* return if nothing was encoded */
      if (G_UNLIKELY (encoded == NULL))
        {
          g_free (contents);
          return FALSE;
        }

      /* set the new contents */
      g_free (contents);
      contents = encoded;
      length = written;
    }

  /* handle line endings */
  if (file->line_ending == MOUSEPAD_EOL_MAC)
    {
      /* replace the unix with a mac line ending */
      for (p = contents; *p != '\0'; p++)
        if (G_UNLIKELY (*p == '\n'))
          *p = '\r';
    }
  else if (file->line_ending == MOUSEPAD_EOL_DOS)
    {
      /* split the contents into chunks */
      chunks = g_strsplit (contents, "\n", -1);
      g_free (contents);

      /* join the chunks with dos line endings in between */
      contents = g_strjoinv ("\r\n", chunks);
      g_strfreev (chunks);

      /* new contents length */
      length = strlen (contents);
    }

  /* add line ending at end of last line if not present */
  if (length > 0 && MOUSEPAD_SETTING_GET_BOOLEAN (ADD_LAST_EOL))
    {
      switch (file->line_ending)
        {
          case MOUSEPAD_EOL_UNIX:
            if (contents[length - 1] != '\n')
              {
                contents = g_realloc (contents, length + 2);
                contents[length] = '\n';
                length++;
                eol = "\n";
              }
            break;

          case MOUSEPAD_EOL_MAC:
            if (contents[length - 1] != '\r')
              {
                contents = g_realloc (contents, length + 2);
                contents[length] = '\r';
                length++;
                eol = "\r";
              }
            break;

          case MOUSEPAD_EOL_DOS:
            if (contents[length - 1] != '\n' || (length > 1 && contents[length - 2] != '\r'))
              {
                contents = g_realloc (contents, length + 3);
                contents[length] = '\r';
                contents[length + 1] = '\n';
                length += 2;
                eol = "\r\n";
              }
            break;
        }

      contents[length] = '\0';
    }

  /* add a bom at the start of the contents if needed */
  if (file->write_bom)
    mousepad_encoding_write_bom (&(file->encoding), &length, &contents);

  /* assign output variables */
  *out_contents = contents;
  *out_length = length;
  if (out_eol != NULL && eol != NULL)
    *out_eol = g_strdup (eol);

  return TRUE;
}



gboolean
mousepad_file_save (MousepadFile  *file,
                    gboolean       forced,
                    GError       **error)
{
  GtkTextIter  iter;
  gchar       *contents, *eol = NULL, *etag = NULL;
  gsize        length;

  g_return_val_if_fail (MOUSEPAD_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* prepare save contents */
  if (! mousepad_file_prepare_save_contents (file, &contents, &length, &eol, error))
    return FALSE;

  /* write the buffer to the file */
  if (mousepad_file_replace_contents (file, file->location, contents, length,
                                      (file->temporary || forced) ? NULL : file->etag,
                                      MOUSEPAD_SETTING_GET_BOOLEAN (MAKE_BACKUP),
                                      G_FILE_CREATE_NONE, &etag, NULL, error))
    {
      /* update etag */
      g_free (file->etag);
      file->etag = etag;

      /* update read-only status in case of save as */
      if (G_UNLIKELY (file->temporary))
        mousepad_file_set_read_only (file, FALSE);

      /* add last eol if needed */
      if (eol != NULL)
        {
          gtk_text_buffer_get_end_iter (file->buffer, &iter);
          gtk_text_buffer_insert (file->buffer, &iter, eol, -1);
          g_free (eol);
        }

      /* cleanup */
      g_free (contents);
    }
  else
    {
      g_free (contents);
      return FALSE;
    }

  /* everything has been saved */
  gtk_text_buffer_set_modified (file->buffer, FALSE);

  /* re-guess the filetype which could have changed */
  mousepad_file_set_language (file, NULL);

  return TRUE;
}



static void
mousepad_file_autosave_save_finish (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  MousepadFile *file = user_data;
  GError       *error = NULL;

  if (! g_file_replace_contents_finish (G_FILE (source_object), res, NULL, &error))
    {
      g_warning ("Autosave failed: %s", error->message);
      g_error_free (error);
    }
  /* allow new scheduling only if the operation fully succeeded */
  else
    file->autosave_scheduled = FALSE;

  /* decrease application use count in all cases */
  g_application_release (g_application_get_default ());
}



static gboolean
mousepad_file_autosave_save (gpointer data)
{
  MousepadFile *file = data;
  GError       *error = NULL;
  GBytes       *contents;
  gchar        *strcont;
  gsize         length;

  /* autosave cancelled */
  if (! file->autosave_scheduled)
    return FALSE;

  /* prepare save contents */
  if (! mousepad_file_prepare_save_contents (file, &strcont, &length, NULL, &error))
    {
      g_warning ("Autosave failed: %s", error->message);
      g_error_free (error);

      return FALSE;
    }

  /* increase application use count during async saving */
  g_application_hold (g_application_get_default ());

  /* save contents */
  contents = g_bytes_new_take (strcont, length);
  g_file_replace_contents_bytes_async (file->autosave_location, contents, NULL, FALSE,
                                       G_FILE_CREATE_NONE, NULL,
                                       mousepad_file_autosave_save_finish, file);

  /* cleanup */
  g_bytes_unref (contents);

  return FALSE;
}



static void
mousepad_file_autosave_schedule (GtkTextBuffer *buffer,
                                 MousepadFile  *file)
{
  /* we are mainly interested in a switch to the "modified" state, especially when
   * the buffer changes state without real modification: EOL change, BOM change... */
  if (! gtk_text_buffer_get_modified (file->buffer))
    {
      /* cancel any previous scheduled saving */
      file->autosave_scheduled = FALSE;

      return;
    }

  if (! file->autosave_scheduled)
    {
      file->autosave_scheduled = g_timeout_add_seconds (MOUSEPAD_SETTING_GET_UINT (AUTOSAVE_TIMER),
                                                        mousepad_file_autosave_save,
                                                        mousepad_util_source_autoremove (file));
    }
}



static void
mousepad_file_autosave_delete_finish (GObject      *source_object,
                                      GAsyncResult *res,
                                      gpointer      user_data)
{
  GError *error = NULL;

  if (! g_file_delete_finish (G_FILE (source_object), res, &error)
      && ! g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    {
      g_warning ("Autoremove failed: %s", error->message);
      g_error_free (error);
    }

  /* decrease application use count */
  g_application_release (g_application_get_default ());
}



static void
mousepad_file_autosave_delete (GtkTextBuffer *buffer,
                               MousepadFile  *file)
{
  /* we are only interested in a switch to the "unmodified" state, i.e. when the file
   * was regularly saved, or changes were reverted by some means */
  if (gtk_text_buffer_get_modified (file->buffer))
    return;

  /* increase application use count during async delete */
  g_application_hold (g_application_get_default ());

  /* delete file */
  g_file_delete_async (file->autosave_location, G_PRIORITY_DEFAULT, NULL,
                       mousepad_file_autosave_delete_finish, NULL);
}



static void
mousepad_file_autosave_timer_changed (MousepadFile *file)
{
  const gchar *autosave_uri;

  /* disabled -> enabled */
  if (file->autosave_location == NULL && MOUSEPAD_SETTING_GET_UINT (AUTOSAVE_TIMER) > 0)
    {
      /* get autosave location */
      if (file->location != NULL
          && (autosave_uri = mousepad_object_get_data (file->location, "autosave-uri")) != NULL)
        file->autosave_location = g_file_new_for_uri (autosave_uri);
      else
        file->autosave_location = mousepad_history_autosave_get_location ();

      /* schedule a first saving if needed (autosave enabling after startup) */
      if (gtk_text_buffer_get_modified (file->buffer))
        mousepad_file_autosave_schedule (file->buffer, file);

      /* connect to required signals to monitor buffer state */
      g_signal_connect (file->buffer, "changed",
                        G_CALLBACK (mousepad_file_autosave_schedule), file);
      g_signal_connect (file->buffer, "modified-changed",
                        G_CALLBACK (mousepad_file_autosave_schedule), file);
      g_signal_connect (file->buffer, "modified-changed",
                        G_CALLBACK (mousepad_file_autosave_delete), file);
      g_signal_connect (file->buffer, "modified-changed",
                        G_CALLBACK (mousepad_history_session_save), NULL);

    }
  /* enabled -> disabled */
  else if (file->autosave_location != NULL && MOUSEPAD_SETTING_GET_UINT (AUTOSAVE_TIMER) == 0)
    {
      /* reset autosave location */
      g_object_unref (file->autosave_location);
      file->autosave_location = NULL;

      /* disconnect handlers */
      mousepad_disconnect_by_func (file->buffer, mousepad_file_autosave_schedule, file);
      mousepad_disconnect_by_func (file->buffer, mousepad_file_autosave_delete, file);
      mousepad_disconnect_by_func (file->buffer, mousepad_history_session_save, NULL);
    }
}



void
mousepad_file_autosave_init (MousepadFile *file)
{
  /* bind to the settings */
  mousepad_file_autosave_timer_changed (file);
  MOUSEPAD_SETTING_CONNECT_OBJECT (AUTOSAVE_TIMER, mousepad_file_autosave_timer_changed,
                                   file, G_CONNECT_SWAPPED);
}



gboolean
mousepad_file_autosave_location_is_set (MousepadFile *file)
{
  return file->autosave_location != NULL;
}



gchar *
mousepad_file_autosave_get_uri (MousepadFile *file)
{
  return g_file_get_uri (file->autosave_location);
}



gboolean
mousepad_file_autosave_save_sync (MousepadFile *file)
{
  GtkWindow *window;
  GError    *error = NULL;
  gchar     *contents;
  gsize      length;

  /* file already saved */
  if (! file->autosave_scheduled)
    return TRUE;

  /* update autosave state, in particular to prevent any concurrent async saving */
  file->autosave_scheduled = FALSE;

  /* prepare save contents */
  if (! mousepad_file_prepare_save_contents (file, &contents, &length, NULL, &error))
    {
      window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
      mousepad_dialogs_show_error (window, error, MOUSEPAD_MESSAGE_IO_ERROR_SAVE);
      g_error_free (error);

      return FALSE;
    }

  /* save contents */
  if (! g_file_replace_contents (file->autosave_location, contents, length, NULL, FALSE,
                                 G_FILE_CREATE_NONE, NULL, NULL, &error))
    {
      window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
      mousepad_dialogs_show_error (window, error, MOUSEPAD_MESSAGE_IO_ERROR_SAVE);
      g_error_free (error);
      g_free (contents);

      return FALSE;
    }

  /* cleanup */
  g_free (contents);

  return TRUE;
}
