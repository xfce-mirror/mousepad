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

#include <xfconf/xfconf.h>



/* GApplication virtual functions, in the order in which they are called on launch */
static gint        mousepad_application_handle_local_options      (GApplication             *gapplication,
                                                                   GVariantDict             *options);
static void        mousepad_application_startup                   (GApplication             *gapplication);
static gint        mousepad_application_command_line              (GApplication             *gapplication,
                                                                   GApplicationCommandLine  *command_line);
static void        mousepad_application_activate                  (GApplication             *gapplication);
static void        mousepad_application_open                      (GApplication             *gapplication,
                                                                   GFile                   **files,
                                                                   gint                      n_files,
                                                                   const gchar              *hint);
static void        mousepad_application_shutdown                  (GApplication             *gapplication);

/* MousepadApplication own functions */
static GtkWidget  *mousepad_application_create_window             (MousepadApplication      *application);
static void        mousepad_application_new_window_with_document  (MousepadWindow           *existing,
                                                                   MousepadDocument         *document,
                                                                   gint                      x,
                                                                   gint                      y,
                                                                   MousepadApplication      *application);
static void        mousepad_application_new_window                (MousepadWindow           *existing,
                                                                   MousepadApplication      *application);
static void        mousepad_application_active_window_changed     (MousepadApplication      *application);
static void        mousepad_application_create_languages_menu     (MousepadApplication      *application);
static void        mousepad_application_create_style_schemes_menu (MousepadApplication      *application);
static void        mousepad_application_action_quit               (GSimpleAction            *action,
                                                                   GVariant                 *value,
                                                                   gpointer                  data);



struct _MousepadApplicationClass
{
  GtkApplicationClass __parent__;
};

struct _MousepadApplication
{
  GtkApplication      __parent__;

  /* the preferences dialog when shown */
  GtkWidget *prefs_dialog;

  /* opening mode provided on the command line */
  gint       opening_mode;
};



/* command line options */
static const GOptionEntry option_entries[] =
{
  {
    "opening-mode", 'o', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_STRING, NULL,
    NC_("The words 'tab', 'window' and 'mixed' enclosed in quotation marks must not be translated",
        "File opening mode: \"tab\", \"window\" or \"mixed\" (open tabs in a new window)"),
    N_("MODE")
  },
  {
    "disable-server", '\0', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, NULL,
    N_("Do not register with the D-BUS session message bus"), NULL
  },
  {
    "quit", 'q', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, NULL,
    N_("Quit a running Mousepad primary instance"), NULL
  },
  {
    "version", 'v', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, NULL,
    N_("Print version information and exit"), NULL
  },
  {
    G_OPTION_REMAINING, '\0', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_FILENAME_ARRAY, NULL,
    NULL, N_("[FILES...]")
  },
  { NULL }
};



/* application actions */
static const GActionEntry action_entries[] =
{
  { "quit", mousepad_application_action_quit, NULL, NULL, NULL }
};



/* opening mode provided on the command line */
enum
{
  TAB = 0,
  WINDOW,
  MIXED
};



G_DEFINE_TYPE (MousepadApplication, mousepad_application, GTK_TYPE_APPLICATION)



static void
mousepad_application_class_init (MousepadApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  application_class->handle_local_options = mousepad_application_handle_local_options;
  application_class->startup = mousepad_application_startup;
  application_class->command_line = mousepad_application_command_line;
  application_class->activate = mousepad_application_activate;
  application_class->open = mousepad_application_open;
  application_class->shutdown = mousepad_application_shutdown;
}



static void
mousepad_application_init (MousepadApplication *application)
{
  /* default application name */
  g_set_application_name (_("Mousepad"));

  /* use the Mousepad icon as default for new windows */
  gtk_window_set_default_icon_name ("org.xfce.mousepad");

  /* add our option entries to the application */
  g_application_add_main_option_entries (G_APPLICATION (application), option_entries);
}



