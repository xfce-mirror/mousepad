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
#include <mousepad/mousepad-plugin-provider.h>

#include <gmodule.h>



/* GObject virtual functions */
static void     mousepad_plugin_provider_finalize (GObject     *object);

/* GTypeModule virtual functions */
static gboolean mousepad_plugin_provider_load     (GTypeModule *type_module);
static void     mousepad_plugin_provider_unload   (GTypeModule *type_module);



struct _MousepadPluginProviderClass
{
  GTypeModuleClass __parent__;
};

struct _MousepadPluginProvider
{
  GTypeModule __parent__;

  /* dynamically loaded module */
  GModule *module;

  /* list of instantiated types */
  GList *instances;

  /* GTypeModule use count */
  guint use_count;

  /* minimal set of pre-instantiation functions that each plugin must implement */
  void          (*initialize)     (MousepadPluginProvider  *provider);
  gboolean      (*is_destroyable) (void);
  const gchar * (*get_label)      (void);
  const gchar * (*get_category)   (void);
  void          (*get_types)      (const GType            **types,
                                   gint                    *n_types);
};



G_DEFINE_TYPE (MousepadPluginProvider, mousepad_plugin_provider, G_TYPE_TYPE_MODULE)



static void
mousepad_plugin_provider_class_init (MousepadPluginProviderClass *klass)
{
  GObjectClass     *gobject_class = G_OBJECT_CLASS (klass);
  GTypeModuleClass *gtype_module_class = G_TYPE_MODULE_CLASS (klass);

  gobject_class->finalize = mousepad_plugin_provider_finalize;

  gtype_module_class->load = mousepad_plugin_provider_load;
  gtype_module_class->unload = mousepad_plugin_provider_unload;
}



static void
mousepad_plugin_provider_init (MousepadPluginProvider *provider)
{
  provider->module = NULL;
  provider->instances = NULL;
  provider->use_count = 0;
  provider->initialize = NULL;
  provider->is_destroyable = NULL;
  provider->get_label = NULL;
  provider->get_category = NULL;
  provider->get_types = NULL;
}



static void
mousepad_plugin_provider_finalize (GObject *object)
{
  MousepadPluginProvider *provider = MOUSEPAD_PLUGIN_PROVIDER (object);

  if (provider->module != NULL)
    g_module_close (provider->module);

  G_OBJECT_CLASS (mousepad_plugin_provider_parent_class)->finalize (object);
}



static gboolean
mousepad_plugin_provider_load (GTypeModule *type_module)
{
  MousepadPluginProvider *provider = MOUSEPAD_PLUGIN_PROVIDER (type_module);
  gchar                  *path;

  /* load the module */
  path = g_module_build_path (MOUSEPAD_PLUGIN_DIRECTORY, type_module->name);
  provider->module = g_module_open (path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  g_free (path);

  /* check if the load operation was successful */
  if (G_LIKELY (provider->module != NULL))
    {
      /* initialize the plugin if all required public symbols are present */
      if (g_module_symbol (provider->module, "mousepad_plugin_initialize",
                           (gpointer *) &(provider->initialize))
          && g_module_symbol (provider->module, "mousepad_plugin_is_destroyable",
                              (gpointer *) &(provider->is_destroyable))
          && g_module_symbol (provider->module, "mousepad_plugin_get_label",
                              (gpointer *) &(provider->get_label))
          && g_module_symbol (provider->module, "mousepad_plugin_get_category",
                              (gpointer *) &(provider->get_category))
          && g_module_symbol (provider->module, "mousepad_plugin_get_types",
                              (gpointer *) &(provider->get_types)))
        {
          provider->initialize (provider);
          provider->use_count++;

          return TRUE;
        }
      else
        {
          g_warning ("Plugin \"%s\" lacks required symbols", type_module->name);
          g_type_module_unuse (G_TYPE_MODULE (provider));

          return FALSE;
        }
    }
  else
    {
      g_message ("Failed to load plugin \"%s\": %s", type_module->name, g_module_error ());
      return FALSE;
    }
}



static void
mousepad_plugin_provider_unload (GTypeModule *type_module)
{
  MousepadPluginProvider *provider = MOUSEPAD_PLUGIN_PROVIDER (type_module);

  /* destroy the plugin */
  mousepad_plugin_provider_destroy_plugin (provider);

  /* unload the module */
  g_module_close (provider->module);
  provider->module = NULL;

  /* reset provider state */
  provider->initialize = NULL;
  provider->is_destroyable = NULL;
  provider->get_label = NULL;
  provider->get_category = NULL;
  provider->get_types = NULL;
}



MousepadPluginProvider *
mousepad_plugin_provider_new (const gchar *name)
{
  MousepadPluginProvider *provider;

  provider = g_object_new (MOUSEPAD_TYPE_PLUGIN_PROVIDER, NULL);
  g_type_module_set_name (G_TYPE_MODULE (provider), name);

  return provider;
}



gboolean
mousepad_plugin_provider_is_destroyable (MousepadPluginProvider *provider)
{
  return provider->is_destroyable ();
}



gboolean
mousepad_plugin_provider_is_instantiated (MousepadPluginProvider *provider)
{
  return provider->instances != NULL;
}



const gchar *
mousepad_plugin_provider_get_label (MousepadPluginProvider *provider)
{
  return provider->get_label ();
}



const gchar *
mousepad_plugin_provider_get_category (MousepadPluginProvider *provider)
{
  return provider->get_category ();
}



void
mousepad_plugin_provider_new_plugin (MousepadPluginProvider *provider)
{
  const GType *types;
  gint         n, n_types;

  /* if ever there is an interest in having multi-instance plugins, this could be changed */
  if (provider->instances != NULL)
    {
      g_warning ("plugin '%s' is already instantiated", G_TYPE_MODULE (provider)->name);
      return;
    }

  /* get plugin types */
  provider->get_types (&types, &n_types);

  /* instanciate all types */
  for (n = 0; n < n_types; n++)
    provider->instances = g_list_prepend (provider->instances, g_object_new (types[n], NULL));

  /* increase use count only at first intantiation */
  if (provider->use_count == 1)
    provider->use_count += n_types;
}



void
mousepad_plugin_provider_destroy_plugin (MousepadPluginProvider *provider)
{
  g_list_free_full (provider->instances, g_object_unref);
  provider->instances = NULL;
}



void
mousepad_plugin_provider_unuse (gpointer data)
{
  MousepadPluginProvider *provider = data;

  /* decrease use count until unload */
  while (provider->use_count-- > 0)
    g_type_module_unuse (G_TYPE_MODULE (provider));
}
