#ifndef HELLO_WORLD_PLUGIN_H
#define HELLO_WORLD_PLUGIN_H

#include <gtk/gtk.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define HELLO_WORLD_TYPE_PLUGIN         (hello_world_plugin_get_type ())
#define HELLO_WORLD_PLUGIN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), HELLO_WORLD_TYPE_PLUGIN, HelloWorldPlugin))
#define HELLO_WORLD_PLUGIN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), HELLO_WORLD_TYPE_PLUGIN, HelloWorldPlugin))
#define HELLO_WORLD_IS_PLUGIN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), HELLO_WORLD_TYPE_PLUGIN))
#define HELLO_WORLD_IS_PLUGIN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), HELLO_WORLD_TYPE_PLUGIN))
#define HELLO_WORLD_PLUGIN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), HELLO_WORLD_TYPE_PLUGIN, HelloWorldPluginClass))

typedef struct _HelloWorldPlugin      HelloWorldPlugin;
typedef struct _HelloWorldPluginClass HelloWorldPluginClass;

GType                hello_world_plugin_get_type (void) G_GNUC_CONST;
G_MODULE_EXPORT void peas_register_types         (PeasObjectModule *module);

G_END_DECLS

#endif /* HELLO_WORLD_PLUGIN_H */