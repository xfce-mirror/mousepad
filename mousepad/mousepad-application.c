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
#include <mousepad/mousepad-util.h>

#include <xfconf/xfconf.h>



/* GObject virtual functions */
static void        mousepad_application_set_property              (GObject                  *object,
                                                                   guint                     prop_id,
                                                                   const GValue             *value,
                                                                   GParamSpec               *pspec);
static void        mousepad_application_get_property              (GObject                  *object,
                                                                   guint                     prop_id,
                                                                   GValue                   *value,
                                                                   GParamSpec               *pspec);

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
static gboolean    mousepad_application_parse_encoding            (const gchar              *option_name,
                                                                   const gchar              *value,
                                                                   gpointer                  data,
                                                                   GError                  **error);
static GtkWidget  *mousepad_application_create_window             (MousepadApplication      *application);
static void        mousepad_application_new_window_with_document  (MousepadWindow           *existing,
                                                                   MousepadDocument         *document,
                                                                   gint                      x,
                                                                   gint                      y,
                                                                   MousepadApplication      *application);
static void        mousepad_application_new_window                (MousepadWindow           *existing,
                                                                   MousepadApplication      *application);
static void        mousepad_application_active_window_changed     (MousepadApplication      *application);
static void        mousepad_application_set_shared_menu_parts     (MousepadApplication      *application,
                                                                   GMenuModel               *model);
static void        mousepad_application_create_languages_menu     (MousepadApplication      *application);
static void        mousepad_application_create_style_schemes_menu (MousepadApplication      *application);
static void        mousepad_application_action_quit               (GSimpleAction            *action,
                                                                   GVariant                 *value,
                                                                   gpointer                  data);
static void        mousepad_application_toggle_activate           (GSimpleAction            *action,
                                                                   GVariant                 *parameter,
                                                                   gpointer                  data);
static void        mousepad_application_radio_activate            (GSimpleAction            *action,
                                                                   GVariant                 *parameter,
                                                                   gpointer                  data);
static void        mousepad_application_action_update             (MousepadApplication      *application,
                                                                   gchar                    *key,
                                                                   GSettings                *settings);
static void        mousepad_application_action_whitespace         (GSimpleAction            *action,
                                                                   GVariant                 *state,
                                                                   gpointer                  data);



struct _MousepadApplicationClass
{
  GtkApplicationClass __parent__;
};

struct _MousepadApplication
{
  GtkApplication      __parent__;

  /* preferences dialog related */
  GtkWidget                   *prefs_dialog;
  GtkSourceSpaceLocationFlags  space_location_flags;

  /* command line options */
  gint             opening_mode;
  MousepadEncoding encoding;
};

/* MousepadApplication properties */
enum
{
  PROP_SPACE_LOCATION=1,
  N_PROPERTIES
};



