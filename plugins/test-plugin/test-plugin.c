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
#include <mousepad/mousepad-dialogs.h>

#include <test-plugin/test-plugin.h>



/* GObject virtual functions */
static void test_plugin_finalize (GObject        *object);

/* MousepadPlugin virtual functions */
static void test_plugin_enable   (MousepadPlugin *mplugin);
static void test_plugin_disable  (MousepadPlugin *mplugin);

#define LOG_COMMAND(command) g_printerr ("Command: %s: %s\n", G_STRLOC, command);
#define LOG_WARNING(warning) g_printerr ("%s: %s\n", G_STRLOC, warning);



struct _TestPluginClass
{
  MousepadPluginClass __parent__;
};

struct _TestPlugin
{
  MousepadPlugin __parent__;

  /* actions opening a blocking or non-blocking dialog */
  GHashTable *dialog_actions;
};

static GApplication* application;



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
  /* get the mousepad application */
  application = g_application_get_default ();

  /* setup the dialog actions hash table */
  plugin->dialog_actions = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (plugin->dialog_actions, "preferences", GINT_TO_POINTER (FALSE));

  test_plugin_enable (MOUSEPAD_PLUGIN (plugin));
}



static void
test_plugin_finalize (GObject *object)
{
  TestPlugin *plugin = TEST_PLUGIN (object);

  test_plugin_disable (MOUSEPAD_PLUGIN (plugin));

  g_hash_table_destroy (plugin->dialog_actions);

  G_OBJECT_CLASS (test_plugin_parent_class)->finalize (object);
}



static gboolean
test_plugin_dialog_shown (gpointer instance)
{
  gpointer  dialog;
  GList    *windows, *window;

  /* disconnect this handler */
  if (G_IS_ACTION (instance))
    mousepad_disconnect_by_func (instance, test_plugin_dialog_shown, NULL);

  /* search for a GtkDialog in the GdkWindow list */
  windows = gdk_screen_get_toplevel_windows (gdk_screen_get_default ());
  for (window = windows; window != NULL; window = window->next)
    {
      gdk_window_get_user_data (window->data, &dialog);
      if (GTK_IS_DIALOG (dialog) && gtk_widget_get_visible (dialog))
        {
          if (g_application_get_is_busy (application))
            g_application_unmark_busy (application);
          else
            gtk_dialog_response (dialog, MOUSEPAD_RESPONSE_CANCEL);

          break;
        }
    }

  g_list_free (windows);

  if (window == NULL)
    LOG_WARNING ("Dialog not found");

  return FALSE;
}



static void
test_plugin_dialog_shown_timeout (gpointer instance)
{
  g_timeout_add_seconds (2, test_plugin_dialog_shown, instance);
}



static void
test_plugin_is_busy_changed (GApplication *gapplication)
{
  if (! g_application_get_is_busy (gapplication))
    while (g_source_remove_by_user_data (gapplication));
}



static void
test_plugin_action_added (GActionGroup *action_group,
                          const gchar  *action_name,
                          TestPlugin   *plugin)
{
  GAction  *action;
  gpointer  blocking;

  if (g_hash_table_lookup_extended (plugin->dialog_actions, action_name, NULL, &blocking))
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (action_group), action_name);
      if (blocking)
        test_plugin_dialog_shown_timeout (action);
      else
        g_signal_connect (action, "activate", G_CALLBACK (test_plugin_dialog_shown), NULL);
    }
}



static void
test_plugin_window_shown (TestPlugin *plugin)
{
  GtkWindow *window;

  window = gtk_application_get_active_window (GTK_APPLICATION (application));
  if (window != NULL && gtk_widget_get_visible (GTK_WIDGET (window)))
    {
      g_application_unmark_busy (application);

      /* `mousepad --disable-server` case: the test script can't run `mousepad --quit`
       * to terminate the current instance, so let's do it internally */
      if (g_application_get_flags (application) & G_APPLICATION_NON_UNIQUE)
        {
          LOG_COMMAND ("app.quit");
          g_action_group_activate_action (G_ACTION_GROUP (application), "quit", NULL);
        }
    }
}



static void
test_plugin_enable (MousepadPlugin *mplugin)
{
  /* mark application as busy by default */
  g_application_mark_busy (application);

  /* connect to all required signals to handle application state */
  g_signal_connect_object (application, "activate", G_CALLBACK (test_plugin_window_shown), mplugin,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect_object (application, "open", G_CALLBACK (test_plugin_window_shown), mplugin,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  g_signal_connect (application, "action-added", G_CALLBACK (test_plugin_action_added), mplugin);

  /* `mousepad --encoding -- file`: a special case where the dialog is not opened by an action */
  g_signal_connect (application, "open", G_CALLBACK (test_plugin_dialog_shown_timeout), mplugin);
  g_signal_connect (application, "notify::is-busy",
                    G_CALLBACK (test_plugin_is_busy_changed), mplugin);
}



static void
test_plugin_disable (MousepadPlugin *mplugin)
{
  /* disconnect signal handlers */
  g_signal_handlers_disconnect_by_data (application, mplugin);
}
