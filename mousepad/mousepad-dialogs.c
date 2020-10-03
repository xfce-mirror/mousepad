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
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-util.h>

#include <gtksourceview/gtksource.h>



void
mousepad_dialogs_show_about (GtkWindow *parent)
{
  GList *windows, *window;
  static const gchar *authors[] =
  {
    "Nick Schermer <nick@xfce.org>",
    "Erik Harrison <erikharrison@xfce.org>",
    "Matthew Brush <matt@xfce.org>",
    NULL
  };

  /* show the dialog */
  gtk_show_about_dialog (parent,
                         "authors", authors,
                         "comments", _("Mousepad is a fast text editor for the Xfce Desktop Environment."),
                         "copyright", "Copyright \xc2\xa9 2005-2020 - the Mousepad developers",
                         "destroy-with-parent", TRUE,
                         "license-type", GTK_LICENSE_GPL_2_0,
                         "logo-icon-name", "org.xfce.mousepad",
                         "program-name", PACKAGE_NAME,
                         "version", PACKAGE_VERSION,
                         "translator-credits", _("translator-credits"),
                         "website", "https://docs.xfce.org/apps/mousepad/start",
                         NULL);

  /* retrieve the dialog window and add it to the application windows list */
  windows = gtk_window_list_toplevels ();
  for (window = windows; window != NULL; window = window->next)
    if (GTK_IS_ABOUT_DIALOG (window->data) && gtk_window_get_transient_for (window->data) == parent)
      {
        gtk_window_set_application (GTK_WINDOW (window->data), gtk_window_get_application (parent));
        break;
      }

  /* cleanup */
  g_list_free (windows);
}



void
mousepad_dialogs_show_error (GtkWindow    *parent,
                             const GError *error,
                             const gchar  *message)
{
  GtkWidget *dialog;

  /* create the warning dialog */
  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s.", message);

  /* add the dialog to the application windows list */
  gtk_window_set_application (GTK_WINDOW (dialog), gtk_window_get_application (parent));

  /* set secondary text if an error is provided */
  if (G_LIKELY (error != NULL))
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);

  /* display the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* cleanup */
  gtk_widget_destroy (dialog);
}



void
mousepad_dialogs_show_help (GtkWindow   *parent,
                            const gchar *page,
                            const gchar *offset)
{
#if !GTK_CHECK_VERSION (3, 22, 0)
  GdkScreen   *screen;
#endif
  GError      *error = NULL;
  const gchar *uri = "https://docs.xfce.org/apps/mousepad/start";

  /* try to run the documentation browser */
#if GTK_CHECK_VERSION (3, 22, 0)
  if (!gtk_show_uri_on_window (parent, uri, gtk_get_current_event_time (), &error))
    {
      /* display an error message to the user */
      mousepad_dialogs_show_error (parent, error, _("Failed to open the documentation browser"));
      g_error_free (error);
    }
#else

#if G_GNUC_CHECK_VERSION (4, 3)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

  /* get screen */
  if (G_LIKELY (parent))
    screen = gtk_widget_get_screen (GTK_WIDGET (parent));
  else
    screen = gdk_screen_get_default ();

  if (!gtk_show_uri (screen, uri, gtk_get_current_event_time (), &error))
    {
      /* display an error message to the user */
      mousepad_dialogs_show_error (parent, error, _("Failed to open the documentation browser"));
      g_error_free (error);
    }

#if G_GNUC_CHECK_VERSION (4, 3)
# pragma GCC diagnostic pop
#endif

#endif
}



gint
mousepad_dialogs_other_tab_size (GtkWindow *parent,
                                 gint      active_size)
{
  GtkWidget *dialog;
  GtkWidget *area;
  GtkWidget *scale;

  /* build dialog */
  dialog = gtk_dialog_new_with_buttons (_("Select Tab Size"),
                                        parent,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL,
                                        _("_OK"), MOUSEPAD_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_OK);

  /* add the dialog to the application windows list */
  gtk_window_set_application (GTK_WINDOW (dialog), gtk_window_get_application (parent));

  /* create scale widget */
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 1, 32, 1);
  gtk_range_set_value (GTK_RANGE (scale), active_size);
  gtk_scale_set_digits (GTK_SCALE (scale), 0);
  gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);
  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_pack_start (GTK_BOX (area), scale, TRUE, TRUE, 0);
  gtk_widget_show (scale);

  /* run the dialog */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == MOUSEPAD_RESPONSE_OK)
    active_size = gtk_range_get_value (GTK_RANGE (scale));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return active_size;
}



