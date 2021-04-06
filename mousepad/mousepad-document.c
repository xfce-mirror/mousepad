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
#include <mousepad/mousepad-close-button.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-view.h>
#include <mousepad/mousepad-window.h>



static void      mousepad_document_finalize                (GObject                *object);
static void      mousepad_document_notify_cursor_position  (MousepadDocument       *document);
static void      mousepad_document_notify_encoding         (MousepadFile           *file,
                                                            MousepadEncoding        encoding,
                                                            MousepadDocument       *document);
static void      mousepad_document_notify_language         (GtkSourceBuffer        *buffer,
                                                            GParamSpec             *pspec,
                                                            MousepadDocument       *document);
static void      mousepad_document_notify_overwrite        (GtkTextView            *textview,
                                                            GParamSpec             *pspec,
                                                            MousepadDocument       *document);
static void      mousepad_document_drag_data_received      (GtkWidget              *widget,
                                                            GdkDragContext         *context,
                                                            gint                    x,
                                                            gint                    y,
                                                            GtkSelectionData       *selection_data,
                                                            guint                   info,
                                                            guint                   drag_time,
                                                            MousepadDocument       *document);
static void      mousepad_document_location_changed        (MousepadDocument       *document,
                                                            GFile                  *file);
static void      mousepad_document_label_color             (MousepadDocument       *document);
static void      mousepad_document_tab_button_clicked      (GtkWidget              *widget,
                                                            MousepadDocument       *document);
static void      mousepad_document_search_completed        (GObject                *object,
                                                            GAsyncResult           *result,
                                                            gpointer                data);
static void      mousepad_document_emit_search_signal      (MousepadDocument       *document,
                                                            GParamSpec             *pspec,
                                                            GtkSourceSearchContext *search_context);
static void      mousepad_document_search_widget_visible   (MousepadDocument       *document,
                                                            GParamSpec             *pspec,
                                                            MousepadWindow         *window);



enum
{
  CLOSE_TAB,
  CURSOR_CHANGED,
  ENCODING_CHANGED,
  LANGUAGE_CHANGED,
  OVERWRITE_CHANGED,
  SEARCH_COMPLETED,
  LAST_SIGNAL
};

struct _MousepadDocumentClass
{
  GtkScrolledWindowClass __parent__;
};

struct _MousepadDocumentPrivate
{
  GtkScrolledWindow      __parent__;

  /* the tab label, ebox and CSS provider */
  GtkWidget              *ebox;
  GtkWidget              *label;
  GtkCssProvider         *css_provider;

  /* utf-8 valid document names */
  gchar                  *utf8_filename;
  gchar                  *utf8_basename;

  /* search contexts */
  GtkSourceSearchContext *search_context, *selection_context;
  GtkSourceBuffer        *selection_buffer;
};



static guint document_signals[LAST_SIGNAL];



MousepadDocument *
mousepad_document_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_DOCUMENT, NULL);
}



G_DEFINE_TYPE_WITH_PRIVATE (MousepadDocument, mousepad_document, GTK_TYPE_SCROLLED_WINDOW)



