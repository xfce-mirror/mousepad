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

#include "mousepad-private.h"
#include "mousepad-application.h"
#include "mousepad-dialogs.h"
#include "mousepad-encoding.h"
#include "mousepad-plugin-provider.h"
#include "mousepad-prefs-dialog.h"
#include "mousepad-settings.h"
#include "mousepad-util.h"

#if defined(GDK_WINDOWING_X11) && !GTK_CHECK_VERSION(4, 0, 0)
#include <gdk/gdkx.h>
#endif



#define WID_NOTEBOOK "/prefs/main-notebook"

/* View page */
#define WID_RIGHT_MARGIN_SPIN "/prefs/view/display/long-line-spin"
#define WID_FONT_BUTTON "/prefs/view/font/chooser-button"
#define WID_SCHEME_COMBO "/prefs/view/color-scheme/combo"
#define WID_SCHEME_MODEL "/prefs/view/color-scheme/model"

/* Editor page */
#define WID_TAB_WIDTH_SPIN "/prefs/editor/indentation/tab-width-spin"
#define WID_TAB_MODE_COMBO "/prefs/editor/indentation/tab-mode-combo"
#define WID_SMART_HOME_END_COMBO "/prefs/editor/smart-keys/smart-home-end-combo"

/* Window page */
#define WID_TOOLBAR_STYLE_COMBO "/prefs/window/toolbar/style-combo"
#define WID_TOOLBAR_ICON_SIZE_COMBO "/prefs/window/toolbar/icon-size-combo"
#define WID_TOOLBAR_ITEMS_LIST "/prefs/window/toolbar/items-list"
#define WID_OPENING_MODE_COMBO "/prefs/window/notebook/opening-mode-combo"

/* File page */
#define WID_RECENT_SPIN "/prefs/file/history/recent-spin"
#define WID_SEARCH_SPIN "/prefs/file/history/search-spin"
#define WID_SESSION_COMBO "/prefs/file/history/session-combo"
#define WID_ENCODING_COMBO "/prefs/file/history/encoding-combo"
#define WID_ENCODING_MODEL "/prefs/file/history/encoding-model"

/* Plugins page */
#define WID_PLUGINS_TAB "/prefs/plugins/scrolled-window"
#define WID_PLUGINS_BOX "/prefs/plugins/content-area"



enum
{
  COLUMN_ID,
  COLUMN_NAME,
};



struct _MousepadPrefsDialog
{
  GtkDialog parent;

  GtkBuilder *builder;
  gboolean blocked;
};



static void
mousepad_prefs_dialog_constructed (GObject *object);
static void
mousepad_prefs_dialog_finalize (GObject *object);



G_DEFINE_TYPE (MousepadPrefsDialog, mousepad_prefs_dialog, GTK_TYPE_DIALOG)



static void
mousepad_prefs_dialog_class_init (MousepadPrefsDialogClass *klass)
{
  GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->constructed = mousepad_prefs_dialog_constructed;
  g_object_class->finalize = mousepad_prefs_dialog_finalize;
}



static void
mousepad_prefs_dialog_constructed (GObject *object)
{
  GtkWindow *dialog = GTK_WINDOW (object);

  G_OBJECT_CLASS (mousepad_prefs_dialog_parent_class)->constructed (object);

  /* setup CSD titlebar */
  mousepad_util_set_titlebar (dialog);
}



static void
mousepad_prefs_dialog_finalize (GObject *object)
{
  MousepadPrefsDialog *self;

  g_return_if_fail (MOUSEPAD_IS_PREFS_DIALOG (object));

  self = MOUSEPAD_PREFS_DIALOG (object);

  /* destroy the GtkBuilder instance */
  if (self->builder != NULL)
    g_object_unref (self->builder);

  G_OBJECT_CLASS (mousepad_prefs_dialog_parent_class)->finalize (object);
}



/* this is necessary to preserve the setting box, which is apparently not only removed
 * from the popover when it is detroyed */
static void
mousepad_prefs_dialog_remove_setting_box (gpointer popover)
{
  GtkWidget *child;

  if ((child = gtk_bin_get_child (popover)) != NULL)
    gtk_container_remove (popover, child);
}



#if defined(GDK_WINDOWING_X11) && !GTK_CHECK_VERSION(4, 0, 0)
/*
 * Popovers suffer from a limitation in GTK 3 on X11: they cannot extend outside their
 * toplevel window. So we have to resize the dialog before showing the popover in this case.
 * See https://gitlab.gnome.org/GNOME/gtk/-/issues/543
 */
