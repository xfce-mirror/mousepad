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
#include <mousepad/mousepad-settings.h>
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-marshal.h>



static void              mousepad_replace_dialog_unrealize              (GtkWidget             *widget);
static void              mousepad_replace_dialog_finalize               (GObject               *object);
static void              mousepad_replace_dialog_response               (GtkWidget             *widget,
                                                                         gint                   response_id);
static void              mousepad_replace_dialog_search_completed       (MousepadReplaceDialog *dialog,
                                                                         gint                   n_matches,
                                                                         const gchar           *search_string,
                                                                         MousepadSearchFlags    flags);
static void              mousepad_replace_dialog_changed                (MousepadReplaceDialog *dialog);
static void              mousepad_replace_dialog_entry_activate         (MousepadReplaceDialog *dialog);
static void              mousepad_replace_dialog_entry_reverse_activate (MousepadReplaceDialog *dialog);
static void              mousepad_replace_dialog_history_combo_box      (GtkComboBoxText       *combo_box);
static void              mousepad_replace_dialog_history_insert_text    (const gchar           *text);



struct _MousepadReplaceDialogClass
{
  GtkDialogClass __parent__;
};

struct _MousepadReplaceDialog
{
  GtkDialog __parent__;

  /* dialog widgets */
  GtkWidget *search_entry;
  GtkWidget *replace_entry;
  GtkWidget *find_button;
  GtkWidget *replace_button;
  GtkWidget *search_location_combo;
  GtkWidget *hits_label;
  GtkWidget *spinner;
};

enum
{
  IN_SELECTION = 0,
  IN_DOCUMENT,
  IN_ALL_DOCUMENTS
};

enum
{
  DIRECTION_UP = 0,
  DIRECTION_DOWN
};

enum
{
  SEARCH,
  LAST_SIGNAL
};



static GSList *history_list = NULL;
static guint   dialog_signals[LAST_SIGNAL];



G_DEFINE_TYPE (MousepadReplaceDialog, mousepad_replace_dialog, GTK_TYPE_DIALOG)


static void
mousepad_replace_dialog_class_init (MousepadReplaceDialogClass *klass)
{
  GObjectClass     *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass   *widget_class = GTK_WIDGET_CLASS (klass);
  GtkApplication   *application;
  GdkModifierType   accel_mods;
  guint             n, accel_key;
  gchar           **accels;
  const gchar      *actions[] = { "win.edit.cut", "win.edit.copy", "win.edit.paste",
                                  "win.edit.select-all" };
  const gchar      *signals[] = { "cut-clipboard", "copy-clipboard", "paste-clipboard",
                                  "select-all" };

  gobject_class->finalize = mousepad_replace_dialog_finalize;

  widget_class->unrealize = mousepad_replace_dialog_unrealize;

  dialog_signals[SEARCH] =
    g_signal_new (I_("search"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, _mousepad_marshal_VOID__FLAGS_STRING_STRING, G_TYPE_NONE, 3,
                  MOUSEPAD_TYPE_SEARCH_FLAGS, G_TYPE_STRING, G_TYPE_STRING);

  /* add an activate-backwards signal to GtkEntry */
  widget_class = g_type_class_ref (GTK_TYPE_ENTRY);
  if (G_LIKELY (g_signal_lookup ("activate-backward", GTK_TYPE_ENTRY) == 0))
    {
      g_signal_new ("activate-backward", GTK_TYPE_ENTRY, G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
      gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, GDK_SHIFT_MASK,
                                           "activate-backward", NULL);
      gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, GDK_SHIFT_MASK,
                                           "activate-backward", NULL);
    }

  g_type_class_unref (widget_class);

  /* add a select-all signal to GtkText (GtkEntry child) */
  widget_class = g_type_class_ref (GTK_TYPE_TEXT);
  if (G_LIKELY (g_signal_lookup ("select-all", GTK_TYPE_TEXT) == 0))
    {
      g_signal_new ("select-all", GTK_TYPE_TEXT, G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
      gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_a, GDK_CONTROL_MASK,
                                           "select-all", NULL);
    }

  /* make search entry keybindings consistent with those of the text view */
  application = GTK_APPLICATION (g_application_get_default ());
  accels = gtk_application_get_accels_for_action (application, "win.edit.delete-selection");
  if (accels[0] != NULL)
    {
      gtk_accelerator_parse (accels[0], &accel_key, &accel_mods);
      gtk_widget_class_add_binding_signal (widget_class, accel_key, accel_mods,
                                           "delete-from-cursor", "(ui)");
    }

  g_strfreev (accels);

  for (n = 0; n < G_N_ELEMENTS (actions); n++)
    {
      accels = gtk_application_get_accels_for_action (application, actions[n]);
      if (accels[0] != NULL)
        {
          gtk_accelerator_parse (accels[0], &accel_key, &accel_mods);
          gtk_widget_class_add_binding_signal (widget_class, accel_key, accel_mods,
                                               signals[n], NULL);
        }

      g_strfreev (accels);
    }

  /* cleanup */
  g_type_class_unref (widget_class);
}



