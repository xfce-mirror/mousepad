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
#include <mousepad/mousepad-history.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-window.h>

#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif



static void     mousepad_history_recent_init                (void);
static gboolean mousepad_history_session_external_signal    (gpointer      data);
static void     mousepad_history_session_init               (void);
static guint    mousepad_history_autosave_check_basename    (const gchar  *basename);
static void     mousepad_history_autosave_cleanup_directory (guint         ids);
static void     mousepad_history_autosave_timer_changed     (void);
static void     mousepad_history_autosave_init              (void);



/* recent data */
enum
{
  CURSOR,
  ENCODING,
  LANGUAGE,
  N_RECENT_DATA
};

struct MousepadRecentData
{
  const gchar *str;
  gsize        len;
};

static struct MousepadRecentData recent_data[N_RECENT_DATA];

/* session data */
#define CORRUPTED_SESSION_DATA "Corrupted session data in " MOUSEPAD_ID "." MOUSEPAD_SETTING_SESSION
#define SESSION_N_SIGNALS      3

static gint  session_quitting = MOUSEPAD_SESSION_QUITTING_NO;
static guint session_source_ids[SESSION_N_SIGNALS] = { 0 };

/* autosave data */
#define AUTOSAVE_PREFIX     "autosave-"
#define AUTOSAVE_PREFIX_LEN G_N_ELEMENTS (AUTOSAVE_PREFIX) - 1
#define AUTOSAVE_ORPHANS    "Some '%s*' files in directory '%s/Mousepad' do not correspond to "    \
                            "any session backup anymore. They will not be deleted automatically: " \
                            "please do it manually to remove this warning."

static guint autosave_ids = 0;



void
mousepad_history_init (void)
{
  mousepad_history_recent_init ();
  mousepad_history_session_init ();
  mousepad_history_autosave_init ();
}



static void
mousepad_history_recent_items_changed (void)
{
  if (MOUSEPAD_SETTING_GET_UINT (RECENT_MENU_ITEMS) == 0)
    mousepad_history_recent_clear ();
}



static void
mousepad_history_recent_init (void)
{
  recent_data[CURSOR].str = "Cursor: ";
  recent_data[CURSOR].len = strlen (recent_data[CURSOR].str);

  recent_data[ENCODING].str = "Encoding: ";
  recent_data[ENCODING].len = strlen (recent_data[ENCODING].str);

  recent_data[LANGUAGE].str = "Language: ";
  recent_data[LANGUAGE].len = strlen (recent_data[LANGUAGE].str);

  /* disable and wipe recent history when 'recent-menu-items' is set to 0 */
  mousepad_history_recent_items_changed ();
  MOUSEPAD_SETTING_CONNECT (RECENT_MENU_ITEMS, mousepad_history_recent_items_changed, NULL, 0);
}



void
mousepad_history_recent_add (MousepadFile *file)
{
  GtkRecentData  info;
  GtkTextBuffer *buffer;
  GtkTextIter    iter;
  gchar         *uri, *description;
  const gchar   *language = "", *charset;
  gchar         *cursor;
  static gchar  *groups[] = { PACKAGE_NAME, NULL };

  /* don't insert in the recent history if history disabled */
  if (MOUSEPAD_SETTING_GET_UINT (RECENT_MENU_ITEMS) == 0)
    return;

  /* get the data */
  charset = mousepad_encoding_get_charset (mousepad_file_get_encoding (file));
  buffer = mousepad_file_get_buffer (file);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));
  cursor = g_strdup_printf ("%d:%d",
                            gtk_text_iter_get_line (&iter),
                            mousepad_util_get_real_line_offset (&iter));
  if (mousepad_file_get_user_set_language (file))
    language = mousepad_file_get_language (file);

  /* build description */
  description = g_strdup_printf ("%s%s; %s%s; %s%s;",
                                 recent_data[LANGUAGE].str, language,
                                 recent_data[ENCODING].str, charset,
                                 recent_data[CURSOR].str, cursor);

  /* create the recent info */
  info.display_name = NULL;
  info.description  = description;
  info.mime_type    = "text/plain";
  info.app_name     = PACKAGE_NAME;
  info.app_exec     = PACKAGE " %u";
  info.groups       = groups;
  info.is_private   = FALSE;

  /* add the new recent info to the recent manager */
  uri = mousepad_file_get_uri (file);
  gtk_recent_manager_add_full (gtk_recent_manager_get_default (), uri, &info);

  /* cleanup */
  g_free (description);
  g_free (cursor);
  g_free (uri);
}