static gboolean
mousepad_prefs_dialog_drawn (GtkWidget *dialog,
                             cairo_t *cr,
                             gpointer popover)
{
  /* disconnect this handler */
  mousepad_disconnect_by_func (dialog, mousepad_prefs_dialog_drawn, popover);

  /* it's finally time to show the popover */
  gtk_widget_show (popover);

  return FALSE;
}



static gboolean
mousepad_prefs_dialog_popover_allocate_finish (gpointer popover)
{
  GtkWidget *dialog, *button;
  GtkAllocation balloc, dalloc, palloc;

  /* get widget allocations */
  button = gtk_popover_get_relative_to (popover);
  dialog = gtk_widget_get_ancestor (button, MOUSEPAD_TYPE_PREFS_DIALOG);
  gtk_widget_get_allocation (button, &balloc);
  gtk_widget_get_allocation (dialog, &dalloc);
  gtk_widget_get_allocation (popover, &palloc);

  /* resize the dialog so it contains the popover */
  gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_LEFT);
  gtk_window_resize (GTK_WINDOW (dialog),
                     MAX (dalloc.width, palloc.width + dalloc.width - balloc.x),
                     MAX (dalloc.height, palloc.height));

  /* we will show the popover when the dialog is effectively resized */
  g_signal_connect_after (dialog, "draw", G_CALLBACK (mousepad_prefs_dialog_drawn), popover);

  return FALSE;
}



static void
mousepad_prefs_dialog_popover_allocate (GtkWidget *popover,
                                        GdkRectangle *palloc,
                                        GtkWidget *dialog)
{
  GtkAllocation dalloc;
  gint dx, dy, px, py;

  /* disconnect this handler */
  mousepad_disconnect_by_func (popover, mousepad_prefs_dialog_popover_allocate, dialog);

  /* do nothing if the dialog already contains the popover */
  gdk_window_get_origin (gtk_widget_get_window (dialog), &dx, &dy);
  gdk_window_get_origin (gtk_widget_get_window (popover), &px, &py);
  gtk_widget_get_allocation (dialog, &dalloc);
  if (px >= dx && px + palloc->width <= dx + dalloc.width
      && py >= dy && py + palloc->height <= dy + dalloc.height)
    return;

  /* we will show the popover when the dialog is resized */
  gtk_widget_hide (popover);

  /* we need first to exit this handler */
  g_idle_add_full (G_PRIORITY_HIGH, mousepad_prefs_dialog_popover_allocate_finish, popover, NULL);
}
#endif



static gboolean
mousepad_prefs_dialog_checkbox_toggled_idle (gpointer data)
{
  MousepadPluginProvider *provider;
  GtkWidget *button = data, *box, *popover;
  gboolean visible;

  provider = mousepad_object_get_data (button, "provider");
  box = mousepad_plugin_provider_get_setting_box (provider);
  visible = gtk_widget_get_visible (button);

  /* the plugin has a setting box which is not already packed (e.g. in a dialog)
   * and the prefs button is hidden: it is time to show it with its popover */
  if (box != NULL && !visible && gtk_widget_get_parent (box) == NULL)
    {
      popover = gtk_popover_new (button);
      gtk_container_add (GTK_CONTAINER (popover), box);
      g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_show), popover);
      g_signal_connect_swapped (button, "destroy",
                                G_CALLBACK (mousepad_prefs_dialog_remove_setting_box),
                                popover);
#if defined(GDK_WINDOWING_X11) && !GTK_CHECK_VERSION(4, 0, 0)
      /* see comment at the beginning of the corresponding code section above */
      if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        g_signal_connect (popover, "size-allocate",
                          G_CALLBACK (mousepad_prefs_dialog_popover_allocate),
                          gtk_widget_get_ancestor (button, MOUSEPAD_TYPE_PREFS_DIALOG));
#endif
      gtk_widget_show (button);
    }
  /* the setting box was destroyed (normally with its plugin): hide the prefs button */
  else if (box == NULL && visible)
    gtk_widget_hide (button);

  return FALSE;
}



static void
mousepad_prefs_dialog_checkbox_toggled (GtkToggleButton *checkbox,
                                        GtkWidget *button)
{
  /* wait for the plugin to get its setting box, or for this box to be destroyed */
  g_idle_add (mousepad_prefs_dialog_checkbox_toggled_idle,
              mousepad_util_source_autoremove (button));
}