/* command line options */
static const GOptionEntry option_entries[] =
{
  {
    "encoding", 'e', G_OPTION_FLAG_OPTIONAL_ARG,
    G_OPTION_ARG_CALLBACK, mousepad_application_parse_encoding,
    N_("Encoding to be used to open files (leave empty to open files in the encoding dialog)"),
    N_("ENCODING")
  },
  {
    "list-encodings", '\0', G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE, NULL,
    N_("Display a list of possible encodings to open files"), NULL
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

/* opening mode provided on the command line */
enum
{
  TAB = 0,
  WINDOW,
  MIXED
};



/* application actions */

/* stateless actions */
static const GActionEntry stateless_actions[] =
{
  /* command line options */
  { "quit", mousepad_application_action_quit, NULL, NULL, NULL }
};
#define N_STATELESS G_N_ELEMENTS (stateless_actions)

/* preferences dialog */
static const GActionEntry dialog_actions[] =
{
  /* "View" tab */
  { MOUSEPAD_SETTING_SHOW_LINE_NUMBERS, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_SHOW_WHITESPACE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_SHOW_LINE_ENDINGS, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_SHOW_RIGHT_MARGIN, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_HIGHLIGHT_CURRENT_LINE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_MATCH_BRACES, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_WORD_WRAP, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_USE_DEFAULT_FONT, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_COLOR_SCHEME, mousepad_application_radio_activate, "s", "'none'", NULL },

  /* "Editor" tab */
  { MOUSEPAD_SETTING_INSERT_SPACES, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_AUTO_INDENT, mousepad_application_toggle_activate, NULL, "false", NULL },

  /* "Window" tab */
  { MOUSEPAD_SETTING_STATUSBAR_VISIBLE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_PATH_IN_TITLE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_REMEMBER_SIZE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_REMEMBER_POSITION, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_REMEMBER_STATE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_TOOLBAR_VISIBLE, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_ALWAYS_SHOW_TABS, mousepad_application_toggle_activate, NULL, "false", NULL },
  { MOUSEPAD_SETTING_CYCLE_TABS, mousepad_application_toggle_activate, NULL, "false", NULL }
};
#define N_DIALOG G_N_ELEMENTS (dialog_actions)

/* settings only accessible from the menubar */
static const GActionEntry menubar_actions[] =
{
  { MOUSEPAD_SETTING_SEARCH_HIGHLIGHT_ALL, mousepad_application_toggle_activate, NULL, "false", NULL }
};
#define N_MENUBAR G_N_ELEMENTS (menubar_actions)

/* whitespace location settings, only accessible from GSettings */
static const GActionEntry whitespace_actions[] =
{
  { MOUSEPAD_SETTING_SHOW_WHITESPACE_LEADING, mousepad_application_toggle_activate, NULL,
    "false", mousepad_application_action_whitespace },
  { MOUSEPAD_SETTING_SHOW_WHITESPACE_INSIDE, mousepad_application_toggle_activate, NULL,
    "false", mousepad_application_action_whitespace },
  { MOUSEPAD_SETTING_SHOW_WHITESPACE_TRAILING, mousepad_application_toggle_activate, NULL,
    "false", mousepad_application_action_whitespace },
};
#define N_WHITESPACE G_N_ELEMENTS (whitespace_actions)

/* concatenate all setting actions */
static const GActionEntry* setting_actions[] =
{
  dialog_actions,
  menubar_actions,
  whitespace_actions
};
static const guint n_setting_actions[] =
{
  N_DIALOG,
  N_MENUBAR,
  N_WHITESPACE
};
#define N_SETTING G_N_ELEMENTS (n_setting_actions)



G_DEFINE_TYPE (MousepadApplication, mousepad_application, GTK_TYPE_APPLICATION)



static void
mousepad_application_class_init (MousepadApplicationClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  gobject_class->set_property = mousepad_application_set_property;
  gobject_class->get_property = mousepad_application_get_property;

  application_class->handle_local_options = mousepad_application_handle_local_options;
  application_class->startup = mousepad_application_startup;
  application_class->command_line = mousepad_application_command_line;
  application_class->activate = mousepad_application_activate;
  application_class->open = mousepad_application_open;
  application_class->shutdown = mousepad_application_shutdown;

  g_object_class_install_property (gobject_class, PROP_SPACE_LOCATION,
    g_param_spec_flags ("space-location", "SpaceLocation", "The space location setting",
                        GTK_SOURCE_TYPE_SPACE_LOCATION_FLAGS, GTK_SOURCE_SPACE_LOCATION_ALL,
                        G_PARAM_READWRITE));
}



static void
mousepad_application_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_SPACE_LOCATION:
      application->space_location_flags = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_application_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_SPACE_LOCATION:
      g_value_set_flags (value, application->space_location_flags);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_application_init (MousepadApplication *application)
{
  gchar *option_desc;

  /* initialize mousepad settings */
  mousepad_settings_init ();

  /* initialize application attributes */
  application->prefs_dialog = NULL;
  application->space_location_flags = GTK_SOURCE_SPACE_LOCATION_ALL;
  application->opening_mode = TAB;
  application->encoding = mousepad_encoding_get_default ();

  /* default application name */
  g_set_application_name (_("Mousepad"));

  /* use the Mousepad icon as default for new windows */
  gtk_window_set_default_icon_name (MOUSEPAD_ID);

  /* this option is added separately using g_strdup_printf() for the description, to be sure
   * that opening modes will be excluded from the translation (experience shows that using a
   * translation context is not enough to ensure it) */
  option_desc = g_strdup_printf (
    _("File opening mode: \"%s\", \"%s\" or \"%s\" (open tabs in a new window)"),
    "tab", "window", "mixed");
  g_application_add_main_option (G_APPLICATION (application), "opening-mode", 'o',
                                 G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, option_desc, _("MODE"));
  g_free (option_desc);

  /* add our option entries to the application */
  g_application_add_main_option_entries (G_APPLICATION (application), option_entries);
}



static gboolean
mousepad_application_parse_encoding (const gchar  *option_name,
                                     const gchar  *value,
                                     gpointer      data,
                                     GError      **error)
{
  MousepadApplication *application;

  application = MOUSEPAD_APPLICATION (g_application_get_default ());

  /* we will redirect the user to the encoding dialog */
  if (value == NULL)
    application->encoding = MOUSEPAD_ENCODING_NONE;
  else
    {
      /* try to find the encoding for the charset provided on the command line */
      application->encoding = mousepad_encoding_find (value);

      /* fallback to default if charset was invalid */
      if (application->encoding == MOUSEPAD_ENCODING_NONE)
        {
          g_printerr ("Invalid encoding '%s': ignored\n", value);
          application->encoding = mousepad_encoding_get_default ();
        }
    }

  return TRUE;
}



static gint
mousepad_application_handle_local_options (GApplication *gapplication,
                                           GVariantDict *options)
{
  MousepadEncoding   encoding;
  GApplicationFlags  flags;
  GError            *error = NULL;

  if (g_variant_dict_contains (options, "version"))
    {
      g_print ("%s %s\n\n", PACKAGE_NAME, PACKAGE_VERSION);
      g_print ("%s\n", "Copyright (C) 2005-2021");
      g_print ("\t%s\n\n", _("The Mousepad developers. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  if (g_variant_dict_contains (options, "list-encodings"))
    {
      for (encoding = 1; encoding < MOUSEPAD_N_ENCODINGS; encoding++)
        g_print ("%s\n", mousepad_encoding_get_charset (encoding));

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
mousepad_application_update_accels (GtkApplication  *application,
                                    const gchar     *accel_path,
                                    guint            accel_key,
                                    GdkModifierType  accel_mods,
                                    GtkAccelMap     *object)
{
  GVariant    *target;
  gchar       *action, *accel;
  const gchar *accels[] = { NULL, NULL };

  /* make sure to not crash because of corrupted data */
  if (g_str_has_prefix (accel_path, "<Actions>/") && g_strcmp0 (accel_path, "<Actions>/") != 0
      && g_action_parse_detailed_name (accel_path + 10, &action, &target, NULL))
    {
      accel = gtk_accelerator_name (accel_key, accel_mods);
      if (*accel != '\0')
        accels[0] = accel;

      gtk_application_set_accels_for_action (application, accel_path + 10, accels);

      /* cleanup */
      g_free (accel);
      g_free (action);
      if (target != NULL)
        g_variant_unref (target);
    }
}



static void
mousepad_application_complete_accel_map (GtkApplication *application)
{
  GtkWindow    *window;
  gchar        *accel_path, *filename;
  guint         n;
  gchar       **action_names;
  const gchar  *excluded_actions[] = { "win.insensitive" };

  /* disconnect this handler */
  mousepad_disconnect_by_func (application, mousepad_application_complete_accel_map, NULL);

  /* complete the accel map with window actions that do not have a default accel */
  window = gtk_application_get_active_window (application);
  action_names = g_action_group_list_actions (G_ACTION_GROUP (window));
  n = 0;
  while (action_names[n] != NULL)
    {
      /* add accel map entry to fill the accels file at shutdown */
      accel_path = g_strconcat ("<Actions>/win.", action_names[n], NULL);
      if (! gtk_accel_map_lookup_entry (accel_path, NULL))
        gtk_accel_map_add_entry (accel_path, 0, 0);

      g_free (accel_path);
      n++;
    }

  g_strfreev (action_names);

  /* prevent actions that should not have a (configurable) keybinding from being saved
   * in the accels file at shutdown */
  for (n = 0; n < G_N_ELEMENTS (excluded_actions); n++)
    {
      accel_path = g_strconcat ("<Actions>/", excluded_actions[n], NULL);
      gtk_accel_map_add_filter (accel_path);
      g_free (accel_path);
    }

  /* connect a handler to update accels when reading the accels file */
  g_signal_connect_swapped (gtk_accel_map_get (), "changed",
                            G_CALLBACK (mousepad_application_update_accels), application);

  /* read the accels file */
  filename = mousepad_util_get_save_location (MOUSEPAD_ACCELS_RELPATH, FALSE);
  if (G_LIKELY (filename != NULL))
    {
      gtk_accel_map_load (filename);
      g_free (filename);
    }
}



static void
mousepad_application_set_accels (MousepadApplication *application)
{
  GdkModifierType   accel_mods;
  guint             n, accel_key;
  gchar            *accel_path;
  gchar           **action_names;
  const gchar      *accels[] = { NULL, NULL };
  const gchar      *accel_maps[][2] =
  {
    /* increase/decrease font size from keyboard/mouse */
    { "win.increase-font-size", "<Control>plus" }, { "win.decrease-font-size", "<Control>minus" },

    /* "File" menu */
    { "win.file.new", "<Control>N" }, { "win.file.new-window", "<Control><Shift>N" },
    { "win.file.open", "<Control>O" }, { "win.file.save", "<Control>S" },
    { "win.file.save-as", "<Control><Shift>S" }, { "win.file.print", "<Control>P" },
    { "win.file.detach-tab", "<Control>D" }, { "win.file.close-tab", "<Control>W" },
    { "win.file.close-window", "<Control><Shift>W" }, { "app.quit", "<Control>Q" },

    /* "Edit" menu */
    { "win.edit.undo", "<Control>Z" }, { "win.edit.redo", "<Control>Y" },
    { "win.edit.cut", "<Control>X" }, { "win.edit.copy", "<Control>C" },
    { "win.edit.paste", "<Control>V" }, { "win.edit.delete-selection", "Delete" },
    { "win.edit.delete-line", "<Control><Shift>Delete" }, { "win.edit.select-all", "<Control>A" },
    { "win.edit.convert.to-opposite-case", "<Alt><Control>U" },
    { "win.edit.convert.transpose", "<Control>T" },
    { "win.edit.move-selection.line-up", "<Alt>Page_Up" },
    { "win.edit.move-selection.line-down", "<Alt>Page_Down" },
    { "win.edit.increase-indent", "<Control>I" }, { "win.edit.decrease-indent", "<Control>U" },

    /* "Search" menu */
    { "win.search.find", "<Control>F" }, { "win.search.find-next", "<Control>G" },
    { "win.search.find-previous", "<Control><Shift>G" },
    { "win.search.find-and-replace", "<Control>R" },
    { "win.search.go-to", "<Control>L" },

    /* "View" menu */
    { "win.preferences.window.menubar-visible", "<Control>M" }, { "win.view.fullscreen", "F11" },

    /* "Document" menu */
    { "win.document.previous-tab", "<Control>Page_Up" },
    { "win.document.next-tab", "<Control>Page_Down" },
    { "win.document.go-to-tab(0)", "<Alt>1" }, { "win.document.go-to-tab(1)", "<Alt>2" },
    { "win.document.go-to-tab(2)", "<Alt>3" }, { "win.document.go-to-tab(3)", "<Alt>4" },
    { "win.document.go-to-tab(4)", "<Alt>5" }, { "win.document.go-to-tab(5)", "<Alt>6" },
    { "win.document.go-to-tab(6)", "<Alt>7" }, { "win.document.go-to-tab(7)", "<Alt>8" },
    { "win.document.go-to-tab(8)", "<Alt>9" },

    /* "Help" menu */
    { "win.help.contents", "F1" }
  };

  /* actions that have a default accel */
  for (n = 0; n < G_N_ELEMENTS (accel_maps); n++)
    {
      /* add accel map entry to fill the accels file at shutdown */
      accel_path = g_strconcat ("<Actions>/", accel_maps[n][0], NULL);
      gtk_accelerator_parse (accel_maps[n][1], &accel_key, &accel_mods);
      gtk_accel_map_add_entry (accel_path, accel_key, accel_mods);
      g_free (accel_path);

      /* add accel to the application */
      accels[0] = accel_maps[n][1];
      gtk_application_set_accels_for_action (GTK_APPLICATION (application),
                                             accel_maps[n][0], accels);
    }

  /* actions that do not have a default accel */
  action_names = g_action_group_list_actions (G_ACTION_GROUP (application));
  n = 0;
  while (action_names[n] != NULL)
    {
      /* add accel map entry to fill the accels file at shutdown */
      accel_path = g_strconcat ("<Actions>/app.", action_names[n], NULL);
      if (! gtk_accel_map_lookup_entry (accel_path, NULL))
        gtk_accel_map_add_entry (accel_path, 0, 0);

      g_free (accel_path);
      n++;
    }

  g_strfreev (action_names);

  /* we will complete the accel map with window actions when the first window is set */
  g_signal_connect (application, "notify::active-window",
                    G_CALLBACK (mousepad_application_complete_accel_map), NULL);
}



static void
mousepad_application_startup (GApplication *gapplication)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (gapplication);
  GVariant            *state;
  GAction             *action;
  GMenu               *menu;
  guint                m, n;

  /* chain up to parent */
  G_APPLICATION_CLASS (mousepad_application_parent_class)->startup (gapplication);

  /* add application actions */
  g_action_map_add_action_entries (G_ACTION_MAP (application), stateless_actions,
                                   N_STATELESS, application);
  for (m = 0; m < N_SETTING; m++)
    g_action_map_add_action_entries (G_ACTION_MAP (application), setting_actions[m],
                                     n_setting_actions[m], application);

  /* associate flags to whitespace actions: to do after mapping and before initialization */
  for (m = 0; m < N_WHITESPACE; m++)
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (application), whitespace_actions[m].name);
      mousepad_object_set_data (action, "flag", GUINT_TO_POINTER (1 << m));
    }

  for (m = 0; m < N_SETTING; m++)
    for (n = 0; n < n_setting_actions[m]; n++)
      {
        /* sync the action state to its setting */
        mousepad_setting_connect_object (setting_actions[m][n].name,
                                         G_CALLBACK (mousepad_application_action_update),
                                         application, G_CONNECT_SWAPPED);

        /* initialize the action state */
        state = mousepad_setting_get_variant (setting_actions[m][n].name);
        g_action_group_change_action_state (G_ACTION_GROUP (application),
                                            setting_actions[m][n].name, state);
        g_variant_unref (state);
      }

  /* set shared menu parts in the application menus */
  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "shared-sections");
  mousepad_application_set_shared_menu_parts (application, G_MENU_MODEL (menu));

  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "tab-menu");
  mousepad_application_set_shared_menu_parts (application, G_MENU_MODEL (menu));

  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "textview-menu");
  mousepad_application_set_shared_menu_parts (application, G_MENU_MODEL (menu));

  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "toolbar");
  mousepad_application_set_shared_menu_parts (application, G_MENU_MODEL (menu));

  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (application), "menubar");
  mousepad_application_set_shared_menu_parts (application, G_MENU_MODEL (menu));

  /* set accels for actions */
  mousepad_application_set_accels (application);

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
      g_critical ("Failed to initialize xfconf: %s", error->message);
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
          g_application_command_line_printerr (command_line,
                                               "Invalid opening mode '%s': ignored\n",
                                               opening_mode);
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

  /* when opening mode is "tab", retrieve the last active MousepadWindow, if any */
  if (application->opening_mode == TAB)
    window = GTK_WIDGET (gtk_application_get_active_window (GTK_APPLICATION (application)));

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
  GFile               *valid_files[n_files];
  gchar               *uri;
  gint                 n, valid = 0, opened = 0;

  /* URIs may be invalid when entered by the user */
  for (n = 0; n < n_files; n++)
    if (mousepad_util_get_path (files[n]) == NULL)
      {
        /* inform the user */
        uri = g_file_get_uri (files[n]);
        g_message ("Invalid URI: %s", uri);
        g_free (uri);
      }
    else
      valid_files[valid++] = files[n];

  if (valid > 0)
    {
      /* open the files in tabs */
      if (application->opening_mode != WINDOW)
        {
          /* get the window to open the files */
          window = mousepad_application_get_window_for_files (application);

          /* open the files */
          opened = mousepad_window_open_files (MOUSEPAD_WINDOW (window), valid_files, valid,
                                               application->encoding, FALSE);

          /* if at least one file was finally opened, show the window */
          if (opened > 0)
            gtk_widget_show (window);
          else if (GTK_IS_WIDGET (window))
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
              opened = mousepad_window_open_files (MOUSEPAD_WINDOW (window), valid_files + n, 1,
                                                   application->encoding, FALSE);

              /* if the file was finally opened, show the window */
              if (opened > 0)
                gtk_widget_show (window);
              else if (GTK_IS_WIDGET (window))
                gtk_widget_destroy (window);
            }
        }
    }
}



