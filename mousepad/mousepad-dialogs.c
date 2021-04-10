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
#include <mousepad/mousepad-encoding-dialog.h>
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-window.h>



static void
mousepad_dialogs_response_cancel (GtkDialog *dialog)
{
  gtk_dialog_response (dialog, MOUSEPAD_RESPONSE_CANCEL);
}



static void
mousepad_dialogs_destroy_with_parent (GtkWidget *dialog,
                                      GtkWindow *parent)
{
  /* make sure to connect to a MousepadWindow "destroy" signal, so that stacked dialogs
   * are recursively destroyed */
  while (! MOUSEPAD_IS_WINDOW (parent))
    {
      parent = gtk_window_get_transient_for (parent);
      if (G_UNLIKELY (parent == NULL))
        return;
    }

  /* connect to "unrealize" instead of "destroy" which is not received in a recursive
   * main loop (at least as it is invoqued below) */
  g_signal_connect_object (parent, "unrealize",
                           G_CALLBACK (mousepad_dialogs_response_cancel),
                           dialog, G_CONNECT_SWAPPED);
}



static gboolean
mousepad_dialogs_run_close (GtkDialog *dialog)
{
  mousepad_dialogs_response_cancel (dialog);

  /* we will destroy the dialog ourselves */
  return TRUE;
}



static void
mousepad_dialogs_run_response (GtkWidget *dialog,
                               gint       response,
                               GMainLoop *loop)
{
  mousepad_object_set_data (dialog, "response", GINT_TO_POINTER (response));
  g_main_loop_quit (loop);
}



gint
mousepad_dialogs_run (GtkWidget *dialog,
                      GtkWindow *parent)
{
  GMainLoop *loop;

  loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect (dialog, "response", G_CALLBACK (mousepad_dialogs_run_response), loop);
  g_signal_connect (dialog, "close-request", G_CALLBACK (mousepad_dialogs_run_close), NULL);
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  gtk_widget_show (dialog);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  return GPOINTER_TO_INT (mousepad_object_get_data (dialog, "response"));
}



void
mousepad_dialogs_show_about (GtkWindow *parent)
{
  static const gchar *authors[] =
  {
    "Nick Schermer <nick@xfce.org>",
    "Erik Harrison <erikharrison@xfce.org>",
    "Matthew Brush <matt@xfce.org>",
    "Gaël Bonithon <gael@xfce.org>",
    NULL
  };

  /* show the dialog */
  gtk_show_about_dialog (parent,
                         "authors", authors,
                         "comments", _("Mousepad is a fast text editor for the Xfce Desktop Environment."),
                         "copyright", "Copyright \xc2\xa9 2005-2021 - the Mousepad developers",
                         "destroy-with-parent", TRUE,
                         "license-type", GTK_LICENSE_GPL_2_0,
                         "logo-icon-name", MOUSEPAD_ID,
                         "program-name", PACKAGE_NAME,
                         "version", PACKAGE_VERSION,
                         "translator-credits", _("translator-credits"),
                         "website", "https://docs.xfce.org/apps/mousepad/start",
                         NULL);
}



void
mousepad_dialogs_show_error (GtkWindow    *parent,
                             const GError *error,
                             const gchar  *message)
{
  GtkWidget *dialog;

  /* create the warning dialog */
  dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE, "%s.", message);

  /* set secondary text if an error is provided */
  if (G_LIKELY (error != NULL))
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);

  /* run the dialog */
  mousepad_dialogs_run (dialog, parent);

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));
}



gint
mousepad_dialogs_other_tab_size (GtkWindow *parent,
                                 gint      active_size)
{
  GtkWidget *dialog, *area, *scale;

  /* build dialog */
  dialog = gtk_dialog_new_with_buttons (_("Select Tab Size"), parent, GTK_DIALOG_MODAL,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL,
                                        _("_OK"), MOUSEPAD_RESPONSE_OK, NULL);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_OK);

  /* create scale widget */
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 1, 32, 1);
  gtk_range_set_value (GTK_RANGE (scale), active_size);
  gtk_scale_set_digits (GTK_SCALE (scale), 0);
  gtk_scale_set_draw_value (GTK_SCALE (scale), TRUE);
  gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_TOP);

  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (area), GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (area), scale);

  /* run the dialog */
  if (mousepad_dialogs_run (dialog, parent) == MOUSEPAD_RESPONSE_OK)
    active_size = gtk_range_get_value (GTK_RANGE (scale));

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));

  return active_size;
}