static void
mousepad_prefs_dialog_plugins_tab (GtkNotebook *notebook,
                                   GtkWidget *page,
                                   guint page_num,
                                   GtkWidget *plugins_tab)
{
  MousepadPrefsDialog *self;
  MousepadApplication *application;
  GtkWidget *box, *widget, *child, *grid = NULL;
  GdkModifierType mods;
  GTypeModule *module;
  GList *providers, *provider;
  gchar **accels;
  const gchar *category = NULL;
  gchar *str;
  guint n, key;

  /* filter other tabs */
  if (page != plugins_tab)
    return;

  /* disconnect this handler */
  mousepad_disconnect_by_func (notebook, mousepad_prefs_dialog_plugins_tab, plugins_tab);

  /* setup the "Plugins" tab */
  self = MOUSEPAD_PREFS_DIALOG (gtk_widget_get_ancestor (page, MOUSEPAD_TYPE_PREFS_DIALOG));
  box = GTK_WIDGET (gtk_builder_get_object (self->builder, WID_PLUGINS_BOX));
  application = MOUSEPAD_APPLICATION (g_application_get_default ());
  providers = mousepad_application_get_providers (application);
  for (provider = providers, n = 0; provider != NULL; provider = provider->next, n++)
    {
      /* update current category: new frame, new grid */
      str = (gchar *) mousepad_plugin_provider_get_category (provider->data);
      if (g_strcmp0 (category, str) != 0)
        {
          category = str;

          str = g_strdup_printf ("<b>%s</b>", category);
          child = gtk_label_new (str);
          gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
          g_free (str);

          widget = gtk_frame_new (NULL);
          gtk_frame_set_label_widget (GTK_FRAME (widget), child);
          gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_NONE);
          gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);

          grid = gtk_grid_new ();
          gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
          gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
          gtk_widget_set_margin_start (grid, 12);
          gtk_widget_set_margin_end (grid, 6);
          gtk_widget_set_margin_top (grid, 6);
          gtk_widget_set_margin_bottom (grid, 6);
          gtk_container_add (GTK_CONTAINER (widget), grid);

          /* show all widgets in the frame */
          gtk_widget_show_all (widget);
        }

      /* add the provider checkbox to the grid and build its action name */
      widget = gtk_check_button_new ();
      gtk_grid_attach (GTK_GRID (grid), widget, 0, n, 1, 1);
      module = provider->data;
      str = g_strconcat ("app.", module->name, NULL);

      /* add an accel label and a tooltip to the provider checkbox */
      child = gtk_accel_label_new (mousepad_plugin_provider_get_label (provider->data));
      gtk_widget_set_hexpand (child, TRUE);
      accels = gtk_application_get_accels_for_action (GTK_APPLICATION (application), str);
      key = mods = 0;
      if (accels[0] != NULL)
        gtk_accelerator_parse (accels[0], &key, &mods);

      gtk_accel_label_set_accel (GTK_ACCEL_LABEL (child), key, mods);
      g_strfreev (accels);
      gtk_container_add (GTK_CONTAINER (widget), child);
      gtk_widget_set_tooltip_text (widget, mousepad_plugin_provider_get_tooltip (provider->data));

      /* add a prefs button to the grid */
      child = gtk_button_new_from_icon_name ("preferences-system", GTK_ICON_SIZE_BUTTON);
      gtk_grid_attach (GTK_GRID (grid), child, 1, n, 1, 1);

      /* show the button if the plugin already has a setting box, or when it gets one */
      mousepad_object_set_data (child, "provider", provider->data);
      mousepad_prefs_dialog_checkbox_toggled_idle (child);
      g_signal_connect (widget, "toggled",
                        G_CALLBACK (mousepad_prefs_dialog_checkbox_toggled), child);

      /* make the provider checkbox actionable, triggering in particular the handler above */
      gtk_actionable_set_action_name (GTK_ACTIONABLE (widget), str);
      g_free (str);

      /* show all widgets in the checkbox */
      gtk_widget_show_all (widget);
    }
}



/* update the color scheme when the prefs dialog widget changes */
static void
mousepad_prefs_dialog_color_scheme_changed (MousepadPrefsDialog *self,
                                            GtkComboBox *combo)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gchar *scheme_id = NULL;

  store = GTK_LIST_STORE (gtk_builder_get_object (self->builder, WID_SCHEME_MODEL));

  gtk_combo_box_get_active_iter (combo, &iter);

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, COLUMN_ID, &scheme_id, -1);

  self->blocked = TRUE;
  MOUSEPAD_SETTING_SET_STRING (COLOR_SCHEME, scheme_id);
  self->blocked = FALSE;

  g_free (scheme_id);
}



