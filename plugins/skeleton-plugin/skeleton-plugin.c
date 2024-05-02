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

#include "mousepad/mousepad-private.h"
#include "mousepad/mousepad-application.h"

#include "skeleton-plugin/skeleton-plugin.h"



/* GObject virtual functions */
static void
skeleton_plugin_constructed (GObject *object);
static void
skeleton_plugin_finalize (GObject *object);

/* MousepadPlugin virtual functions */
static void
skeleton_plugin_enable (MousepadPlugin *mplugin);
static void
skeleton_plugin_disable (MousepadPlugin *mplugin);

/* SkeletonPlugin own functions */



struct _SkeletonPlugin
{
  MousepadPlugin __parent__;
};



G_DEFINE_DYNAMIC_TYPE (SkeletonPlugin, skeleton_plugin, MOUSEPAD_TYPE_PLUGIN);



void
skeleton_plugin_register (MousepadPluginProvider *plugin)
{
  skeleton_plugin_register_type (G_TYPE_MODULE (plugin));
}



static void
skeleton_plugin_class_init (SkeletonPluginClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  MousepadPluginClass *mplugin_class = MOUSEPAD_PLUGIN_CLASS (klass);

  gobject_class->constructed = skeleton_plugin_constructed;
  gobject_class->finalize = skeleton_plugin_finalize;

  mplugin_class->enable = skeleton_plugin_enable;
  mplugin_class->disable = skeleton_plugin_disable;
}



static void
skeleton_plugin_class_finalize (SkeletonPluginClass *klass)
{
}



static void
skeleton_plugin_init (SkeletonPlugin *plugin)
{
  /* initialization */

  skeleton_plugin_enable (MOUSEPAD_PLUGIN (plugin));
}



static void
skeleton_plugin_constructed (GObject *object)
{
  MousepadPluginProvider *provider;
  GtkWidget *vbox;

  /* request the creation of the plugin setting box */
  g_object_get (object, "provider", &provider, NULL);
  vbox = mousepad_plugin_provider_create_setting_box (provider);

  /* chain-up to MousepadPlugin */
  G_OBJECT_CLASS (skeleton_plugin_parent_class)->constructed (object);
}



static void
skeleton_plugin_finalize (GObject *object)
{
  SkeletonPlugin *plugin = SKELETON_PLUGIN (object);

  skeleton_plugin_disable (MOUSEPAD_PLUGIN (plugin));

  G_OBJECT_CLASS (skeleton_plugin_parent_class)->finalize (object);
}



static void
skeleton_plugin_enable (MousepadPlugin *mplugin)
{
  SkeletonPlugin *plugin = SKELETON_PLUGIN (mplugin);
  MousepadApplication *application;

  /* get the mousepad application */
  application = MOUSEPAD_APPLICATION (g_application_get_default ());
}



static void
skeleton_plugin_disable (MousepadPlugin *mplugin)
{
  SkeletonPlugin *plugin = SKELETON_PLUGIN (mplugin);
  MousepadApplication *application;

  /* get the mousepad application */
  application = MOUSEPAD_APPLICATION (g_application_get_default ());
}