static void
mousepad_application_shutdown (GApplication *gapplication)
{
  MousepadApplication *application = MOUSEPAD_APPLICATION (gapplication);
  GList               *windows, *window;
  gchar               *filename;

  /* save the current accel map */
  filename = mousepad_util_get_save_location (MOUSEPAD_ACCELS_RELPATH, TRUE);
  if (G_LIKELY (filename != NULL))
    {
      gtk_accel_map_save (filename);
      g_free (filename);
    }

  /* flush the history items of the replace dialog
   * this is a bit of an ugly place, but cleaning on a window close
   * isn't a good option eighter */
  mousepad_replace_dialog_history_clean ();

  /* destroy the preferences dialog */
  if (GTK_IS_WIDGET (application->prefs_dialog))
    gtk_widget_destroy (application->prefs_dialog);

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
  g_signal_connect (window, "new-window-with-document",
                    G_CALLBACK (mousepad_application_new_window_with_document), application);
  g_signal_connect (window, "new-window",
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

      /* update window dependent menu items */
      mousepad_window_update_window_menu_items (MOUSEPAD_WINDOW (app_windows->data));
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
  gtk_widget_destroy (application->prefs_dialog);
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
mousepad_application_update_menu (GMenuModel *shared_menu,
                                  gint        position,
                                  gint        removed,
                                  gint        added,
                                  GMenuModel *model)
{
  GMenuItem *item;
  gint       n, index;

  /* update the target menu when shared menu items are added or removed: some of them may
   * be temporarily added/removed during an update process, of permanently added/removed */
  for (n = 0; n < removed; n++)
    {
      index = position + n;
      item = g_menu_item_new_from_model (shared_menu, index);
      g_menu_remove (G_MENU (model), index);
      g_object_unref (item);
    }

  for (n = 0; n < added; n++)
    {
      index = position + n;
      item = g_menu_item_new_from_model (shared_menu, index);
      g_menu_insert_item (G_MENU (model), index, item);
      g_object_unref (item);
    }
}



static void
mousepad_application_update_menu_item (GMenuModel *shared_item,
                                       gint        position,
                                       gint        removed,
                                       gint        added,
                                       GMenuModel *model)
{
  GMenuItem *item;
  GVariant  *value;
  gint       n;

  /* update the target menu item only when the shared menu item is added, that is at the end
   * of its own update: an isolated shared menu item is not supposed to be permanently removed */
  if (added)
    {
      value = g_menu_model_get_item_attribute_value (shared_item, 0, "item-share-id",
                                                     G_VARIANT_TYPE_STRING);
      n = GPOINTER_TO_INT (mousepad_object_get_data (model, g_variant_get_string (value, NULL)));
      g_variant_unref (value);

      item = g_menu_item_new_from_model (shared_item, 0);
      g_menu_remove (G_MENU (model), n);
      g_menu_insert_item (G_MENU (model), n, item);
      g_object_unref (item);
    }
}



static void
mousepad_application_set_shared_menu_parts (MousepadApplication *application,
                                            GMenuModel          *model)
{
  GMenuModel  *section, *shared_menu, *shared_item, *submodel;
  GVariant    *value;
  const gchar *share_id;
  gint         n;

  for (n = 0; n < g_menu_model_get_n_items (model); n++)
    {
      /* section GMenuItem with a share id: insert the shared menu */
      if ((section = g_menu_model_get_item_link (model, n, G_MENU_LINK_SECTION))
          && ((value = g_menu_model_get_item_attribute_value (model, n, "section-share-id",
                                                              G_VARIANT_TYPE_STRING))))
        {
          share_id = g_variant_get_string (value, NULL);
          g_variant_unref (value);

          shared_menu = G_MENU_MODEL (gtk_application_get_menu_by_id (
                                        GTK_APPLICATION (application), share_id));
          mousepad_application_update_menu (shared_menu, 0, 0,
                                            g_menu_model_get_n_items (shared_menu), section);

          g_signal_connect_object (shared_menu, "items-changed",
                                   G_CALLBACK (mousepad_application_update_menu), section, 0);
        }
      /* section GMenuItem without a share id: go ahead recursively */
      else if (section != NULL)
        mousepad_application_set_shared_menu_parts (application, section);
      /* real GMenuItem */
      else
        {
          /* set the menu item when shared */
          if ((value = g_menu_model_get_item_attribute_value (model, n, "item-share-id",
                                                              G_VARIANT_TYPE_STRING)))
            {
              share_id = g_variant_get_string (value, NULL);
              g_variant_unref (value);

              shared_item = G_MENU_MODEL (gtk_application_get_menu_by_id (
                                            GTK_APPLICATION (application), share_id));
              mousepad_object_set_data (model, g_intern_string (share_id), GINT_TO_POINTER (n));
              mousepad_application_update_menu_item (shared_item, 0, 0, 1, model);

              g_signal_connect_object (shared_item, "items-changed",
                                       G_CALLBACK (mousepad_application_update_menu_item), model, 0);
            }

          /* submenu GMenuItem with a share id: insert the shared menu */
          if ((submodel = g_menu_model_get_item_link (model, n, G_MENU_LINK_SUBMENU))
              && ((value = g_menu_model_get_item_attribute_value (model, n, "submenu-share-id",
                                                                  G_VARIANT_TYPE_STRING))))
            {
              share_id = g_variant_get_string (value, NULL);
              g_variant_unref (value);

              shared_menu = G_MENU_MODEL (gtk_application_get_menu_by_id (
                                            GTK_APPLICATION (application), share_id));
              mousepad_application_update_menu (shared_menu, 0, 0,
                                                g_menu_model_get_n_items (shared_menu), submodel);

              g_signal_connect_object (shared_menu, "items-changed",
                                       G_CALLBACK (mousepad_application_update_menu), submodel, 0);
            }
          /* submenu GMenuItem without a share id: go ahead recursively */
          else if (submodel != NULL)
            mousepad_application_set_shared_menu_parts (application, submodel);
        }
    }
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

  sections = mousepad_util_get_sorted_language_sections ();

  for (iter_sect = sections; iter_sect != NULL; iter_sect = g_slist_next (iter_sect))
    {
      /* create submenu item */
      label = iter_sect->data;
      submenu = g_menu_new ();
      item = g_menu_item_new_submenu (label, G_MENU_MODEL (submenu));

      /* set tooltip and append menu item */
      tooltip = iter_sect->data;
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
          tooltip = g_strdup_printf ("%s/%s", (gchar *) iter_sect->data, label);
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

  schemes = mousepad_util_get_sorted_style_schemes ();

  for (iter = schemes; iter != NULL; iter = g_slist_next (iter))
    {
      /* create menu item */
      label = gtk_source_style_scheme_get_id (iter->data);
      action_name = g_strconcat ("app.preferences.view.color-scheme('", label, "')", NULL);
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
  GList    *windows, *window;
  GAction  *close_action;
  GVariant *v_succeed;
  gboolean  succeed;

  /* try to close all windows, abort at the first failure */
  windows = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (data)));
  for (window = windows; window != NULL; window = window->next)
    {
      close_action = g_action_map_lookup_action (G_ACTION_MAP (window->data), "file.close-window");
      g_action_activate (close_action, NULL);
      v_succeed = g_action_get_state (close_action);
      succeed = g_variant_get_int32 (v_succeed);
      g_variant_unref (v_succeed);
      if (! succeed)
        break;
    }

  g_list_free (windows);
}



static void
mousepad_application_toggle_activate (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       data)
{
  gboolean state;

  /* save the setting */
  state = ! g_variant_get_boolean (g_action_get_state (G_ACTION (action)));
  mousepad_setting_set_boolean (g_action_get_name (G_ACTION (action)), state);
}



static void
mousepad_application_radio_activate (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       data)
{
  /* save the setting */
  mousepad_setting_set_variant (g_action_get_name (G_ACTION (action)), parameter);
}



static void
mousepad_application_action_update (MousepadApplication *application,
                                    gchar               *key,
                                    GSettings           *settings)
{
  GVariant *state;
  gchar    *schema_id, *action_name;

  /* retrieve the action name from the setting schema id */
  g_object_get (settings, "schema_id", &schema_id, NULL);
  action_name = g_strdup_printf ("%s.%s", schema_id + MOUSEPAD_ID_LEN + 1, key);

  /* request for the action state to be changed according to the setting */
  state = mousepad_setting_get_variant (action_name);
  g_action_group_change_action_state (G_ACTION_GROUP (application), action_name, state);

  /* cleanup */
  g_free (schema_id);
  g_free (action_name);
  g_variant_unref (state);
}



/*
 * This job is done only once here, when whitespace location settings are updated,
 * then all the views update their own location flags from those of the application.
 */
static void
mousepad_application_action_whitespace (GSimpleAction *action,
                                        GVariant      *state,
                                        gpointer       data)
{
  GtkSourceSpaceLocationFlags flags, flag;

  /* change action state */
  g_simple_action_set_state (action, state);

  /* update the space location flags */
  flags = MOUSEPAD_APPLICATION (data)->space_location_flags;
  flag = GPOINTER_TO_UINT (mousepad_object_get_data (action, "flag"));
  g_variant_get_boolean (state) ? (flags |= flag) : (flags &= ~flag);

  /* set the application property */
  g_object_set (data, "space-location", flags, NULL);
}