static void
mousepad_dialogs_go_to_line_changed (GtkSpinButton *line_spin,
                                     GtkSpinButton *col_spin)
{
  GtkTextBuffer *buffer;
  GtkTextIter    iter;
  gint           line, total_columns;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (line_spin));
  g_return_if_fail (GTK_IS_SPIN_BUTTON (col_spin));

  /* get the text buffer */
  buffer = mousepad_object_get_data (col_spin, "buffer");

  line = gtk_spin_button_get_value_as_int (line_spin);

  if (line > 0)
    line--;
  else if (line < 0)
    line += gtk_text_buffer_get_line_count (buffer);

  /* get iter at line */
  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);

  /* move the iter to the end of the line if needed */
  if (! gtk_text_iter_ends_line (&iter))
    gtk_text_iter_forward_to_line_end (&iter);

  total_columns = mousepad_util_get_real_line_offset (&iter);

  /* update column spin button range */
  gtk_spin_button_set_range (col_spin, -(total_columns + 1), total_columns);
}



gboolean
mousepad_dialogs_go_to (GtkWindow     *parent,
                        GtkTextBuffer *buffer)
{
  GtkWidget    *dialog, *area, *vbox, *hbox, *widget, *line_spin, *col_spin;
  GtkSizeGroup *size_group;
  GtkTextIter   iter;
  gint          line, column, lines;
  gboolean      succeed;

  /* build the dialog */
  dialog = gtk_dialog_new_with_buttons (_("Go To"), parent, GTK_DIALOG_MODAL,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  /* add button */
  widget = mousepad_util_image_button ("go-jump", _("_Jump to"), 4, 4, 0, 0);
  gtk_window_set_default_widget (GTK_WINDOW (dialog), widget);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), widget, MOUSEPAD_RESPONSE_JUMP_TO);

  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_start (vbox, 6);
  gtk_widget_set_margin_end (vbox, 6);
  gtk_widget_set_margin_top (vbox, 6);
  gtk_widget_set_margin_bottom (vbox, 6);
  gtk_box_append (GTK_BOX (area), vbox);

  /* create size group */
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  /* line number box */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_append (GTK_BOX (vbox), hbox);

  widget = gtk_label_new_with_mnemonic (_("_Line number:"));
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_size_group_add_widget (size_group, widget);
  gtk_label_set_xalign (GTK_LABEL (widget), 0.0);
  gtk_label_set_yalign (GTK_LABEL (widget), 0.5);

  /* get number of lines */
  lines = gtk_text_buffer_get_line_count (buffer);

  line_spin = gtk_spin_button_new_with_range (-lines, lines, 1);
  gtk_widget_activate_default (line_spin);
  gtk_box_append (GTK_BOX (hbox), line_spin);
  gtk_label_set_mnemonic_widget (GTK_LABEL (widget), line_spin);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (line_spin), TRUE);
  gtk_editable_set_width_chars (GTK_EDITABLE (line_spin), 8);

  /* column box */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_append (GTK_BOX (vbox), hbox);

  widget = gtk_label_new_with_mnemonic (_("C_olumn number:"));
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_size_group_add_widget (size_group, widget);
  gtk_label_set_xalign (GTK_LABEL (widget), 0.0);
  gtk_label_set_yalign (GTK_LABEL (widget), 0.5);

  /* release size group */
  g_object_unref (size_group);

  col_spin = gtk_spin_button_new_with_range (0, 0, 1);
  gtk_widget_activate_default (col_spin);
  mousepad_object_set_data (col_spin, "buffer", buffer);
  gtk_box_append (GTK_BOX (hbox), col_spin);
  gtk_label_set_mnemonic_widget (GTK_LABEL (widget), col_spin);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (col_spin), TRUE);
  gtk_editable_set_width_chars (GTK_EDITABLE (col_spin), 8);

  /* get cursor iter */
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));
  line = gtk_text_iter_get_line (&iter) + 1;
  column = mousepad_util_get_real_line_offset (&iter);

  /* signal to monitor column number */
  g_signal_connect (line_spin, "value-changed",
                    G_CALLBACK (mousepad_dialogs_go_to_line_changed), col_spin);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (line_spin), line);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (col_spin), column);

  /* run the dialog */
  succeed = (mousepad_dialogs_run (dialog, parent) == MOUSEPAD_RESPONSE_JUMP_TO);
  if (succeed)
    {
      /* hide the dialog */
      gtk_widget_hide (dialog);

      /* get new position */
      line = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (line_spin));
      if (line > 0)
        line--;
      column = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (col_spin));

      /* place cursor at (line, column) */
      mousepad_util_place_cursor (buffer, line, column);
    }

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));

  return succeed;
}



