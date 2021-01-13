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



void
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

  g_signal_connect_object (parent, "destroy",
                           G_CALLBACK (mousepad_dialogs_response_cancel),
                           dialog, G_CONNECT_SWAPPED);
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
  mousepad_dialogs_destroy_with_parent (dialog, parent);

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
  GError      *error = NULL;
  const gchar *uri = "https://docs.xfce.org/apps/mousepad/start";

  /* try to run the documentation browser */
  if (!gtk_show_uri_on_window (parent, uri, gtk_get_current_event_time (), &error))
    {
      /* display an error message to the user */
      mousepad_dialogs_show_error (parent, error, _("Failed to open the documentation browser"));
      g_error_free (error);
    }
}



gint
mousepad_dialogs_other_tab_size (GtkWindow *parent,
                                 gint      active_size)
{
  GtkWidget *dialog;
  GtkWidget *area;
  GtkWidget *scale;

  /* build dialog */
  dialog = gtk_dialog_new_with_buttons (_("Select Tab Size"), parent, GTK_DIALOG_MODAL,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL,
                                        _("_OK"), MOUSEPAD_RESPONSE_OK, NULL);
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  /* set properties */
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_OK);

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
  buffer = mousepad_object_get_data (col_spin, "buffer");

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
  dialog = gtk_dialog_new_with_buttons (_("Go To"), parent, GTK_DIALOG_MODAL,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  /* add button */
  button = mousepad_util_image_button ("go-jump", _("_Jump to"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_JUMP_TO);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_JUMP_TO);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

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

  /* release size group */
  g_object_unref (size_group);

  col_spin = gtk_spin_button_new_with_range (0, 0, 1);
  gtk_entry_set_activates_default (GTK_ENTRY (col_spin), TRUE);
  mousepad_object_set_data (col_spin, "buffer", buffer);
  gtk_box_pack_start (GTK_BOX (hbox), col_spin, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), col_spin);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (col_spin), TRUE);
  gtk_entry_set_width_chars (GTK_ENTRY (col_spin), 8);
  gtk_widget_show (col_spin);

  /* signal to monitor column number */
  g_signal_connect (line_spin, "value-changed",
                    G_CALLBACK (mousepad_dialogs_go_to_line_changed), col_spin);
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
  dialog = gtk_dialog_new_with_buttons (_("Clear Documents History"), parent, GTK_DIALOG_MODAL,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  /* set button */
  button = mousepad_util_image_button ("edit-clear", _("Clea_r"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_CLEAR);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);

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
  dialog = gtk_dialog_new_with_buttons (_("Save Changes"), parent, GTK_DIALOG_MODAL,
                                        _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  /* set properties */
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                mousepad_util_image_button ("edit-delete", _("_Don't Save")),
                                MOUSEPAD_RESPONSE_DONT_SAVE);

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
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  /* add title */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Externally Modified"));
  if (text_2 != NULL)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", text_2);

  /* add buttons */
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_CANCEL);

  button = mousepad_util_image_button (icon, label);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, button_response);

  if (saving)
    {
      button = mousepad_util_image_button ("document-save", _("_Save"));
      gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE);
    }

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
  dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                   _("Do you want to save your changes before reloading?"));
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  /* set subtitle */
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                   _("If you revert the file, all unsaved changes will be lost."));

  /* add buttons */
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Cancel"), MOUSEPAD_RESPONSE_CANCEL, NULL);

  button = mousepad_util_image_button ("document-save-as", _("_Save As"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_SAVE_AS);

  button = mousepad_util_image_button ("document-revert", _("_Revert"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_RELOAD);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

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
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  /* set subtitle */
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("If not, the guessed encoding will be used."));

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

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
  GSList              *files;
  GFile               *g_file;
  GFileIOStream       *iostream = NULL;
  GError              *error = NULL;
  gint                 row_type, n_rows = 2;
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

      /* "Open" dialog */
      if (gtk_file_chooser_get_action (GTK_FILE_CHOOSER (dialog))
          == GTK_FILE_CHOOSER_ACTION_OPEN)
        {
          /* get a list of selected locations, must be non empty */
          if ((files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog))) != NULL)
            {
              file = mousepad_file_new (GTK_TEXT_BUFFER (gtk_source_buffer_new (NULL)));
              mousepad_file_set_location (file, files->data, TRUE);
              g_slist_free_full (files, g_object_unref);
            }
          /* ask the user to select a file */
          else
            mousepad_dialogs_show_error (GTK_WINDOW (dialog), NULL, _("Please select a file"));
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
          gtk_combo_box_set_wrap_width (combo, n_rows / 10 + (n_rows % 10 != 0));
        }
      /* go back to shorten list */
      else
        {
          /* set shorten list */
          gtk_combo_box_set_model (combo, short_model);
          gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 2);
          gtk_combo_box_set_wrap_width (combo, 1);

          /* cleanup */
          g_object_unref (model);
          short_model = NULL;
        }

      /* reopen combo box */
      g_idle_add_full (G_PRIORITY_LOW, mousepad_dialogs_combo_popup, combo, NULL);
    }
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

  /* packing */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), hbox);
  gtk_widget_show (hbox);

  /* combo box label */
  widget = gtk_label_new_with_mnemonic (_("_Encoding:"));
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

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
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (widget), combo);
  gtk_widget_show (combo);

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
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  /* add button */
  button = mousepad_util_image_button ("document-save", _("_Save"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);

  /* set properties */
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

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
    gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (dialog),
                                              last_save_location, NULL);

  /* run the dialog */
  if ((response = gtk_dialog_run (GTK_DIALOG (dialog))) == GTK_RESPONSE_ACCEPT)
    {
      /* get the new location */
      *file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

      /* get the selected encoding */
      gtk_combo_box_get_active_iter (combo, &iter);
      gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter, 1, encoding, -1);
    }

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}



gint
mousepad_dialogs_open (GtkWindow         *parent,
                       GFile             *file,
                       GSList           **files,
                       MousepadEncoding  *encoding)
{
  GtkWidget   *dialog, *button;
  GtkComboBox *combo;
  GtkTreeIter  iter;
  gint         response;

  /* create new file chooser dialog */
  dialog = gtk_file_chooser_dialog_new (_("Open File"), parent, GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL, NULL);
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  /* add button */
  button = mousepad_util_image_button ("document-open", _("_Open"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);

  /* set properties */
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);

  /* add file filter */
  mousepad_dialogs_add_file_filter (GTK_FILE_CHOOSER (dialog));

  /* encoding selector */
  combo = mousepad_dialogs_add_encoding_combo (dialog);

  /* select the active document in the file chooser */
  if (file != NULL && g_file_query_exists (file, NULL))
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (dialog), file, NULL);

  /* run the dialog */
  if ((response = gtk_dialog_run (GTK_DIALOG (dialog))) == GTK_RESPONSE_ACCEPT)
    {
      /* get a list of selected locations */
      *files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog));

      /* get the selected encoding */
      gtk_combo_box_get_active_iter (combo, &iter);
      gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter, 1, encoding, -1);
    }

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  return response;
}



void
mousepad_dialogs_select_font (GtkWindow *parent)
{
  GtkWidget *dialog;
  gchar     *font;

  /* create new font chooser dialog */
  dialog = gtk_font_chooser_dialog_new (_("Choose Mousepad Font"), parent);
  mousepad_dialogs_destroy_with_parent (dialog, parent);

  /* set the current font */
  if ((font = MOUSEPAD_SETTING_GET_STRING (FONT)) != NULL)
    {
      gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), font);
      g_free (font);
    }

  /* run the dialog */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
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
  gtk_widget_destroy (dialog);
}
