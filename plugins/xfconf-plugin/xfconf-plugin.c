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
#include <mousepad/mousepad-settings.h>

#include <xfconf-plugin/xfconf-plugin.h>

#include <xfconf/xfconf.h>



/* GObject virtual functions */
static void xfconf_plugin_constructed (GObject        *object);
static void xfconf_plugin_finalize    (GObject        *object);

/* MousepadPlugin virtual functions */
static void xfconf_plugin_enable      (MousepadPlugin *mplugin);
static void xfconf_plugin_disable     (MousepadPlugin *mplugin);



struct _XfconfPluginClass
{
  MousepadPluginClass __parent__;
};

struct _XfconfPlugin
{
  MousepadPlugin __parent__;

  /* xfconf state */
  gboolean initialized;
};



G_DEFINE_DYNAMIC_TYPE (XfconfPlugin, xfconf_plugin, MOUSEPAD_TYPE_PLUGIN);



void
xfconf_plugin_register (MousepadPluginProvider *plugin)
{
  xfconf_plugin_register_type (G_TYPE_MODULE (plugin));
}



static void
xfconf_plugin_class_init (XfconfPluginClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  MousepadPluginClass *mplugin_class = MOUSEPAD_PLUGIN_CLASS (klass);

  gobject_class->constructed = xfconf_plugin_constructed;
  gobject_class->finalize = xfconf_plugin_finalize;

  mplugin_class->enable = xfconf_plugin_enable;
  mplugin_class->disable = xfconf_plugin_disable;
}



static void
xfconf_plugin_class_finalize (XfconfPluginClass *klass)
{
}



static void
xfconf_plugin_init (XfconfPlugin *plugin)
{
  plugin->initialized = FALSE;
}



static void
xfconf_plugin_constructed (GObject *object)
{
  xfconf_plugin_enable (MOUSEPAD_PLUGIN (object));

  /* chain-up to MousepadPlugin */
  G_OBJECT_CLASS (xfconf_plugin_parent_class)->constructed (object);
}



static void
xfconf_plugin_finalize (GObject *object)
{
  xfconf_plugin_disable (MOUSEPAD_PLUGIN (object));

  G_OBJECT_CLASS (xfconf_plugin_parent_class)->finalize (object);
}



static gboolean
xfconf_plugin_request_deactivation (gpointer data)
{
  XfconfPlugin *plugin = data;
  GApplication *application;
  GTypeModule  *provider;
  GAction      *action;

  if (IS_XFCONF_PLUGIN (plugin))
    {
      application = g_application_get_default ();
      g_object_get (plugin, "provider", &provider, NULL);
      action = g_action_map_lookup_action (G_ACTION_MAP (application), provider->name);
      g_signal_emit_by_name (action, "activate", NULL, application);
    }

  return FALSE;
}



static void
xfconf_plugin_set_property (XfconfChannel *channel,
                            const gchar   *property,
                            const gchar   *setting)
{
  GVariant            *variant;
  const GVariantType  *type;
  const gchar        **strv;

  variant = mousepad_setting_get_variant (setting);
  type = g_variant_get_type (variant);

  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    xfconf_channel_set_bool (channel, property, g_variant_get_boolean (variant));
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
    xfconf_channel_set_int (channel, property, g_variant_get_int32 (variant));
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
    xfconf_channel_set_uint (channel, property, g_variant_get_uint32 (variant));
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    xfconf_channel_set_string (channel, property, g_variant_get_string (variant, NULL));
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING_ARRAY))
    {
      strv = g_variant_get_strv (variant, NULL);
      xfconf_channel_set_string_list (channel, property, strv);
      g_free (strv);
    }

  g_variant_unref (variant);
}



static void
xfconf_plugin_set_schema (XfconfChannel         *channel,
                          const gchar           *schema_id,
                          GSettingsSchemaSource *source,
                          GSettings             *settings)
{
  GSettingsSchema  *schema;
  GSettings        *child_settings;
  gchar           **keys, **key, **children, **child;
  gchar            *property, *setting, *child_schema_id;
  const gchar      *path, *short_id;

  /* bind each key in schema with an xfconf property */
  schema = g_settings_schema_source_lookup (source, schema_id, TRUE);
  path = g_settings_schema_get_path (schema) + MOUSEPAD_ID_LEN + 1;
  short_id = schema_id + MOUSEPAD_ID_LEN + 1;
  keys = g_settings_schema_list_keys (schema);
  for (key = keys; *key != NULL; key++)
    {
      property = g_strconcat (path, *key, NULL);
      setting = g_strdup_printf ("%s.%s", short_id, *key);
      xfconf_plugin_set_property (channel, property, setting);
      g_free (property);
      g_free (setting);
    }

  /* go ahead recursively */
  children = g_settings_schema_list_children (schema);
  for (child = children; *child != NULL; child++)
    {
      child_settings = g_settings_get_child (settings, *child);
      child_schema_id = g_strdup_printf ("%s.%s", schema_id, *child);
      xfconf_plugin_set_schema (channel, child_schema_id, source, child_settings);
      g_object_unref (child_settings);
      g_free (child_schema_id);
    }

  /* cleanup */
  g_settings_schema_unref (schema);
  g_strfreev (keys);
  g_strfreev (children);
}



static void
xfconf_plugin_enable (MousepadPlugin *mplugin)
{
  XfconfPlugin    *plugin = XFCONF_PLUGIN (mplugin);
  GApplication    *application;
  GSettings       *settings;
  GError          *error = NULL;

  /* initialize xfconf */
  if (G_UNLIKELY (! (plugin->initialized = xfconf_init (&error))))
    {
      /* inform the user */
      g_warning ("Failed to initialize xfconf: %s", error->message);
      g_error_free (error);

      /* request plugin deactivation at the end of the current process */
      g_idle_add (xfconf_plugin_request_deactivation, plugin);

      return;
    }

  /* unbind application "default-font" property from GNOME settings if needed */
  application = g_application_get_default ();
  g_object_get (application, "default-font-object", &settings, NULL);
  if (settings != NULL)
    g_settings_unbind (application, "default-font");

  /* bind it to xfconf instead */
  xfconf_g_property_bind (xfconf_channel_get ("xsettings"), "/Gtk/MonospaceFontName",
                          G_TYPE_STRING, application, "default-font");

  /* bind mousepad settings with xfconf */
  xfconf_plugin_set_schema (xfconf_channel_get ("mousepad"), MOUSEPAD_ID,
                            g_settings_schema_source_get_default (),
                            mousepad_settings_get_root ());
}



static void
xfconf_plugin_disable (MousepadPlugin *mplugin)
{
  XfconfPlugin *plugin = XFCONF_PLUGIN (mplugin);
  GApplication *application;
  GSettings    *settings;

  if (G_UNLIKELY (! plugin->initialized))
    return;

  /* shutdown xfconf */
  xfconf_shutdown ();

  /* rebind application "default-font" property to GNOME settings if needed */
  application = g_application_get_default ();
  g_object_get (application, "default-font-object", &settings, NULL);
  if (settings != NULL)
    g_settings_bind (settings, "monospace-font-name", application, "default-font",
                     G_SETTINGS_BIND_GET);
}