gboolean
mousepad_dialogs_clear_recent (GtkWindow *parent)
{
  GtkWidget *dialog, *area, *hbox, *vbox, *widget;
  gboolean   succeed;

  /* create the question dialog */
  dialog = gtk_dialog_new_with_buttons (_("Clear Documents History"), parent, GTK_DIALOG_MODAL,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);

  /* set button */
  widget = mousepad_util_image_button ("edit-clear", _("Clea_r"), 4, 4, 0, 0);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), widget, MOUSEPAD_RESPONSE_CLEAR);

  /* the content area */
  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_vexpand (hbox, TRUE);
  gtk_widget_set_margin_top (hbox, 6);
  gtk_widget_set_margin_bottom (hbox, 6);
  gtk_box_append (GTK_BOX (area), hbox);

  /* the dialog icon */
  widget = gtk_image_new_from_icon_name ("edit-clear");
  gtk_image_set_icon_size (GTK_IMAGE (widget), GTK_ICON_SIZE_LARGE);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  /* the dialog message */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_hexpand (vbox, TRUE);
  gtk_widget_set_margin_start (vbox, 6);
  gtk_widget_set_margin_end (vbox, 6);
  gtk_box_append (GTK_BOX (hbox), vbox);

  /* primary text */
  widget = gtk_label_new (NULL);
  gtk_widget_set_vexpand (widget, TRUE);
  gtk_label_set_markup (GTK_LABEL (widget),
                        _("<big><b>Remove all entries from the documents history?</b></big>"));
  gtk_box_append (GTK_BOX (vbox), widget);

  /* secondary text */
  widget = gtk_label_new (_("Clearing the documents history will permanently "
                           "remove all currently listed entries."));
  gtk_widget_set_vexpand (widget, TRUE);
  gtk_label_set_wrap (GTK_LABEL (widget), TRUE);
  gtk_box_append (GTK_BOX (vbox), widget);

  /* popup the dialog */
  succeed = (mousepad_dialogs_run (dialog, parent) == MOUSEPAD_RESPONSE_CLEAR);

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));

  return succeed;
}