static void
mousepad_replace_dialog_entry_select_all (GtkEntry *entry)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}



static void
mousepad_replace_dialog_bind_setting (MousepadReplaceDialog *dialog,
                                      const gchar           *path,
                                      gpointer               object,
                                      const gchar           *property)
{
  mousepad_setting_bind (path, object, property, G_SETTINGS_BIND_DEFAULT);

  mousepad_setting_connect_object (path,
                                   G_CALLBACK (mousepad_replace_dialog_changed),
                                   dialog,
                                   G_CONNECT_SWAPPED);
}



static void
mousepad_replace_dialog_update_label (MousepadReplaceDialog *dialog,
                                      GtkWidget             *check)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check));

  gtk_button_set_label (GTK_BUTTON (dialog->replace_button),
                        active ? _("_Replace All") : _("_Replace"));
}



static void
mousepad_replace_dialog_post_init (MousepadReplaceDialog *dialog)
{
  GtkWindow *window;

  /* disconnect this handler */
  mousepad_disconnect_by_func (dialog, mousepad_replace_dialog_post_init, NULL);

  /* setup CSD titlebar */
  mousepad_util_set_titlebar (GTK_WINDOW (dialog));

  /* get the transient parent window */
  window = gtk_window_get_transient_for (GTK_WINDOW (dialog));

  /* connect to the "search-completed" parent window signal */
  g_signal_connect_object (window, "search-completed",
                           G_CALLBACK (mousepad_replace_dialog_search_completed),
                           dialog, G_CONNECT_SWAPPED);

  /* give the dialog its definite size by setting a fake occurrences label */
  gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->search_entry));
  gtk_entry_set_text (GTK_ENTRY (dialog->search_entry), "fake-text");
  mousepad_replace_dialog_search_completed (dialog, 99999, "fake-text",
                                            MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION
                                            | MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS);

  /* show all widgets */
  gtk_widget_show_all (GTK_WIDGET (dialog));

  /* reset search entry and occurrences label */
  gtk_entry_set_text (GTK_ENTRY (dialog->search_entry), "");
  gtk_label_set_text (GTK_LABEL (dialog->hits_label), NULL);
}