static void
mousepad_history_recent_get_data (GFile    *file,
                                  gint      data_type,
                                  gpointer  data)
{
  MousepadEncoding   encoding;
  GtkRecentInfo     *info;
  const gchar       *description, *p, *q, *r;
  gint             **cursor = data;
  gchar            **strv;
  gchar             *str, *m_end, *n_end;
  gint64             m, n;

  /* get file info */
  str = g_file_get_uri (file);
  info = gtk_recent_manager_lookup_item (gtk_recent_manager_get_default (), str, NULL);
  g_free (str);

  /* exit if the given file is not in the recent history... */
  if (info == NULL)
    return;
  /* ... or if the description is null or doesn't look valid */
  else if (G_UNLIKELY (
            (description = gtk_recent_info_get_description (info)) == NULL
            || (p = g_strstr_len (description, -1, recent_data[data_type].str)) == NULL
            || (q = g_strstr_len (r = p + recent_data[data_type].len, -1, ";")) == NULL
          ))
    {
      gtk_recent_info_unref (info);
      return;
    }

  /* try to fill in 'data', leaving it unchanged if that fails */
  str = g_strndup (r, q - r);
  switch (data_type)
    {
    case CURSOR:
      if (g_strstr_len (str, -1, ":") != NULL)
        {
          strv = g_strsplit_set (str, ":", 2);
          m = g_ascii_strtoll (strv[0], &m_end, 10);
          n = g_ascii_strtoll (strv[1], &n_end, 10);
          if (*(strv[0]) != '\0' && *m_end == '\0'
              && *(strv[1]) != '\0' && *n_end == '\0')
            {
              *(cursor[0]) = m;
              *(cursor[1]) = n;
            }

          g_strfreev (strv);
        }

      break;

    case ENCODING:
      if ((encoding = mousepad_encoding_find (str)) != MOUSEPAD_ENCODING_NONE)
        *((gint *) data) = encoding;

      break;

    case LANGUAGE:
      if (g_strcmp0 (str, MOUSEPAD_LANGUAGE_NONE) == 0 ||
          gtk_source_language_manager_get_language (
            gtk_source_language_manager_get_default (), str) != NULL)
        *((gchar **) data) = g_strdup (str);

      break;

    default:
      break;
    }

  /* cleanup */
  g_free (str);
  gtk_recent_info_unref (info);
}



void
mousepad_history_recent_get_language (GFile  *file,
                                      gchar **language)
{
  mousepad_history_recent_get_data (file, LANGUAGE, language);
}



void
mousepad_history_recent_get_encoding (GFile            *file,
                                      MousepadEncoding *encoding)
{
  mousepad_history_recent_get_data (file, ENCODING, encoding);
}



void
mousepad_history_recent_get_cursor (GFile *file,
                                    gint  *line,
                                    gint  *column)
{
  gint *cursor[2] = { line, column };

  mousepad_history_recent_get_data (file, CURSOR, cursor);
}