gint
mousepad_dialogs_save_changes (GtkWindow *parent,
                               gboolean   readonly)
{
  GtkWidget *dialog, *area, *hbox, *vbox, *button, *image, *label;
  gint       response;

  /* create the question dialog */
  dialog = gtk_dialog_new_with_buttons (_("Save Changes"), parent, GTK_DIALOG_MODAL,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);

  /* add buttons */
  button = mousepad_util_image_button ("edit-delete", _("_Don't Save"), 4, 4, 0, 0);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_DONT_SAVE);

  /* we show the save as button instead of save for readonly document */
  if (G_UNLIKELY (readonly))
    {
      image = gtk_image_new_from_icon_name ("document-save-as");
      button = mousepad_util_image_button ("document-save-as", _("_Save As"), 0, 4, 0, 0);
      gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE_AS);
    }
  else
    {
      image = gtk_image_new_from_icon_name ("document-save");
      button = mousepad_util_image_button ("document-save", _("_Save"), 0, 4, 0, 0);
      gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE);
    }

  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_window_set_default_widget (GTK_WINDOW (dialog), button);

  /* the content area */
  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_vexpand (hbox, TRUE);
  gtk_widget_set_margin_top (hbox, 6);
  gtk_widget_set_margin_bottom (hbox, 6);
  gtk_box_append (GTK_BOX (area), hbox);

  /* the dialog icon */
  gtk_widget_set_hexpand (image, TRUE);
  gtk_box_append (GTK_BOX (hbox), image);

  /* the dialog message */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_hexpand (vbox, TRUE);
  gtk_widget_set_margin_start (hbox, 6);
  gtk_widget_set_margin_end (hbox, 6);
  gtk_box_append (GTK_BOX (hbox), vbox);

  /* primary text */
  label = gtk_label_new (NULL);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_label_set_markup (GTK_LABEL (label),
                        _("<big><b>Do you want to save the changes before closing?</b></big>"));
  gtk_box_append (GTK_BOX (vbox), label);

  /* secondary text */
  label = gtk_label_new (_("If you don't save the document, all the changes will be lost."));
  gtk_widget_set_vexpand (label, TRUE);
  gtk_box_append (GTK_BOX (vbox), label);

  /* run the dialog and wait for a response */
  response = mousepad_dialogs_run (dialog, parent);

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));

  return response;
}



gint
mousepad_dialogs_externally_modified (GtkWindow *parent,
                                      gboolean   saving,
                                      gboolean   modified)
{
  GtkWidget   *dialog, *button;
  const gchar *text_1, *text_2, *icon, *label;
  gint         button_response, response;

  /* set icons and texts to display */
  if (saving)
    {
      text_1 = _("The document has been externally modified. Do you want to continue saving?");
      text_2 = _("If you save the document, all of the external changes will be lost.");
      icon = "document-save-as";
      label = _("Save _As");
      button_response = MOUSEPAD_RESPONSE_SAVE_AS;
    }
  else
    {
      text_1 = _("The document has been externally modified. Do you want to reload it from disk?");
      button_response = MOUSEPAD_RESPONSE_RELOAD;

      if (modified)
        {
          text_2 = _("You have unsaved changes. If you revert the file, they will be lost.");
          icon = "document-revert";
          label = _("_Revert");
        }
      else
        {
          text_2 = NULL;
          icon = "view-refresh";
          label = _("Re_load");
        }
    }

  /* create the question dialog */
  dialog = gtk_message_dialog_new_with_markup (parent, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
                                               GTK_BUTTONS_NONE, "<b><big>%s</big></b>", text_1);

  /* add title */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Externally Modified"));
  if (text_2 != NULL)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", text_2);

  /* add buttons */
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);

  button = mousepad_util_image_button (icon, label, 4, 0, 0, 0);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, button_response);

  if (saving)
    {
      button = mousepad_util_image_button ("document-save", _("_Save"), 4, 0, 0, 0);
      gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE);
    }

  /* run the dialog */
  response = mousepad_dialogs_run (dialog, parent);

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));

  return response;
}



gint
mousepad_dialogs_revert (GtkWindow *parent)
{
  GtkWidget *dialog, *button;
  gint       response;

  /* setup the question dialog */
  dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                   _("Do you want to save your changes before reloading?"));

  /* set subtitle */
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                   _("If you revert the file, all unsaved changes will be lost."));

  /* add buttons */
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);

  button = mousepad_util_image_button ("document-save-as", _("_Save As"), 4, 4, 0, 0);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE_AS);

  button = mousepad_util_image_button ("document-revert", _("_Revert"), 0, 0, 0, 0);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_RELOAD);

  /* run the dialog */
  response = mousepad_dialogs_run (dialog, parent);

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));

  return response;
}



