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

#include "mousepad/mousepad-private.h"
#include "mousepad/mousepad-statusbar.h"
#include "mousepad/mousepad-util.h"
#include "mousepad/mousepad-window.h"



/* event handlers */
static void
mousepad_statusbar_overwrite_clicked (GtkGestureClick *gesture,
                                      int n_press,
                                      double x,
                                      double y,
                                      MousepadStatusbar *statusbar);
static void
mousepad_statusbar_language_clicked (GtkGestureClick *gesture,
                                     int n_press,
                                     double x,
                                     double y,
                                     MousepadStatusbar *statusbar);



enum
{
  ENABLE_OVERWRITE,
  LAST_SIGNAL,
};

struct _MousepadStatusbar
{
  GtkBox __parent__;

  /* the gtk statusbar, added as child widget */
  GtkWidget *gtkbar;

  /* whether overwrite is enabled */
  guint overwrite_enabled : 1;

  /* extra labels in the statusbar */
  GtkWidget *language;
  GtkWidget *encoding;
  GtkWidget *position;
  GtkWidget *overwrite;
};



static guint statusbar_signals[LAST_SIGNAL];



G_DEFINE_TYPE (MousepadStatusbar, mousepad_statusbar, GTK_TYPE_BOX)



GtkWidget *
mousepad_statusbar_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_STATUSBAR, NULL);
}



static void
mousepad_statusbar_class_init (MousepadStatusbarClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  statusbar_signals[ENABLE_OVERWRITE] = g_signal_new (I_ ("enable-overwrite"),
                                                      G_TYPE_FROM_CLASS (gobject_class),
                                                      G_SIGNAL_RUN_LAST,
                                                      0, NULL, NULL,
                                                      g_cclosure_marshal_VOID__BOOLEAN,
                                                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}



static void
mousepad_statusbar_init (MousepadStatusbar *statusbar)
{
  GtkBox *box;
  GtkWidget *separator, *label;
  GtkEventController *controller;

  /* create a new gtk statusbar added as child widget */
  statusbar->gtkbar = gtk_statusbar_new ();
  gtk_widget_set_hexpand (statusbar->gtkbar, TRUE);
  gtk_box_append (GTK_BOX (statusbar), statusbar->gtkbar);

  /* retrieve the gtk statusbar box */
  box = GTK_BOX (gtk_widget_get_first_child (statusbar->gtkbar));
  gtk_box_set_spacing (box, 8);

  /* main label */
  label = gtk_widget_get_first_child (GTK_WIDGET (box));
  gtk_widget_set_hexpand (label, TRUE);

  /* separator */
  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (box, separator);

  /* language/filetype */
  label = statusbar->language = gtk_label_new (_("Filetype: None"));
  gtk_widget_set_tooltip_text (label, _("Choose a filetype"));
  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect (controller, "pressed",
                    G_CALLBACK (mousepad_statusbar_language_clicked), statusbar);
  gtk_widget_add_controller (label, controller);
  gtk_box_append (box, label);

  /* separator */
  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (box, separator);

  /* encoding */
  statusbar->encoding = gtk_label_new (NULL);
  gtk_box_append (box, statusbar->encoding);

  /* separator */
  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (box, separator);

  /* line and column numbers */
  statusbar->position = gtk_label_new (NULL);
  gtk_box_append (box, statusbar->position);

  /* separator */
  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (box, separator);

  /* overwrite label */
  label = statusbar->overwrite = gtk_label_new (_("OVR"));
  gtk_widget_set_tooltip_text (label, _("Toggle the overwrite mode"));
  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect (controller, "pressed",
                    G_CALLBACK (mousepad_statusbar_overwrite_clicked), statusbar);
  gtk_widget_add_controller (label, controller);
  gtk_box_append (box, label);
}



static void
mousepad_statusbar_overwrite_clicked (GtkGestureClick *gesture,
                                      int n_press,
                                      double x,
                                      double y,
                                      MousepadStatusbar *statusbar)
{
  g_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  /* swap the overwrite mode */
  statusbar->overwrite_enabled = !statusbar->overwrite_enabled;

  /* send the signal */
  g_signal_emit (statusbar, statusbar_signals[ENABLE_OVERWRITE], 0, statusbar->overwrite_enabled);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}



static void
mousepad_statusbar_language_clicked (GtkGestureClick *gesture,
                                     int n_press,
                                     double x,
                                     double y,
                                     MousepadStatusbar *statusbar)
{
  GtkWidget *window, *menu;

  g_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  /* get the languages menu from the window */
  window = gtk_widget_get_ancestor (GTK_WIDGET (statusbar), MOUSEPAD_TYPE_WINDOW);
  menu = mousepad_window_get_languages_menu (MOUSEPAD_WINDOW (window));
  if (gtk_widget_get_parent (menu) == NULL)
    gtk_widget_set_parent (menu, statusbar->language);

  /* show the menu */
  gtk_widget_show (menu);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}



void
mousepad_statusbar_set_cursor_position (MousepadStatusbar *statusbar,
                                        gint line,
                                        gint column,
                                        gint selection)
{
  gchar string[64];

  g_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  /* create printable string */
  if (G_UNLIKELY (selection > 0))
    g_snprintf (string, sizeof (string), _("Line: %d Column: %d Selection: %d"),
                line, column, selection);
  else
    g_snprintf (string, sizeof (string), _("Line: %d Column: %d"), line, column);

  /* set label */
  gtk_label_set_text (GTK_LABEL (statusbar->position), string);
}



void
mousepad_statusbar_set_encoding (MousepadStatusbar *statusbar,
                                 MousepadEncoding encoding)
{
  g_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  if (encoding == MOUSEPAD_ENCODING_NONE)
    encoding = mousepad_encoding_get_default ();

  gtk_label_set_text (GTK_LABEL (statusbar->encoding), mousepad_encoding_get_charset (encoding));
}



void
mousepad_statusbar_set_language (MousepadStatusbar *statusbar,
                                 GtkSourceLanguage *language)
{
  gchar *label;

  g_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  if (language == NULL)
    gtk_label_set_text (GTK_LABEL (statusbar->language), _("Filetype: None"));
  else
    {
      label = g_strdup_printf (_("Filetype: %s"), gtk_source_language_get_name (language));
      gtk_label_set_text (GTK_LABEL (statusbar->language), label);
      g_free (label);
    }
}



void
mousepad_statusbar_set_overwrite (MousepadStatusbar *statusbar,
                                  gboolean overwrite)
{
  g_return_if_fail (MOUSEPAD_IS_STATUSBAR (statusbar));

  if (!overwrite)
    gtk_widget_add_css_class (statusbar->overwrite, "dim-label");
  else
    gtk_widget_remove_css_class (statusbar->overwrite, "dim-label");

  statusbar->overwrite_enabled = overwrite;
}


void
mousepad_statusbar_push_tooltip (MousepadStatusbar *statusbar,
                                 const gchar *tooltip)
{
  GtkStatusbar *bar = GTK_STATUSBAR (statusbar->gtkbar);
  gint id;

  if (tooltip != NULL)
    {
      /* show the tooltip */
      id = gtk_statusbar_get_context_id (bar, "tooltip");
      gtk_statusbar_push (bar, id, tooltip);
    }
}



void
mousepad_statusbar_pop_tooltip (MousepadStatusbar *statusbar)
{
  GtkStatusbar *bar = GTK_STATUSBAR (statusbar->gtkbar);
  gint id;

  /* drop the widget's tooltip from the statusbar */
  id = gtk_statusbar_get_context_id (bar, "tooltip");
  gtk_statusbar_pop (bar, id);
}