static void
mousepad_replace_dialog_init (MousepadReplaceDialog *dialog)
{
  GtkWidget    *button, *area, *hbox, *combo, *label, *check;
  GtkSizeGroup *size_group;

  /* we will complete initialization when the parent window is set */
  g_signal_connect (dialog, "notify::transient-for",
                    G_CALLBACK (mousepad_replace_dialog_post_init), NULL);

  /* set dialog properties */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Find and Replace"));
  gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (mousepad_replace_dialog_response), NULL);

  /* dialog buttons */
  dialog->find_button = mousepad_util_image_button ("edit-find", _("_Find"));
  gtk_widget_set_can_default (dialog->find_button, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                dialog->find_button, MOUSEPAD_RESPONSE_FIND);

  dialog->replace_button = mousepad_util_image_button ("edit-find-replace", _("_Replace"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
                                dialog->replace_button, MOUSEPAD_RESPONSE_REPLACE);

  button = mousepad_util_image_button ("window-close", _("_Close"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, MOUSEPAD_RESPONSE_CLOSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_FIND);

  /* set content area */
  area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_set_spacing (GTK_BOX (area), 4);
  gtk_widget_set_margin_top (area, 6);

  /* horizontal box for search string */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start (hbox, 6);
  gtk_widget_set_margin_end (hbox, 6);
  gtk_box_append (GTK_BOX (area), hbox);

  /* create a size group */
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  label = gtk_label_new_with_mnemonic (_("_Search for:"));
  gtk_box_append (GTK_BOX (hbox), label);
  gtk_size_group_add_widget (size_group, label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);

  combo = gtk_combo_box_text_new_with_entry ();
  mousepad_replace_dialog_history_combo_box (GTK_COMBO_BOX_TEXT (combo));
  gtk_box_append (GTK_BOX (hbox), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

  /* store as an entry widget */
  dialog->search_entry = gtk_bin_get_child (GTK_BIN (combo));
  gtk_widget_set_hexpand (dialog->search_entry, TRUE);
  gtk_widget_set_vexpand (dialog->search_entry, TRUE);
  g_signal_connect_swapped (dialog->search_entry, "changed",
                            G_CALLBACK (mousepad_replace_dialog_changed), dialog);
  g_signal_connect_swapped (dialog->search_entry, "activate",
                            G_CALLBACK (mousepad_replace_dialog_entry_activate), dialog);
  g_signal_connect_swapped (dialog->search_entry, "activate-backward",
                            G_CALLBACK (mousepad_replace_dialog_entry_reverse_activate), dialog);
  g_signal_connect (gtk_widget_get_first_child (dialog->search_entry), "select-all",
                    G_CALLBACK (mousepad_replace_dialog_entry_select_all), NULL);

  /* bind the sensitivity of the find and replace buttons to the search text length */
  g_object_bind_property (dialog->search_entry, "text-length",
                          dialog->find_button, "sensitive", G_BINDING_SYNC_CREATE);
  g_object_bind_property (dialog->search_entry, "text-length",
                          dialog->replace_button, "sensitive", G_BINDING_SYNC_CREATE);

  /* horizontal box for replace string */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start (hbox, 6);
  gtk_widget_set_margin_end (hbox, 6);
  gtk_box_append (GTK_BOX (area), hbox);

  label = gtk_label_new_with_mnemonic (_("Replace _with:"));
  gtk_box_append (GTK_BOX (hbox), label);
  gtk_size_group_add_widget (size_group, label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);

  combo = gtk_combo_box_text_new_with_entry ();
  mousepad_replace_dialog_history_combo_box (GTK_COMBO_BOX_TEXT (combo));
  gtk_box_append (GTK_BOX (hbox), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

  /* store as an entry widget */
  dialog->replace_entry = gtk_bin_get_child (GTK_BIN (combo));
  gtk_widget_set_hexpand (dialog->replace_entry, TRUE);
  gtk_widget_set_vexpand (dialog->replace_entry, TRUE);
  g_signal_connect (gtk_widget_get_first_child (dialog->replace_entry), "select-all",
                    G_CALLBACK (mousepad_replace_dialog_entry_select_all), NULL);

  /* search direction */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start (hbox, 6);
  gtk_widget_set_margin_end (hbox, 6);
  gtk_box_append (GTK_BOX (area), hbox);

  label = gtk_label_new_with_mnemonic (_("Search _direction:"));
  gtk_box_append (GTK_BOX (hbox), label);
  gtk_size_group_add_widget (size_group, label);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);

  combo = gtk_combo_box_text_new ();
  gtk_box_append (GTK_BOX (hbox), combo);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Up"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Down"));

  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_DIRECTION,
                                        combo, "active");

  /* release size group */
  g_object_unref (size_group);

  /* wrap around */
  check = gtk_check_button_new_with_mnemonic (_("_Wrap around"));
  gtk_box_append (GTK_BOX (hbox), check);

  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_WRAP_AROUND,
                                        check, "active");

  /* case sensitive */
  check = gtk_check_button_new_with_mnemonic (_("Match _case"));
  gtk_widget_set_margin_start (check, 6);
  gtk_widget_set_margin_end (check, 6);
  gtk_box_append (GTK_BOX (area), check);

  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_MATCH_CASE,
                                        check, "active");

  /* match whole word */
  check = gtk_check_button_new_with_mnemonic (_("_Match whole word"));
  gtk_widget_set_margin_start (check, 6);
  gtk_widget_set_margin_end (check, 6);
  gtk_box_append (GTK_BOX (area), check);

  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_MATCH_WHOLE_WORD,
                                        check, "active");

  /* enable regex search */
  check = gtk_check_button_new_with_mnemonic (_("Regular e_xpression"));
  gtk_widget_set_margin_start (check, 6);
  gtk_widget_set_margin_end (check, 6);
  gtk_box_append (GTK_BOX (area), check);

  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_ENABLE_REGEX,
                                        check, "active");

  /* horizontal box for the replace all options */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_margin_start (hbox, 6);
  gtk_widget_set_margin_end (hbox, 6);
  gtk_widget_set_margin_bottom (hbox, 4);
  gtk_box_append (GTK_BOX (area), hbox);

  check = gtk_check_button_new_with_mnemonic (_("Replace _all in:"));
  gtk_box_append (GTK_BOX (hbox), check);

  combo = dialog->search_location_combo = gtk_combo_box_text_new ();
  gtk_box_append (GTK_BOX (hbox), combo);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Selection"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Document"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("All Documents"));

  g_signal_connect_swapped (check, "toggled",
                            G_CALLBACK (mousepad_replace_dialog_update_label), dialog);
  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_REPLACE_ALL,
                                        check, "active");
  mousepad_replace_dialog_bind_setting (dialog, MOUSEPAD_SETTING_SEARCH_REPLACE_ALL_LOCATION,
                                        combo, "active");
  g_object_bind_property (check, "active", combo, "sensitive", G_BINDING_SYNC_CREATE);

  /* the occurrences label */
  label = dialog->hits_label = gtk_label_new (NULL);
  gtk_box_append (GTK_BOX (hbox), label);

  /* the spinner */
  dialog->spinner = gtk_spinner_new ();
  gtk_box_append (GTK_BOX (hbox), dialog->spinner);
}