static void
mousepad_dialogs_go_to_line_changed (GtkSpinButton *line_spin,
                                     GtkSpinButton *col_spin)
{
  GtkTextBuffer *buffer;
  GtkTextIter    iter;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (line_spin));
  g_return_if_fail (GTK_IS_SPIN_BUTTON (col_spin));

  /* get the text buffer */
  buffer = mousepad_object_get_data (G_OBJECT (col_spin), "buffer");

  /* debug check */
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  /* get iter at line */
  gtk_text_buffer_get_iter_at_line (buffer, &iter, gtk_spin_button_get_value_as_int (line_spin) - 1);

  /* move the iter to the end of the line if needed */
  if (!gtk_text_iter_ends_line (&iter))
    gtk_text_iter_forward_to_line_end (&iter);

  /* update column spin button range */
  gtk_spin_button_set_range (col_spin, 0, gtk_text_iter_get_line_offset (&iter));
}



gboolean
mousepad_dialogs_go_to (GtkWindow     *parent,
                        GtkTextBuffer *buffer)
{
  GtkWidget    *dialog;
  GtkWidget    *area, *vbox, *hbox;
  GtkWidget    *button;
  GtkWidget    *label;
  GtkWidget    *line_spin, *col_spin;
  GtkSizeGroup *size_group;
  GtkTextIter   iter;
  gint          line, column, lines;
  gint          response;

  /* get cursor iter */
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));
  line = gtk_text_iter_get_line (&iter) + 1;

  /* get number of lines */
  lines = gtk_text_buffer_get_line_count (buffer);

  /* build the dialog */
  dialog = gtk_dialog_new_with_buttons (_("Go To"),
                                        parent,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL,
                                        NULL);
  button = mousepad_util_image_button ("go-jump", _("_Jump to"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_JUMP_TO);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_JUMP_TO);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  /* add the dialog to the application windows list */
  gtk_window_set_application (GTK_WINDOW (dialog), gtk_window_get_application (parent));

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_pack_start (GTK_BOX (area), vbox, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_widget_show (vbox);

  /* create size group */
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  /* line number box */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("_Line number:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_size_group_add_widget (size_group, label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_widget_show (label);

  line_spin = gtk_spin_button_new_with_range (1, lines, 1);
  gtk_entry_set_activates_default (GTK_ENTRY (line_spin), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), line_spin, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), line_spin);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (line_spin), TRUE);
  gtk_entry_set_width_chars (GTK_ENTRY (line_spin), 8);
  gtk_widget_show (line_spin);

  /* column box */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("C_olumn number:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_size_group_add_widget (size_group, label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_widget_show (label);

  col_spin = gtk_spin_button_new_with_range (0, 0, 1);
  gtk_entry_set_activates_default (GTK_ENTRY (col_spin), TRUE);
  mousepad_object_set_data (G_OBJECT (col_spin), "buffer", buffer);
  gtk_box_pack_start (GTK_BOX (hbox), col_spin, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), col_spin);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (col_spin), TRUE);
  gtk_entry_set_width_chars (GTK_ENTRY (col_spin), 8);
  gtk_widget_show (col_spin);

  /* signal to monitor column number */
  g_signal_connect (G_OBJECT (line_spin), "value-changed", G_CALLBACK (mousepad_dialogs_go_to_line_changed), col_spin);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (line_spin), line);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == MOUSEPAD_RESPONSE_JUMP_TO)
    {
      /* hide the dialog */
      gtk_widget_hide (dialog);

      /* get new position */
      line = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (line_spin)) - 1;
      column = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (col_spin));

      /* get iter */
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, line, column);

      /* get cursor position */
      gtk_text_buffer_place_cursor (buffer, &iter);
    }

  /* release size group */
  g_object_unref (G_OBJECT (size_group));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return (response == MOUSEPAD_RESPONSE_JUMP_TO);
}



gboolean
mousepad_dialogs_clear_recent (GtkWindow *parent)
{
  GtkWidget *dialog;
  GtkWidget *area, *hbox, *vbox;
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *label;
  gboolean   succeed = FALSE;

  /* create the question dialog */
  dialog = gtk_dialog_new_with_buttons (_("Clear Documents History"),
                                        parent,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL,
                                        NULL);
  button = mousepad_util_image_button ("edit-clear", _("Clea_r"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_CLEAR);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);

  /* add the dialog to the application windows list */
  gtk_window_set_application (GTK_WINDOW (dialog), gtk_window_get_application (parent));

  /* the content area */
  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (area), hbox, TRUE, TRUE, 6);
  gtk_widget_show (hbox);

  /* the dialog icon */
  image = gtk_image_new_from_icon_name ("edit-clear", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 0);
  gtk_widget_show (image);

  /* the dialog message */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 6);
  gtk_widget_show (vbox);

  /* primary text */
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label),
                        _("<big><b>Remove all entries from the documents history?</b></big>"));
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* secondary text */
  label = gtk_label_new (_("Clearing the documents history will permanently "
                           "remove all currently listed entries."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* popup the dialog */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == MOUSEPAD_RESPONSE_CLEAR)
    succeed = TRUE;

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return succeed;
}