void
mousepad_history_recent_clear (void)
{
  GtkRecentManager *manager;
  GtkWindow        *window;
  GList            *items, *li;
  GError           *error = NULL;
  const gchar      *uri;

  /* get all the items in the manager */
  manager = gtk_recent_manager_get_default ();
  items = gtk_recent_manager_get_items (manager);

  /* walk through the items */
  for (li = items; li != NULL; li = li->next)
    {
      /* check if the item is in the Mousepad group */
      if (! gtk_recent_info_has_group (li->data, PACKAGE_NAME))
        continue;

      /* get the uri of the recent item */
      uri = gtk_recent_info_get_uri (li->data);

      /* try to remove it, if it fails, break the loop to avoid multiple errors */
      if (G_UNLIKELY (! gtk_recent_manager_remove_item (manager, uri, &error)))
        break;
     }

  /* cleanup */
  g_list_free_full (items, (GDestroyNotify) gtk_recent_info_unref);

  /* print a warning is there is one */
  if (G_UNLIKELY (error != NULL))
    {
      window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
      mousepad_dialogs_show_error (window, error, _("Failed to clear the recent history"));
      g_error_free (error);
    }
}



static void
mousepad_history_session_external_disconnect (GApplication *application)
{
  gint n;

  mousepad_disconnect_by_func (application, mousepad_history_session_external_signal, NULL);
  for (n = 0; n < SESSION_N_SIGNALS; n++)
    if (session_source_ids[n] != 0)
      {
        g_source_remove (session_source_ids[n]);
        session_source_ids[n] = 0;
      }
}



static gboolean
mousepad_history_session_external_signal (gpointer data)
{
  GApplication *application;

  /* inhibit session logout if possible */
  if (data != NULL)
    {
      application = data;
      gtk_application_inhibit (data, gtk_application_get_active_window (data),
                               GTK_APPLICATION_INHIBIT_LOGOUT, "Mousepad is quitting");
    }
  else
    application = g_application_get_default ();

  /* disconnect this handler and remove sources: we do the same thing regardless
   * of the signal received, and we only do it once */
  mousepad_history_session_external_disconnect (application);

  /* quit non-interactively */
  session_quitting = MOUSEPAD_SESSION_QUITTING_NON_INTERACTIVE;
  g_action_group_activate_action (G_ACTION_GROUP (application), "quit", NULL);

  return FALSE;
}



static void
mousepad_history_session_restore_changed (void)
{
  GApplication *application = g_application_get_default ();
  gint          restore;
#ifdef G_OS_UNIX
  gint          n;
  guint         signals[SESSION_N_SIGNALS] = { SIGHUP, SIGINT, SIGTERM };
#endif

  /* disabled or startup -> enabled */
  restore = MOUSEPAD_SETTING_GET_ENUM (SESSION_RESTORE);
  if (autosave_ids == 0 && restore != MOUSEPAD_SESSION_RESTORE_NEVER)
    {
      /* enable autosave if needed and do a first session save */
      if (MOUSEPAD_SETTING_GET_UINT (AUTOSAVE_TIMER) == 0)
        MOUSEPAD_SETTING_RESET (AUTOSAVE_TIMER);

      mousepad_history_session_save ();

      /* register with the session manager */
      g_object_set (application, "register-session", TRUE, NULL);

#ifdef G_OS_UNIX
      /* trap external signals */
      for (n = 0; n < SESSION_N_SIGNALS; n++)
        session_source_ids[n] = g_unix_signal_add (signals[n],
                                                   mousepad_history_session_external_signal,
                                                   NULL);
#endif

      /* be aware of session logout */
      g_signal_connect (application, "query-end",
                        G_CALLBACK (mousepad_history_session_external_signal), NULL);
    }
  /* any state -> disabled */
  else if (restore == MOUSEPAD_SESSION_RESTORE_NEVER)
    {
      /* clear session array, disable autosave */
      MOUSEPAD_SETTING_RESET (SESSION);
      MOUSEPAD_SETTING_SET_UINT (AUTOSAVE_TIMER, 0);

      /* unregister from the session manager */
      g_object_set (application, "register-session", FALSE, NULL);

      /* disconnect handler and remove sources */
      mousepad_history_session_external_disconnect (application);
    }
}



