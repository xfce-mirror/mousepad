#include "helloworld-configurable.h"

#include <libpeas-gtk/peas-gtk.h>



struct _HelloWorldConfigurable
{
  PeasExtensionBase parent_instance;
};



struct _HelloWorldConfigurableClass
{
  PeasExtensionBaseClass parent_class;
};



static void peas_gtk_configurable_iface_init (PeasGtkConfigurableInterface *iface);



G_DEFINE_DYNAMIC_TYPE_EXTENDED (HelloWorldConfigurable,
                                hello_world_configurable,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_GTK_TYPE_CONFIGURABLE,
                                                               peas_gtk_configurable_iface_init))



static void
hello_world_configurable_init (HelloWorldConfigurable *plugin)
{
}



static GtkWidget *
hello_world_configurable_create_configure_widget (PeasGtkConfigurable *configurable)
{
  GtkWidget      *hbox;
  GtkWidget      *label;
  GtkWidget      *entry;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  label = gtk_label_new ("Recipient:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  return hbox;
}



static void
peas_gtk_configurable_iface_init (PeasGtkConfigurableInterface *iface)
{
  iface->create_configure_widget = hello_world_configurable_create_configure_widget;
}



static void
hello_world_configurable_class_init (HelloWorldConfigurableClass *klass)
{
}



static void
hello_world_configurable_class_finalize (HelloWorldConfigurableClass *klass)
{
}



void
hello_world_configurable_register (GTypeModule *module)
{
  hello_world_configurable_register_type (module);
}