static gint
mousepad_application_handle_local_options (GApplication *gapplication,
                                           GVariantDict *options)
{
  GApplicationFlags  flags;
  GError            *error = NULL;

  if (g_variant_dict_contains (options, "version"))
    {
      g_print ("%s %s\n\n", PACKAGE_NAME, PACKAGE_VERSION);
      g_print ("%s\n", "Copyright (c) 2007");
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  if (g_variant_dict_contains (options, "quit"))
    {
      /* try to register the application */
      if (! g_application_register (gapplication, NULL, &error))
        {
          g_printerr ("%s\n", error->message);
          g_error_free (error);

          return EXIT_FAILURE;
        }

      /* try to find a running primary instance */
      if (! g_application_get_is_remote (gapplication))
        {
          g_printerr ("%s\n", "Failed to find a running Mousepad primary instance");

          return EXIT_FAILURE;
        }
      else
        g_action_group_activate_action (G_ACTION_GROUP (gapplication), "quit", NULL);

      return EXIT_SUCCESS;
    }

  if (g_variant_dict_contains (options, "disable-server"))
    {
      flags = g_application_get_flags (gapplication);
      g_application_set_flags (gapplication, flags | G_APPLICATION_NON_UNIQUE);
    }

  /* chain up to startup (primary instance) or command_line (remote instance) */
  return -1;
}



static void
mousepad_application_startup (GApplication *gapplication)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (gapplication);
  const gchar         *accels[] = { NULL };

  /* chain up to parent */
  G_APPLICATION_CLASS (mousepad_application_parent_class)->startup (gapplication);

  /* initialize application attributes */
  application->prefs_dialog = NULL;
  application->opening_mode = TAB;

  /* initialize mousepad settings */
  mousepad_settings_init ();

  /* add application actions */
  g_action_map_add_action_entries (G_ACTION_MAP (application), action_entries,
                                   G_N_ELEMENTS (action_entries), application);

  /*
   * Some accels are already active at the widget level for some widgets, so we don't need and
   * should not activate them at the action level, to not introduce conflicts.
   * But we want them to appear in the menubar, as an indication for the user.
   * So this trick does the job: adding them to the GtkBuilder UI file, and deactivating them
   * when the menubar is set.
  */
  gtk_application_set_accels_for_action (GTK_APPLICATION (application), "win.edit.select-all", accels);

  /* add some static submenus to the application menubar */
  mousepad_application_create_languages_menu (application);
  mousepad_application_create_style_schemes_menu (application);

  /* do some actions when the active window changes */
  g_signal_connect (application, "notify::active-window",
                    G_CALLBACK (mousepad_application_active_window_changed), NULL);
}



static gint
mousepad_application_command_line (GApplication            *gapplication,
                                   GApplicationCommandLine *command_line)
{
  MousepadApplication  *application = MOUSEPAD_APPLICATION (gapplication);
  GVariantDict         *options;
  GError               *error = NULL;
  GPtrArray            *files;
  GFile                *file;
  gpointer             *data;
  const gchar          *opening_mode, *working_directory;
  gchar               **filenames = NULL;
  gint                  n, n_files;

  /* initialize xfconf */
  if (G_UNLIKELY (xfconf_init (&error) == FALSE))
    {
      g_application_command_line_printerr (command_line, "%s\n", "Failed to initialize xfconf");
      g_error_free (error);

      return EXIT_FAILURE;
    }

  /* get the option dictionary */
  options = g_application_command_line_get_options_dict (command_line);

  /* see if an opening mode was provided on the command line */
  if (g_variant_dict_lookup (options, "opening-mode", "&s", &opening_mode))
    {
      if (g_strcmp0 (opening_mode, "tab") == 0)
        application->opening_mode = TAB;
      else if (g_strcmp0 (opening_mode, "window") == 0)
        application->opening_mode = WINDOW;
      else if (g_strcmp0 (opening_mode, "mixed") == 0)
        application->opening_mode = MIXED;
      else
        {
          application->opening_mode = MOUSEPAD_SETTING_GET_ENUM (OPENING_MODE);
          g_application_command_line_printerr (command_line, "%s\n",
                                               "Invalid opening mode: ignored");
        }
    }
  /* use the opening mode stored in the settings */
  else
    application->opening_mode = MOUSEPAD_SETTING_GET_ENUM (OPENING_MODE);

  /* extract filenames */
  g_variant_dict_lookup (options, G_OPTION_REMAINING, "^a&ay", &filenames);

  /* get the current working directory */
  working_directory = g_application_command_line_get_cwd (command_line);

  /* open files provided on the command line or an empty document */
  if (working_directory && filenames && (n_files = g_strv_length (filenames)))
    {
      /* prepare the GFiles array */
      files = g_ptr_array_new ();
      for (n = 0; n < n_files; n++)
        {
          file = g_application_command_line_create_file_for_arg (command_line, filenames[n]);
          g_ptr_array_add (files, file);
        }
      data = g_ptr_array_free (files, FALSE);

      /* open the files */
      mousepad_application_open (gapplication, (GFile **) data, n_files, NULL);

      /* cleanup */
      for (n = 0; n < n_files; n++)
        g_object_unref (data[n]);
      g_free (data);
    }
  else
    mousepad_application_activate (gapplication);

  /* cleanup */
  g_free (filenames);

  return EXIT_SUCCESS;
}