static void
mousepad_history_session_init (void)
{
  mousepad_history_session_restore_changed ();
  MOUSEPAD_SETTING_CONNECT (SESSION_RESTORE, mousepad_history_session_restore_changed, NULL, 0);
}



void
mousepad_history_session_set_quitting (gboolean quitting)
{
  if (session_quitting != MOUSEPAD_SESSION_QUITTING_NON_INTERACTIVE)
    session_quitting = quitting ? MOUSEPAD_SESSION_QUITTING_INTERACTIVE
                                : MOUSEPAD_SESSION_QUITTING_NO;
}



gint
mousepad_history_session_get_quitting (void)
{
  return session_quitting;
}



void
mousepad_history_session_save (void)
{
  MousepadDocument  *document;
  GtkNotebook       *notebook;
  GList             *list, *li;
  gchar            **session;
  gchar             *uri, *autosave_uri;
  const gchar       *fmt;
  guint              length = 0, id, current, n_pages, n;
  gboolean           loc_set, autoloc_set;

  /* exit if session save is blocked or disabled */
  if (session_quitting != MOUSEPAD_SESSION_QUITTING_NO
      || MOUSEPAD_SETTING_GET_ENUM (SESSION_RESTORE) == MOUSEPAD_SESSION_RESTORE_NEVER)
    return;

  /* get the application windows list */
  list = gtk_application_get_windows (GTK_APPLICATION (g_application_get_default ()));

  /* too early, or too late */
  if (list == NULL)
    return;

  /* compute the maximum length of the session array */
  for (li = list; li != NULL; li = li->next)
    {
      notebook = GTK_NOTEBOOK (mousepad_window_get_notebook (li->data));
      length += gtk_notebook_get_n_pages (notebook);
    }

  /* allocate the session array */
  session = g_new0 (gchar *, length + 1);

  /* fill in the session array */
  for (li = list, length = 0; li != NULL; li = li->next)
  {
    id = gtk_application_window_get_id (li->data);
    notebook = GTK_NOTEBOOK (mousepad_window_get_notebook (li->data));
    current = gtk_notebook_get_current_page (notebook);
    n_pages = gtk_notebook_get_n_pages (notebook);
    for (n = 0; n < n_pages; n++)
      {
        document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (notebook, n));
        loc_set = mousepad_file_location_is_set (document->file);
        autoloc_set = mousepad_file_autosave_location_is_set (document->file);
        if (loc_set || autoloc_set)
          {
            if (loc_set)
              uri = mousepad_file_get_uri (document->file);
            else
              uri = g_strdup ("");

            if (autoloc_set && gtk_text_buffer_get_modified (document->buffer))
              autosave_uri = mousepad_file_autosave_get_uri (document->file);
            else
              autosave_uri = g_strdup ("");

            fmt = (n == current ? "%d;%s;+%s" : "%d;%s;%s");
            session[length++] = g_strdup_printf (fmt, id, autosave_uri, uri);
            g_free (uri);
            g_free (autosave_uri);
          }
      }
  }

  /* save the session array */
  MOUSEPAD_SETTING_SET_STRV (SESSION, (const gchar *const *) session);

  /* cleanup */
  g_strfreev (session);
}