/* udpate the color schemes combo when the setting changes */
static void
mousepad_prefs_dialog_color_scheme_setting_changed (MousepadPrefsDialog *self,
                                                    gchar *key,
                                                    GSettings *settings)
{
  gchar *new_id;
  GtkTreeIter iter;
  gboolean iter_valid;
  GtkComboBox *combo;
  GtkTreeModel *model;

  /* don't do anything when the combo box is itself updating the setting */
  if (self->blocked)
    return;

  new_id = MOUSEPAD_SETTING_GET_STRING (COLOR_SCHEME);

  combo = GTK_COMBO_BOX (gtk_builder_get_object (self->builder, WID_SCHEME_COMBO));
  model = GTK_TREE_MODEL (gtk_builder_get_object (self->builder, WID_SCHEME_MODEL));

  iter_valid = gtk_tree_model_get_iter_first (model, &iter);
  while (iter_valid)
    {
      gchar *id = NULL;
      gboolean equal;

      gtk_tree_model_get (model, &iter, COLUMN_ID, &id, -1);
      equal = (g_strcmp0 (id, new_id) == 0);
      g_free (id);

      if (equal)
        {
          gtk_combo_box_set_active_iter (combo, &iter);
          break;
        }

      iter_valid = gtk_tree_model_iter_next (model, &iter);
    }

  g_free (new_id);
}



/* add available color schemes to the combo box model */
static void
mousepad_prefs_dialog_setup_color_schemes_combo (MousepadPrefsDialog *self)
{
  GtkSourceStyleSchemeManager *manager;
  const gchar *const *ids;
  const gchar *const *id_iter;
  GtkListStore *store;
  GtkTreeIter iter;

  store = GTK_LIST_STORE (gtk_builder_get_object (self->builder, WID_SCHEME_MODEL));
  manager = gtk_source_style_scheme_manager_get_default ();
  ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

  for (id_iter = ids; *id_iter; id_iter++)
    {
      GtkSourceStyleScheme *scheme;

      scheme = gtk_source_style_scheme_manager_get_scheme (manager, *id_iter);
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          COLUMN_ID, gtk_source_style_scheme_get_id (scheme),
                          COLUMN_NAME, gtk_source_style_scheme_get_name (scheme),
                          -1);
    }

  /* set the active item from the settings */
  mousepad_prefs_dialog_color_scheme_setting_changed (self, NULL, NULL);

  g_signal_connect_swapped (gtk_builder_get_object (self->builder, WID_SCHEME_COMBO), "changed",
                            G_CALLBACK (mousepad_prefs_dialog_color_scheme_changed), self);

  MOUSEPAD_SETTING_CONNECT_OBJECT (COLOR_SCHEME,
                                   mousepad_prefs_dialog_color_scheme_setting_changed,
                                   self, G_CONNECT_SWAPPED);
}



/* update the setting when the prefs dialog widget changes */
static void
mousepad_prefs_dialog_tab_mode_changed (MousepadPrefsDialog *self,
                                        GtkComboBox *combo)
{
  self->blocked = TRUE;
  /* in the combo box, id 0 is tabs, and 1 is spaces */
  MOUSEPAD_SETTING_SET_BOOLEAN (INSERT_SPACES,
                                (gtk_combo_box_get_active (combo) == 1));

  self->blocked = FALSE;
}



/* update the combo box when the setting changes */
static void
mousepad_prefs_dialog_tab_mode_setting_changed (MousepadPrefsDialog *self,
                                                gchar *key,
                                                GSettings *settings)
{
  gboolean insert_spaces;
  GtkComboBox *combo;

  /* don't do anything when the combo box is itself updating the setting */
  if (self->blocked)
    return;

  insert_spaces = MOUSEPAD_SETTING_GET_BOOLEAN (INSERT_SPACES);

  combo = GTK_COMBO_BOX (gtk_builder_get_object (self->builder, WID_TAB_MODE_COMBO));

  /* in the combo box, id 0 is tabs, and 1 is spaces */
  gtk_combo_box_set_active (combo, insert_spaces ? 1 : 0);
}



/* update encoding when the prefs dialog widget changes */
static void
mousepad_prefs_dialog_encoding_changed (MousepadPrefsDialog *self,
                                        GtkComboBox *combo)
{
  MousepadEncoding encoding;

  encoding = gtk_combo_box_get_active (combo) + 1;

  self->blocked = TRUE;
  MOUSEPAD_SETTING_SET_STRING (DEFAULT_ENCODING, mousepad_encoding_get_charset (encoding));
  self->blocked = FALSE;
}



