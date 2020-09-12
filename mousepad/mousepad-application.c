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
#include <mousepad/mousepad-application.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-prefs-dialog.h>
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-window-ui.h>



static void        mousepad_application_finalize                  (GObject                   *object);
static void        mousepad_application_window_destroyed          (GtkWidget                 *window,
                                                                   MousepadApplication        *application);
static GtkWidget  *mousepad_application_create_window             (MousepadApplication        *application);
static void        mousepad_application_new_window_with_document  (MousepadWindow             *existing,
                                                                   MousepadDocument           *document,
                                                                   gint                        x,
                                                                   gint                        y,
                                                                   MousepadApplication        *application);
static void        mousepad_application_new_window                (MousepadWindow             *existing,
                                                                   MousepadApplication        *application);



struct _MousepadApplicationClass
{
  GtkApplicationClass __parent__;
};

struct _MousepadApplication
{
  GtkApplication      __parent__;

  /* internal list of all the opened windows */
  GSList     *windows;

  /* the preferences dialog when shown */
  GtkWidget  *prefs_dialog;

  /* the menu builder */
  GtkBuilder *builder;
};



G_DEFINE_TYPE (MousepadApplication, mousepad_application, GTK_TYPE_APPLICATION)



static void
mousepad_application_class_init (MousepadApplicationClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_application_finalize;
}



static void
mousepad_application_init (MousepadApplication *application)
{
  gchar *filename;

  /* GApplication properties */
  /*
   * Defining the application_id seems useless, since the uniqueness is not handled at this level
   * for now, and this would cause a GLib-GIO-CRITICAL from g_application_set_application_id() below,
   * complaining that the application is already registered, although it isn't
   * A possible TODO: make full use of GApplication features, by launching Mousepad with
   * g_application_run() from main()
   * See https://developer.gnome.org/gio/stable/GApplication.html
   */
  /* g_application_set_application_id (G_APPLICATION (application), "org.xfce.mousepad"); */
  g_application_set_flags (G_APPLICATION (application), G_APPLICATION_FLAGS_NONE);

  /* TODO: error handling (related to the TODO above) */
  g_application_register (G_APPLICATION (application), NULL, NULL);

  /* set the builder and the menubar */
  application->builder = gtk_builder_new_from_string (mousepad_window_ui,
                                                      mousepad_window_ui_length);
  gtk_application_set_menubar (GTK_APPLICATION (application),
                               G_MENU_MODEL (gtk_builder_get_object (application->builder, "menubar")));

  mousepad_settings_init ();
  application->prefs_dialog = NULL;

  /* check if we have a saved accel map */
  filename = mousepad_util_get_save_location (MOUSEPAD_ACCELS_RELPATH, FALSE);
  if (G_LIKELY (filename != NULL))
    {
      /* load the accel map */
      gtk_accel_map_load (filename);

      /* cleanup */
      g_free (filename);
    }
}



static void
mousepad_application_finalize (GObject *object)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (object);
  GSList              *li;
  gchar               *filename;

  if (GTK_IS_WIDGET (application->prefs_dialog))
    gtk_widget_destroy (application->prefs_dialog);

  /* flush the history items of the replace dialog
   * this is a bit of an ugly place, but cleaning on a window close
   * isn't a good option eighter */
  mousepad_replace_dialog_history_clean ();

  /* save the current accel map */
  filename = mousepad_util_get_save_location (MOUSEPAD_ACCELS_RELPATH, TRUE);
  if (G_LIKELY (filename != NULL))
    {
      /* save the accel map */
      gtk_accel_map_save (filename);

      /* cleanup */
      g_free (filename);
    }

  /* destroy the windows if they are still opened */
  for (li = application->windows; li != NULL; li = li->next)
    {
      mousepad_disconnect_by_func (G_OBJECT (li->data), mousepad_application_window_destroyed, application);
      gtk_widget_destroy (GTK_WIDGET (li->data));
    }

  /* cleanup the list of windows */
  g_slist_free (application->windows);

  mousepad_settings_finalize ();

  /* destroy the GtkBuilder instance */
  if (G_IS_OBJECT (application->builder))
    g_object_unref (application->builder);

  (*G_OBJECT_CLASS (mousepad_application_parent_class)->finalize) (object);
}



MousepadApplication*
mousepad_application_get (void)
{
  static MousepadApplication *application = NULL;

  if (G_UNLIKELY (application == NULL))
    {
      application = g_object_new (MOUSEPAD_TYPE_APPLICATION, NULL);
      g_object_add_weak_pointer (G_OBJECT (application), (gpointer) &application);
    }
  else
    {
      g_object_ref (G_OBJECT (application));
    }

  return application;
}



gboolean
mousepad_application_has_windows (MousepadApplication *application)
{
  g_return_val_if_fail (MOUSEPAD_IS_APPLICATION (application), FALSE);

  return (application->windows != NULL);
}



static void
mousepad_application_window_destroyed (GtkWidget           *window,
                                       MousepadApplication *application)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_APPLICATION (application));
  g_return_if_fail (g_slist_find (application->windows, window) != NULL);

  /* remove the window from the list */
  application->windows = g_slist_remove (application->windows, window);

  /* quit if there are no windows opened */
  if (application->windows == NULL)
    gtk_main_quit ();
}



void
mousepad_application_take_window (MousepadApplication *application,
                                  GtkWindow           *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_APPLICATION (application));
  g_return_if_fail (g_slist_find (application->windows, window) == NULL);

  /* connect to the "destroy" signal */
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (mousepad_application_window_destroyed), application);

  /* add the window to our internal list */
  application->windows = g_slist_prepend (application->windows, window);
}



