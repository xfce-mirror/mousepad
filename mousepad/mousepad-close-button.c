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



struct MousepadCloseButton_
{
  GtkButton parent;
};

struct MousepadCloseButtonClass_
{
  GtkButtonClass  parent_class;
};



G_DEFINE_TYPE (MousepadCloseButton, mousepad_close_button, GTK_TYPE_BUTTON)



static void
mousepad_close_button_class_init (MousepadCloseButtonClass *klass)
{
}



static void
mousepad_close_button_init (MousepadCloseButton *button)
{
  GtkWidget       *image;
  GtkCssProvider  *css_provider;
  GtkStyleContext *context;

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

  image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
  gtk_button_set_child (GTK_BUTTON (button), image);
  gtk_widget_show (image);

  g_object_set (button, "relief", GTK_RELIEF_NONE, "focus-on-click", FALSE, NULL);
}



GtkWidget *
mousepad_close_button_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_CLOSE_BUTTON, NULL);
}