/* udpate encoding combo when the setting changes */
static void
mousepad_prefs_dialog_encoding_setting_changed (MousepadPrefsDialog *self,
                                                gchar *key,
                                                GSettings *settings)
{
  GtkComboBox *combo;
  gchar *charset;

  /* don't do anything when the combo box is itself updating the setting */
  if (self->blocked)
    return;

  combo = GTK_COMBO_BOX (gtk_builder_get_object (self->builder, WID_ENCODING_COMBO));
  charset = MOUSEPAD_SETTING_GET_STRING (DEFAULT_ENCODING);

  gtk_combo_box_set_active (combo, mousepad_encoding_find (charset) - 1);

  g_free (charset);
}



/* add available encodings to the combo box model */
static void
mousepad_prefs_dialog_setup_encoding_combo (MousepadPrefsDialog *self)
{
  MousepadEncoding encoding;
  GObject *combo;
  GtkListStore *store;
  gint n_rows;

  store = GTK_LIST_STORE (gtk_builder_get_object (self->builder, WID_ENCODING_MODEL));
  for (encoding = 1; encoding < MOUSEPAD_N_ENCODINGS; encoding++)
    gtk_list_store_insert_with_values (store, NULL, encoding - 1, COLUMN_ID, encoding,
                                       COLUMN_NAME, mousepad_encoding_get_charset (encoding), -1);

  n_rows = MOUSEPAD_N_ENCODINGS - 1;
  combo = gtk_builder_get_object (self->builder, WID_ENCODING_COMBO);
  gtk_combo_box_set_wrap_width (GTK_COMBO_BOX (combo), n_rows / 10 + (n_rows % 10 != 0));

  /* set the active item from the settings and connect handlers */
  mousepad_prefs_dialog_encoding_setting_changed (self, NULL, NULL);

  g_signal_connect_swapped (combo, "changed",
                            G_CALLBACK (mousepad_prefs_dialog_encoding_changed), self);

  MOUSEPAD_SETTING_CONNECT_OBJECT (DEFAULT_ENCODING,
                                   mousepad_prefs_dialog_encoding_setting_changed,
                                   self, G_CONNECT_SWAPPED);
}



/* ask user for confirmation before clearing recent history */
static void
mousepad_prefs_dialog_recent_spin_changed (MousepadPrefsDialog *self,
                                           GtkSpinButton *button)
{
  guint n_items;

  n_items = gtk_spin_button_get_value (button);
  if (n_items > 0 || mousepad_dialogs_clear_recent (GTK_WINDOW (self)))
    MOUSEPAD_SETTING_SET_UINT (RECENT_MENU_ITEMS, n_items);
  else
    MOUSEPAD_SETTING_RESET (RECENT_MENU_ITEMS);
}



static void
mousepad_prefs_dialog_toolbar_item_toggled (GtkToggleButton *button,
                                            MousepadPrefsDialog *self)
{
  gchar **hidden_items;
  const gchar *action_name;
  GPtrArray *new_list;
  gboolean active;
  gchar **iter;

  action_name = g_object_get_data (G_OBJECT (button), "action-name");
  active = gtk_toggle_button_get_active (button);

  hidden_items = MOUSEPAD_SETTING_GET_STRV (TOOLBAR_HIDDEN_ITEMS);
  new_list = g_ptr_array_new_with_free_func (g_free);

  /* copy existing items, skipping action_name if present */
  for (iter = hidden_items; *iter != NULL; iter++)
    {
      if (g_strcmp0 (*iter, action_name) != 0)
        g_ptr_array_add (new_list, g_strdup (*iter));
    }

  /* if inactive (hidden), add to the list */
  if (!active)
    g_ptr_array_add (new_list, g_strdup (action_name));

  g_ptr_array_add (new_list, NULL);

  MOUSEPAD_SETTING_SET_STRV (TOOLBAR_HIDDEN_ITEMS, (const gchar *const *) new_list->pdata);

  g_ptr_array_free (new_list, TRUE);
  g_strfreev (hidden_items);
}



static void
mousepad_prefs_dialog_toolbar_alignment_changed (GtkComboBox *combo,
                                                 MousepadPrefsDialog *self)
{
  gchar **right_items;
  const gchar *action_name;
  GPtrArray *new_list;
  gint alignment; /* 0=Left, 1=Right */
  gchar **iter;
  gboolean exists = FALSE;

  action_name = g_object_get_data (G_OBJECT (combo), "action-name");
  alignment = gtk_combo_box_get_active (combo);

  right_items = MOUSEPAD_SETTING_GET_STRV (TOOLBAR_RIGHT_ITEMS);
  new_list = g_ptr_array_new_with_free_func (g_free);

  for (iter = right_items; *iter != NULL; iter++)
    {
      if (g_strcmp0 (*iter, action_name) == 0)
        exists = TRUE;

      if (alignment == 0 && g_strcmp0 (*iter, action_name) == 0)
        continue; /* Remove from right items (move to left) */

      g_ptr_array_add (new_list, g_strdup (*iter));
    }

  if (alignment == 1 && !exists)
    g_ptr_array_add (new_list, g_strdup (action_name));

  g_ptr_array_add (new_list, NULL);

  MOUSEPAD_SETTING_SET_STRV (TOOLBAR_RIGHT_ITEMS, (const gchar *const *) new_list->pdata);

  g_ptr_array_free (new_list, TRUE);
  g_strfreev (right_items);
}