static void
mousepad_document_class_init (MousepadDocumentClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_document_finalize;

  document_signals[CLOSE_TAB] =
    g_signal_new (I_("close-tab"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  document_signals[CURSOR_CHANGED] =
    g_signal_new (I_("cursor-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, _mousepad_marshal_VOID__INT_INT_INT,
                  G_TYPE_NONE, 3, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

  document_signals[ENCODING_CHANGED] =
    g_signal_new (I_("encoding-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__ENUM, G_TYPE_NONE, 1, G_TYPE_INT);

  document_signals[LANGUAGE_CHANGED] =
    g_signal_new (I_("language-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_SOURCE_TYPE_LANGUAGE);

  document_signals[OVERWRITE_CHANGED] =
    g_signal_new (I_("overwrite-changed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  document_signals[SEARCH_COMPLETED] =
    g_signal_new (I_("search-completed"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, _mousepad_marshal_VOID__INT_STRING_FLAGS,
                  G_TYPE_NONE, 3, G_TYPE_INT, G_TYPE_STRING, MOUSEPAD_TYPE_SEARCH_FLAGS);
}



static void
mousepad_document_post_init (MousepadDocument *document)
{
  GtkWidget *window;
  gboolean   visible;

  /* disconnect this handler */
  mousepad_disconnect_by_func (document, mousepad_document_post_init, NULL);

  /* get the ancestor MousepadWindow */
  window = gtk_widget_get_ancestor (GTK_WIDGET (document), MOUSEPAD_TYPE_WINDOW);

  /* there might not be a MousepadWindow ancestor, e.g. when the document is packed
   * in a MousepadEncodingDialog */
  if (window != NULL)
    {
      /* bind some search properties to the "search-widget-visible" window property */
      g_signal_connect_object (window, "notify::search-widget-visible",
                               G_CALLBACK (mousepad_document_search_widget_visible),
                               document, G_CONNECT_SWAPPED);

      /* get the window property */
      g_object_get (window, "search-widget-visible", &visible, NULL);

      /* block search context handlers, so that they are unblocked below without warnings */
      if (visible)
        {
          g_signal_handlers_block_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                           g_signal_lookup ("insert-text", GTK_TYPE_TEXT_BUFFER),
                                           0, NULL, NULL, document->priv->search_context);
          g_signal_handlers_block_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                           g_signal_lookup ("delete-range", GTK_TYPE_TEXT_BUFFER),
                                           0, NULL, NULL, document->priv->search_context);
        }

      /* initialize binding */
      mousepad_document_search_widget_visible (document, NULL, MOUSEPAD_WINDOW (window));
    }
}



static void
mousepad_document_init (MousepadDocument *document)
{
  GtkTargetList           *target_list;
  GtkSourceSearchSettings *search_settings;

  /* we will complete initialization when the document is anchored */
  g_signal_connect (document, "hierarchy-changed", G_CALLBACK (mousepad_document_post_init), NULL);

  /* private structure */
  document->priv = mousepad_document_get_instance_private (document);

  /* initialize the variables */
  document->priv->utf8_filename = NULL;
  document->priv->utf8_basename = NULL;
  document->priv->label = NULL;
  document->priv->css_provider = gtk_css_provider_new ();
  document->priv->selection_context = NULL;
  document->priv->selection_buffer = NULL;

  /* setup the scrolled window */
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (document),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (document), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (document), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (document), NULL);

  /* create a textbuffer and associated search context */
  document->buffer = GTK_TEXT_BUFFER (gtk_source_buffer_new (NULL));
  document->priv->search_context = gtk_source_search_context_new (
                                     GTK_SOURCE_BUFFER (document->buffer), NULL);

  /* bind search settings to Mousepad settings, except "regex-enabled" to prevent prohibitive
   * computation times in some situations (see
   * mousepad_document_prevent_endless_scanning() below) */
  search_settings = gtk_source_search_context_get_settings (document->priv->search_context);
  MOUSEPAD_SETTING_BIND (SEARCH_WRAP_AROUND, search_settings,
                         "wrap-around", G_SETTINGS_BIND_GET);
  MOUSEPAD_SETTING_BIND (SEARCH_MATCH_CASE, search_settings,
                         "case-sensitive", G_SETTINGS_BIND_GET);
  MOUSEPAD_SETTING_BIND (SEARCH_MATCH_WHOLE_WORD, search_settings,
                         "at-word-boundaries", G_SETTINGS_BIND_GET);

  /* forward the search context signal */
  g_signal_connect_swapped (document->priv->search_context, "notify::occurrences-count",
                            G_CALLBACK (mousepad_document_emit_search_signal), document);

  /* initialize the file */
  document->file = mousepad_file_new (document->buffer);
  g_signal_connect_swapped (document->file, "location-changed",
                            G_CALLBACK (mousepad_document_location_changed), document);

  /* setup the textview */
  document->textview = g_object_new (MOUSEPAD_TYPE_VIEW, "buffer", document->buffer, NULL);
  gtk_container_add (GTK_CONTAINER (document), GTK_WIDGET (document->textview));
  gtk_widget_show (GTK_WIDGET (document->textview));

  /* also allow dropping of uris and tabs in the textview */
  target_list = gtk_drag_dest_get_target_list (GTK_WIDGET (document->textview));
  gtk_target_list_add_table (target_list, drop_targets, G_N_ELEMENTS (drop_targets));

  /* connect handlers to the document attribute signals */
  g_signal_connect_swapped (document->buffer, "modified-changed",
                            G_CALLBACK (mousepad_document_label_color), document);
  g_signal_connect_swapped (document->file, "readonly-changed",
                            G_CALLBACK (mousepad_document_label_color), document);
  g_signal_connect_swapped (document->textview, "notify::editable",
                            G_CALLBACK (mousepad_document_label_color), document);
  g_signal_connect (document->textview, "drag-data-received",
                    G_CALLBACK (mousepad_document_drag_data_received), document);

  /* forward some document attribute signals more or less directly */
  g_signal_connect_swapped (document->buffer, "notify::cursor-position",
                            G_CALLBACK (mousepad_document_notify_cursor_position), document);
  MOUSEPAD_SETTING_CONNECT_OBJECT (TAB_WIDTH,
                                   G_CALLBACK (mousepad_document_notify_cursor_position),
                                   document, G_CONNECT_SWAPPED);
  g_signal_connect (document->file, "encoding-changed",
                    G_CALLBACK (mousepad_document_notify_encoding), document);
  g_signal_connect (document->buffer, "notify::language",
                    G_CALLBACK (mousepad_document_notify_language), document);
  g_signal_connect (document->textview, "notify::overwrite",
                    G_CALLBACK (mousepad_document_notify_overwrite), document);
}



static void
mousepad_document_post_finalize (GtkSourceSearchContext *search_context,
                                 GParamSpec             *pspec,
                                 gpointer                buffer)
{
  g_object_unref (search_context);
  g_object_unref (buffer);
}



static void
mousepad_document_finalize_search (MousepadDocument       *document,
                                   gpointer                buffer,
                                   GtkSourceSearchContext *search_context)
{
  GtkSourceSearchSettings *search_settings;

  /* disconnect any handler connected to the buffer or the search context, to not interfer
   * with what follows */
  g_signal_handlers_disconnect_by_data (search_context, document);
  g_signal_handlers_disconnect_by_data (buffer, document);

  /* reset some critical settings to ensure that the last search is almost instantaneous
   * (see mousepad_document_prevent_endless_scanning() below) */
  search_settings = gtk_source_search_context_get_settings (search_context);
  gtk_source_search_context_set_highlight (search_context, FALSE);
  gtk_source_search_settings_set_regex_enabled (search_settings, FALSE);

  /* cancel any current search by launching a last one, the result of which will trigger
   * post_finalize() */
  g_signal_connect (search_context, "notify::occurrences-count",
                    G_CALLBACK (mousepad_document_post_finalize), buffer);
  gtk_source_search_settings_set_search_text (search_settings, NULL);
}



static void
mousepad_document_finalize (GObject *object)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (object);

  /* cleanup */
  g_free (document->priv->utf8_filename);
  g_free (document->priv->utf8_basename);
  g_object_unref (document->priv->css_provider);

  /* release the file */
  g_object_unref (document->file);

  /*
   * We will release buffers and search contexts when a last search is completed,
   * including buffers scanning, to prevent too late accesses to the buffers.
   */
  mousepad_document_finalize_search (document, document->buffer, document->priv->search_context);
  if (GTK_SOURCE_IS_BUFFER (document->priv->selection_buffer))
    mousepad_document_finalize_search (document, document->priv->selection_buffer,
                                       document->priv->selection_context);

  (*G_OBJECT_CLASS (mousepad_document_parent_class)->finalize) (object);
}



static void
mousepad_document_notify_cursor_position (MousepadDocument *document)
{
  GtkTextIter iter;
  gint        line, column, selection;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* get the current iter position */
  gtk_text_buffer_get_iter_at_mark (document->buffer, &iter,
                                    gtk_text_buffer_get_insert (document->buffer));

  /* get the current line number */
  line = gtk_text_iter_get_line (&iter) + 1;

  /* get the column */
  column = mousepad_util_get_real_line_offset (&iter);

  /* get length of the selection */
  selection = mousepad_view_get_selection_length (document->textview);

  /* emit the signal */
  g_signal_emit (document, document_signals[CURSOR_CHANGED], 0, line, column, selection);
}



static void
mousepad_document_notify_encoding (MousepadFile     *file,
                                   MousepadEncoding  encoding,
                                   MousepadDocument *document)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* emit the signal */
  g_signal_emit (document, document_signals[ENCODING_CHANGED], 0, encoding);
}



static void
mousepad_document_notify_language (GtkSourceBuffer  *buffer,
                                   GParamSpec       *pspec,
                                   MousepadDocument *document)
{
  GtkSourceLanguage *language;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

  /* the new language */
  language = gtk_source_buffer_get_language (buffer);

  /* emit the signal */
  g_signal_emit (document, document_signals[LANGUAGE_CHANGED], 0, language);
}



static void
mousepad_document_notify_overwrite (GtkTextView      *textview,
                                    GParamSpec       *pspec,
                                    MousepadDocument *document)
{
  gboolean overwrite;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_TEXT_VIEW (textview));

  /* whether overwrite is enabled */
  overwrite = gtk_text_view_get_overwrite (textview);

  /* emit the signal */
  g_signal_emit (document, document_signals[OVERWRITE_CHANGED], 0, overwrite);
}



void
mousepad_document_send_signals (MousepadDocument *document)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* re-send the cursor changed signal */
  mousepad_document_notify_cursor_position (document);

  /* re-send the encoding signal */
  mousepad_document_notify_encoding (document->file,
                                     mousepad_file_get_encoding (document->file), document);

  /* re-send the language signal */
  mousepad_document_notify_language (GTK_SOURCE_BUFFER (document->buffer), NULL, document);

  /* re-send the overwrite signal */
  mousepad_document_notify_overwrite (GTK_TEXT_VIEW (document->textview), NULL, document);
}



static void
mousepad_document_drag_data_received (GtkWidget        *widget,
                                      GdkDragContext   *context,
                                      gint              x,
                                      gint              y,
                                      GtkSelectionData *selection_data,
                                      guint             info,
                                      guint             drag_time,
                                      MousepadDocument *document)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* emit the drag-data-received signal from the document when a tab or uri has been dropped */
  if (info == TARGET_TEXT_URI_LIST || info == TARGET_GTK_NOTEBOOK_TAB)
    g_signal_emit_by_name (document, "drag-data-received", context,
                           x, y, selection_data, info, drag_time);
}



static void
mousepad_document_location_changed (MousepadDocument *document,
                                    GFile            *file)
{
  gchar       *utf8_filename, *utf8_short_filename, *utf8_basename;
  const gchar *home;
  size_t       home_len;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (file != NULL);

  /* convert the title into a utf-8 valid version for display */
  utf8_filename = g_filename_to_utf8 (mousepad_util_get_path (file), -1, NULL, NULL, NULL);
  if (G_LIKELY (utf8_filename))
    {
      /* create a shorter display filename: replace $HOME with a tilde if user is not root */
      if (geteuid () && (home = g_get_home_dir ()) && (home_len = strlen (home))
          && g_str_has_prefix (utf8_filename, home))
        {
          utf8_short_filename = g_strconcat ("~", utf8_filename + home_len, NULL);
          g_free (utf8_filename);
          utf8_filename = utf8_short_filename;
        }

      /* create the display name */
      utf8_basename = g_filename_display_basename (utf8_filename);

      /* remove the old names */
      g_free (document->priv->utf8_filename);
      g_free (document->priv->utf8_basename);

      /* set the new names */
      document->priv->utf8_filename = utf8_filename;
      document->priv->utf8_basename = utf8_basename;

      /* update the tab label and tooltip */
      if (G_UNLIKELY (document->priv->label))
        {
          /* set the tab label */
          gtk_label_set_text (GTK_LABEL (document->priv->label), utf8_basename);

          /* set the tab tooltip */
          gtk_widget_set_tooltip_text (document->priv->ebox, utf8_filename);

          /* update label color */
          mousepad_document_label_color (document);
        }
    }
}



static void
mousepad_document_label_color (MousepadDocument *document)
{
  GtkStyleContext *context;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (document->buffer));
  g_return_if_fail (MOUSEPAD_IS_FILE (document->file));

  if (document->priv->label)
    {
      context = gtk_widget_get_style_context (document->priv->label);

      /* grey out the label text */
      if (mousepad_file_get_read_only (document->file)
          || ! gtk_text_view_get_editable (GTK_TEXT_VIEW (document->textview)))
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_DIM_LABEL);
      else
        gtk_style_context_remove_class (context, GTK_STYLE_CLASS_DIM_LABEL);

      /* change the label text color */
      if (gtk_text_buffer_get_modified (document->buffer))
        {
          gtk_css_provider_load_from_data (document->priv->css_provider,
                                           "label { color: red; }", -1, NULL);
          gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (document->priv->css_provider),
                                          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }
      else
        gtk_style_context_remove_provider (context, GTK_STYLE_PROVIDER (document->priv->css_provider));
    }
}



void
mousepad_document_set_overwrite (MousepadDocument *document,
                                 gboolean          overwrite)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  gtk_text_view_set_overwrite (GTK_TEXT_VIEW (document->textview), overwrite);
}



