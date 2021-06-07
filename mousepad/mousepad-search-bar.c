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
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-util.h>
#include <mousepad/mousepad-window.h>



static void      mousepad_search_bar_finalize                   (GObject                 *object);
static void      mousepad_search_bar_find_string                (MousepadSearchBar       *bar,
                                                                 MousepadSearchFlags      flags);
static void      mousepad_search_bar_search_completed           (MousepadSearchBar       *bar,
                                                                 gint                     n_matches,
                                                                 const gchar             *search_string,
                                                                 MousepadSearchFlags      flags);
static void      mousepad_search_bar_hide_clicked               (MousepadSearchBar       *bar);
static void      mousepad_search_bar_entry_activate             (MousepadSearchBar       *bar);
static void      mousepad_search_bar_entry_activate_backward    (MousepadSearchBar       *bar);
static void      mousepad_search_bar_entry_changed              (MousepadSearchBar       *bar);



enum
{
  HIDE_BAR,
  SEARCH,
  LAST_SIGNAL
};

struct _MousepadSearchBarClass
{
  GtkBoxClass __parent__;
};

struct _MousepadSearchBar
{
  GtkBox      __parent__;

  /* bar widgets */
  GtkWidget *entry;
  GtkWidget *hits_label;
  GtkWidget *spinner;
};



static guint search_bar_signals[LAST_SIGNAL];



GtkWidget *
mousepad_search_bar_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_SEARCH_BAR, NULL);
}



G_DEFINE_TYPE (MousepadSearchBar, mousepad_search_bar, GTK_TYPE_BOX)



static void
mousepad_search_bar_class_init (MousepadSearchBarClass *klass)
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

  gobject_class->finalize = mousepad_search_bar_finalize;

  /* signals */
  search_bar_signals[HIDE_BAR] =
    g_signal_new (I_("hide-bar"), G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  search_bar_signals[SEARCH] =
    g_signal_new (I_("search"), G_TYPE_FROM_CLASS (gobject_class), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, _mousepad_marshal_VOID__FLAGS_STRING_STRING,
                  G_TYPE_NONE, 3, MOUSEPAD_TYPE_SEARCH_FLAGS, G_TYPE_STRING, G_TYPE_STRING);

  /* setup key bindings for the search bar */
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Escape, 0, "hide-bar", NULL);

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
mousepad_search_bar_entry_select_all (GtkEntry *entry)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}



static void
mousepad_search_bar_post_init (MousepadSearchBar *bar)
{
  GtkWidget *window;

  /* disconnect this handler */
  mousepad_disconnect_by_func (bar, mousepad_search_bar_post_init, NULL);

  /* get the ancestor MousepadWindow */
  window = gtk_widget_get_ancestor (GTK_WIDGET (bar), MOUSEPAD_TYPE_WINDOW);

  /* connect to the "search-completed" window signal */
  g_signal_connect_object (window, "search-completed",
                           G_CALLBACK (mousepad_search_bar_search_completed),
                           bar, G_CONNECT_SWAPPED);
}