static GtkWidget *
mousepad_application_get_window_for_files (MousepadApplication *application)
{
  GtkWidget *window = NULL;
  GList     *list, *li;

  /* when opening mode is "tab", retrieve the last active MousepadWindow, if any */
  if (application->opening_mode == TAB)
    {
      list = gtk_application_get_windows (GTK_APPLICATION (application));
      for (li = list; li != NULL; li = li->next)
        if (MOUSEPAD_IS_WINDOW (li->data))
          {
            window = li->data;
            break;
          }
    }

  /* create a new window (signals added and already hooked up) if needed */
  if (window == NULL)
    window = mousepad_application_create_window (application);

  return window;
}



static void
mousepad_application_activate (GApplication *gapplication)
{
  GtkWidget *window;

  /* get the window to open a new document */
  window = mousepad_application_get_window_for_files (MOUSEPAD_APPLICATION (gapplication));

  /* create a new document and add it to the window */
  g_action_group_activate_action (G_ACTION_GROUP (window), "file.new", NULL);

  /* show the window */
  gtk_widget_show (window);
}



static void
mousepad_application_open (GApplication  *gapplication,
                           GFile        **files,
                           gint           n_files,
                           const gchar   *hint)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (gapplication);
  GtkWidget           *window;
  GError              *error = NULL;
  GPtrArray           *uris;
  gpointer            *data;
  gchar               *filename, *uri;
  gchar               *temp_uri[] = { NULL, NULL };
  gint                 n, valid = 0, opened = 0;

  /* prepare the uris array */
  uris = g_ptr_array_new ();
  for (n = 0; n < n_files; n++)
    {
      uri = g_file_get_uri (files[n]);
      filename = g_filename_from_uri (uri, NULL, &error);

      /* there could be invalid uris when the call comes from gdbus */
      if (filename)
        {
          g_ptr_array_add (uris, uri);
          valid++;
        }
      else
        {
          g_printerr ("%s\n", error->message);
          g_clear_error (&error);
          g_free (uri);
        }

      g_free (filename);
    }

  if (valid > 0)
    {
      /* get the underlying filename array */
      g_ptr_array_add (uris, NULL);
      data = g_ptr_array_free (uris, FALSE);

      /* open the files in tabs */
      if (application->opening_mode != WINDOW)
        {
          /* get the window to open the files */
          window = mousepad_application_get_window_for_files (application);

          /* open the files */
          opened = mousepad_window_open_files (MOUSEPAD_WINDOW (window), (gchar **) data);

          /* if at least one file was finally opened, show the window */
          if (opened > 0)
            gtk_widget_show (window);
          else
            gtk_widget_destroy (window);
        }
      /* open the files in windows */
      else
        {
          for (n = 0; n < valid; n++)
            {
              /* create a new window (signals added and already hooked up) */
              window = mousepad_application_create_window (application);

              /* open the file */
              temp_uri[0] = data[n];
              opened = mousepad_window_open_files (MOUSEPAD_WINDOW (window), temp_uri);

              /* if the file was finally opened, show the window */
              if (opened > 0)
                gtk_widget_show (window);
              else
                gtk_widget_destroy (window);
            }
        }

      /* cleanup */
      g_strfreev ((gchar **) data);
    }
  else
    g_ptr_array_free (uris, TRUE);
}



static void
mousepad_application_shutdown (GApplication *gapplication)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (gapplication);
  GList               *windows, *window;

  if (GTK_IS_WIDGET (application->prefs_dialog))
    gtk_widget_destroy (application->prefs_dialog);

  /* flush the history items of the replace dialog
   * this is a bit of an ugly place, but cleaning on a window close
   * isn't a good option eighter */
  mousepad_replace_dialog_history_clean ();

  /* destroy the windows if they are still opened */
  windows = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (application)));
  for (window = windows; window != NULL; window = window->next)
    gtk_widget_destroy (window->data);

  g_list_free (windows);

  /* finalize mousepad settings */
  mousepad_settings_finalize ();

  /* shutdown xfconf */
  xfconf_shutdown ();

  /* chain up to parent */
  G_APPLICATION_CLASS (mousepad_application_parent_class)->shutdown (gapplication);
}



