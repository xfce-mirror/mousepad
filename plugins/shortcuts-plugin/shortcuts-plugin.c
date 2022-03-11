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
#include <mousepad/mousepad-plugin.h>
#include <mousepad/mousepad-application.h>

#include <shortcuts-plugin/shortcuts-plugin.h>

/* prevent I18N macros redefinition */
#ifndef __G_I18N_LIB_H__
#define __G_I18N_LIB_H__
#endif
#ifdef I_
#undef I_
#endif
#include <libxfce4kbd-private-3/libxfce4kbd-private/xfce-shortcuts-editor.h>



/* GObject virtual functions */
static void shortcuts_plugin_finalize (GObject        *object);

/* MousepadPlugin virtual functions */
static void shortcuts_plugin_enable   (MousepadPlugin *mplugin);
static void shortcuts_plugin_disable  (MousepadPlugin *mplugin);

/* ShortcutsPlugin own functions */



struct _ShortcutsPluginClass
{
  MousepadPluginClass __parent__;
};

struct _ShortcutsPlugin
{
  MousepadPlugin __parent__;

  GList *menubar_entries, *menubar_sections;
};



G_DEFINE_DYNAMIC_TYPE (ShortcutsPlugin, shortcuts_plugin, MOUSEPAD_TYPE_PLUGIN);



void
shortcuts_plugin_register (MousepadPluginProvider *plugin)
{
  shortcuts_plugin_register_type (G_TYPE_MODULE (plugin));
}



static void
shortcuts_plugin_class_init (ShortcutsPluginClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  MousepadPluginClass *mplugin_class = MOUSEPAD_PLUGIN_CLASS (klass);

  gobject_class->finalize = shortcuts_plugin_finalize;

  mplugin_class->enable = shortcuts_plugin_enable;
  mplugin_class->disable = shortcuts_plugin_disable;
}



static void
shortcuts_plugin_class_finalize (ShortcutsPluginClass *klass)
{
}



static void
shortcuts_plugin_init (ShortcutsPlugin *plugin)
{
  /* initialization */
  plugin->menubar_entries = NULL;
  plugin->menubar_sections = NULL;

  shortcuts_plugin_enable (MOUSEPAD_PLUGIN (plugin));
}



static void
shortcuts_plugin_finalize (GObject *object)
{
  ShortcutsPlugin *plugin = SHORTCUTS_PLUGIN (object);

  shortcuts_plugin_disable (MOUSEPAD_PLUGIN (plugin));

  G_OBJECT_CLASS (shortcuts_plugin_parent_class)->finalize (object);
}



static void
shortcuts_plugin_disable_scrolled_window (GtkWidget *widget)
{
  if (GTK_IS_SCROLLED_WINDOW (widget))
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
                                    GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  else if (GTK_IS_CONTAINER (widget))
    {
      GList *list;

      list = gtk_container_get_children (GTK_CONTAINER (widget));
      shortcuts_plugin_disable_scrolled_window (list->data);
      g_list_free (list);
    }
}



static void
shortcuts_plugin_pack_frame (GList       *sections,
                             GtkWidget   *sbox,
                             const gchar *title)
{
  GtkWidget *frame, *fbox, *label;
  gchar     *str;

  /* prevent markup syntax mistakes in translations for the label */
  str = g_strdup_printf ("<b>%s</b>", title);
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);

  /* pack the frame */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_box_pack_start (GTK_BOX (sbox), frame, TRUE, TRUE, 0);
  fbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (frame), fbox);

  /* pack shortcut sections by disabling their scrolled window */
  for (GList *lp = sections; lp != NULL; lp = lp->next)
    {
      shortcuts_plugin_disable_scrolled_window (lp->data);
      gtk_box_pack_start (GTK_BOX (fbox), lp->data, TRUE, TRUE, 0);
      if (lp->next == NULL)
        gtk_widget_set_margin_bottom (lp->data, 6);
    }
}