static void
mousepad_search_bar_init (MousepadSearchBar *bar)
{
  GtkWidget *item, *box;

  /* we will complete initialization when the bar is anchored */
  g_signal_connect (bar, "notify::root", G_CALLBACK (mousepad_search_bar_post_init), NULL);

  /* the close button */
  item = gtk_button_new_from_icon_name ("window-close-symbolic");
  gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
  g_signal_connect_swapped (item, "clicked", G_CALLBACK (mousepad_search_bar_hide_clicked), bar);
  gtk_box_append (GTK_BOX (bar), item);

  /* box for the search entry and its buttons */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (box, "linked");
  gtk_widget_set_margin_end (box, 6);
  gtk_box_append (GTK_BOX (bar), box);

  /* the entry field */
  bar->entry = gtk_entry_new ();
  g_signal_connect_swapped (bar->entry, "changed",
                            G_CALLBACK (mousepad_search_bar_entry_changed), bar);
  g_signal_connect_swapped (bar->entry, "activate",
                            G_CALLBACK (mousepad_search_bar_entry_activate), bar);
  g_signal_connect_swapped (bar->entry, "activate-backward",
                            G_CALLBACK (mousepad_search_bar_entry_activate_backward), bar);
  g_signal_connect (gtk_widget_get_first_child (bar->entry), "select-all",
                    G_CALLBACK (mousepad_search_bar_entry_select_all), NULL);
  gtk_box_append (GTK_BOX (box), bar->entry);

  /* previous button */
  item = gtk_button_new_from_icon_name ("go-up-symbolic");
  gtk_widget_set_can_focus (item, FALSE);
  g_signal_connect_swapped (item, "clicked", G_CALLBACK (mousepad_search_bar_find_previous), bar);
  gtk_box_append (GTK_BOX (box), item);

  /* next button */
  item = gtk_button_new_from_icon_name ("go-down-symbolic");
  gtk_widget_set_can_focus (item, FALSE);
  g_signal_connect_swapped (item, "clicked", G_CALLBACK (mousepad_search_bar_find_next), bar);
  gtk_box_append (GTK_BOX (box), item);

  /* check button for case sensitive, including the proxy menu item */
  item = gtk_check_button_new_with_mnemonic (_("Match _case"));
  MOUSEPAD_SETTING_BIND (SEARCH_MATCH_CASE, item, "active", G_SETTINGS_BIND_DEFAULT);
  g_signal_connect_swapped (item, "toggled", G_CALLBACK (mousepad_search_bar_entry_changed), bar);
  gtk_box_append (GTK_BOX (bar), item);

/* TODO Toolbar */
#if 0
  menu_item = gtk_check_menu_item_new_with_mnemonic (_("Match _case"));
  gtk_tool_item_set_proxy_menu_item (item, "case-sensitive", menu_item);
  g_object_bind_property (item, "active", menu_item, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
#endif

  /* check button for enabling regex, including the proxy menu item */
  item = gtk_check_button_new_with_mnemonic (_("Regular e_xpression"));
  MOUSEPAD_SETTING_BIND (SEARCH_ENABLE_REGEX, item, "active", G_SETTINGS_BIND_DEFAULT);
  g_signal_connect_swapped (item, "toggled", G_CALLBACK (mousepad_search_bar_entry_changed), bar);
  gtk_box_append (GTK_BOX (bar), item);

/* TODO Toolbar */
#if 0
  menu_item = gtk_check_menu_item_new_with_mnemonic (_("Regular e_xpression"));
  gtk_tool_item_set_proxy_menu_item (item, "enable-regex", menu_item);
  g_object_bind_property (item, "active", menu_item, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
#endif

  /* the occurrences label */
  item = bar->hits_label = gtk_label_new (NULL);
  gtk_widget_add_css_class (item, "dim-label");
  gtk_widget_set_margin_start (GTK_WIDGET (item), 6);
  gtk_box_append (GTK_BOX (bar), item);

  /* the spinner */
  item = bar->spinner = gtk_spinner_new ();
  gtk_box_append (GTK_BOX (bar), item);

/* TODO Toolbar */
#if 0
  /* overflow menu item for the spinner and the occurrences label */
  menu_item = gtk_menu_item_new ();
  gtk_tool_item_set_proxy_menu_item (item, "hits-label", menu_item);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (menu_item), box);

  item = gtk_label_new (NULL);
  gtk_widget_add_css_class (item, "dim-label");
  g_object_bind_property (bar->hits_label, "label", item, "label", G_BINDING_DEFAULT);
  gtk_box_append (GTK_BOX (box), item);

  item = gtk_spinner_new ();
  g_object_bind_property (bar->spinner, "active", item, "active", G_BINDING_DEFAULT);
  gtk_box_append (GTK_BOX (box), item);
#endif

  /* don't show the search bar yet */
  gtk_widget_hide (GTK_WIDGET (bar));
}



static void
mousepad_search_bar_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (mousepad_search_bar_parent_class)->finalize) (object);
}



static void
mousepad_search_bar_reset_display (MousepadSearchBar *bar)
{
  const gchar *string;

  /* reset occurrences label */
  gtk_label_set_text (GTK_LABEL (bar->hits_label), NULL);

  /* start the spinner or reset entry color */
  string = gtk_editable_get_text (GTK_EDITABLE (bar->entry));
  if (string != NULL && *string != '\0')
    gtk_spinner_start (GTK_SPINNER (bar->spinner));
  else
    mousepad_util_entry_error (bar->entry, FALSE);
}