void
mousepad_document_focus_textview (MousepadDocument *document)
{
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* focus the textview */
  gtk_widget_grab_focus (GTK_WIDGET (document->textview));
}



GtkWidget *
mousepad_document_get_tab_label (MousepadDocument *document)
{
  GtkWidget  *hbox, *button;

  /* create the box */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (hbox);

  /* the ebox */
  document->priv->ebox = g_object_new (GTK_TYPE_EVENT_BOX, "border-width", 2,
                                       "visible-window", FALSE, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), document->priv->ebox, TRUE, TRUE, 0);
  gtk_widget_set_tooltip_text (document->priv->ebox, document->priv->utf8_filename);
  gtk_widget_show (document->priv->ebox);

  /* create the label */
  document->priv->label = gtk_label_new (mousepad_document_get_basename (document));
  gtk_label_set_ellipsize (GTK_LABEL (document->priv->label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_container_add (GTK_CONTAINER (document->priv->ebox), document->priv->label);
  gtk_widget_show (document->priv->label);

  /* set label color */
  mousepad_document_label_color (document);

  /* create the button */
  button = mousepad_close_button_new ();
  gtk_widget_show (button);

  /* pack button, add signal and tooltip */
  gtk_widget_set_tooltip_text (button, _("Close this tab"));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (mousepad_document_tab_button_clicked), document);

  return hbox;
}