gint
mousepad_dialogs_confirm_encoding (const gchar *charset,
                                   const gchar *user_charset)
{
  GtkWindow *parent;
  GtkWidget *dialog;
  gint       response;

  /* get the parent window */
  parent = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));

  /* setup the question dialog */
  dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                                   _("The file seems to be encoded in %s, but you have chosen %s"
                                     " encoding. Do you confirm this choice?"),
                                   charset, user_charset);

  /* set subtitle */
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("If not, the guessed encoding will be used."));

  /* run the dialog */
  response = mousepad_dialogs_run (dialog, parent);

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));

  return response;
}



static gboolean
mousepad_dialogs_combo_insert_separator (GtkTreeModel *model,
                                         GtkTreeIter  *iter,
                                         gpointer      data)
{
  gint row_type;

  /* get the selected row type */
  gtk_tree_model_get (model, iter, 1, &row_type, -1);

  return row_type == -1;
}



static gboolean
mousepad_dialogs_combo_popup (gpointer data)
{
  /* the combo box may no longer exist when we get here */
  if (GTK_IS_COMBO_BOX (data))
    gtk_combo_box_popup (GTK_COMBO_BOX (data));

  return FALSE;
}



static void
mousepad_dialogs_combo_changed (GtkComboBox *combo,
                                GtkWidget   *dialog)
{
  MousepadFile        *file = NULL, *current_file;
  MousepadEncoding     encoding, cell_encoding;
  GtkTreeModel        *model;
  GtkListStore        *list;
  GtkTreeIter          iter;
  GListModel          *files;
  GFile               *g_file;
  GFileIOStream       *iostream = NULL;
  GError              *error = NULL;
  gint                 row_type = 0, n_rows = 2;
  gboolean             found = FALSE;
  static GtkTreeModel *short_model = NULL;

  /* get the model */
  model = gtk_combo_box_get_model (combo);

  /* get the selected row type */
  if (gtk_combo_box_get_active_iter (combo, &iter))
    gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 1, &row_type, -1);

  /* request for opening the encoding dialog */
  if (row_type == -2)
    {
      /* fallback to default encoding as a last resort */
      gtk_combo_box_set_active (combo, 2);

      /* ensure the combo box is closed before to open a dialog */
      gtk_combo_box_popdown (combo);

      /* "Open" dialog */
      if (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog))
          == GTK_FILE_CHOOSER_ACTION_OPEN)
        {
          /* get a list of selected locations, must be non empty */
          files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog));
          if ((g_file = g_list_model_get_item (files, 0)) != NULL)
            {
              file = mousepad_file_new (GTK_TEXT_BUFFER (gtk_source_buffer_new (NULL)));
              mousepad_file_set_location (file, g_file, TRUE);
              g_object_unref (g_file);
            }
          /* ask the user to select a file */
          else
            mousepad_dialogs_show_error (GTK_WINDOW (dialog), NULL, _("Please select a file"));

          g_object_unref (files);
        }
      /* "Save As" dialog */
      else
        {
          /* try to create a temporary file to save the current buffer */
          if ((g_file = g_file_new_tmp (NULL, &iostream, &error)) != NULL)
            {
              current_file = mousepad_object_get_data (dialog, "file");
              file = mousepad_file_new (mousepad_file_get_buffer (current_file));
              mousepad_file_set_location (file, g_file, TRUE);
              if ((encoding = mousepad_file_get_encoding (current_file)) != MOUSEPAD_ENCODING_NONE)
                mousepad_file_set_encoding (file, encoding);
              else
                mousepad_file_set_encoding (file, mousepad_encoding_get_default ());

              mousepad_file_save (file, FALSE, &error);

              /* cleanup */
              g_object_unref (g_file);
              g_object_unref (iostream);
            }

          /* inform the user in case of problem */
          if (error != NULL)
            {
              mousepad_dialogs_show_error (GTK_WINDOW (dialog), error,
                _("Failed to prepare the temporary file for encoding tests"));

              /* cleanup */
              g_error_free (error);
              if (file != NULL)
                {
                  g_file_delete (mousepad_file_get_location (file), NULL, NULL);
                  g_object_unref (file);
                  file = NULL;
                }
            }
        }

      /* run the encoding dialog */
      if (file != NULL && mousepad_encoding_dialog (GTK_WINDOW (dialog), file, TRUE, &encoding)
          == MOUSEPAD_RESPONSE_OK
          && gtk_tree_model_iter_nth_child (model, &iter, NULL, 1))
        {
          /* cleanup */
          if (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog))
              == GTK_FILE_CHOOSER_ACTION_SAVE)
            g_file_delete (mousepad_file_get_location (file), NULL, NULL);

          g_object_unref (file);

          /* try to find the chosen encoding in the combo box */
          while (gtk_tree_model_iter_next (model, &iter))
            {
              gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 1, &cell_encoding, -1);
              if (encoding == cell_encoding)
                {
                  found = TRUE;
                  break;
                }
              else
                n_rows++;
            }

          /* add encoding to the combo box if needed */
          if (! found)
            {
              n_rows++;
              gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, n_rows - 3,
                                                 0, mousepad_encoding_get_charset (encoding),
                                                 1, encoding, -1);
              gtk_tree_model_iter_nth_child (model, &iter, NULL, n_rows - 3);
            }

          /* select encoding in the combo box */
          gtk_combo_box_set_active_iter (combo, &iter);
        }
    }
  /* request for showing all encodings or going back to shorten list */
  else if (row_type == -3)
    {
      /* show all encodings */
      if (short_model == NULL)
        {
          /* store the shorten list */
          short_model = model;

          /* create a new list */
          list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
          gtk_list_store_insert_with_values (list, NULL, 0, 0, _("Open encoding dialog"),
                                             1, -2, -1);
          gtk_list_store_insert_with_values (list, NULL, 1, 0, _("Go back to shorten list"),
                                             1, -3, -1);

          /* insert all encodings */
          for (encoding = 1; encoding < MOUSEPAD_N_ENCODINGS; encoding++)
            gtk_list_store_insert_with_values (list, NULL, encoding + 1,
                                               0, mousepad_encoding_get_charset (encoding),
                                               1, encoding, -1);

          /* set full list, spreading it over several columns */
          gtk_combo_box_set_model (combo, GTK_TREE_MODEL (list));
          gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 2);
          n_rows = MOUSEPAD_N_ENCODINGS + 1;
/* TODO ComboBox */
#if 0
          gtk_combo_box_set_wrap_width (combo, n_rows / 10 + (n_rows % 10 != 0));
#endif
        }
      /* go back to shorten list */
      else
        {
          /* set shorten list */
          gtk_combo_box_set_model (combo, short_model);
          gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 2);
