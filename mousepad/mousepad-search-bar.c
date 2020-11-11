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

#include <gdk/gdk.h>



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
  GtkToolbarClass __parent__;
};

struct _MousepadSearchBar
{
  GtkToolbar      __parent__;

  /* bar widgets */
  GtkWidget *entry;
  GtkWidget *hits_label;
};



static guint search_bar_signals[LAST_SIGNAL];



GtkWidget *
mousepad_search_bar_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_SEARCH_BAR,
                       "toolbar-style", GTK_TOOLBAR_BOTH_HORIZ,
                       "icon-size", GTK_ICON_SIZE_MENU,
                       NULL);
}



G_DEFINE_TYPE (MousepadSearchBar, mousepad_search_bar, GTK_TYPE_TOOLBAR)



static void
mousepad_search_bar_class_init (MousepadSearchBarClass *klass)
{
  GObjectClass  *gobject_class;
  GObjectClass  *entry_class;
  GtkBindingSet *binding_set;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_search_bar_finalize;

  /* signals */
  search_bar_signals[HIDE_BAR] =
    g_signal_new (I_("hide-bar"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  search_bar_signals[SEARCH] =
    g_signal_new (I_("search"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__FLAGS_STRING_STRING,
                  G_TYPE_NONE, 3,
                  MOUSEPAD_TYPE_SEARCH_FLAGS,
                  G_TYPE_STRING, G_TYPE_STRING);

  /* setup key bindings for the search bar */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "hide-bar", 0);

  /* add an activate-backwards signal to GtkEntry */
  entry_class = g_type_class_ref (GTK_TYPE_ENTRY);
  if (G_LIKELY (g_signal_lookup ("activate-backward", GTK_TYPE_ENTRY) == 0))
    {
      /* install the signal */
      g_signal_new ("activate-backward",
                   GTK_TYPE_ENTRY,
                   G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                   0, NULL, NULL,
                   g_cclosure_marshal_VOID__VOID,
                   G_TYPE_NONE, 0);
      binding_set = gtk_binding_set_by_class (entry_class);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return,
                                    GDK_SHIFT_MASK, "activate-backward", 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter,
                                    GDK_SHIFT_MASK, "activate-backward", 0);
    }
  g_type_class_unref (entry_class);
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
  GtkWidget   *check, *menuitem;
  GtkToolItem *item;

  /* we will complete initialization when the bar is anchored */
  g_signal_connect (bar, "hierarchy-changed", G_CALLBACK (mousepad_search_bar_post_init), NULL);

  /* the close button */
  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "window-close-symbolic");
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  g_signal_connect_swapped (item, "clicked", G_CALLBACK (mousepad_search_bar_hide_clicked), bar);

  /* the entry field */
  bar->entry = gtk_entry_new ();
  g_signal_connect_swapped (bar->entry, "changed",
                            G_CALLBACK (mousepad_search_bar_entry_changed), bar);
  g_signal_connect_swapped (bar->entry, "activate",
                            G_CALLBACK (mousepad_search_bar_entry_activate), bar);
  g_signal_connect_swapped (bar->entry, "activate-backward",
                            G_CALLBACK (mousepad_search_bar_entry_activate_backward), bar);

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), bar->entry);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  /* previous button */
  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "go-up-symbolic");
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  g_signal_connect_swapped (item, "clicked", G_CALLBACK (mousepad_search_bar_find_previous), bar);

  /* next button */
  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "go-down-symbolic");
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  g_signal_connect_swapped (item, "clicked", G_CALLBACK (mousepad_search_bar_find_next), bar);

  /* the occurrences label */
  item = gtk_tool_item_new ();
  bar->hits_label = gtk_label_new (NULL);
  gtk_container_add (GTK_CONTAINER (item), bar->hits_label);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  /* insert an invisible separator to push checkboxes to the right */
  item = gtk_separator_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);
  gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (item), FALSE);
  gtk_tool_item_set_expand (item, TRUE);

  /* check button for case sensitive, including the proxy menu item */
  check = gtk_check_button_new_with_mnemonic (_("Match _case"));
  g_signal_connect_swapped (check, "toggled", G_CALLBACK (mousepad_search_bar_entry_changed), bar);

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), check);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  /* keep the widget in sync with the GSettings */
  MOUSEPAD_SETTING_BIND (SEARCH_MATCH_CASE, check, "active", G_SETTINGS_BIND_DEFAULT);

  /* overflow menu item for when window is too narrow to show the tool bar item */
  menuitem = gtk_check_menu_item_new_with_mnemonic (_("Match _case"));
  gtk_tool_item_set_proxy_menu_item (item, "case-sensitive", menuitem);

  /* keep toolbar check button and overflow proxy menu item in sync */
  g_object_bind_property (check, "active", menuitem, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* check button for enabling regex, including the proxy menu item */
  check = gtk_check_button_new_with_mnemonic (_("Regular e_xpression"));
  g_signal_connect_swapped (check, "toggled", G_CALLBACK (mousepad_search_bar_entry_changed), bar);

  item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (item), check);
  gtk_toolbar_insert (GTK_TOOLBAR (bar), item, -1);

  /* keep the widget in sync with the GSettings */
  MOUSEPAD_SETTING_BIND (SEARCH_ENABLE_REGEX, check, "active", G_SETTINGS_BIND_DEFAULT);

  /* overflow menu item for when window is too narrow to show the tool bar item */
  menuitem = gtk_check_menu_item_new_with_mnemonic (_("Regular e_xpression"));
  gtk_tool_item_set_proxy_menu_item (item, "enable-regex", menuitem);

  /* keep toolbar check button and overflow proxy menu item in sync */
  g_object_bind_property (check, "active", menuitem, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  /* show all widgets but the search bar */
  gtk_widget_show_all (GTK_WIDGET (bar));
  gtk_widget_hide (GTK_WIDGET (bar));
}



static void
mousepad_search_bar_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (mousepad_search_bar_parent_class)->finalize) (object);
}



static void
mousepad_search_bar_find_string (MousepadSearchBar   *bar,
                                 MousepadSearchFlags  flags)
{
  const gchar *string;

  /* always true when using the search bar */
  flags |= MOUSEPAD_SEARCH_FLAGS_ACTION_SELECT;

  /* get the entry string */
  string = gtk_entry_get_text (GTK_ENTRY (bar->entry));

  /* reset entry color and occurrences label */
  mousepad_util_entry_error (bar->entry, FALSE);
  gtk_label_set_text (GTK_LABEL (bar->hits_label), NULL);

  /* emit signal */
  g_signal_emit (G_OBJECT (bar), search_bar_signals[SEARCH], 0, flags, string, NULL);
}



static void
mousepad_search_bar_search_completed (MousepadSearchBar   *bar,
                                      gint                 n_matches,
                                      const gchar         *search_string,
                                      MousepadSearchFlags  flags)
{
  gchar       *message;
  const gchar *string;

  /* get the entry string */
  string = gtk_entry_get_text (GTK_ENTRY (bar->entry));

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
  g_signal_emit (G_OBJECT (bar), search_bar_signals[HIDE_BAR], 0);
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

  /* find */
  mousepad_search_bar_find_string (bar, flags);
}



GtkEditable *
mousepad_search_bar_entry (MousepadSearchBar *bar)
{
  if (bar && gtk_widget_has_focus (bar->entry))
    return GTK_EDITABLE (bar->entry);
  else
    return NULL;
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
mousepad_search_bar_set_text (MousepadSearchBar *bar, gchar *text)
{
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (bar));

  gtk_entry_set_text (GTK_ENTRY (bar->entry), text);
}