gint
mousepad_dialogs_save_changes (GtkWindow *parent,
                               gboolean   readonly)
{
  GtkWidget *dialog;
  GtkWidget *area, *hbox, *vbox;
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *label;
  gint       response;

  /* create the question dialog */
  dialog = gtk_dialog_new_with_buttons (_("Save Changes"),
                                        parent,
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL,
                                        NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                mousepad_util_image_button ("edit-delete", _("_Don't Save")),
                                MOUSEPAD_RESPONSE_DONT_SAVE);

  /* add the dialog to the application windows list */
  gtk_window_set_application (GTK_WINDOW (dialog), gtk_window_get_application (parent));

  /* we show the save as button instead of save for readonly document */
  if (G_UNLIKELY (readonly))
    {
      image = gtk_image_new_from_icon_name ("document-save-as", GTK_ICON_SIZE_DIALOG);
      button = mousepad_util_image_button ("document-save-as", _("_Save As"));
      gtk_widget_set_can_default (button, TRUE);
      gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE_AS);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_SAVE_AS);
    }
  else
    {
      image = gtk_image_new_from_icon_name ("document-save", GTK_ICON_SIZE_DIALOG);
      button = mousepad_util_image_button ("document-save", _("_Save"));
      gtk_widget_set_can_default (button, TRUE);
      gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_SAVE);
    }

  /* the content area */
  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (area), hbox, TRUE, TRUE, 6);
  gtk_widget_show (hbox);

  /* the dialog icon */
  gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 0);
  gtk_widget_show (image);

  /* the dialog message */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 6);
  gtk_widget_show (vbox);

  /* primary text */
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label),
                        _("<big><b>Do you want to save the changes before closing?</b></big>"));
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* secondary text */
  label = gtk_label_new (_("If you don't save the document, all the changes will be lost."));
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* run the dialog and wait for a response */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}



gint
mousepad_dialogs_externally_modified (GtkWindow *parent)
{
  GtkWidget *dialog;
  GtkWidget *button;
  gint       response;

  /* create the question dialog */
  dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
                                   _("The document has been externally modified. "
                                     "Do you want to continue saving?"));
  gtk_window_set_title (GTK_WINDOW (dialog), _("Externally Modified"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("If you save the document, all of the external "
                                              "changes will be lost."));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);
  button = mousepad_util_image_button ("document-save-as", _("Save _As"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE_AS);
  button = mousepad_util_image_button ("document-save", _("_Save"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);

  /* add the dialog to the application windows list */
  gtk_window_set_application (GTK_WINDOW (dialog), gtk_window_get_application (parent));

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}



gint
mousepad_dialogs_revert (GtkWindow *parent)
{
  GtkWidget *dialog;
  GtkWidget *button;
  gint       response;

  /* setup the question dialog */
  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                   _("Do you want to save your changes before reloading?"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                   _("If you revert the file, all unsaved changes will be lost."));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);
  button = mousepad_util_image_button ("document-save-as", _("_Save As"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE_AS);
  button = mousepad_util_image_button ("document-revert", _("_Revert"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_REVERT);

  /* add the dialog to the application windows list */
  gtk_window_set_application (GTK_WINDOW (dialog), gtk_window_get_application (parent));

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}



gint
mousepad_dialogs_save_as (GtkWindow    *parent,
                          const gchar  *current_filename,
                          const gchar  *last_save_location,
                          gchar       **filename)
{
  GtkWidget *dialog, *button;
  gint       response;

  /* create the dialog */
  dialog = gtk_file_chooser_dialog_new (_("Save As"),
                                        parent, GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        NULL);

  button = mousepad_util_image_button ("document-save", _("_Save"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* add the dialog to the application windows list */
  gtk_window_set_application (GTK_WINDOW (dialog), gtk_window_get_application (parent));

  /* set the current filename if there is one, or use the last save location */
  if (current_filename)
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), current_filename);
  else if (last_save_location)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), last_save_location);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* get the new filename */
  *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}



gint
mousepad_dialogs_open (GtkWindow    *parent,
                       const gchar  *filename,
                       GSList      **filenames)
{
  GtkWidget *dialog, *button, *hbox, *label, *combobox;
  gint       response;

  /* create new file chooser dialog */
  dialog = gtk_file_chooser_dialog_new (_("Open File"),
                                         parent,
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         _("_Cancel"), GTK_RESPONSE_CANCEL,
                                         NULL);
  button = mousepad_util_image_button ("document-open", _("_Open"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);

  /* set parent window */
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  /* add the dialog to the application windows list */
  gtk_window_set_application (GTK_WINDOW (dialog), gtk_window_get_application (parent));

  /* encoding selector */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), hbox);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("_Encoding:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  combobox = gtk_combo_box_new ();
  gtk_box_pack_start (GTK_BOX (hbox), combobox, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combobox);
  gtk_widget_show (combobox);

  /* select the active document in the file chooser */
  if (filename && g_file_test (filename, G_FILE_TEST_EXISTS))
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), filename);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* get a list of selected filenames */
  *filenames = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}