gboolean
mousepad_history_session_restore (MousepadApplication *application)
{
  GtkWindow    *window;
  GtkWidget    *notebook;
  GFile       **files;
  GFile        *file, *autosave_file;
  gchar       **session, **p;
  gchar        *autosave_uri, *basename;
  const gchar  *uri;
  guint         n_uris, n_files, n, sid, wid, fid, fids = 0, current;
  gint          restore_setting;
  gboolean      autosaved = FALSE, restore = TRUE, restore_autosaved = TRUE, restored = FALSE;

  /* get the session array */
  session = MOUSEPAD_SETTING_GET_STRV (SESSION);
  n_uris = g_strv_length (session);
  if (n_uris == 0)
   {
      g_strfreev (session);

      /* warn if there are orphans in Mousepad data dir */
      if (autosave_ids != 0)
        g_warning (AUTOSAVE_ORPHANS, AUTOSAVE_PREFIX, g_get_user_data_dir ());

      return FALSE;
   }

  /* see what we have to restore by default */
  restore_setting = MOUSEPAD_SETTING_GET_ENUM (SESSION_RESTORE);
  if (restore_setting == MOUSEPAD_SESSION_RESTORE_CRASH)
    restore = FALSE;
  else if (restore_setting == MOUSEPAD_SESSION_RESTORE_SAVED)
    restore_autosaved = FALSE;

  /* walk the session array in reverse order: last open, first display plan */
  sid = g_signal_lookup ("open", G_TYPE_APPLICATION);
  p = session + n_uris;
  do
    {
      /* get the number of files for this window */
      wid = atoi (*(--p));
      n_uris = 1;
      while (p != session && atoi (*(--p)) == (gint) wid)
        n_uris++;

      /* readjust position */
      if (p != session)
        p++;

      /* allocate the GFile array */
      files = g_new (GFile *, n_uris);

      /* add files from the session array */
      for (n = 0, current = 0, n_files = 0; n < n_uris; n++)
        {
          /* skip the window id */
          uri = g_strstr_len (*(p + n), -1, ";");

          /* guard against corrupted data */
          if (uri == NULL)
            {
              g_warning (CORRUPTED_SESSION_DATA);
              continue;
            }

          /* see if there is a valid autosave uri */
          autosave_uri = NULL;
          autosave_file = NULL;
          if (*(++uri) != ';')
            {
              /* search for the end of the autosave uri */
              autosave_uri = (gchar *) uri;
              uri = g_strstr_len (uri, -1, ";");
              if (uri == NULL)
                {
                  g_warning (CORRUPTED_SESSION_DATA);
                  continue;
                }

              autosave_uri = g_strndup (autosave_uri, uri - autosave_uri);
              autosave_file = g_file_new_for_uri (autosave_uri);

              /* validate file */
              if (! mousepad_util_validate_file (autosave_file))
                {
                  g_warning (CORRUPTED_SESSION_DATA);
                  g_object_unref (autosave_file);

                  continue;
                }
              else
                {
                  /* push validation further, store file id for latter check */
                  basename = g_file_get_basename (autosave_file);
                  if ((fid = mousepad_history_autosave_check_basename (basename)) == (guint) -1)
                    {
                      g_warning (CORRUPTED_SESSION_DATA);
                      g_object_unref (autosave_file);
                      g_free (basename);

                      continue;
                    }
                  else
                    fids |= 1 << fid;

                  g_free (basename);
                }
            }

          /* see if there is a current tab mark */
          if (*(++uri) == '+')
            {
              current = n_files;
              uri++;
            }

          /* see if there is a valid uri */
          file = NULL;
          if (*uri != '\0')
            {
              file = g_file_new_for_uri (uri);
              if (! mousepad_util_validate_file (file))
                {
                  g_warning (CORRUPTED_SESSION_DATA);
                  g_object_unref (file);
                  if (autosave_file != NULL)
                    g_object_unref (autosave_file);

                  if (current == n_files)
                    current = 0;

                  continue;
                }
            }

          /* there is at least one autosaved file to restore */
          if (! autosaved && autosave_file != NULL && g_file_query_exists (autosave_file, NULL))
            {
              autosaved = TRUE;

              /* if the user uses session restore only on crash, ask him if he wants
               * to restore this time */
              if (restore_setting == MOUSEPAD_SESSION_RESTORE_CRASH
                  && mousepad_dialogs_session_restore () != GTK_RESPONSE_NO)
                restore = TRUE;
              /* the same if he normally uses session restore only for saved documents */
              else if (restore_setting == MOUSEPAD_SESSION_RESTORE_SAVED
                       && mousepad_dialogs_session_restore () != GTK_RESPONSE_NO)
                restore_autosaved = TRUE;
            }

          /* add the file or drop it if it was removed since last session */
          if (file != NULL && g_file_query_exists (file, NULL) && (
                restore_setting != MOUSEPAD_SESSION_RESTORE_UNSAVED
                || (autosave_file != NULL && g_file_query_exists (autosave_file, NULL))
              ))
            {
              mousepad_object_set_data_full (file, "autosave-uri", autosave_uri, g_free);
              files[n_files++] = file;
              if (autosave_file != NULL)
                g_object_unref (autosave_file);
            }
          else if (restore_autosaved && autosave_file != NULL
                   && g_file_query_exists (autosave_file, NULL))
            {
              /* keep original uri if it exists */
              if (file != NULL)
                g_object_unref (autosave_file);
              else
                file = autosave_file;

              mousepad_object_set_data_full (file, "autosave-uri", autosave_uri, g_free);
              files[n_files++] = file;
            }
          else
            {
              g_free (autosave_uri);
              if (file != NULL)
                g_object_unref (file);

              if (autosave_file != NULL)
                g_object_unref (autosave_file);

              if (current == n_files)
                current = 0;
            }
        }

      if (n_files > 0 && restore)
        {
          /* try to open files */
          g_signal_emit (application, sid, 0, files, n_files, NULL, NULL);

          /* set current tab, if opening wasn't cancelled for some reason */
          window = gtk_application_get_active_window (GTK_APPLICATION (application));
          if (window != NULL)
            {
              restored = TRUE;
              notebook = mousepad_window_get_notebook (MOUSEPAD_WINDOW (window));
              gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), current);
            }
        }

      /* free the GFile array */
      for (n = 0; n < n_files; n++)
        g_object_unref (files[n]);

      g_free (files);
    }
  while (p != session);

  /* warn if there are orphans in Mousepad data dir */
  if ((autosave_ids & fids) != autosave_ids)
    g_warning (AUTOSAVE_ORPHANS, AUTOSAVE_PREFIX, g_get_user_data_dir ());

  /* cleanup */
  g_strfreev (session);
  if ((restore_setting == MOUSEPAD_SESSION_RESTORE_CRASH && ! restore)
      || (restore_setting == MOUSEPAD_SESSION_RESTORE_SAVED && ! restore_autosaved))
    mousepad_history_autosave_cleanup_directory (fids);

  return restored;
}