static gchar *
mousepad_prefs_dialog_strip_mnemonic (const gchar *label)
{
  GString *str;
  const gchar *p;

  if (label == NULL)
    return NULL;

  str = g_string_sized_new (strlen (label));

  for (p = label; *p != '\0'; p++)
    {
      if (*p == '_')
        {
          if (p[1] == '_')
            {
              p++;
              g_string_append_c (str, '_');
            }
        }
      else
        g_string_append_c (str, *p);
    }

  return g_string_free (str, FALSE);
}



static void
mousepad_prefs_dialog_toolbar_add_row (MousepadPrefsDialog *self,
                                       GtkListBox *listbox,
                                       const gchar *label,
                                       const gchar *action_name,
                                       gboolean alignable,
                                       gboolean visible,
                                       gboolean is_right)
{
  GtkBox *box;
  GtkWidget *check, *combo, *row;

  /* Create Row Layout */
  box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));

  /* Visibility Check */
  check = gtk_check_button_new_with_label (label);
  gtk_widget_set_visible (check, TRUE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), visible);
  gtk_widget_set_hexpand (check, TRUE);
  g_object_set_data_full (G_OBJECT (check), "action-name", g_strdup (action_name), g_free);
  g_signal_connect (check, "toggled",
                    G_CALLBACK (mousepad_prefs_dialog_toolbar_item_toggled), self);
  gtk_box_pack_start (box, check, TRUE, TRUE, 0);

  /* Alignment Combo (only if alignable) */
  if (alignable)
    {
      combo = gtk_combo_box_text_new ();
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Left"));
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Right"));
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), is_right ? 1 : 0);
      gtk_widget_set_visible (combo, TRUE);
      g_object_set_data_full (G_OBJECT (combo), "action-name", g_strdup (action_name), g_free);
      g_signal_connect (combo, "changed",
                        G_CALLBACK (mousepad_prefs_dialog_toolbar_alignment_changed), self);
      gtk_box_pack_start (box, combo, FALSE, FALSE, 0);
    }

  /* Add to listbox */
  gtk_widget_set_visible (GTK_WIDGET (box), TRUE);
  gtk_list_box_insert (listbox, GTK_WIDGET (box), -1);

  /* Allow row activation */
  row = gtk_widget_get_parent (GTK_WIDGET (box));
  if (row)
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
}



static void
mousepad_prefs_dialog_toolbar_add_item (MousepadPrefsDialog *self,
                                        GtkListBox *listbox,
                                        GMenuModel *model,
                                        gint index,
                                        const gchar *const *hidden_items)
{
  gchar *label = NULL, *tooltip = NULL, *action = NULL, *share_id = NULL;
  gchar *clean_label;
  const gchar *display_label;
  const gchar *id_key;
  gboolean visible;
  gchar **right_items;
  gboolean is_right = FALSE;

  /* get attributes */
  if (g_menu_model_get_item_attribute (model, index, G_MENU_ATTRIBUTE_LABEL, "s", &label)
      && label != NULL && *label == '\0')
    {
      g_free (label);
      label = NULL;
    }

  g_menu_model_get_item_attribute (model, index, "tooltip", "s", &tooltip);
  g_menu_model_get_item_attribute (model, index, G_MENU_ATTRIBUTE_ACTION, "s", &action);
  g_menu_model_get_item_attribute (model, index, "item-share-id", "s", &share_id);

  /* determine display label */
  display_label = label ? label : (tooltip ? tooltip : (action ? action : share_id));
  if (display_label == NULL) /* should not happen */
    display_label = "???";

  /* clean the label */
  clean_label = mousepad_prefs_dialog_strip_mnemonic (display_label);

  /* determine ID key (prefer action, fallback to share-id) */
  id_key = action ? action : share_id;

  if (id_key != NULL)
    {
      /* determine visibility state */
      visible = !g_strv_contains (hidden_items, id_key);

      /* determine alignment state */
      right_items = MOUSEPAD_SETTING_GET_STRV (TOOLBAR_RIGHT_ITEMS);
      if (g_strv_contains ((const gchar *const *) right_items, id_key))
        is_right = TRUE;
      g_strfreev (right_items);

      /* Add row using helper */
      mousepad_prefs_dialog_toolbar_add_row (self, listbox, clean_label, id_key, TRUE, visible, is_right);
    }

  g_free (clean_label);
  g_free (label);
  g_free (tooltip);
  g_free (action);
  g_free (share_id);
}



