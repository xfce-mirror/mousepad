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

#ifndef __SHORTCUTS_PLUGIN_H__
#define __SHORTCUTS_PLUGIN_H__

#include <mousepad/mousepad-plugin-provider.h>

G_BEGIN_DECLS

typedef struct _ShortcutsPluginClass  ShortcutsPluginClass;
typedef struct _ShortcutsPlugin       ShortcutsPlugin;

#define TYPE_SHORTCUTS_PLUGIN            (shortcuts_plugin_get_type ())
#define SHORTCUTS_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SHORTCUTS_PLUGIN, ShortcutsPlugin))
#define SHORTCUTS_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SHORTCUTS_PLUGIN, ShortcutsPluginClass))
#define IS_SHORTCUTS_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SHORTCUTS_PLUGIN))
#define IS_SHORTCUTS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_SHORTCUTS_PLUGIN)
#define SHORTCUTS_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_SHORTCUTS_PLUGIN, ShortcutsPluginClass))

GType shortcuts_plugin_get_type (void) G_GNUC_CONST;

void  shortcuts_plugin_register (MousepadPluginProvider *plugin);

G_END_DECLS

#endif /* !__SHORTCUTS_PLUGIN_H__ */