static void
shortcuts_plugin_setting_box_packed (GObject *object)
{
  GtkWidget *parent;

  g_object_get (object, "parent", &parent, NULL);
  if (! GTK_IS_POPOVER (parent))
    return;

  gtk_widget_set_hexpand (parent, TRUE);
  gtk_widget_set_vexpand (parent, TRUE);
}



static void
shortcuts_plugin_set_setting_box (ShortcutsPlugin *plugin)
{
  MousepadPluginProvider *provider;
  GtkWidget              *vbox, *swin, *sbox;

  /* request the creation of the plugin setting box */
  g_object_get (plugin, "provider", &provider, NULL);
  vbox = mousepad_plugin_provider_create_setting_box (provider);

  /* we need to make the parent popover expand in that case */
  g_signal_connect (vbox, "notify::parent",
                    G_CALLBACK (shortcuts_plugin_setting_box_packed), NULL);

  /* pack a scrolled window with a reasonable minimum size, which is necessary in particular
   * for the popover to expand properly */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (swin), 400);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (swin), 600);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);
  sbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add (GTK_CONTAINER (swin), sbox);

  /* pack frames */
  shortcuts_plugin_pack_frame (plugin->menubar_sections, sbox, _("Menubar"));

  /* show all widgets in the setting box */
  gtk_widget_show_all (vbox);
}



static void
shortcuts_plugin_fake_callback (void)
{
}



static void
shortcuts_plugin_get_menu_entries (GtkAccelMap        *map,
                                   GMenuModel         *menu,
                                   XfceGtkActionEntry *entries,
                                   guint              *size)
{
  GMenuModel *submenu;
  GVariant   *action, *target, *label;
  gchar      *path, *temp, *target_str;
  guint       n_items;

  n_items = g_menu_model_get_n_items (menu);
  for (guint n = 0; n < n_items; n++)
    {
      /* section or submenu GMenuItem: go ahead recursively */
      if ((submenu = g_menu_model_get_item_link (menu, n, G_MENU_LINK_SECTION)) != NULL
          || (submenu = g_menu_model_get_item_link (menu, n, G_MENU_LINK_SUBMENU)) != NULL)
        shortcuts_plugin_get_menu_entries (map, submenu, entries, size);
      /* real GMenuItem */
      else if ((action = g_menu_model_get_item_attribute_value (menu, n, "action",
                                                                G_VARIANT_TYPE_STRING)) != NULL)
        {
          /* build path from action name and target */
          path = g_strconcat ("<Actions>/", g_variant_get_string (action, NULL), NULL);
          g_variant_unref (action);
          target = g_menu_model_get_item_attribute_value (menu, n, "target", NULL);
          if (target != NULL)
            {
              target_str = g_variant_print (target, TRUE);
              temp = path;
              path = g_strdup_printf ("%s(%s)", path, target_str);
              g_free (temp);
              g_free (target_str);
              g_variant_unref (target);
            }

          if (! gtk_accel_map_lookup_entry (path, NULL))
            {
              g_free (path);
              continue;
            }

          mousepad_object_set_data (map, g_intern_string (path), GINT_TO_POINTER (TRUE));
          label = g_menu_model_get_item_attribute_value (menu, n, "label", G_VARIANT_TYPE_STRING);
          entries[*size].menu_item_label_text = g_strdup (g_variant_get_string (label, NULL));
          entries[*size].accel_path = path;
          entries[*size].default_accelerator = g_strdup (mousepad_object_get_data (map, path + 10));
          entries[*size].callback = shortcuts_plugin_fake_callback;

          g_variant_unref (label);
          (*size)++;
        }
    }
}



static void
shortcuts_plugin_count_accels (gpointer data,
                               const gchar *accel_path,
                               guint accel_key,
                               GdkModifierType accel_mods,
                               gboolean changed)
{
  (*((guint *) data))++;
}