static void
mousepad_search_bar_find_string (MousepadSearchBar   *bar,
                                 MousepadSearchFlags  flags)
{
  const gchar *string;

  /* always true when using the search bar */
  flags |= MOUSEPAD_SEARCH_FLAGS_WRAP_AROUND;

  /* always select unless it is a silent search */
  if (! (flags & MOUSEPAD_SEARCH_FLAGS_ACTION_NONE))
    flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;

  /* get the entry string */
  string = gtk_editable_get_text (GTK_EDITABLE (bar->entry));

  /* reset display widgets */
  mousepad_search_bar_reset_display (bar);

  /* emit signal */
  g_signal_emit (bar, search_bar_signals[SEARCH], 0, flags, string, NULL);
}



static void
mousepad_search_bar_search_completed (MousepadSearchBar   *bar,
                                      gint                 n_matches,
                                      const gchar         *search_string,
                                      MousepadSearchFlags  flags)
{
  gchar       *message;
  const gchar *string;

  /* stop the spinner */
  gtk_spinner_stop (GTK_SPINNER (bar->spinner));

  /* get the entry string */
  string = gtk_editable_get_text (GTK_EDITABLE (bar->entry));

  /* leave the search bar unchanged if the search was launched from the replace dialog
   * for a different string or irrelevant settings for the search bar*/
  if (g_strcmp0 (string, search_string) != 0
      || (flags & MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS)
      || (flags & MOUSEPAD_SEARCH_FLAGS_AREA_SELECTION))
    return;

  if (string != NULL && *string != '\0')
    {
      /* update entry color */
      mousepad_util_entry_error (bar->entry, n_matches == 0);

      /* update counter */
      message = g_strdup_printf (ngettext ("%d occurrence", "%d occurrences", n_matches),
                                 n_matches);
      gtk_label_set_markup (GTK_LABEL (bar->hits_label), message);
      g_free (message);
    }
}



static void
mousepad_search_bar_hide_clicked (MousepadSearchBar *bar)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* emit the signal */
  g_signal_emit (bar, search_bar_signals[HIDE_BAR], 0);
}



static void
mousepad_search_bar_entry_activate (MousepadSearchBar *bar)
{
  mousepad_search_bar_find_next (bar);
}



static void
mousepad_search_bar_entry_activate_backward (MousepadSearchBar *bar)
{
  mousepad_search_bar_find_previous (bar);
}



static void
mousepad_search_bar_entry_changed (MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  /* set the search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START
          | MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD;

  if (! MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_FIND_AS_YOU_TYPE))
    flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_NONE;

  /* find */
  mousepad_search_bar_find_string (bar, flags);
}



void
mousepad_search_bar_focus (MousepadSearchBar *bar)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* focus the entry field */
  gtk_widget_grab_focus (bar->entry);

  /* select the entire entry */
  gtk_editable_select_region (GTK_EDITABLE (bar->entry), 0, -1);
}



void
mousepad_search_bar_find_next (MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* set search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ITER_SEL_END
          | MOUSEPAD_SEARCH_FLAGS_DIR_FORWARD;

  /* search */
  mousepad_search_bar_find_string (bar, flags);
}



void
mousepad_search_bar_find_previous (MousepadSearchBar *bar)
{
  MousepadSearchFlags flags;

  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* set search flags */
  flags = MOUSEPAD_SEARCH_FLAGS_ITER_SEL_START
          | MOUSEPAD_SEARCH_FLAGS_DIR_BACKWARD;

  /* search */
  mousepad_search_bar_find_string (bar, flags);
}



void
mousepad_search_bar_page_switched (MousepadSearchBar *bar,
                                   GtkTextBuffer     *old_buffer,
                                   GtkTextBuffer     *new_buffer,
                                   gboolean           search)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  /* disconnect from old buffer signals */
  if (old_buffer != NULL)
    mousepad_disconnect_by_func (old_buffer, mousepad_search_bar_reset_display, bar);

  /* connect to new buffer signals to update display widgets on change */
  g_signal_connect_object (new_buffer, "insert-text",
                           G_CALLBACK (mousepad_search_bar_reset_display),
                           bar, G_CONNECT_SWAPPED);
  g_signal_connect_object (new_buffer, "delete-range",
                           G_CALLBACK (mousepad_search_bar_reset_display),
                           bar, G_CONNECT_SWAPPED);

  /* run a silent search */
  if (search)
    mousepad_search_bar_find_string (bar, MOUSEPAD_SEARCH_FLAGS_ACTION_NONE);
}



void
mousepad_search_bar_set_text (MousepadSearchBar *bar,
                              const gchar       *text)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  gtk_editable_set_text (GTK_EDITABLE (bar->entry), text);
}