static void
mousepad_replace_dialog_unrealize (GtkWidget *widget)
{
  MousepadReplaceDialog *dialog = MOUSEPAD_REPLACE_DIALOG (widget);
  const gchar           *text;

  g_return_if_fail (GTK_IS_ENTRY (dialog->replace_entry));
  g_return_if_fail (GTK_IS_ENTRY (dialog->search_entry));

  text = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));
  mousepad_replace_dialog_history_insert_text (text);

  text = gtk_entry_get_text (GTK_ENTRY (dialog->replace_entry));
  mousepad_replace_dialog_history_insert_text (text);

  (*GTK_WIDGET_CLASS (mousepad_replace_dialog_parent_class)->unrealize) (widget);
}



static void
mousepad_replace_dialog_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (mousepad_replace_dialog_parent_class)->finalize) (object);
}



static void
mousepad_replace_dialog_reset_display (MousepadReplaceDialog *dialog)
{
  const gchar *string;

  /* reset occurrences label */
  gtk_label_set_text (GTK_LABEL (dialog->hits_label), NULL);

  /* start the spinner or reset entry color */
  string = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));
  if (string != NULL && *string != '\0')
    gtk_spinner_start (GTK_SPINNER (dialog->spinner));
  else
    mousepad_util_entry_error (dialog->search_entry, FALSE);
}



static void
mousepad_replace_dialog_response (GtkWidget *widget,
                                  gint       response_id)
{
  MousepadReplaceDialog *dialog = MOUSEPAD_REPLACE_DIALOG (widget);
  MousepadSearchFlags    flags;
  const gchar           *search_str, *replace_str;
  gint                   search_direction, replace_all_location;

  /* close dialog */
  if (response_id == MOUSEPAD_RESPONSE_CLOSE || response_id < 0)
    {
      gtk_widget_destroy (widget);
      return;
    }

  /* search direction */
  search_direction = MOUSEPAD_SETTING_GET_UINT (SEARCH_DIRECTION);
  if ((search_direction == DIRECTION_UP && response_id != MOUSEPAD_RESPONSE_REVERSE_FIND)
      || (search_direction != DIRECTION_UP && response_id == MOUSEPAD_RESPONSE_REVERSE_FIND))
    flags = MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD;
  else
    flags = MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD;

  /* search area */
  if (MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_REPLACE_ALL))
    {
      replace_all_location = MOUSEPAD_SETTING_GET_UINT (SEARCH_REPLACE_ALL_LOCATION);
      flags |= MOUSEPAD_SEARCH_FLAGS_ENTIRE_AREA;
      if (replace_all_location == IN_ALL_DOCUMENTS)
        flags |= MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS;
      else if (replace_all_location == IN_SELECTION)
        flags |= MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION;
    }

  /* start position */
  if (response_id == MOUSEPAD_RESPONSE_FIND || response_id == MOUSEPAD_RESPONSE_REVERSE_FIND)
    {
      /* select the first match */
      flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;

      /* start at the 'end' of the selection */
      if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START;
      else
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END;
    }
  else if (response_id == MOUSEPAD_RESPONSE_ENTRY_CHANGED)
    {
      /* select the first match if incremental search is enabled */
      if (MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_INCREMENTAL))
        flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;
      else
        flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_NONE;

      /* start at the 'beginning' of the selection */
      if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END;
      else
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START;
    }
  else if (response_id == MOUSEPAD_RESPONSE_REPLACE)
    {
      /* replace matches */
      flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_REPLACE;

      /* start at the 'beginning' of the selection */
      if (flags & MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD)
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END;
      else
        flags |= MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START;
    }

  /* get strings */
  search_str = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));
  replace_str = gtk_entry_get_text (GTK_ENTRY (dialog->replace_entry));

  /* reset display widgets */
  mousepad_replace_dialog_reset_display (dialog);

  /* emit the signal */
  g_signal_emit (dialog, dialog_signals[SEARCH], 0, flags, search_str, replace_str);
}