static guint
mousepad_history_autosave_check_basename (const gchar *basename)
{
  gchar       *end;
  const gchar *strid;
  guint        id;

  if (g_str_has_prefix (basename, AUTOSAVE_PREFIX))
    {
      strid = basename + AUTOSAVE_PREFIX_LEN;
      id = g_ascii_strtoll (strid, &end, 10);
      if (*(strid) != '\0' && *end == '\0')
        return id;
    }

  return -1;
}



static GDir *
mousepad_history_autosave_open_directory (void)
{
  GDir   *dir;
  GError *error = NULL;
  gchar  *dirname;

  dirname = g_build_filename (g_get_user_data_dir (), "Mousepad", NULL);
  dir = g_dir_open (dirname, 0, &error);
  if (dir == NULL && ! g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      g_critical ("Failed to open directory '%s', autosave disabled: %s",
                  dirname, error->message);
      MOUSEPAD_SETTING_DISCONNECT (AUTOSAVE_TIMER,
                                   mousepad_history_autosave_timer_changed, NULL);
      MOUSEPAD_SETTING_SET_UINT (AUTOSAVE_TIMER, 0);
      g_error_free (error);
    }

  /* cleanup */
  g_free (dirname);

  return dir;
}



static void
mousepad_history_autosave_cleanup_directory (guint ids)
{
  GDir        *dir;
  GError      *error = NULL;
  GFile       *location;
  gchar       *dirname, *filename;
  const gchar *basename;
  guint        id;

  /* try to open Mousepad data dir */
  if ((dir = mousepad_history_autosave_open_directory ()) == NULL)
    return;

  dirname = g_build_filename (g_get_user_data_dir (), "Mousepad", NULL);
  for (basename = g_dir_read_name (dir); basename != NULL; basename = g_dir_read_name (dir))
    if ((id = mousepad_history_autosave_check_basename (basename)) != (guint) -1
        && (ids == (guint) -1 || ids & (1 << id)))
      {
        filename = g_build_filename (dirname, basename, NULL);
        location = g_file_new_for_path (filename);
        if (! g_file_delete (location, NULL, &error))
          {
            g_warning ("Autoremove failed: %s", error->message);
            g_clear_error (&error);
          }

        /* cleanup */
        g_free (filename);
        g_object_unref (location);
      }

  /* cleanup */
  g_free (dirname);
  g_dir_close (dir);
}



