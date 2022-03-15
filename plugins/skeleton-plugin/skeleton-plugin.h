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

#ifndef __SKELETON_PLUGIN_H__
#define __SKELETON_PLUGIN_H__

#include <mousepad/mousepad-plugin-provider.h>

G_BEGIN_DECLS

typedef struct _SkeletonPluginClass SkeletonPluginClass;
typedef struct _SkeletonPlugin      SkeletonPlugin;

#define TYPE_SKELETON_PLUGIN            (skeleton_plugin_get_type ())
#define SKELETON_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SKELETON_PLUGIN, SkeletonPlugin))
#define SKELETON_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SKELETON_PLUGIN, SkeletonPluginClass))
#define IS_SKELETON_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SKELETON_PLUGIN))
#define IS_SKELETON_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_SKELETON_PLUGIN)
#define SKELETON_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_SKELETON_PLUGIN, SkeletonPluginClass))

GType skeleton_plugin_get_type (void) G_GNUC_CONST;

void  skeleton_plugin_register (MousepadPluginProvider *plugin);

G_END_DECLS

#endif /* !__SKELETON_PLUGIN_H__ */