static void
mousepad_document_tab_button_clicked (GtkWidget        *widget,
                                      MousepadDocument *document)
{
  g_signal_emit (document, document_signals[CLOSE_TAB], 0);
}



const gchar *
mousepad_document_get_basename (MousepadDocument *document)
{
  static gint untitled_counter = 0;

  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), NULL);

  /* check if there is a filename set */
  if (document->priv->utf8_basename == NULL)
    {
      /* create an unique untitled document name */
      document->priv->utf8_basename = g_strdup_printf ("%s %d", _("Untitled"), ++untitled_counter);
    }

  return document->priv->utf8_basename;
}



const gchar *
mousepad_document_get_filename (MousepadDocument *document)
{
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), NULL);

  return document->priv->utf8_filename;
}



static void
mousepad_document_search_completed (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      data)
{
  MousepadDocument        *document;
  GtkSourceSearchContext  *search_context = GTK_SOURCE_SEARCH_CONTEXT (object);
  GtkSourceSearchSettings *search_settings;
  GtkTextBuffer           *selection_buffer;
  MousepadSearchFlags      flags;
  GtkTextIter              iter, start, end;
  gchar                   *selected_text;
  const gchar             *string, *replace;
  gboolean                 found;

  /* exit if the document or the view were destroyed during the search process */
  if (! MOUSEPAD_IS_DOCUMENT (data)
      || ! MOUSEPAD_IS_VIEW (MOUSEPAD_DOCUMENT (data)->textview))
    return;
  else
    document = MOUSEPAD_DOCUMENT (data);

  /* retrieve the first stage data */
  flags = GPOINTER_TO_INT (mousepad_object_get_data (search_context, "flags"));
  replace = mousepad_object_get_data (search_context, "replace");
  search_settings = gtk_source_search_context_get_settings (search_context);
  string = gtk_source_search_settings_get_search_text (search_settings);

  if (flags & MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START)
    gtk_text_buffer_get_selection_bounds (document->buffer, &iter, NULL);
  else
    gtk_text_buffer_get_selection_bounds (document->buffer, NULL, &iter);

  /* get the search result */
#if GTK_SOURCE_MAJOR_VERSION >= 4
  if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
    found = gtk_source_search_context_backward_finish (search_context, result,
                                                       &start, &end, NULL, NULL);
  else
    found = gtk_source_search_context_forward_finish (search_context, result,
                                                      &start, &end, NULL, NULL);
#else
  if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
    found = gtk_source_search_context_backward_finish2 (search_context, result,
                                                        &start, &end, NULL, NULL);
  else
    found = gtk_source_search_context_forward_finish2 (search_context, result,
                                                       &start, &end, NULL, NULL);
#endif

  /* force the signal emission, to cover cases where Mousepad search settings change without
   * changing GtkSourceView settings (e.g. when switching between single-document mode and
   * multi-document mode) */
  if (gtk_source_search_context_get_occurrences_count (search_context) != -1)
    g_object_notify (G_OBJECT (search_context), "occurrences-count");

  /* handle the action */
  if (found && (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT)
      && ! (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION))
    gtk_text_buffer_select_range (document->buffer, &start, &end);
  else if (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE)
    {
      if (found && ! (flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA))
        {
          /* replace selected occurrence */
#if GTK_SOURCE_MAJOR_VERSION >= 4
          gtk_source_search_context_replace (search_context, &start, &end, replace, -1, NULL);
#else
          gtk_source_search_context_replace2 (search_context, &start, &end, replace, -1, NULL);
#endif

          /* select next occurrence */
          flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;
          flags &= ~ MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE;
          mousepad_document_search (document, string, NULL, flags);
          return;
        }
      else if (flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA)
        {
          /* replace all occurrences in the buffer in use */
          gtk_source_search_context_replace_all (search_context, replace, -1, NULL);

          if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION)
            {
              /* get the text in the virtual selection buffer */
              selection_buffer = GTK_TEXT_BUFFER (gtk_source_search_context_get_buffer (search_context));
              gtk_text_buffer_get_bounds (selection_buffer, &start, &end);
              selected_text = gtk_text_buffer_get_text (selection_buffer, &start, &end, FALSE);

              /* replace selection in the real buffer by the text in the virtual selection buffer */
              gtk_text_buffer_get_selection_bounds (document->buffer, &start, &end);
              gtk_text_buffer_begin_user_action (document->buffer);
              gtk_text_buffer_delete (document->buffer, &start, &end);
              gtk_text_buffer_insert (document->buffer, &start, selected_text, -1);
              gtk_text_buffer_end_user_action (document->buffer);
              g_free (selected_text);
            }
        }
    }
  else if (! (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION))
    gtk_text_buffer_place_cursor (document->buffer, &iter);
}