/* TODO ComboBox */
#if 0
          gtk_combo_box_set_wrap_width (combo, 1);
#endif

          /* cleanup */
          g_object_unref (model);
          short_model = NULL;
        }

      /* reopen combo box */
      g_idle_add_full (G_PRIORITY_LOW, mousepad_dialogs_combo_popup, combo, NULL);
    }
}



static GtkWidget *
mousepad_dialogs_find_choice_box (GtkWidget *widget)
{
  GtkWidget *child, *candidate;

  if (GTK_IS_BOX (widget) && GTK_IS_CHECK_BUTTON (gtk_widget_get_first_child (widget)))
    return widget;

  /* go in reverse order to not search into upper widgets uselessly: just a little optimization */
  for (child = gtk_widget_get_last_child (widget); child != NULL;
       child = gtk_widget_get_prev_sibling (child))
    {
      candidate = mousepad_dialogs_find_choice_box (child);
      if (GTK_IS_BOX (candidate) && GTK_IS_CHECK_BUTTON (gtk_widget_get_first_child (candidate)))
        return candidate;
    }

  return NULL;
}



static GtkComboBox *
mousepad_dialogs_add_encoding_combo (GtkWidget *dialog)
{
  MousepadEncoding  default_encoding, system_encoding, current_encoding = MOUSEPAD_ENCODING_NONE;
  MousepadFile     *file;
  GtkWidget        *hbox, *widget, *combo;
  GtkListStore     *list;
  GtkCellRenderer  *cell;
  const gchar      *charset;
  gchar            *label;
  guint             n_rows = 0, n;
  MousepadEncoding  encodings[] = { MOUSEPAD_ENCODING_UTF_8, MOUSEPAD_ENCODING_ISO_8859_15 };

  /* make the choice widget appear */
  gtk_file_chooser_add_choice (GTK_FILE_CHOOSER (dialog), "", "", NULL, NULL);

  /* retrieve the choice widget box and pack encoding widgets in it */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  if ((widget = mousepad_dialogs_find_choice_box (dialog)) != NULL)
    {
      /* hide choice widget instead of removing it: usually preferable */
      gtk_widget_hide (gtk_widget_get_first_child (widget));
      gtk_box_append (GTK_BOX (widget), hbox);
    }
  else
    g_warn_if_reached ();

  /* combo box label */
  widget = gtk_label_new_with_mnemonic (_("_Encoding:"));
  gtk_box_append (GTK_BOX (hbox), widget);

  /* build the combo box model */
  list = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
  gtk_list_store_insert_with_values (list, NULL, n_rows++,
                                     0, _("Open encoding dialog"), 1, -2, -1);
  gtk_list_store_insert_with_values (list, NULL, n_rows++, 0, NULL, 1, -1, -1);

  /* add default charset */
  default_encoding = mousepad_encoding_get_default ();
  label = g_strdup_printf ("%s (%s)", _("Default"),
                           mousepad_encoding_get_charset (default_encoding));
  gtk_list_store_insert_with_values (list, NULL, n_rows++, 0, label, 1, default_encoding, -1);
  g_free (label);

  /* add system charset if supported and different from default */
  g_get_charset (&charset);
  system_encoding = mousepad_encoding_find (charset);
  if (system_encoding != MOUSEPAD_ENCODING_NONE && system_encoding != default_encoding)
    {
      label = g_strdup_printf ("%s (%s)", _("System"), charset);
      gtk_list_store_insert_with_values (list, NULL, n_rows++, 0, label, 1, system_encoding, -1);
      g_free (label);
    }

  /* add current charset if different from default and system */
  if (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog)) == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      file = mousepad_object_get_data (dialog, "file");
      current_encoding = mousepad_file_get_encoding (file);
      if (current_encoding != MOUSEPAD_ENCODING_NONE && current_encoding != default_encoding
          && current_encoding != system_encoding)
        {
          label = g_strdup_printf ("%s (%s)", _("Current"),
                                   mousepad_encoding_get_charset (current_encoding));
          gtk_list_store_insert_with_values (list, NULL, n_rows++, 0, label, 1, current_encoding, -1);
          g_free (label);
        }
    }

  /* add other encodings */
  for (n = 0; n < G_N_ELEMENTS (encodings); n++)
    if (encodings[n] != default_encoding && encodings[n] != system_encoding
        && encodings[n] != current_encoding)
      gtk_list_store_insert_with_values (list, NULL, n + n_rows++,
                                         0, mousepad_encoding_get_charset (encodings[n]),
                                         1, encodings[n], -1);

  /* add last items */
  gtk_list_store_insert_with_values (list, NULL, n_rows++, 0, NULL, 1, -1, -1);
  gtk_list_store_insert_with_values (list, NULL, n_rows++,
                                     0, _("Show all encodings"), 1, -3, -1);

  /* create combo box */
  combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (list));
  gtk_box_append (GTK_BOX (hbox), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (widget), combo);

  /* set combo box handlers */
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo),
                                        mousepad_dialogs_combo_insert_separator, NULL, NULL);
  g_signal_connect (combo, "changed", G_CALLBACK (mousepad_dialogs_combo_changed), dialog);

  /* set combo box cell renderer */
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", 0, NULL);

  /* select default encoding */
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 2);

  return GTK_COMBO_BOX (combo);
}