/* Helper to create a pseudo-item row */
static void
mousepad_prefs_dialog_toolbar_add_pseudo_item (MousepadPrefsDialog *self,
                                               GtkListBox *listbox,
                                               const gchar *label,
                                               const gchar *action_name,
                                               gboolean alignable,
                                               const gchar *const *hidden_items,
                                               const gchar *const *right_items)
{
  gboolean visible, is_right = FALSE;

  /* determine visibility state */
  visible = !g_strv_contains (hidden_items, action_name);

  /* determine alignment state */
  if (alignable && g_strv_contains ((const gchar *const *) right_items, action_name))
    is_right = TRUE;

  /* Add row using helper */
  mousepad_prefs_dialog_toolbar_add_row (self, listbox, label, action_name, alignable, visible, is_right);
}

static void
mousepad_prefs_dialog_setup_toolbar_items (MousepadPrefsDialog *self)
{
  GtkListBox *listbox;
  GtkApplication *application;
  GMenuModel *model;
  GMenuModel *section;
  gchar **hidden_items;
  gint m, n, n_items;

  listbox = GTK_LIST_BOX (gtk_builder_get_object (self->builder, WID_TOOLBAR_ITEMS_LIST));
  application = GTK_APPLICATION (g_application_get_default ());
  model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "toolbar"));
  hidden_items = MOUSEPAD_SETTING_GET_STRV (TOOLBAR_HIDDEN_ITEMS);

  /* Add pseudo-items for Menu Button and App Icon */
  {
    gchar **right_items = MOUSEPAD_SETTING_GET_STRV (TOOLBAR_RIGHT_ITEMS);

    /* App Icon (not alignable - position is controlled by window decoration layout) */
    mousepad_prefs_dialog_toolbar_add_pseudo_item (self, listbox, _("Icon"), "mousepad.app-icon", FALSE,
                                                   (const gchar *const *) hidden_items, (const gchar *const *) right_items);

    /* Menu Button (alignable) */
    mousepad_prefs_dialog_toolbar_add_pseudo_item (self, listbox, _("Menu"), "mousepad.menu-button", TRUE,
                                                   (const gchar *const *) hidden_items, (const gchar *const *) right_items);

    g_strfreev (right_items);
  }

  /* iterate toolbar model */
  for (m = 0; m < g_menu_model_get_n_items (model); m++)
    {
      if ((section = g_menu_model_get_item_link (model, m, G_MENU_LINK_SECTION))
          && (n_items = g_menu_model_get_n_items (section)))
        {
          for (n = 0; n < n_items; n++)
            mousepad_prefs_dialog_toolbar_add_item (self, listbox, section, n, (const gchar *const *) hidden_items);
        }
      else
        {
          mousepad_prefs_dialog_toolbar_add_item (self, listbox, model, m, (const gchar *const *) hidden_items);
        }
    }

  g_strfreev (hidden_items);
}



#define mousepad_builder_get_widget(builder, name) \
  GTK_WIDGET (gtk_builder_get_object (builder, name))