void
mousepad_document_search (MousepadDocument    *document,
                          const gchar         *string,
                          const gchar         *replace,
                          MousepadSearchFlags  flags)
{
  GtkSourceSearchContext  *search_context;
  GtkSourceSearchSettings *search_settings, *search_settings_doc;
  GtkTextIter              iter, start, end;
  gchar                   *selected_text;
  const gchar             *reference = "";
  gboolean                 has_references;

  /* get the search iter */
  if (flags & MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START)
    gtk_text_buffer_get_selection_bounds (document->buffer, &iter, NULL);
  else
    gtk_text_buffer_get_selection_bounds (document->buffer, NULL, &iter);

  /* search in selected text only: substitute a virtual selection buffer to the real buffer
   * and work inside */
  if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION)
    {
      /* initialize selection buffer and search context if needed */
      if (! GTK_SOURCE_IS_BUFFER (document->priv->selection_buffer))
        {
          document->priv->selection_buffer = gtk_source_buffer_new (NULL);
          document->priv->selection_context =
            gtk_source_search_context_new (document->priv->selection_buffer, NULL);
        }

      /* get the text in the selection area */
      gtk_text_buffer_get_selection_bounds (document->buffer, &start, &end);
      selected_text = gtk_text_buffer_get_text (document->buffer, &start, &end, FALSE);

      /* put the text in the selection buffer */
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (document->priv->selection_buffer),
                                selected_text, -1);
      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (document->priv->selection_buffer), &iter);
      g_free (selected_text);

      /* associate the search context */
      search_context = document->priv->selection_context;
      gtk_source_search_context_set_highlight (search_context, FALSE);
      g_signal_connect_swapped (search_context, "notify::occurrences-count",
                                G_CALLBACK (mousepad_document_emit_search_signal), document);

      /* copy search settings from those of the document */
      search_settings = gtk_source_search_context_get_settings (search_context);
      search_settings_doc = gtk_source_search_context_get_settings (document->priv->search_context);

      gtk_source_search_settings_set_case_sensitive (
        search_settings, gtk_source_search_settings_get_case_sensitive (search_settings_doc));
      gtk_source_search_settings_set_at_word_boundaries (
        search_settings, gtk_source_search_settings_get_at_word_boundaries (search_settings_doc));
      gtk_source_search_settings_set_regex_enabled (
        search_settings, gtk_source_search_settings_get_regex_enabled (search_settings_doc));
    }
  /* search in the whole document */
  else
    search_context = document->priv->search_context;

  /* set the string to search for */
  search_settings = gtk_source_search_context_get_settings (search_context);
  gtk_source_search_settings_set_search_text (search_settings, string);

  /* set wrap around: always true for the search bar, bound to GSettings otherwise */
  gtk_source_search_settings_set_wrap_around (search_settings,
                                              (flags & MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND)
                                              || MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_WRAP_AROUND));

  /* special treatments if regex search is enabled */
  if (gtk_source_search_settings_get_regex_enabled (search_settings))
    {
      /* disable highlight to prevent prohibitive computation times in some situations
       * (see mousepad_document_prevent_endless_scanning() below) */
      gtk_source_search_context_set_highlight (search_context, FALSE);

      /* trick gtk_source_search_context_replace_all() into thinking its replacement text
       * contains back references even if it does not, so that g_regex_replace() is always
       * used to replace text in this function, which finally behaves the same as
       * gtk_source_search_context_replace(): see
       * https://gitlab.gnome.org/GNOME/gtksourceview/-/issues/172 */
      if (replace != NULL && (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE)
          && (flags & MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA)
          && g_regex_check_replacement (replace, &has_references, NULL)
          && ! has_references)
        reference = "\\g<1>";
    }

  /* attach some data for the second stage */
  mousepad_object_set_data (search_context, "flags", GINT_TO_POINTER (flags));
  mousepad_object_set_data_full (search_context, "replace",
                                 g_strconcat (reference, replace, NULL), g_free);

  /* search the string */
  if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
    gtk_source_search_context_backward_async (search_context, &iter, NULL,
                                              mousepad_document_search_completed, document);
  else
    gtk_source_search_context_forward_async (search_context, &iter, NULL,
                                             mousepad_document_search_completed, document);
}