static void
mousepad_dialogs_add_file_filter (GtkFileChooser *dialog)
{
  GtkFileFilter *filter;

  /* file filter on mime type */
  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "text/plain");
  gtk_file_filter_set_name (filter, _("Text Files"));
  gtk_file_chooser_add_filter (dialog, filter);

  /* no filter */
  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_chooser_add_filter (dialog, filter);
}



gint
mousepad_dialogs_save_as (GtkWindow         *parent,
                          MousepadFile      *current_file,
                          GFile             *last_save_location,
                          GFile            **file,
                          MousepadEncoding  *encoding)
{
  GtkWidget   *dialog, *button;
  GtkComboBox *combo;
  GtkTreeIter  iter;
  gint         response;

  /* create the dialog */
  dialog = gtk_file_chooser_dialog_new (_("Save As"), parent, GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL, NULL);

  /* add button */
  button = mousepad_util_image_button ("document-save", _("_Save"), 4, 0, 0, 0);
  gtk_window_set_default_widget (GTK_WINDOW (dialog), button);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);

  /* add file filter */
  mousepad_dialogs_add_file_filter (GTK_FILE_CHOOSER (dialog));

  /* encoding selector */
  mousepad_object_set_data (dialog, "file", current_file);
  combo = mousepad_dialogs_add_encoding_combo (dialog);

  /* set the current location if there is one, or use the last save location */
  if (mousepad_file_location_is_set (current_file))
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (dialog),
                               mousepad_file_get_location (current_file), NULL);
  else if (last_save_location != NULL)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                         last_save_location, NULL);

  /* run the dialog */
  if ((response = mousepad_dialogs_run (dialog, parent)) == GTK_RESPONSE_ACCEPT)
    {
      /* get the new location */
      *file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

      /* get the selected encoding */
      gtk_combo_box_get_active_iter (combo, &iter);
      gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter, 1, encoding, -1);
    }

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));

  return response;
}