static void
mousepad_prefs_dialog_init (MousepadPrefsDialog *self)
{
  MousepadApplication *application;
  GtkWidget *widget, *child;
  GError *error = NULL;

  self->builder = gtk_builder_new ();

  /* load the gtkbuilder xml that is compiled into the binary, or else die */
  if (!gtk_builder_add_from_resource (self->builder,
                                      "/org/xfce/mousepad/ui/mousepad-prefs-dialog.ui",
                                      &error))
    {
      g_error ("Failed to load the internal preferences dialog: %s", error->message);
      /* not reached */
      g_error_free (error);
    }

  /* make the application actions usable in the prefs dialog */
  application = MOUSEPAD_APPLICATION (g_application_get_default ());
  gtk_widget_insert_action_group (GTK_WIDGET (self), "app", G_ACTION_GROUP (application));

  /* add the Glade/GtkBuilder notebook into this dialog */
  widget = gtk_dialog_get_content_area (GTK_DIALOG (self));
  child = mousepad_builder_get_widget (self->builder, WID_NOTEBOOK);
  gtk_box_pack_start (GTK_BOX (widget), child, FALSE, TRUE, 0);
  gtk_widget_show (child);

  /* setup the window properties */
  gtk_window_set_title (GTK_WINDOW (self), _("Mousepad Preferences"));
  gtk_window_set_icon_name (GTK_WINDOW (self), "preferences-desktop");

  /* setup color scheme combo box */
  mousepad_prefs_dialog_setup_color_schemes_combo (self);

  /* setup tab mode combo box */
  widget = mousepad_builder_get_widget (self->builder, WID_TAB_MODE_COMBO);
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), MOUSEPAD_SETTING_GET_BOOLEAN (INSERT_SPACES));
  g_signal_connect_swapped (widget, "changed",
                            G_CALLBACK (mousepad_prefs_dialog_tab_mode_changed), self);
  MOUSEPAD_SETTING_CONNECT_OBJECT (INSERT_SPACES, mousepad_prefs_dialog_tab_mode_setting_changed,
                                   self, G_CONNECT_SWAPPED);

  /* setup encoding combo box */
  mousepad_prefs_dialog_setup_encoding_combo (self);

  /* bind the font button "font" property to the "font" setting */
  MOUSEPAD_SETTING_BIND (FONT, gtk_builder_get_object (self->builder, WID_FONT_BUTTON),
                         "font", G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

  /* bind the recent menu spin button to the setting */
  MOUSEPAD_SETTING_BIND (RECENT_MENU_ITEMS, gtk_builder_get_object (self->builder, WID_RECENT_SPIN),
                         "value", G_SETTINGS_BIND_GET);
  g_signal_connect_swapped (gtk_builder_get_object (self->builder, WID_RECENT_SPIN),
                            "value-changed",
                            G_CALLBACK (mousepad_prefs_dialog_recent_spin_changed), self);

  /* bind the search spin button to the setting */
  MOUSEPAD_SETTING_BIND (SEARCH_HISTORY_SIZE, gtk_builder_get_object (self->builder, WID_SEARCH_SPIN),
                         "value", G_SETTINGS_BIND_DEFAULT);

  /* bind simple spin buttons to the settings */
  MOUSEPAD_SETTING_BIND (RIGHT_MARGIN_POSITION,
                         gtk_builder_get_object (self->builder, WID_RIGHT_MARGIN_SPIN),
                         "value", G_SETTINGS_BIND_DEFAULT);
  MOUSEPAD_SETTING_BIND (TAB_WIDTH, gtk_builder_get_object (self->builder, WID_TAB_WIDTH_SPIN),
                         "value", G_SETTINGS_BIND_DEFAULT);

  /* bind simple combo boxes to the settings */
  MOUSEPAD_SETTING_BIND (SMART_HOME_END,
                         gtk_builder_get_object (self->builder, WID_SMART_HOME_END_COMBO),
                         "active-id", G_SETTINGS_BIND_DEFAULT);
  MOUSEPAD_SETTING_BIND (TOOLBAR_STYLE,
                         gtk_builder_get_object (self->builder, WID_TOOLBAR_STYLE_COMBO),
                         "active-id", G_SETTINGS_BIND_DEFAULT);
  MOUSEPAD_SETTING_BIND (TOOLBAR_ICON_SIZE,
                         gtk_builder_get_object (self->builder, WID_TOOLBAR_ICON_SIZE_COMBO),
                         "active-id", G_SETTINGS_BIND_DEFAULT);

  mousepad_prefs_dialog_setup_toolbar_items (self);
  MOUSEPAD_SETTING_BIND (OPENING_MODE,
                         gtk_builder_get_object (self->builder, WID_OPENING_MODE_COMBO),
                         "active-id", G_SETTINGS_BIND_DEFAULT);
  MOUSEPAD_SETTING_BIND (SESSION_RESTORE,
                         gtk_builder_get_object (self->builder, WID_SESSION_COMBO),
                         "active-id", G_SETTINGS_BIND_DEFAULT);

  /* show the "Plugins" tab only if there is at least one plugin and fill it on demand,
   * to not slow down the dialog opening */
  if (mousepad_application_get_providers (application) != NULL)
    {
      widget = mousepad_builder_get_widget (self->builder, WID_NOTEBOOK);
      child = mousepad_builder_get_widget (self->builder, WID_PLUGINS_TAB);
      g_signal_connect (widget, "switch-page",
                        G_CALLBACK (mousepad_prefs_dialog_plugins_tab), child);
      gtk_widget_show (child);
    }
}



GtkWidget *
mousepad_prefs_dialog_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_PREFS_DIALOG, NULL);
}
