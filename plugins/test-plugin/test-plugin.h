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

#ifndef __TEST_PLUGIN_H__
#define __TEST_PLUGIN_H__

#include <mousepad/mousepad-plugin-provider.h>

G_BEGIN_DECLS

typedef struct _TestPluginClass  TestPluginClass;
typedef struct _TestPlugin       TestPlugin;

#define TYPE_TEST_PLUGIN            (test_plugin_get_type ())
#define TEST_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TEST_PLUGIN, TestPlugin))
#define TEST_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_TEST_PLUGIN, TestPluginClass))
#define IS_TEST_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TEST_PLUGIN))
#define IS_TEST_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_TEST_PLUGIN)
#define TEST_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_TEST_PLUGIN, TestPluginClass))

GType test_plugin_get_type (void) G_GNUC_CONST;

void  test_plugin_register (MousepadPluginProvider *plugin);

G_END_DECLS

#endif /* !__TEST_PLUGIN_H__ */