static void
mousepad_history_autosave_disable (void)
{
  /* reset autosave ids */
  autosave_ids = 0;

  /* clear the directory */
  mousepad_history_autosave_cleanup_directory (-1);
}



static gboolean
mousepad_history_autosave_enable (void)
{
  GDir        *dir;
  gchar       *dirname;
  const gchar *basename;
  guint        id;

  /* try to create Mousepad data dir if needed */
  dirname = g_build_filename (g_get_user_data_dir (), "Mousepad", NULL);
  if (g_mkdir_with_parents (dirname, 0700) == -1)
    {
      g_critical ("Failed to create directory '%s', autosave disabled", dirname);
      MOUSEPAD_SETTING_DISCONNECT (AUTOSAVE_TIMER,
                                   mousepad_history_autosave_timer_changed, NULL);
      MOUSEPAD_SETTING_SET_UINT (AUTOSAVE_TIMER, 0);
      g_free (dirname);

      return FALSE;
    }
  /* try to open the directory */
  else if ((dir = mousepad_history_autosave_open_directory ()) == NULL)
    return FALSE;

  /* get current file list, store taken ids as flags */
  for (basename = g_dir_read_name (dir); basename != NULL; basename = g_dir_read_name (dir))
    if ((id = mousepad_history_autosave_check_basename (basename)) != (guint) -1)
      autosave_ids |= 1 << id;

  /* cleanup */
  g_free (dirname);
  g_dir_close (dir);

  return TRUE;
}



static void
mousepad_history_autosave_timer_changed (void)
{
  guint timer;

  /* disabled or startup -> enabled */
  timer = MOUSEPAD_SETTING_GET_UINT (AUTOSAVE_TIMER);
  if (autosave_ids == 0 && timer > 0
      && mousepad_history_autosave_enable ()
      && MOUSEPAD_SETTING_GET_ENUM (SESSION_RESTORE) == MOUSEPAD_SESSION_RESTORE_NEVER)
    MOUSEPAD_SETTING_RESET (SESSION_RESTORE);
  /* any state -> disabled */
  else if (timer == 0)
    {
      MOUSEPAD_SETTING_SET_ENUM (SESSION_RESTORE, MOUSEPAD_SESSION_RESTORE_NEVER);
      mousepad_history_autosave_disable ();
    }
}



static void
mousepad_history_autosave_init (void)
{
  mousepad_history_autosave_timer_changed ();
  MOUSEPAD_SETTING_CONNECT (AUTOSAVE_TIMER, mousepad_history_autosave_timer_changed, NULL, 0);
}



GFile *
mousepad_history_autosave_get_location (void)
{
  GFile *location;
  gchar *basename, *filename;

  static guint autosave_id = 0;

  /* update autosave id */
  while (autosave_ids & (1 << autosave_id++));

  /* build location */
  basename = g_strdup_printf (AUTOSAVE_PREFIX "%d", autosave_id - 1);
  filename = g_build_filename (g_get_user_data_dir (), "Mousepad", basename, NULL);
  location = g_file_new_for_path (filename);
  g_free (basename);
  g_free (filename);

  return location;
}
