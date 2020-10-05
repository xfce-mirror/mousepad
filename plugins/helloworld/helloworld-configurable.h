#ifndef HELLO_WORLD_CONFIGURABLE_H
#define HELLO_WORLD_CONFIGURABLE_H

#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <libpeas-gtk/peas-gtk.h>

G_BEGIN_DECLS

#define HELLO_WORLD_TYPE_CONFIGURABLE         (hello_world_configurable_get_type ())
#define HELLO_WORLD_CONFIGURABLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), HELLO_WORLD_TYPE_CONFIGURABLE, HelloWorldConfigurable))
#define HELLO_WORLD_CONFIGURABLE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), HELLO_WORLD_TYPE_CONFIGURABLE, HelloWorldConfigurable))
#define HELLO_WORLD_IS_CONFIGURABLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), HELLO_WORLD_TYPE_CONFIGURABLE))
#define HELLO_WORLD_IS_CONFIGURABLE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), HELLO_WORLD_TYPE_CONFIGURABLE))
#define HELLO_WORLD_CONFIGURABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), HELLO_WORLD_TYPE_CONFIGURABLE, HelloWorldConfigurableClass))

typedef struct _HelloWorldConfigurable      HelloWorldConfigurable;
typedef struct _HelloWorldConfigurableClass HelloWorldConfigurableClass;

GType hello_world_configurable_get_type (void) G_GNUC_CONST;
void  hello_world_configurable_register (GTypeModule *module);

G_END_DECLS

#endif /* HELLO_WORLD_CONFIGURABLE_H */