static void
mousepad_document_emit_search_signal (MousepadDocument       *document,
                                      GParamSpec             *pspec,
                                      GtkSourceSearchContext *search_context)
{
  GtkSourceSearchSettings *search_settings;
  MousepadSearchFlags      flags;
  gint                     n_matches;
  const gchar             *string;

  /* retrieve data */
  flags = GPOINTER_TO_INT (mousepad_object_get_data (search_context, "flags"));
  n_matches = gtk_source_search_context_get_occurrences_count (search_context);
  search_settings = gtk_source_search_context_get_settings (search_context);
  string = gtk_source_search_settings_get_search_text (search_settings);

  /* emit the signal */
  g_signal_emit (document, document_signals[SEARCH_COMPLETED], 0, n_matches, string, flags);
}



static void
mousepad_document_scanning_started (MousepadDocument *document)
{
  gtk_source_search_context_set_highlight (document->priv->search_context, FALSE);
}



static void
mousepad_document_scanning_completed (MousepadDocument *document)
{
  gtk_source_search_context_set_highlight (document->priv->search_context, TRUE);
}



/*
 * Disable highlight during regex searches: a workaround to prevent prohibitive computation
 * times in some situations (see also mousepad_document_search() and mousepad_document_init()
 * above).
 * See https://gitlab.gnome.org/GNOME/gtksourceview/-/issues/164
 */
