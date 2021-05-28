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
#include <mousepad/mousepad-prefs-dialog.h>
#include <mousepad/mousepad-file.h>
#include <mousepad/mousepad-encoding-dialog.h>

#include <test-plugin/test-plugin.h>



/* GObject virtual functions */
static void test_plugin_finalize (GObject        *object);

/* MousepadPlugin virtual functions */
static void test_plugin_enable   (MousepadPlugin *mplugin);
static void test_plugin_disable  (MousepadPlugin *mplugin);



struct _TestPluginClass
{
  MousepadPluginClass __parent__;
};

struct _TestPlugin
{
  MousepadPlugin __parent__;

  /* dialog types for action names */
  GHashTable *dialog_types;
};



G_DEFINE_DYNAMIC_TYPE (TestPlugin, test_plugin, MOUSEPAD_TYPE_PLUGIN);



void
test_plugin_register (MousepadPluginProvider *plugin)
{
  test_plugin_register_type (G_TYPE_MODULE (plugin));
}



static void
test_plugin_class_init (TestPluginClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  MousepadPluginClass *mplugin_class = MOUSEPAD_PLUGIN_CLASS (klass);

  gobject_class->finalize = test_plugin_finalize;

  mplugin_class->enable = test_plugin_enable;
  mplugin_class->disable = test_plugin_disable;
}



static void
test_plugin_class_finalize (TestPluginClass *klass)
{
}



static void
test_plugin_init (TestPlugin *plugin)
{
  plugin->dialog_types = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (plugin->dialog_types, "preferences",
                       GSIZE_TO_POINTER (MOUSEPAD_TYPE_PREFS_DIALOG));

  test_plugin_enable (MOUSEPAD_PLUGIN (plugin));
}



static void
test_plugin_finalize (GObject *object)
{
  TestPlugin *plugin = TEST_PLUGIN (object);

  test_plugin_disable (MOUSEPAD_PLUGIN (plugin));

  g_hash_table_destroy (plugin->dialog_types);

  G_OBJECT_CLASS (test_plugin_parent_class)->finalize (object);
}



static gboolean
test_plugin_dialog_shown (gpointer data)
{
  gpointer  dialog;
  GType     dialog_type;
  GList    *windows, *window;

  windows = gdk_screen_get_toplevel_windows (gdk_screen_get_default ());
  for (window = windows; window != NULL; window = window->next)
    {
      gdk_window_get_user_data (window->data, &dialog);
      dialog_type = GPOINTER_TO_SIZE (mousepad_object_get_data (data, "dialog-type"));
      if (G_TYPE_CHECK_INSTANCE_TYPE (dialog, dialog_type) && gtk_widget_get_visible (dialog))
        {
          g_application_unmark_busy (g_application_get_default ());
          break;
        }
    }

  g_list_free (windows);

  return FALSE;
}



static void
test_plugin_action_added (GActionGroup *action_group,
                          const gchar  *action_name,
                          TestPlugin   *plugin)
{
  gpointer  dialog_type;
  GAction  *action;

  if ((dialog_type = g_hash_table_lookup (plugin->dialog_types, action_name)) != NULL)
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group), action_name);
      mousepad_object_set_data (action, "dialog-type", dialog_type);
      g_signal_connect (action, "activate", G_CALLBACK (test_plugin_dialog_shown), plugin);
    }
}



static void
test_plugin_dialog_shown_timeout (gpointer instance)
{
  g_timeout_add_seconds (2, test_plugin_dialog_shown, instance);
}



static void
test_plugin_window_shown (TestPlugin *plugin)
{
  GApplication *application;
  GtkWindow    *window;

  /* get the mousepad application and the active window */
  application = g_application_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (application));

  if (window != NULL && gtk_widget_get_visible (GTK_WIDGET (window)))
    {
      g_application_unmark_busy (application);

      /* `mousepad --disable-server` case: the test script can't run `mousepad --quit`
       * to terminate the current instance, so let's do it internally */
      if (g_application_get_flags (application) & G_APPLICATION_NON_UNIQUE)
        g_action_group_activate_action (G_ACTION_GROUP (application), "quit", NULL);
    }
}



static void
test_plugin_enable (MousepadPlugin *mplugin)
{
  GApplication *application;

  /* get the mousepad application */
  application = g_application_get_default ();

  /* mark application as busy by default */
  g_application_mark_busy (application);

  /* connect to all required signals to handle application state */
  g_signal_connect_object (application, "activate", G_CALLBACK (test_plugin_window_shown), mplugin,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_object (application, "open", G_CALLBACK (test_plugin_window_shown), mplugin,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect (application, "action-added", G_CALLBACK (test_plugin_action_added), mplugin);

  /* `mousepad --encoding -- file`: a special case where the dialog is not opened by an action */
  mousepad_object_set_data (application, "dialog-type",
                            GSIZE_TO_POINTER (MOUSEPAD_TYPE_ENCODING_DIALOG));
  g_signal_connect (application, "open", G_CALLBACK (test_plugin_dialog_shown_timeout), mplugin);
}



static void
test_plugin_disable (MousepadPlugin *mplugin)
{
  GApplication *application;
  GAction      *action;

  /* get the mousepad application */
  application = g_application_get_default ();

  /* disconnect signal handlers */
  g_signal_handlers_disconnect_by_data (application, mplugin);

  action = g_action_map_lookup_action (G_ACTION_MAP (application), "preferences");
  g_signal_handlers_disconnect_by_data (action, mplugin);
}