static GtkWidget *
mousepad_application_create_window (MousepadApplication *application)
{
  GtkWidget *window;

  /* create a new window */
  window = mousepad_window_new ();

  /* window post-initialization */
  gtk_window_set_application (GTK_WINDOW (window), GTK_APPLICATION (application));

  /*
   * Outsource the creation of the menubar from
   * gtk/gtk/gtkapplicationwindow.c:gtk_application_window_update_menubar(), to make the menubar
   * a window attribute, and be able to access its items to show their tooltips in the statusbar.
   * With GTK+ 3, this leads to use gtk_menu_bar_new_from_model()
   * With GTK+ 4, this will lead to use gtk_popover_menu_bar_new_from_model()
   */
  gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (window), FALSE);
  mousepad_window_create_menubar (MOUSEPAD_WINDOW (window),
                                  gtk_application_get_menubar (GTK_APPLICATION (application)));

  /* hook up the new window */
  mousepad_application_take_window (application, GTK_WINDOW (window));

  /* connect signals */
  g_signal_connect (G_OBJECT (window), "new-window-with-document",
                    G_CALLBACK (mousepad_application_new_window_with_document), application);
  g_signal_connect (G_OBJECT (window), "new-window",
                    G_CALLBACK (mousepad_application_new_window), application);

  return window;
}



static void
mousepad_application_new_window_with_document (MousepadWindow      *existing,
                                               MousepadDocument    *document,
                                               gint                 x,
                                               gint                 y,
                                               MousepadApplication *application)
{
  GtkWidget *window;
  GdkScreen *screen;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (existing));
  g_return_if_fail (document == NULL || MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (MOUSEPAD_IS_APPLICATION (application));

  /* create a new window (signals added and already hooked up) */
  window = mousepad_application_create_window (application);

  /* place the new window on the same screen as the existing window */
  screen = gtk_window_get_screen (GTK_WINDOW (existing));
  if (G_LIKELY (screen != NULL))
    gtk_window_set_screen (GTK_WINDOW (window), screen);

  /* move the window on valid cooridinates */
  if (x > -1 && y > -1)
    gtk_window_move (GTK_WINDOW (window), x, y);

  /* create an empty document if no document was send */
  if (document == NULL)
    document = mousepad_document_new ();

  /* add the document to the new window */
  mousepad_window_add (MOUSEPAD_WINDOW (window), document);

  /* show the window */
  gtk_widget_show (window);
}



static void
mousepad_application_new_window (MousepadWindow      *existing,
                                 MousepadApplication *application)
{
  /* trigger new document function */
  mousepad_application_new_window_with_document (existing, NULL, -1, -1, application);
}



gboolean
mousepad_application_new_window_with_files (MousepadApplication  *application,
                                            GdkScreen            *screen,
                                            const gchar          *working_directory,
                                            gchar               **filenames)
{
  GtkWidget        *window;
  gboolean          succeed = FALSE;
  MousepadDocument *document;
  GtkWindowGroup   *window_group;

  g_return_val_if_fail (MOUSEPAD_IS_APPLICATION (application), succeed);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), succeed);

  /* create a new window (signals added and already hooked up) */
  window = mousepad_application_create_window (application);

  /* add window to own window group so that grabs only affect parent window */
  window_group = gtk_window_group_new ();
  gtk_window_group_add_window (window_group, GTK_WINDOW (window));
  g_object_unref (window_group);

  /* place the window on the right screen */
  gtk_window_set_screen (GTK_WINDOW (window), screen ? screen : gdk_screen_get_default ());

  /* try to open the files if any, or open an empty document */
  if (working_directory && filenames && g_strv_length (filenames))
    succeed = mousepad_window_open_files (MOUSEPAD_WINDOW (window), working_directory, filenames);
  else
    {
      /* create a new document */
      document = mousepad_document_new ();

      /* add the document to the new window */
      mousepad_window_add (MOUSEPAD_WINDOW (window), document);
      succeed = TRUE;
    }

  /* show the window */
  if (succeed == TRUE)
    gtk_widget_show (window);

  return succeed;
}



static void
mousepad_application_prefs_dialog_response (MousepadApplication *application,
                                            gint                 response_id,
                                            MousepadPrefsDialog *dialog)
{
  gtk_widget_destroy (GTK_WIDGET (application->prefs_dialog));
  application->prefs_dialog = NULL;
}



void
mousepad_application_show_preferences (MousepadApplication  *application,
                                       GtkWindow            *transient_for)
{
  /* if the dialog isn't already shown, create one */
  if (! GTK_IS_WIDGET (application->prefs_dialog))
    {
      application->prefs_dialog = mousepad_prefs_dialog_new ();

      /* destroy the dialog when it's close button is pressed */
      g_signal_connect_swapped (application->prefs_dialog,
                                "response",
                                G_CALLBACK (mousepad_application_prefs_dialog_response),
                                application);
    }

  /* if no transient window was specified, used the first application window
   * or NULL if no windows exists (shouldn't happen) */
  if (! GTK_IS_WINDOW (transient_for))
    {
      if (application->windows && GTK_IS_WINDOW (application->windows->data))
        transient_for = GTK_WINDOW (application->windows->data);
      else
        transient_for = NULL;
    }

  /* associate it with one of the windows */
  gtk_window_set_transient_for (GTK_WINDOW (application->prefs_dialog), transient_for);

  /* show it to the user */
  gtk_window_present (GTK_WINDOW (application->prefs_dialog));
}



GtkBuilder*
mousepad_application_get_builder (MousepadApplication *application)
{
  g_return_val_if_fail (MOUSEPAD_IS_APPLICATION (application), NULL);

  return application->builder;
}
