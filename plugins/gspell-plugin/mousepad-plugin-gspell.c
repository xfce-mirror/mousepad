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

#include <gspell-plugin/gspell-plugin.h>

#include <gmodule.h>


G_MODULE_EXPORT void         mousepad_plugin_initialize     (MousepadPluginProvider  *provider);
G_MODULE_EXPORT gboolean     mousepad_plugin_is_destroyable (void);
G_MODULE_EXPORT const gchar *mousepad_plugin_get_label      (void);
G_MODULE_EXPORT const gchar *mousepad_plugin_get_category   (void);
G_MODULE_EXPORT void         mousepad_plugin_get_types      (const GType            **types,
                                                             gint                    *n_types);



static GType type_list[1];



void
mousepad_plugin_initialize (MousepadPluginProvider *provider)
{
  /* register the types provided by this plugin */
  gspell_plugin_register (provider);

  /* set up the plugin type list */
  type_list[0] = TYPE_GSPELL_PLUGIN;
}



gboolean
mousepad_plugin_is_destroyable (void)
{
  return FALSE;
}



const gchar *
mousepad_plugin_get_label (void)
{
  return _("Spell Checking");
}



const gchar *
mousepad_plugin_get_category (void)
{
  return _("Editor");
}



void
mousepad_plugin_get_types (const GType **types,
                           gint         *n_types)
{
  *types = type_list;
  *n_types = G_N_ELEMENTS (type_list);
}