static void
mousepad_replace_dialog_search_completed (MousepadReplaceDialog *dialog,
                                          gint                   n_matches,
                                          const gchar           *search_string,
                                          MousepadSearchFlags    flags)
{
  gchar       *message;
  const gchar *string;

  /* get the entry string */
  string = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));

  /* leave the dialog unchanged if the search was launched from the search bar
   * for a different string... */
  if (g_strcmp0 (string, search_string) != 0)
    {
      /* stop the spinner */
      gtk_spinner_stop (GTK_SPINNER (dialog->spinner));
      return;
    }
  /* ... or if irrelevant settings for it are in use here, without stopping the spinner
   * in this case (we are in multi-document mode and this is a partial result) */
  else if (MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_REPLACE_ALL)
           && MOUSEPAD_SETTING_GET_UINT (SEARCH_REPLACE_ALL_LOCATION) != IN_DOCUMENT
           && ! (flags & (MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION
                          | MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS)))
    return;

  /* stop the spinner */
  gtk_spinner_stop (GTK_SPINNER (dialog->spinner));

  if (string != NULL && *string != '\0')
    {
      /* update entry color */
      mousepad_util_entry_error (dialog->search_entry, n_matches == 0);

      /* update counter */
      message = g_strdup_printf (ngettext ("%d occurrence", "%d occurrences", n_matches),
                                 n_matches);
      gtk_label_set_markup (GTK_LABEL (dialog->hits_label), message);
      g_free (message);
    }
}



static void
mousepad_replace_dialog_changed (MousepadReplaceDialog *dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_ENTRY_CHANGED);
}



static void
mousepad_replace_dialog_entry_activate (MousepadReplaceDialog *dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_FIND);
}



static void
mousepad_replace_dialog_entry_reverse_activate (MousepadReplaceDialog *dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), MOUSEPAD_RESPONSE_REVERSE_FIND);
}



/**
 * History functions
 **/
static void
mousepad_replace_dialog_history_combo_box (GtkComboBoxText *combo_box)
{
  GSList *li;

  g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

  /* append the items from the history to the combobox */
  for (li = history_list; li != NULL; li = li->next)
    gtk_combo_box_text_append_text (combo_box, li->data);
}



static void
mousepad_replace_dialog_history_insert_text (const gchar *text)
{
  GSList *li;

  /* quit if the box is empty */
  if (text == NULL || *text == '\0')
    return;

  /* check if the string is already in the history */
  for (li = history_list; li != NULL; li = li->next)
    if (strcmp (li->data, text) == 0)
      return;

  /* prepend the string */
  history_list = g_slist_prepend (history_list, g_strdup (text));
}



void
mousepad_replace_dialog_history_clean (void)
{
  GSList *li;

  if (history_list)
    {
      /* remove all the entries */
      for (li = history_list; li != NULL; li = li->next)
        {
          /* cleanup the string */
          g_free (li->data);

          /* remove the item from the list */
          history_list = g_slist_delete_link (history_list, li);
        }

      /* cleanup the list */
      g_slist_free (history_list);
    }
}



GtkWidget *
mousepad_replace_dialog_new (MousepadWindow *window)
{
  return g_object_new (MOUSEPAD_TYPE_REPLACE_DIALOG, "transient-for", window,
                       "destroy-with-parent", TRUE, NULL);
}



void
mousepad_replace_dialog_page_switched (MousepadReplaceDialog *dialog,
                                       GtkTextBuffer         *old_buffer,
                                       GtkTextBuffer         *new_buffer)
{
  /* disconnect from old buffer signals */
  if (old_buffer != NULL)
    mousepad_disconnect_by_func (old_buffer, mousepad_replace_dialog_reset_display, dialog);

  /* connect to new buffer signals to update display widgets on change */
  g_signal_connect_object (new_buffer, "insert-text",
                           G_CALLBACK (mousepad_replace_dialog_reset_display),
                           dialog, G_CONNECT_SWAPPED);
  g_signal_connect_object (new_buffer, "delete-range",
                           G_CALLBACK (mousepad_replace_dialog_reset_display),
                           dialog, G_CONNECT_SWAPPED);

  /* run a search */
  mousepad_replace_dialog_changed (dialog);
}



void
mousepad_replace_dialog_set_text (MousepadReplaceDialog *dialog,
                                  const gchar           *text)
{
  gtk_entry_set_text (GTK_ENTRY (dialog->search_entry), text);
  gtk_editable_select_region (GTK_EDITABLE (dialog->search_entry), 0, -1);
}