gint
mousepad_dialogs_open (GtkWindow         *parent,
                       GFile             *file,
                       GListModel       **files,
                       MousepadEncoding  *encoding)
{
  GtkWidget   *dialog, *button;
  GtkComboBox *combo;
  GtkTreeIter  iter;
  gint         response;

  /* create new file chooser dialog */
  dialog = gtk_file_chooser_dialog_new (_("Open File"), parent, GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL, NULL);

  /* add button */
  button = mousepad_util_image_button ("document-open", _("_Open"), 4, 0, 0, 0);
  gtk_window_set_default_widget (GTK_WINDOW (dialog), button);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);

  /* set properties */
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);

  /* add file filter */
  mousepad_dialogs_add_file_filter (GTK_FILE_CHOOSER (dialog));

  /* encoding selector */
  combo = mousepad_dialogs_add_encoding_combo (dialog);

  /* select the active document in the file chooser */
  if (file != NULL && g_file_query_exists (file, NULL))
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (dialog), file, NULL);

  /* run the dialog */
  if ((response = mousepad_dialogs_run (dialog, parent)) == GTK_RESPONSE_ACCEPT)
    {
      /* get a list of selected locations */
      *files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog));

      /* get the selected encoding */
      gtk_combo_box_get_active_iter (combo, &iter);
      gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter, 1, encoding, -1);
    }

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));

  return response;
}



void
mousepad_dialogs_select_font (GtkWindow *parent)
{
  GtkWidget *dialog;
  gchar     *font;

  /* create new font chooser dialog */
  dialog = gtk_font_chooser_dialog_new (_("Choose Mousepad Font"), parent);

  /* set the current font */
  if ((font = MOUSEPAD_SETTING_GET_STRING (FONT)) != NULL)
    {
      gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), font);
      g_free (font);
    }

  /* run the dialog */
  if (mousepad_dialogs_run (dialog, parent) == GTK_RESPONSE_OK)
    {
      /* get the selected font from the dialog */
      font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (dialog));

      /* store the font in the preferences */
      MOUSEPAD_SETTING_SET_STRING (FONT, font);

      /* stop using default font */
      MOUSEPAD_SETTING_SET_BOOLEAN (USE_DEFAULT_FONT, FALSE);

      /* cleanup */
      g_free (font);
    }

  /* destroy the dialog */
  gtk_window_destroy (GTK_WINDOW (dialog));
}