static void
shortcuts_plugin_accel_map_ready (ShortcutsPlugin *plugin)
{
  MousepadPluginProvider *provider;
  XfceGtkActionEntry     *entries;
  GtkAccelMap            *map;
  GtkApplication         *application;
  GtkWidget              *section;
  GMenuModel             *menubar, *menu;
  GVariant               *label;
  GStrv                   strv;
  gchar                  *escaped;
  guint                   n_accels = 0, size, n, n_sections;

  /* get the mousepad application */
  application = GTK_APPLICATION (g_application_get_default ());

  /* disconnect this handler */
  mousepad_disconnect_by_func (application, shortcuts_plugin_accel_map_ready, plugin);

  /* get the accel map size */
  map = gtk_accel_map_get ();
  gtk_accel_map_foreach (&n_accels, shortcuts_plugin_count_accels);

  /* walk the menubar */
  menubar = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "menubar"));
  n_sections = g_menu_model_get_n_items (menubar);
  for (n = 0; n < n_sections; n++)
    {
      /* allocate a sufficiently large XfceGtkActionEntry array */
      entries = g_new0 (XfceGtkActionEntry, n_accels);

      /* get action entries for this menu */
      menu = g_menu_model_get_item_link (menubar, n, G_MENU_LINK_SUBMENU);
      size = 0;
      shortcuts_plugin_get_menu_entries (map, menu, entries, &size);
      label = g_menu_model_get_item_attribute_value (menubar, n, "label", G_VARIANT_TYPE_STRING);
      strv = g_strsplit (g_variant_get_string (label, NULL), "_", 0);
      escaped = g_strjoinv (NULL, strv);

      /* get shortcuts editor section for this menu */
      section = xfce_shortcuts_editor_new (4, escaped, entries, size);
      plugin->menubar_sections = g_list_prepend (plugin->menubar_sections, section);

      /* resize and prepend the XfceGtkActionEntry array to the list */
      entries = g_renew (XfceGtkActionEntry, entries, size + 1);
      plugin->menubar_entries = g_list_prepend (plugin->menubar_entries, entries);

      /* cleanup */
      g_variant_unref (label);
      g_object_unref (menu);
      g_strfreev (strv);
      g_free (escaped);
    }

  plugin->menubar_sections = g_list_reverse (plugin->menubar_sections);

  /* we can't always do that in constructed() since the above might not have been done yet */
  g_object_get (plugin, "provider", &provider, NULL);
  if (provider == NULL)
    g_signal_connect (plugin, "notify::provider",
                      G_CALLBACK (shortcuts_plugin_set_setting_box), NULL);
  else
    shortcuts_plugin_set_setting_box (plugin);
}



static void
shortcuts_plugin_enable (MousepadPlugin *mplugin)
{
  ShortcutsPlugin *plugin = SHORTCUTS_PLUGIN (mplugin);
  GtkApplication  *application;

  /* get the mousepad application */
  application = GTK_APPLICATION (g_application_get_default ());

  /* the accel map is completed when the first window is set so we have to connect after */
  if (gtk_application_get_windows (application) == NULL)
    g_signal_connect_object (application, "notify::active-window",
                             G_CALLBACK (shortcuts_plugin_accel_map_ready), plugin,
                             G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  else
    shortcuts_plugin_accel_map_ready (plugin);
}



static void
shortcuts_plugin_free_entry_array (gpointer data)
{
  for (XfceGtkActionEntry *p = data; p->menu_item_label_text != NULL; p++)
    {
      g_free (p->menu_item_label_text);
      g_free ((gchar *) p->accel_path);
      g_free ((gchar *) p->default_accelerator);
    }

  g_free (data);
}



static void
shortcuts_plugin_disable (MousepadPlugin *mplugin)
{
  ShortcutsPlugin *plugin = SHORTCUTS_PLUGIN (mplugin);

  g_list_free_full (plugin->menubar_entries, shortcuts_plugin_free_entry_array);
  g_list_free_full (plugin->menubar_sections, (GDestroyNotify) gtk_widget_destroy);
}