static void
mousepad_document_prevent_endless_scanning (MousepadDocument *document,
                                            gboolean          visible)
{
  if (visible && MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_HIGHLIGHT_ALL)
      && MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_ENABLE_REGEX))
    {
      g_signal_connect_swapped (document->buffer, "insert-text",
                                G_CALLBACK (mousepad_document_scanning_started), document);
      g_signal_connect_swapped (document->buffer, "delete-range",
                                G_CALLBACK (mousepad_document_scanning_started), document);
      g_signal_connect_swapped (document->priv->search_context, "notify::occurrences-count",
                                G_CALLBACK (mousepad_document_scanning_completed), document);
    }
  else
    {
      mousepad_disconnect_by_func (document->buffer, mousepad_document_scanning_started, document);
      mousepad_disconnect_by_func (document->priv->search_context,
                                   mousepad_document_scanning_completed, document);
    }
}



static void
mousepad_document_search_widget_visible (MousepadDocument *document,
                                         GParamSpec       *pspec,
                                         MousepadWindow   *window)
{
  GtkSourceSearchSettings *search_settings;
  gboolean                 visible;

  /* get the window property */
  g_object_get (window, "search-widget-visible", &visible, NULL);

  /* get the search settings */
  search_settings = gtk_source_search_context_get_settings (document->priv->search_context);

  if (visible)
    {
      /* unblock search context handlers */
      g_signal_handlers_unblock_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                         g_signal_lookup ("insert-text", GTK_TYPE_TEXT_BUFFER),
                                         0, NULL, NULL, document->priv->search_context);
      g_signal_handlers_unblock_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                         g_signal_lookup ("delete-range", GTK_TYPE_TEXT_BUFFER),
                                         0, NULL, NULL, document->priv->search_context);

      /* activate the workaround to prevent endless buffer scanning */
      mousepad_document_prevent_endless_scanning (document, visible);
      MOUSEPAD_SETTING_CONNECT_OBJECT (SEARCH_HIGHLIGHT_ALL,
                                       G_CALLBACK (mousepad_document_prevent_endless_scanning),
                                       document, G_CONNECT_SWAPPED);
      MOUSEPAD_SETTING_CONNECT_OBJECT (SEARCH_ENABLE_REGEX,
                                       G_CALLBACK (mousepad_document_prevent_endless_scanning),
                                       document, G_CONNECT_SWAPPED);

      /* bind "highlight" and "regex-enabled" search settings to Mousepad settings */
      MOUSEPAD_SETTING_BIND (SEARCH_HIGHLIGHT_ALL, document->priv->search_context,
                             "highlight", G_SETTINGS_BIND_GET);
      MOUSEPAD_SETTING_BIND (SEARCH_ENABLE_REGEX, search_settings,
                             "regex-enabled", G_SETTINGS_BIND_GET);
    }
  else
    {
      /* block search context handlers */
      g_signal_handlers_block_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                       g_signal_lookup ("insert-text", GTK_TYPE_TEXT_BUFFER),
                                       0, NULL, NULL, document->priv->search_context);
      g_signal_handlers_block_matched (document->buffer, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_ID,
                                       g_signal_lookup ("delete-range", GTK_TYPE_TEXT_BUFFER),
                                       0, NULL, NULL, document->priv->search_context);

      /* deactivate the workaround to prevent endless buffer scanning */
      mousepad_document_prevent_endless_scanning (document, visible);
      MOUSEPAD_SETTING_DISCONNECT (SEARCH_HIGHLIGHT_ALL,
                                   G_CALLBACK (mousepad_document_prevent_endless_scanning),
                                   document);
      MOUSEPAD_SETTING_DISCONNECT (SEARCH_ENABLE_REGEX,
                                   G_CALLBACK (mousepad_document_prevent_endless_scanning),
                                   document);

      /* unbind "highlight" and "regex-enabled" search settings and turn them off */
      g_settings_unbind (document->priv->search_context, "highlight");
      g_settings_unbind (search_settings, "regex-enabled");
      gtk_source_search_context_set_highlight (document->priv->search_context, FALSE);
      gtk_source_search_settings_set_regex_enabled (search_settings, FALSE);
    }
}



GType
mousepad_document_search_flags_get_type (void)
{
  static GType type = G_TYPE_NONE;

  if (G_UNLIKELY (type == G_TYPE_NONE))
    {
      /* use empty values table */
      static const GFlagsValue values[] =
      {
        { 0, NULL, NULL }
      };

      /* register the type */
      type = g_flags_register_static (I_("MousepadSearchFlags"), values);
    }

  return type;
}
