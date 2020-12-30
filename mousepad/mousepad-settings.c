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
#include <mousepad/mousepad-settings-store.h>



static MousepadSettingsStore *settings_store = NULL;



void
mousepad_settings_finalize (void)
{
  g_settings_sync ();

  if (MOUSEPAD_IS_SETTINGS_STORE (settings_store))
    {
      g_object_unref (settings_store);
      settings_store = NULL;
    }
}



void
mousepad_settings_init (void)
{
  if (! MOUSEPAD_IS_SETTINGS_STORE (settings_store))
    settings_store = mousepad_settings_store_new ();
}



gboolean
mousepad_setting_bind (const gchar        *setting,
                       gpointer            object,
                       const gchar        *prop,
                       GSettingsBindFlags  flags)
{
  gboolean     result = FALSE;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (setting != NULL, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (prop != NULL, FALSE);

  if (mousepad_settings_store_lookup (settings_store, setting, &key_name, &settings))
    {
      g_settings_bind (settings, key_name, object, prop, flags);
      return TRUE;
    }

  return result;
}



gulong
mousepad_setting_connect (const gchar   *setting,
                          GCallback      callback,
                          gpointer       user_data,
                          GConnectFlags  connect_flags)
{
  gulong       signal_id = 0;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (setting != NULL, 0);
  g_return_val_if_fail (callback != NULL, 0);

  if (mousepad_settings_store_lookup (settings_store, setting, &key_name, &settings))
    {
      gchar *signal_name;

      signal_name = g_strdup_printf ("changed::%s", key_name);

      signal_id = g_signal_connect_data (settings,
                                         signal_name,
                                         callback,
                                         user_data,
                                         NULL,
                                         connect_flags);

      g_free (signal_name);
    }

  return signal_id;
}



gulong
mousepad_setting_connect_object (const gchar   *setting,
                                 GCallback      callback,
                                 gpointer       gobject,
                                 GConnectFlags  connect_flags)
{
  gulong       signal_id = 0;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (setting != NULL, 0);
  g_return_val_if_fail (callback != NULL, 0);
  g_return_val_if_fail (G_IS_OBJECT (gobject), 0);

  if (mousepad_settings_store_lookup (settings_store, setting, &key_name, &settings))
    {
      gchar *signal_name;

      signal_name = g_strdup_printf ("changed::%s", key_name);

      signal_id = g_signal_connect_object (settings,
                                           signal_name,
                                           callback,
                                           gobject,
                                           connect_flags);

      g_free (signal_name);
    }

  return signal_id;
}



void
mousepad_setting_disconnect (const gchar *setting,
                             GCallback    callback,
                             gpointer     user_data)
{
  GSettings *settings;

  g_return_if_fail (setting != NULL);
  g_return_if_fail (callback != NULL);

  settings = mousepad_settings_store_lookup_settings (settings_store, setting);

  if (G_IS_SETTINGS (settings))
    mousepad_disconnect_by_func (settings, callback, user_data);
  else
    g_warn_if_reached ();
}



gboolean
mousepad_setting_get (const gchar *setting,
                      const gchar *format_string,
                      ...)
{
  gboolean     result = FALSE;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (setting != NULL, FALSE);
  g_return_val_if_fail (format_string != NULL, FALSE);

  if (mousepad_settings_store_lookup (settings_store, setting, &key_name, &settings))
    {
      GVariant *variant;
      va_list   ap;

      variant = g_settings_get_value (settings, key_name);

      va_start (ap, format_string);
      g_variant_get_va (variant, format_string, NULL, &ap);
      va_end (ap);

      g_variant_unref (variant);

      result = TRUE;
    }

  return result;
}



gboolean
mousepad_setting_set (const gchar *setting,
                      const gchar *format_string,
                      ...)
{
  gboolean     result = FALSE;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (setting != NULL, FALSE);
  g_return_val_if_fail (format_string != NULL, FALSE);

  if (mousepad_settings_store_lookup (settings_store, setting, &key_name, &settings))
    {
      GVariant *variant;
      va_list   ap;

      va_start (ap, format_string);
      variant = g_variant_new_va (format_string, NULL, &ap);
      va_end (ap);

      g_variant_ref_sink (variant);

      g_settings_set_value (settings, key_name, variant);

      g_variant_unref (variant);

      result = TRUE;
    }


  return result;
}



gboolean
mousepad_setting_get_boolean (const gchar *setting)
{
  gboolean value = FALSE;
  gboolean result = mousepad_setting_get (setting, "b", &value);
  g_warn_if_fail (result);
  return value;
}



void
mousepad_setting_set_boolean (const gchar *setting,
                              gboolean     value)
{
  mousepad_setting_set (setting, "b", value);
}



gint
mousepad_setting_get_int (const gchar *setting)
{
  gint     value = 0;
  gboolean result = mousepad_setting_get (setting, "i", &value);
  g_warn_if_fail (result);
  return value;
}



void
mousepad_setting_set_int (const gchar *setting,
                          gint         value)
{
  mousepad_setting_set (setting, "i", value);
}



gchar *
mousepad_setting_get_string (const gchar *setting)
{
  gchar    *value = NULL;
  gboolean  result = mousepad_setting_get (setting, "s", &value);
  g_warn_if_fail (result);
  return value;
}



void
mousepad_setting_set_string (const gchar *setting,
                             const gchar *value)
{
  mousepad_setting_set (setting, "s", value != NULL ? value : "");
}



gint
mousepad_setting_get_enum (const gchar *setting)
{
  gint         result = 0;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (setting != NULL, FALSE);

  if (mousepad_settings_store_lookup (settings_store, setting, &key_name, &settings))
    result = g_settings_get_enum (settings, key_name);
  else
    g_warn_if_reached ();

  return result;
}



void
mousepad_setting_set_enum (const gchar *setting,
                           gint         value)
{
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_if_fail (setting != NULL);

  if (mousepad_settings_store_lookup (settings_store, setting, &key_name, &settings))
    g_settings_set_enum (settings, key_name, value);
  else
    g_warn_if_reached ();
}



GVariant *
mousepad_setting_get_variant (const gchar *setting)
{
  GVariant    *variant = NULL;
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_val_if_fail (setting != NULL, FALSE);

  if (mousepad_settings_store_lookup (settings_store, setting, &key_name, &settings))
    variant = g_settings_get_value (settings, key_name);
  else
    g_warn_if_reached ();

  return variant;
}



void
mousepad_setting_set_variant (const gchar *setting,
                              GVariant    *variant)
{
  const gchar *key_name = NULL;
  GSettings   *settings = NULL;

  g_return_if_fail (setting != NULL);

  if (mousepad_settings_store_lookup (settings_store, setting, &key_name, &settings))
    {
      g_variant_ref_sink (variant);
      g_settings_set_value (settings, key_name, variant);
      g_variant_unref (variant);
    }
  else
    g_warn_if_reached ();
}
