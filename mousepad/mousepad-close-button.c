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
#include <mousepad/mousepad-close-button.h>



#define ICON_NAME_MODIFIED "media-record-symbolic"
#define ICON_NAME_UNMODIFIED "window-close"



/* GObject virtual functions */
static void     mousepad_close_button_finalize           (GObject          *object);



struct MousepadCloseButton_
{
  GtkButton parent;

  GtkTextBuffer *buffer;
};

struct MousepadCloseButtonClass_
{
  GtkButtonClass  parent_class;
};



G_DEFINE_TYPE (MousepadCloseButton, mousepad_close_button, GTK_TYPE_BUTTON)



static void
mousepad_close_button_class_init (MousepadCloseButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mousepad_close_button_finalize;
}



static void
mousepad_close_button_motion_enter_event (GtkEventControllerMotion *controller,
                                          gdouble                   x,
                                          gdouble                   y,
                                          MousepadCloseButton      *button)
{
  if (gtk_text_buffer_get_modified (button->buffer))
    gtk_button_set_icon_name (GTK_BUTTON (button), ICON_NAME_UNMODIFIED);
}



static void
mousepad_close_button_motion_leave_event (GtkEventControllerMotion *controller,
                                          gdouble                   x,
                                          gdouble                   y,
                                          MousepadCloseButton      *button)
{
  if (gtk_text_buffer_get_modified (button->buffer))
    gtk_button_set_icon_name (GTK_BUTTON (button), ICON_NAME_MODIFIED);
}



static void
mousepad_close_button_init (MousepadCloseButton *button)
{
  GtkCssProvider     *css_provider;
  GtkStyleContext    *context;
  GtkEventController *controller;

  static const gchar *button_style =
    "* {\n"
    "  outline-width: 0;\n"
    "  outline-offset: 0;\n"
    "  padding: 0;\n"
    "}\n";

  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider, button_style, -1);

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);

  controller = gtk_event_controller_motion_new ();
  gtk_widget_add_controller (GTK_WIDGET (button), controller);
  g_signal_connect (controller, "enter",
                    G_CALLBACK (mousepad_close_button_motion_enter_event), button);
  g_signal_connect (controller, "leave",
                    G_CALLBACK (mousepad_close_button_motion_leave_event), button);
}



static void
mousepad_close_button_finalize (GObject *object)
{
  MousepadCloseButton *button = MOUSEPAD_CLOSE_BUTTON (object);

  g_object_unref (button->buffer);

  G_OBJECT_CLASS (mousepad_close_button_parent_class)->finalize (object);
}



static void
mousepad_close_button_modified_changed (GtkTextBuffer       *buffer,
                                        MousepadCloseButton *button)
{
  const gchar *icon_name;

  icon_name = gtk_text_buffer_get_modified (buffer) ? ICON_NAME_MODIFIED : ICON_NAME_UNMODIFIED;
  gtk_button_set_icon_name (GTK_BUTTON (button), icon_name);
}



GtkWidget *
mousepad_close_button_new (GtkTextBuffer *buffer)
{
  MousepadCloseButton *button;

  button = g_object_new (MOUSEPAD_TYPE_CLOSE_BUTTON, "focus-on-click", FALSE,
                         "has-frame", FALSE, NULL);
  button->buffer = g_object_ref (buffer);
  mousepad_close_button_modified_changed (buffer, button);
  g_signal_connect_object (buffer, "modified-changed",
                           G_CALLBACK (mousepad_close_button_modified_changed), button, 0);

  return GTK_WIDGET (button);
}
