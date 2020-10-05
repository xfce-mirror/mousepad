#include "helloworld-plugin.h"
#include <mousepad/mousepad-app-activatable.h>
#include <mousepad/mousepad-application.h>



struct _HelloWorldPlugin
{
  PeasExtensionBase    parent_instance;
  MousepadApplication *application;
  gchar               *recipient;
};



struct _HelloWorldPluginClass
{
  PeasExtensionBaseClass parent_class;
};


static void mousepad_app_activatable_iface_init (MousepadAppActivatableInterface *iface);


G_DEFINE_DYNAMIC_TYPE_EXTENDED (HelloWorldPlugin,
                                hello_world_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (MOUSEPAD_TYPE_APP_ACTIVATABLE,
                                                               mousepad_app_activatable_iface_init))



enum
{
  PROP_0,
  PROP_APPLICATION,
  PROP_RECIPIENT,
  N_PROPS,
};



static GParamSpec *hello_world_plugin_properties[N_PROPS] = { NULL };



static void
hello_world_plugin_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HelloWorldPlugin *plugin = HELLO_WORLD_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_APPLICATION:
      /* take an unowned reference to the application */
      plugin->application = MOUSEPAD_APPLICATION (g_value_get_object (value));
      break;
    case PROP_RECIPIENT:
      g_free (plugin->recipient);
      plugin->recipient = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
hello_world_plugin_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  HelloWorldPlugin *plugin = HELLO_WORLD_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_APPLICATION:
      g_value_set_object (value, plugin->application);
      break;
    case PROP_RECIPIENT:
      g_value_set_string (value, plugin->recipient);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
hello_world_plugin_init (HelloWorldPlugin *plugin)
{
  plugin->recipient = g_strdup ("World");
}



static void
hello_world_plugin_finalize (GObject *object)
{
  HelloWorldPlugin *plugin = HELLO_WORLD_PLUGIN (object);

  /* note: no need to unreference the ->application since it's unowned */

  /* cleanup the recipient string */
  g_free (plugin->recipient);

  G_OBJECT_CLASS (hello_world_plugin_parent_class)->finalize (object);
}



static void
hello_world_plugin_activate (MousepadAppActivatable *app_activatable)
{
  HelloWorldPlugin *plugin = HELLO_WORLD_PLUGIN (app_activatable);
  GtkWidget        *dialog;

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK,
                                   "Hello %s",
                                   plugin->recipient);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}



static void
hello_world_plugin_deactivate (MousepadAppActivatable *app_activatable)
{
  HelloWorldPlugin *plugin = HELLO_WORLD_PLUGIN (app_activatable);
  GtkWidget        *dialog;

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK,
                                   "Goodbye %s",
                                   plugin->recipient);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}



static void
hello_world_plugin_class_init (HelloWorldPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hello_world_plugin_set_property;
  object_class->get_property = hello_world_plugin_get_property;
  object_class->finalize = hello_world_plugin_finalize;

  hello_world_plugin_properties[PROP_APPLICATION] =
    g_param_spec_object ("application",
                         "Application",
                         "The Mousepad application instance",
                         MOUSEPAD_TYPE_APPLICATION,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  hello_world_plugin_properties[PROP_RECIPIENT] =
    g_param_spec_string ("recipient",
                         "Recipient",
                         "Recipient of the greeting",
                         "World",
                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class,
                                     N_PROPS,
                                     hello_world_plugin_properties);
}



static void
mousepad_app_activatable_iface_init (MousepadAppActivatableInterface *iface)
{
  iface->activate = hello_world_plugin_activate;
  iface->deactivate = hello_world_plugin_deactivate;
}



static void
hello_world_plugin_class_finalize (HelloWorldPluginClass *klass)
{
}



G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
  hello_world_plugin_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              MOUSEPAD_TYPE_APP_ACTIVATABLE,
                                              HELLO_WORLD_TYPE_PLUGIN);
}