static GtkWidget *
mousepad_application_create_window (MousepadApplication *application)
{
  GtkWindowGroup *window_group;
  GtkWidget      *window;

  /* create a new window */
  window = mousepad_window_new (application);

  /* add window to own window group so that grabs only affect parent window */
  window_group = gtk_window_group_new ();
  gtk_window_group_add_window (window_group, GTK_WINDOW (window));
  g_object_unref (window_group);

  /* place the window on the right screen */
  gtk_window_set_screen (GTK_WINDOW (window), gdk_screen_get_default ());

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



static void
mousepad_application_active_window_changed (MousepadApplication *application)
{
  GList        *app_windows;
  static GList *windows = NULL;

  /* get the application windows list */
  app_windows = gtk_application_get_windows (GTK_APPLICATION (application));

  /* filter false change and window additions */
  if (windows != NULL && app_windows != NULL && windows->data != app_windows->data
      && g_list_find (windows, app_windows->data))
    {
      /* update document dependent menu items */
      mousepad_window_update_document_menu_items (MOUSEPAD_WINDOW (app_windows->data));
    }

  /* store a copy of the application windows list to compare at next call */
  g_list_free (windows);
  windows = g_list_copy (app_windows);
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
mousepad_application_show_preferences (MousepadApplication *application,
                                       GtkWindow           *transient_for)
{
  GList *windows;

  /* if the dialog isn't already shown, create one */
  if (! GTK_IS_WIDGET (application->prefs_dialog))
    {
      application->prefs_dialog = mousepad_prefs_dialog_new ();

      /* destroy the dialog when it's close button is pressed */
      g_signal_connect_swapped (application->prefs_dialog, "response",
                                G_CALLBACK (mousepad_application_prefs_dialog_response),
                                application);
    }

  /* if no transient window was specified, used the first application window
   * or NULL if no windows exists (shouldn't happen) */
  if (! GTK_IS_WINDOW (transient_for))
    {
      windows = gtk_application_get_windows (GTK_APPLICATION (application));
      if (windows && GTK_IS_WINDOW (windows->data))
        transient_for = GTK_WINDOW (windows->data);
      else
        transient_for = NULL;
    }

  /* associate it with one of the windows */
  gtk_window_set_transient_for (GTK_WINDOW (application->prefs_dialog), transient_for);

  /* show it to the user */
  gtk_window_present (GTK_WINDOW (application->prefs_dialog));
}



static void
mousepad_application_create_languages_menu (MousepadApplication *application)
{
  GMenu       *menu, *submenu;
  GMenuItem   *item;
  GSList      *sections, *languages, *iter_sect, *iter_lang;
  const gchar *label;
  gchar       *action_name, *tooltip;

  /* get the empty "Filetype" submenu and populate it */
  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "document.filetype.list");

  sections = mousepad_util_get_sorted_section_names ();

  for (iter_sect = sections; iter_sect != NULL; iter_sect = g_slist_next (iter_sect))
    {
      /* create submenu item */
      label = iter_sect->data;
      submenu = g_menu_new ();
      item = g_menu_item_new_submenu (label, G_MENU_MODEL (submenu));

      /* set tooltip and append menu item */
      tooltip = (gchar*) iter_sect->data;
      g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));
      g_menu_append_item (menu, item);
      g_object_unref (item);

      languages = mousepad_util_get_sorted_languages_for_section (label);

      for (iter_lang = languages; iter_lang != NULL; iter_lang = g_slist_next (iter_lang))
        {
          /* create menu item */
          label = gtk_source_language_get_id (iter_lang->data);
          action_name = g_strconcat ("win.document.filetype('", label, "')", NULL);
          label = gtk_source_language_get_name (iter_lang->data);
          item = g_menu_item_new (label, action_name);

          /* set tooltip */
          tooltip = g_strdup_printf ("%s/%s", (gchar*) iter_sect->data, label);
          g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));

          /* append menu item */
          g_menu_append_item (submenu, item);

          /* cleanup */
          g_object_unref (item);
          g_free (action_name);
          g_free (tooltip);
        }

      g_slist_free (languages);
    }

  g_slist_free (sections);
}



static void
mousepad_application_create_style_schemes_menu (MousepadApplication *application)
{
  GMenu        *menu;
  GMenuItem    *item;
  GSList       *schemes, *iter;
  const gchar  *label;
  gchar       **authors;
  gchar        *action_name, *author, *tooltip;

  /* get the empty "Color Scheme" submenu and populate it */
  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "view.color-scheme.list");

  schemes = mousepad_util_style_schemes_get_sorted ();

  for (iter = schemes; iter != NULL; iter = g_slist_next (iter))
    {
      /* create menu item */
      label = gtk_source_style_scheme_get_id (iter->data);
      action_name = g_strconcat ("win.view.color-scheme('", label, "')", NULL);
      label = gtk_source_style_scheme_get_name (iter->data);
      item = g_menu_item_new (label, action_name);

      /* set tooltip */
      authors = (gchar**) gtk_source_style_scheme_get_authors (iter->data);
      author = g_strjoinv (", ", authors);
      tooltip = g_strdup_printf (_("%s | Authors: %s | Filename: %s"),
                                 gtk_source_style_scheme_get_description (iter->data),
                                 author,
                                 gtk_source_style_scheme_get_filename (iter->data));
      g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));

      /* append menu item */
      g_menu_append_item (menu, item);

      /* cleanup */
      g_object_unref (item);
      g_free (action_name);
      g_free (author);
      g_free (tooltip);
    }

  g_slist_free (schemes);
}



static void
mousepad_application_action_quit (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  g_application_quit (G_APPLICATION (data));
}
