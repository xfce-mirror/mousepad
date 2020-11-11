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
#include <mousepad/mousepad-marshal.h>
#include <mousepad/mousepad-document.h>
#include <mousepad/mousepad-dialogs.h>
#include <mousepad/mousepad-replace-dialog.h>
#include <mousepad/mousepad-encoding-dialog.h>
#include <mousepad/mousepad-search-bar.h>
#include <mousepad/mousepad-statusbar.h>
#include <mousepad/mousepad-print.h>
#include <mousepad/mousepad-window.h>
#include <mousepad/mousepad-util.h>

#include <glib/gstdio.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif



#define PADDING                   (2)
#define PASTE_HISTORY_MENU_LENGTH (30)

#define MENUBAR        MOUSEPAD_SETTING_MENUBAR_VISIBLE
#define STATUSBAR      MOUSEPAD_SETTING_STATUSBAR_VISIBLE
#define TOOLBAR        MOUSEPAD_SETTING_TOOLBAR_VISIBLE
#define NOTEBOOK_GROUP "Mousepad"



enum
{
  PROP_SEARCH_WIDGET_VISIBLE=1,
  N_PROPERTIES
};

enum
{
  NEW_WINDOW,
  NEW_WINDOW_WITH_DOCUMENT,
  SEARCH_COMPLETED,
  LAST_SIGNAL
};

enum
{
  AUTO = 0,
  NO,
  YES
};



/* overridden parent classes methods */
static void              mousepad_window_set_property                 (GObject                *object,
                                                                       guint                   prop_id,
                                                                       const GValue           *value,
                                                                       GParamSpec             *pspec);
static void              mousepad_window_get_property                 (GObject                *object,
                                                                       guint                   prop_id,
                                                                       GValue                 *value,
                                                                       GParamSpec             *pspec);
static void              mousepad_window_finalize                     (GObject                *object);

static gboolean          mousepad_window_configure_event              (GtkWidget              *widget,
                                                                       GdkEventConfigure      *event);
static gboolean          mousepad_window_delete_event                 (GtkWidget              *widget,
                                                                       GdkEventAny            *event);
static gboolean          mousepad_window_window_state_event           (GtkWidget              *widget,
                                                                       GdkEventWindowState    *event);

/* statusbar tooltips */
static void              mousepad_window_menu_set_tooltips            (MousepadWindow         *window,
                                                                       GtkWidget              *menu,
                                                                       GMenuModel             *model,
                                                                       gint                   *offset);
static void              mousepad_window_menu_item_selected           (GtkWidget              *menu_item,
                                                                       MousepadWindow         *window);
static void              mousepad_window_menu_item_deselected         (GtkWidget              *menu_item,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_tool_item_enter_event        (GtkWidget              *tool_item,
                                                                       GdkEvent               *event,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_tool_item_leave_event        (GtkWidget              *tool_item,
                                                                       GdkEvent               *event,
                                                                       MousepadWindow         *window);

/* window functions */
static gboolean          mousepad_window_open_file                    (MousepadWindow         *window,
                                                                       const gchar            *filename,
                                                                       MousepadEncoding        encoding);
static gboolean          mousepad_window_close_document               (MousepadWindow         *window,
                                                                       MousepadDocument       *document);
static void              mousepad_window_button_close_tab             (MousepadDocument       *document,
                                                                       MousepadWindow         *window);
static void              mousepad_window_set_title                    (MousepadWindow         *window);
static gboolean          mousepad_window_get_in_fullscreen            (MousepadWindow         *window);
static void              mousepad_window_update_bar_visibility        (MousepadWindow         *window,
                                                                       const gchar            *key);

/* notebook signals */
static void              mousepad_window_notebook_switch_page         (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       guint                   page_num,
                                                                       MousepadWindow         *window);
static void              mousepad_window_notebook_added               (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       guint                   page_num,
                                                                       MousepadWindow         *window);
static void              mousepad_window_notebook_removed             (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       guint                   page_num,
                                                                       MousepadWindow         *window);
static gboolean          mousepad_window_notebook_button_release_event (GtkNotebook           *notebook,
                                                                        GdkEventButton        *event,
                                                                        MousepadWindow        *window);
static gboolean          mousepad_window_notebook_button_press_event  (GtkNotebook            *notebook,
                                                                       GdkEventButton         *event,
                                                                       MousepadWindow         *window);
static GtkNotebook      *mousepad_window_notebook_create_window       (GtkNotebook            *notebook,
                                                                       GtkWidget              *page,
                                                                       gint                    x,
                                                                       gint                    y,
                                                                       MousepadWindow         *window);

/* document signals */
static void              mousepad_window_modified_changed             (MousepadWindow         *window);
static void              mousepad_window_readonly_changed             (MousepadWindow         *window);
static void              mousepad_window_cursor_changed               (MousepadDocument       *document,
                                                                       gint                    line,
                                                                       gint                    column,
                                                                       gint                    selection,
                                                                       MousepadWindow         *window);
static void              mousepad_window_selection_changed            (MousepadDocument       *document,
                                                                       gint                    selection,
                                                                       MousepadWindow         *window);
static void              mousepad_window_overwrite_changed            (MousepadDocument       *document,
                                                                       gboolean                overwrite,
                                                                       MousepadWindow         *window);
static void              mousepad_window_buffer_language_changed      (MousepadDocument       *document,
                                                                       GtkSourceLanguage      *language,
                                                                       MousepadWindow         *window);
static void              mousepad_window_can_undo                     (MousepadWindow         *window,
                                                                       GParamSpec             *unused,
                                                                       GObject                *buffer);
static void              mousepad_window_can_redo                     (MousepadWindow         *window,
                                                                       GParamSpec             *unused,
                                                                       GObject                *buffer);

/* menu functions */
static void              mousepad_window_menu_templates_fill          (MousepadWindow         *window,
                                                                       GMenu                  *menu,
                                                                       const gchar            *path);
static void              mousepad_window_menu_templates               (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_menu_tab_sizes_update        (MousepadWindow         *window);
static void              mousepad_window_menu_textview_popup          (GtkTextView            *textview,
                                                                       GtkMenu                *old_menu,
                                                                       MousepadWindow         *window);
static void              mousepad_window_update_actions               (MousepadWindow         *window);
static void              mousepad_window_update_gomenu                (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_update_tabs                  (MousepadWindow         *window,
                                                                       gchar                  *key,
                                                                       GSettings              *settings);

/* recent functions */
static void              mousepad_window_recent_add                   (MousepadWindow         *window,
                                                                       MousepadFile           *file);
static gint              mousepad_window_recent_sort                  (GtkRecentInfo          *a,
                                                                       GtkRecentInfo          *b);
static void              mousepad_window_recent_manager_init          (MousepadWindow         *window);
static void              mousepad_window_recent_menu                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static const gchar      *mousepad_window_recent_get_charset           (GtkRecentInfo          *info);
static void              mousepad_window_recent_clear                 (MousepadWindow         *window);

/* dnd */
static void              mousepad_window_drag_data_received           (GtkWidget              *widget,
                                                                       GdkDragContext         *context,
                                                                       gint                    x,
                                                                       gint                    y,
                                                                       GtkSelectionData       *selection_data,
                                                                       guint                   info,
                                                                       guint                   drag_time,
                                                                       MousepadWindow         *window);

/* find and replace */
static void              mousepad_window_search                       (MousepadWindow         *window,
                                                                       MousepadSearchFlags     flags,
                                                                       const gchar            *string,
                                                                       const gchar            *replacement);
static void              mousepad_window_search_completed             (MousepadWindow         *window,
                                                                       gint                    n_matches_doc,
                                                                       const gchar            *string,
                                                                       MousepadSearchFlags     flags,
                                                                       MousepadDocument       *document);

/* history clipboard functions */
static void              mousepad_window_paste_history_add            (MousepadWindow         *window);
#if !GTK_CHECK_VERSION (3, 22, 0)
static void              mousepad_window_paste_history_menu_position  (GtkMenu                *menu,
                                                                       gint                   *x,
                                                                       gint                   *y,
                                                                       gboolean               *push_in,
                                                                       gpointer                user_data);
#endif
static void              mousepad_window_paste_history_activate       (GtkMenuItem            *item,
                                                                       MousepadWindow         *window);
static GtkWidget        *mousepad_window_paste_history_menu_item      (const gchar            *text,
                                                                       const gchar            *mnemonic);
static GtkWidget        *mousepad_window_paste_history_menu           (MousepadWindow         *window);

/* actions */
static void              mousepad_window_action_new                   (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_new_window            (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_new_from_template     (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_open                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_open_recent           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_clear_recent          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_save                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_save_as               (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_save_all              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_reload                (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_print                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_detach                (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_close                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_close_window          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_undo                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_redo                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_cut                   (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_copy                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_paste                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_paste_history         (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_paste_column          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_delete                (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_select_all            (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_preferences           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_lowercase             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_uppercase             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_titlecase             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_opposite_case         (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_tabs_to_spaces        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_spaces_to_tabs        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_strip_trailing_spaces (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_transpose             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_move_line_up          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_move_line_down        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_duplicate             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_increase_indent       (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_decrease_indent       (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_find                  (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_find_next             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_find_previous         (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_replace               (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_go_to_position        (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_select_font           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_bar_activate          (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_textview              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_menubar_state         (GSimpleAction          *action,
                                                                       GVariant               *state,
                                                                       gpointer                data);
static void              mousepad_window_action_fullscreen            (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_line_ending           (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_tab_size              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_language              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_write_bom             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_prev_tab              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_next_tab              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_go_to_tab             (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_contents              (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);
static void              mousepad_window_action_about                 (GSimpleAction          *action,
                                                                       GVariant               *value,
                                                                       gpointer                data);



struct _MousepadWindowClass
{
  GtkApplicationWindowClass __parent__;
};

struct _MousepadWindow
{
  GtkApplicationWindow __parent__;

  /* the current active document */
  MousepadDocument    *active;

  /* main window widgets */
  GtkWidget           *box;
  GtkWidget           *menubar_box;
  GtkWidget           *menubar;
  GtkWidget           *toolbar;
  GtkWidget           *notebook;
  GtkWidget           *search_bar;
  GtkWidget           *statusbar;
  GtkWidget           *replace_dialog;

  /* contextual gtkmenus created from the application resources */
  GtkWidget           *textview_menu;
  GtkWidget           *tab_menu;
  GtkWidget           *languages_menu;

  /* menubar related */
  GtkRecentManager    *recent_manager;
  const gchar         *gtkmenu_key, *offset_key;

  /* search widgets related */
  gboolean             search_widget_visible;
};



/* menubar actions */
static const GActionEntry action_entries[] =
{
  /* to make menu items insensitive, when needed */
  { "insensitive", NULL, NULL, NULL, NULL },

  /* additional action for the "Textview" menu to show the menubar when hidden */
  { "textview.menubar", mousepad_window_action_textview, NULL, "false", NULL },

  /* "File" menu */
  { "file.new", mousepad_window_action_new, NULL, NULL, NULL },
  { "file.new-window", mousepad_window_action_new_window, NULL, NULL, NULL },
  { "file.new-from-template", NULL, NULL, "false", mousepad_window_menu_templates },
    { "file.new-from-template.new", mousepad_window_action_new_from_template, "s", NULL, NULL },

  { "file.open", mousepad_window_action_open, NULL, NULL, NULL },
  { "file.open-recent", NULL, NULL, "false", mousepad_window_recent_menu },
    { "file.open-recent.new", mousepad_window_action_open_recent, "s", NULL, NULL },
    { "file.open-recent.clear-history", mousepad_window_action_clear_recent, NULL, NULL, NULL },

  { "file.save", mousepad_window_action_save, NULL, "0", NULL },
  { "file.save-as", mousepad_window_action_save_as, NULL, "0", NULL },
  { "file.save-all", mousepad_window_action_save_all, NULL, NULL, NULL },
  { "file.reload", mousepad_window_action_reload, NULL, NULL, NULL },

  { "file.print", mousepad_window_action_print, NULL, NULL, NULL },

  { "file.detach-tab", mousepad_window_action_detach, NULL, NULL, NULL },

  { "file.close-tab", mousepad_window_action_close, NULL, NULL, NULL },
  { "file.close-window", mousepad_window_action_close_window, NULL, NULL, NULL },

  /* "Edit" menu */
  { "edit.undo", mousepad_window_action_undo, NULL, NULL, NULL },
  { "edit.redo", mousepad_window_action_redo, NULL, NULL, NULL },

  { "edit.cut", mousepad_window_action_cut, NULL, NULL, NULL },
  { "edit.copy", mousepad_window_action_copy, NULL, NULL, NULL },
  { "edit.paste", mousepad_window_action_paste, NULL, NULL, NULL },
  /* "Paste Special" submenu */
    { "edit.paste-special.paste-from-history", mousepad_window_action_paste_history, NULL, NULL, NULL },
    { "edit.paste-special.paste-as-column", mousepad_window_action_paste_column, NULL, NULL, NULL },
  { "edit.delete", mousepad_window_action_delete, NULL, NULL, NULL },

  { "edit.select-all", mousepad_window_action_select_all, NULL, NULL, NULL },

  /* "Convert" submenu */
    { "edit.convert.to-lowercase", mousepad_window_action_lowercase, NULL, NULL, NULL },
    { "edit.convert.to-uppercase", mousepad_window_action_uppercase, NULL, NULL, NULL },
    { "edit.convert.to-title-case", mousepad_window_action_titlecase, NULL, NULL, NULL },
    { "edit.convert.to-opposite-case", mousepad_window_action_opposite_case, NULL, NULL, NULL },

    { "edit.convert.tabs-to-spaces", mousepad_window_action_tabs_to_spaces, NULL, NULL, NULL },
    { "edit.convert.spaces-to-tabs", mousepad_window_action_spaces_to_tabs, NULL, NULL, NULL },

    { "edit.convert.strip-trailing-spaces", mousepad_window_action_strip_trailing_spaces, NULL, NULL, NULL },

    { "edit.convert.transpose", mousepad_window_action_transpose, NULL, NULL, NULL },
  /* "Move selection" submenu */
    { "edit.move-selection.line-up", mousepad_window_action_move_line_up, NULL, NULL, NULL },
    { "edit.move-selection.line-down", mousepad_window_action_move_line_down, NULL, NULL, NULL },
  { "edit.duplicate-line-selection", mousepad_window_action_duplicate, NULL, NULL, NULL },
  { "edit.increase-indent", mousepad_window_action_increase_indent, NULL, NULL, NULL },
  { "edit.decrease-indent", mousepad_window_action_decrease_indent, NULL, NULL, NULL },

  { "edit.preferences", mousepad_window_action_preferences, NULL, NULL, NULL },

  /* "Search" menu */
  { "search.find", mousepad_window_action_find, NULL, NULL, NULL },
  { "search.find-next", mousepad_window_action_find_next, NULL, NULL, NULL },
  { "search.find-previous", mousepad_window_action_find_previous, NULL, NULL, NULL },
  { "search.find-and-replace", mousepad_window_action_replace, NULL, NULL, NULL },

  { "search.go-to", mousepad_window_action_go_to_position, NULL, NULL, NULL },

  /* "View" menu */
  { "view.select-font", mousepad_window_action_select_font, NULL, NULL, NULL },
  { MENUBAR, mousepad_window_action_bar_activate, NULL, "false", mousepad_window_action_menubar_state },
  { TOOLBAR, mousepad_window_action_bar_activate, NULL, "false", NULL },
  { STATUSBAR, mousepad_window_action_bar_activate, NULL, "false", NULL },

  { "view.fullscreen", mousepad_window_action_fullscreen, NULL, "false", NULL },

  /* "Document" menu */
  { "document", NULL, NULL, "false", mousepad_window_update_gomenu },
  /* "Tab Size" submenu */
    { "document.tab.tab-size", mousepad_window_action_tab_size, "i", "2", NULL },

  /* "Filetype" submenu */
    { "document.filetype", mousepad_window_action_language, "s", "'plain-text'", NULL },
  /* "Line Ending" submenu */
    { "document.line-ending", mousepad_window_action_line_ending, "i", "0", NULL },

  { "document.write-unicode-bom", mousepad_window_action_write_bom, NULL, "false", NULL },

  { "document.previous-tab", mousepad_window_action_prev_tab, NULL, NULL, NULL },
  { "document.next-tab", mousepad_window_action_next_tab, NULL, NULL, NULL },

  { "document.go-to-tab", mousepad_window_action_go_to_tab, "i", "0", NULL },

  /* "Help" menu */
  { "help.contents", mousepad_window_action_contents, NULL, NULL, NULL },
  { "help.about", mousepad_window_action_about, NULL, NULL, NULL }
};



/* global variables */
static guint   window_signals[LAST_SIGNAL];
static gint    lock_menu_updates = 0;
static GSList *clipboard_history = NULL;
static guint   clipboard_history_ref_count = 0;
static gchar  *last_save_location = NULL;
static guint   last_save_location_ref_count = 0;



G_DEFINE_TYPE (MousepadWindow, mousepad_window, GTK_TYPE_APPLICATION_WINDOW)



GtkWidget *
mousepad_window_new (MousepadApplication *application)
{
  return g_object_new (MOUSEPAD_TYPE_WINDOW, "application", GTK_APPLICATION (application), NULL);
}



static void
mousepad_window_class_init (MousepadWindowClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = mousepad_window_set_property;
  gobject_class->get_property = mousepad_window_get_property;
  gobject_class->finalize = mousepad_window_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->configure_event = mousepad_window_configure_event;
  gtkwidget_class->delete_event = mousepad_window_delete_event;
  gtkwidget_class->window_state_event = mousepad_window_window_state_event;

  window_signals[NEW_WINDOW] =
    g_signal_new (I_("new-window"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  window_signals[NEW_WINDOW_WITH_DOCUMENT] =
    g_signal_new (I_("new-window-with-document"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__OBJECT_INT_INT,
                  G_TYPE_NONE, 3,
                  G_TYPE_OBJECT,
                  G_TYPE_INT, G_TYPE_INT);

  window_signals[SEARCH_COMPLETED] =
    g_signal_new (I_("search-completed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _mousepad_marshal_VOID__INT_STRING_FLAGS,
                  G_TYPE_NONE, 3, G_TYPE_INT, G_TYPE_STRING,
                  MOUSEPAD_TYPE_SEARCH_FLAGS);

  g_object_class_install_property (gobject_class, PROP_SEARCH_WIDGET_VISIBLE,
    g_param_spec_boolean ("search-widget-visible", "SearchWidgetVisible",
                          "At least one search widget is visible or not",
                          FALSE, G_PARAM_READWRITE));
}



static void
mousepad_window_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SEARCH_WIDGET_VISIBLE:
      window->search_widget_visible = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_window_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SEARCH_WIDGET_VISIBLE:
      g_value_set_boolean (value, window->search_widget_visible);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
mousepad_window_finalize (GObject *object)
{
  /* decrease history clipboard ref count */
  clipboard_history_ref_count--;

  /* decrease last save location ref count */
  last_save_location_ref_count--;

  /* free clipboard history if needed */
  if (clipboard_history_ref_count == 0 && clipboard_history != NULL)
    g_slist_free_full (clipboard_history, g_free);

  /* free last save location if needed */
  if (last_save_location_ref_count == 0 && last_save_location != NULL)
    {
      g_free (last_save_location);
      last_save_location = NULL;
    }

  (*G_OBJECT_CLASS (mousepad_window_parent_class)->finalize) (object);
}



/* Called in response to any settings changed which affect the statusbar labels. */
static void
mousepad_window_update_statusbar_settings (MousepadWindow *window,
                                           gchar          *key,
                                           GSettings      *settings)
{
  if (G_LIKELY (MOUSEPAD_IS_DOCUMENT (window->active)))
    mousepad_document_send_signals (window->active);
}



/* Called when always-show-tabs setting changes to update the UI. */
static void
mousepad_window_update_tabs (MousepadWindow *window,
                             gchar          *key,
                             GSettings      *settings)
{
  gint     n_pages;
  gboolean always_show;

  always_show = MOUSEPAD_SETTING_GET_BOOLEAN (ALWAYS_SHOW_TABS);

  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->notebook),
                              (n_pages > 1 || always_show) ? TRUE : FALSE);
}



static void
mousepad_window_update_toolbar (MousepadWindow *window,
                                gchar          *key,
                                GSettings      *settings)
{
  GtkToolbarStyle style;
  GtkIconSize     size;

  style = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_STYLE);
  size = MOUSEPAD_SETTING_GET_ENUM (TOOLBAR_ICON_SIZE);

  gtk_toolbar_set_style (GTK_TOOLBAR (window->toolbar), style);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (window->toolbar), size);
}



static void
mousepad_window_restore_geometry (MousepadWindow *window)
{
  gboolean remember_size, remember_position, remember_state;

  remember_size = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_SIZE);
  remember_position = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_POSITION);
  remember_state = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_STATE);

  /* first restore size */
  if (remember_size)
    {
      gint width, height;

      width = MOUSEPAD_SETTING_GET_INT (WINDOW_WIDTH);
      height = MOUSEPAD_SETTING_GET_INT (WINDOW_HEIGHT);

      gtk_window_set_default_size (GTK_WINDOW (window), width, height);
    }

  /* then restore position */
  if (remember_position)
    {
      gint left, top;

      left = MOUSEPAD_SETTING_GET_INT (WINDOW_LEFT);
      top = MOUSEPAD_SETTING_GET_INT (WINDOW_TOP);

      gtk_window_move (GTK_WINDOW (window), left, top);
    }

  /* finally restore window state */
  if (remember_state)
    {
      gboolean maximized, fullscreen;

      maximized = MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_MAXIMIZED);
      fullscreen = MOUSEPAD_SETTING_GET_BOOLEAN (WINDOW_FULLSCREEN);

      /* first restore if it was maximized */
      if (maximized)
        gtk_window_maximize (GTK_WINDOW (window));

      /* then restore if it was fullscreen and update action state accordingly */
      if (fullscreen)
        g_action_group_activate_action (G_ACTION_GROUP (window), "view.fullscreen", NULL);
    }
}



static void
mousepad_window_post_init (MousepadWindow *window)
{
  GtkApplication *application;
  GMenuModel     *model;
  gchar          *gtkmenu_key, *offset_key;
  gint            window_id;

  /* disconnect this handler */
  mousepad_disconnect_by_func (window, mousepad_window_post_init, NULL);

  /* hide the default menubar */
  gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (window), FALSE);

  /*
   * Outsource the creation of the menubar from
   * gtk/gtk/gtkapplicationwindow.c:gtk_application_window_update_menubar(), to make the menubar
   * a window attribute, and be able to access its items to show their tooltips in the statusbar.
   * With GTK+ 3, this leads to use gtk_menu_bar_new_from_model()
   * With GTK+ 4, this will lead to use gtk_popover_menu_bar_new_from_model()
   */
  application = gtk_window_get_application (GTK_WINDOW (window));
  model = gtk_application_get_menubar (application);
  window->menubar = gtk_menu_bar_new_from_model (model);

  /* set the unique menu and offset keys for this window */
  window_id = gtk_application_window_get_id (GTK_APPLICATION_WINDOW (window));
  gtkmenu_key = g_strdup_printf ("gtkmenu-%d", window_id);
  offset_key = g_strdup_printf ("offset-%d", window_id);
  window->gtkmenu_key = g_intern_string (gtkmenu_key);
  window->offset_key = g_intern_string (offset_key);
  g_free (gtkmenu_key);
  g_free (offset_key);

  /* set tooltips and connect handlers to the menubar items signals */
  mousepad_window_menu_set_tooltips (window, window->menubar, model, NULL);

  /* insert the menubar in its previously reserved space */
  gtk_box_pack_start (GTK_BOX (window->menubar_box), window->menubar, TRUE, TRUE, 0);

  /* create textview menu and set tooltips */
  model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "textview-menu"));
  window->textview_menu = gtk_menu_new_from_model (model);
  gtk_menu_attach_to_widget (GTK_MENU (window->textview_menu),
                             GTK_WIDGET (window), NULL);
  mousepad_window_menu_set_tooltips (window, window->textview_menu, model, NULL);

  /* create tab menu and set tooltips */
  model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "tab-menu"));
  window->tab_menu = gtk_menu_new_from_model (model);
  gtk_menu_attach_to_widget (GTK_MENU (window->tab_menu),
                             GTK_WIDGET (window), NULL);
  mousepad_window_menu_set_tooltips (window, window->tab_menu, model, NULL);

  /* create languages menu and set tooltips */
  model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "document.filetype"));
  window->languages_menu = gtk_menu_new_from_model (model);
  gtk_menu_attach_to_widget (GTK_MENU (window->languages_menu),
                             GTK_WIDGET (window), NULL);
  mousepad_window_menu_set_tooltips (window, window->languages_menu, model, NULL);

  /* restore window geometry settings */
  mousepad_window_restore_geometry (window);

  /* update the menubar visibility and related actions state */
  mousepad_window_update_bar_visibility (window, MENUBAR);

  /* connect to some signals to keep in sync */
  MOUSEPAD_SETTING_CONNECT_OBJECT (MENUBAR_VISIBLE,
                                   G_CALLBACK (mousepad_window_update_bar_visibility),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (MENUBAR_VISIBLE_FULLSCREEN,
                                   G_CALLBACK (mousepad_window_update_bar_visibility),
                                   window, G_CONNECT_SWAPPED);
}



static void
mousepad_window_toolbar_insert (MousepadWindow *window,
                                const gchar    *label,
                                const gchar    *icon_name,
                                const gchar    *tooltip,
                                const gchar    *action_name)
{
  GtkToolItem *item;
  GtkWidget   *child;

  item = gtk_tool_button_new (NULL, label);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), icon_name);
  gtk_tool_item_set_tooltip_text (item, tooltip);

  /* make the widget actionable just as the corresponding menu item */
  gtk_actionable_set_action_name (GTK_ACTIONABLE (item), action_name);

  /* tool items will have GtkButton or other widgets in them, we want the child */
  child = gtk_bin_get_child (GTK_BIN (item));

  /* get events for mouse enter/leave and focus in/out */
  gtk_widget_add_events (child,
                         GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK
                         | GDK_FOCUS_CHANGE_MASK);

  /* connect to signals for the events to show the tooltip in the status bar */
  g_signal_connect_object (child, "enter-notify-event",
                           G_CALLBACK (mousepad_window_tool_item_enter_event),
                           window, 0);
  g_signal_connect_object (child, "leave-notify-event",
                           G_CALLBACK (mousepad_window_tool_item_leave_event),
                           window, 0);
  g_signal_connect_object (child, "focus-in-event",
                           G_CALLBACK (mousepad_window_tool_item_enter_event),
                           window, 0);
  g_signal_connect_object (child, "focus-out-event",
                           G_CALLBACK (mousepad_window_tool_item_leave_event),
                           window, 0);

  /* append the item to the end of the toolbar */
  gtk_toolbar_insert (GTK_TOOLBAR (window->toolbar), item, -1);
}



static void
mousepad_window_create_toolbar (MousepadWindow *window)
{
  GtkToolItem *item;

  /* create the toolbar and set the main properties */
  window->toolbar = gtk_toolbar_new ();
  gtk_toolbar_set_style (GTK_TOOLBAR (window->toolbar), GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (window->toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);

  /* insert items */
  mousepad_window_toolbar_insert (window, _("_New"), "document-new",
                                  _("Create a new document"), "win.file.new");
  mousepad_window_toolbar_insert (window, _("_Open..."), "document-open",
                                  _("Open a file"), "win.file.open");
  mousepad_window_toolbar_insert (window, _("_Save"), "document-save",
                                  _("Save the current document"), "win.file.save");
  mousepad_window_toolbar_insert (window, _("Save _As..."), "document-save-as",
                                  _("Save current document as another file"), "win.file.save-as");
  mousepad_window_toolbar_insert (window, _("Re_load"), "view-refresh",
                                  _("Reload file from disk"), "win.file.reload");
  mousepad_window_toolbar_insert (window, _("Close _Tab"), "window-close",
                                  _("Close the current document"), "win.file.close-tab");

  item = gtk_separator_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (window->toolbar), item, -1);

  mousepad_window_toolbar_insert (window, _("_Undo"), "edit-undo",
                                  _("Undo the last action"), "win.edit.undo");
  mousepad_window_toolbar_insert (window, _("_Redo"), "edit-redo",
                                  _("Redo the last undone action"), "win.edit.redo");
  mousepad_window_toolbar_insert (window, _("Cu_t"), "edit-cut",
                                  _("Cut the selection"), "win.edit.cut");
  mousepad_window_toolbar_insert (window, _("_Copy"), "edit-copy",
                                  _("Copy the selection"), "win.edit.copy");
  mousepad_window_toolbar_insert (window, _("_Paste"), "edit-paste",
                                  _("Paste the clipboard"), "win.edit.paste");

  item = gtk_separator_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (window->toolbar), item, -1);

  mousepad_window_toolbar_insert (window, _("_Find"), "edit-find",
                                  _("Search for text"), "win.search.find");
  mousepad_window_toolbar_insert (window, _("Find and Rep_lace..."), "edit-find-replace",
                                  _("Search for and replace text"), "win.search.find-and-replace");
  mousepad_window_toolbar_insert (window, _("_Go to..."), "go-jump",
                                  _("Go to a specific location in the document"), "win.search.go-to");

  /* make the last toolbar separator so it expands properly */
  item = gtk_separator_tool_item_new ();
  gtk_toolbar_insert (GTK_TOOLBAR (window->toolbar), item, -1);
  gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (item), FALSE);
  gtk_tool_item_set_expand (item, TRUE);

  mousepad_window_toolbar_insert (window, _("_Fullscreen"), "view-fullscreen",
                                  _("Make the window fullscreen"), "win.view.fullscreen");

  /* insert the toolbar in the main window box and show all widgets */
  gtk_box_pack_start (GTK_BOX (window->box), window->toolbar, FALSE, FALSE, 0);
  gtk_widget_show_all (window->toolbar);

  /* update the toolbar visibility and related actions state */
  mousepad_window_update_bar_visibility (window, TOOLBAR);

  /* update the toolbar with the settings */
  mousepad_window_update_toolbar (window, NULL, NULL);

  /* connect to some signals to keep in sync */
  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_VISIBLE,
                                   G_CALLBACK (mousepad_window_update_bar_visibility),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_VISIBLE_FULLSCREEN,
                                   G_CALLBACK (mousepad_window_update_bar_visibility),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_STYLE,
                                   G_CALLBACK (mousepad_window_update_toolbar),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (TOOLBAR_ICON_SIZE,
                                   G_CALLBACK (mousepad_window_update_toolbar),
                                   window, G_CONNECT_SWAPPED);
}



static void
mousepad_window_create_root_warning (MousepadWindow *window)
{
  /* check if we need to add the root warning */
  if (G_UNLIKELY (geteuid () == 0))
    {
      GtkWidget       *ebox, *label, *separator;
      GtkCssProvider  *provider;
      GtkStyleContext *context;
      const gchar     *css_string;

      /* add the box for the root warning */
      ebox = gtk_event_box_new ();
      gtk_box_pack_start (GTK_BOX (window->box), ebox, FALSE, FALSE, 0);
      gtk_widget_show (ebox);

      /* add the label with the root warning */
      label = gtk_label_new (_("Warning: you are using the root account. You may harm your system."));
      gtk_widget_set_margin_start (label, 6);
      gtk_widget_set_margin_end (label, 6);
      gtk_widget_set_margin_top (label, 3);
      gtk_widget_set_margin_bottom (label, 3);
      gtk_container_add (GTK_CONTAINER (ebox), label);
      gtk_widget_show (label);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (window->box), separator, FALSE, FALSE, 0);
      gtk_widget_show (separator);

      /* apply a CSS style to capture the user's attention */
      provider = gtk_css_provider_new ();
      css_string = "label { background-color: #b4254b; color: #fefefe; }";
      context = gtk_widget_get_style_context (GTK_WIDGET (label));
      gtk_css_provider_load_from_data (provider, css_string, -1, NULL);
      gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_object_unref (provider);
    }
}



static void
mousepad_window_create_notebook (MousepadWindow *window)
{
  window->notebook = g_object_new (GTK_TYPE_NOTEBOOK,
                                   "scrollable", TRUE,
                                   "show-border", FALSE,
                                   "show-tabs", FALSE,
                                   NULL);

  /* set the group id */
  gtk_notebook_set_group_name (GTK_NOTEBOOK (window->notebook), NOTEBOOK_GROUP);

  /* destroy the notebook first when destroying the window,
   * so that mousepad_window_notebook_removed() executes properly */
  g_signal_connect_swapped (G_OBJECT (window), "destroy",
                            G_CALLBACK (gtk_widget_destroy), window->notebook);

  /* connect signals to the notebooks */
  g_signal_connect (G_OBJECT (window->notebook), "switch-page",
                    G_CALLBACK (mousepad_window_notebook_switch_page), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-added",
                    G_CALLBACK (mousepad_window_notebook_added), window);
  g_signal_connect (G_OBJECT (window->notebook), "page-removed",
                    G_CALLBACK (mousepad_window_notebook_removed), window);
  g_signal_connect (G_OBJECT (window->notebook), "button-press-event",
                    G_CALLBACK (mousepad_window_notebook_button_press_event), window);
  g_signal_connect (G_OBJECT (window->notebook), "button-release-event",
                    G_CALLBACK (mousepad_window_notebook_button_release_event), window);
  g_signal_connect (G_OBJECT (window->notebook), "create-window",
                    G_CALLBACK (mousepad_window_notebook_create_window), window);

  /* append and show the notebook */
  gtk_box_pack_start (GTK_BOX (window->box), window->notebook, TRUE, TRUE, PADDING);
  gtk_widget_show (window->notebook);
}



static void
mousepad_window_action_statusbar_overwrite (MousepadWindow *window,
                                            gboolean        overwrite)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* set the new overwrite mode */
  mousepad_document_set_overwrite (window->active, overwrite);
}



static void
mousepad_window_create_statusbar (MousepadWindow *window)
{
  /* setup a new statusbar */
  window->statusbar = mousepad_statusbar_new ();

  /* update the statusbar visibility and related actions state */
  mousepad_window_update_bar_visibility (window, STATUSBAR);

#if !GTK_CHECK_VERSION (4, 0, 0)
  /* make the statusbar smaller */
  /*
   * To fix hard-coded, oversized GTK+3 status bar padding, see:
   * https://gitlab.gnome.org/GNOME/gtk/-/commit/94ebe2106817f6bc7aaf868bd00d0fc381d33e7e
   * Fixed in GTK+4:
   * https://gitlab.gnome.org/GNOME/gtk/-/commit/1a7cbddbd4e98e4641e690035013abbfaec130b0
   */
  gtk_widget_set_margin_top (window->statusbar, 0);
  gtk_widget_set_margin_bottom (window->statusbar, 0);
#endif

  /* pack the statusbar into the window UI */
  gtk_box_pack_end (GTK_BOX (window->box), window->statusbar, FALSE, FALSE, 0);

  /* overwrite toggle signal */
  g_signal_connect_swapped (G_OBJECT (window->statusbar), "enable-overwrite",
                            G_CALLBACK (mousepad_window_action_statusbar_overwrite), window);

  /* update the statusbar items */
  if (MOUSEPAD_IS_DOCUMENT (window->active))
    mousepad_document_send_signals (window->active);

  /* connect to some signals to keep in sync */
  MOUSEPAD_SETTING_CONNECT_OBJECT (TAB_WIDTH,
                                   G_CALLBACK (mousepad_window_update_statusbar_settings),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (STATUSBAR_VISIBLE,
                                   G_CALLBACK (mousepad_window_update_bar_visibility),
                                   window, G_CONNECT_SWAPPED);

  MOUSEPAD_SETTING_CONNECT_OBJECT (STATUSBAR_VISIBLE_FULLSCREEN,
                                   G_CALLBACK (mousepad_window_update_bar_visibility),
                                   window, G_CONNECT_SWAPPED);
}



static void
mousepad_window_init (MousepadWindow *window)
{
  GAction *action;

  /* initialize stuff */
  window->active = NULL;
  window->menubar = NULL;
  window->toolbar = NULL;
  window->notebook = NULL;
  window->search_bar = NULL;
  window->statusbar = NULL;
  window->replace_dialog = NULL;
  window->textview_menu = NULL;
  window->tab_menu = NULL;
  window->languages_menu = NULL;
  window->recent_manager = NULL;
  window->gtkmenu_key = NULL;
  window->offset_key = NULL;

  /* increase clipboard history ref count */
  clipboard_history_ref_count++;

  /* increase last save location ref count */
  last_save_location_ref_count++;

  /* add window actions */
  g_action_map_add_action_entries (G_ACTION_MAP (window), action_entries,
                                   G_N_ELEMENTS (action_entries), window);

  /* disable the "insensitive" action to make menu items that use it insensitive */
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "insensitive");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  /* create the main table */
  window->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), window->box);
  gtk_widget_show (window->box);

  /* keep a place for the menubar created later from the application resources */
  window->menubar_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (window->box), window->menubar_box, FALSE, FALSE, 0);
  gtk_widget_show (window->menubar_box);

  /* create the toolbar */
  mousepad_window_create_toolbar (window);

  /* create the root-warning bar (if needed) */
  mousepad_window_create_root_warning (window);

  /* create the notebook */
  mousepad_window_create_notebook (window);

  /* create the statusbar */
  mousepad_window_create_statusbar (window);

  /* defer actions that require the application to be set */
  g_signal_connect (G_OBJECT (window), "notify::application",
                    G_CALLBACK (mousepad_window_post_init), NULL);

  /* allow drops in the window */
  gtk_drag_dest_set (GTK_WIDGET (window),
                     GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                     drop_targets,
                     G_N_ELEMENTS (drop_targets), GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (window), "drag-data-received",
                    G_CALLBACK (mousepad_window_drag_data_received), window);

  /* update the window title when 'path-in-title' setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (PATH_IN_TITLE,
                                   G_CALLBACK (mousepad_window_set_title),
                                   window, G_CONNECT_SWAPPED);

  /* update the tabs when 'always-show-tabs' setting changes */
  MOUSEPAD_SETTING_CONNECT_OBJECT (ALWAYS_SHOW_TABS,
                                   G_CALLBACK (mousepad_window_update_tabs),
                                   window, G_CONNECT_SWAPPED);

  /* sync the tab size menu to its settings */
  MOUSEPAD_SETTING_CONNECT_OBJECT (TAB_WIDTH,
                                   G_CALLBACK (mousepad_window_menu_tab_sizes_update),
                                   window, G_CONNECT_SWAPPED);
}



static gboolean
mousepad_window_save_geometry (MousepadWindow *window)
{
  GdkWindowState state;
  gboolean       remember_size, remember_position, remember_state;

  /* check if we should remember the window geometry */
  remember_size = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_SIZE);
  remember_position = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_POSITION);
  remember_state = MOUSEPAD_SETTING_GET_BOOLEAN (REMEMBER_STATE);

  if (remember_size || remember_position || remember_state)
    {
      /* check if the window is still visible */
      if (gtk_widget_get_visible (GTK_WIDGET (window)))
        {
          /* determine the current state of the window */
          state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));

          /* don't save geometry for maximized or fullscreen windows */
          if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
            {
              if (remember_size)
                {
                  gint width, height;

                  /* determine the current width/height of the window... */
                  gtk_window_get_size (GTK_WINDOW (window), &width, &height);

                  /* ...and remember them as default for new windows */
                  MOUSEPAD_SETTING_SET_INT (WINDOW_WIDTH, width);
                  MOUSEPAD_SETTING_SET_INT (WINDOW_HEIGHT, height);
                }

              if (remember_position)
                {
                  gint left, top;

                  /* determine the current left/top position of the window */
                  gtk_window_get_position (GTK_WINDOW (window), &left, &top);

                  /* and then remember it for next startup */
                  MOUSEPAD_SETTING_SET_INT (WINDOW_LEFT, left);
                  MOUSEPAD_SETTING_SET_INT (WINDOW_TOP, top);
                }
            }

          if (remember_state)
            {
              /* remember whether the window is maximized or full screen or not */
              MOUSEPAD_SETTING_SET_BOOLEAN (WINDOW_MAXIMIZED, (state & GDK_WINDOW_STATE_MAXIMIZED));
              MOUSEPAD_SETTING_SET_BOOLEAN (WINDOW_FULLSCREEN, (state & GDK_WINDOW_STATE_FULLSCREEN));
            }
        }
    }

  return FALSE;
}



static gboolean
mousepad_window_configure_event (GtkWidget         *widget,
                                 GdkEventConfigure *event)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (widget);
  static GSource *source = NULL;
  guint           source_id;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  /*
   * As long as the window geometry changes, new configure events arrive and
   * the three actions below run as a loop.
   * The window geometry backup only takes place once, one second after the
   * last configure event.
   * "event == NULL" is a special case, corresponding to "save on close".
   */

  /* drop the previous timer source */
  if (source != NULL)
    {
      if (! g_source_is_destroyed (source))
        g_source_destroy (source);

      g_source_unref (source);
      source = NULL;
    }

  /* real event */
  if (event != NULL)
    {
      /* schedule a new backup of the window geometry */
      source_id = g_timeout_add_seconds (1, G_SOURCE_FUNC (mousepad_window_save_geometry), window);

      /* retrieve the timer source and increase its ref count to test its destruction next time */
      source = g_main_context_find_source_by_id (NULL, source_id);
      g_source_ref (source);

      /* let gtk+ handle the configure event */
      return GTK_WIDGET_CLASS (mousepad_window_parent_class)->configure_event (widget, event);
    }
  /* save on close */
  else
    return mousepad_window_save_geometry (window);
}



static gboolean
mousepad_window_delete_event (GtkWidget   *widget,
                              GdkEventAny *event)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (widget);

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  /* try to close the window */
  g_action_group_activate_action (G_ACTION_GROUP (window), "file.close-window", NULL);

  /* we will close the window when all the tabs are closed */
  return TRUE;
}



static gboolean
mousepad_window_window_state_event (GtkWidget           *widget,
                                    GdkEventWindowState *event)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (widget);

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  /* update bars visibility when entering/leaving fullscreen mode */
  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
      mousepad_window_update_bar_visibility (window, MENUBAR);
      mousepad_window_update_bar_visibility (window, TOOLBAR);
      mousepad_window_update_bar_visibility (window, STATUSBAR);
    }

  /* let gtk+ handle the window state event */
  return GTK_WIDGET_CLASS (mousepad_window_parent_class)->window_state_event (widget, event);
}



/**
 * Statusbar Tooltip Functions
 **/
static void
mousepad_window_menu_item_selected (GtkWidget      *menu_item,
                                    MousepadWindow *window)
{
  gchar *tooltip;

  tooltip = gtk_widget_get_tooltip_text (menu_item);
  mousepad_statusbar_push_tooltip (MOUSEPAD_STATUSBAR (window->statusbar), tooltip);
  g_free (tooltip);
}



static void
mousepad_window_menu_item_deselected (GtkWidget      *menu_item,
                                      MousepadWindow *window)
{
  mousepad_statusbar_pop_tooltip (MOUSEPAD_STATUSBAR (window->statusbar));
}



static gboolean
mousepad_window_tool_item_enter_event (GtkWidget      *tool_item,
                                       GdkEvent       *event,
                                       MousepadWindow *window)
{
  gchar *tooltip;

  tooltip = gtk_widget_get_tooltip_text (tool_item);
  mousepad_statusbar_push_tooltip (MOUSEPAD_STATUSBAR (window->statusbar), tooltip);
  g_free (tooltip);

  return FALSE;
}



static gboolean
mousepad_window_tool_item_leave_event (GtkWidget      *tool_item,
                                       GdkEvent       *event,
                                       MousepadWindow *window)
{
  mousepad_statusbar_pop_tooltip (MOUSEPAD_STATUSBAR (window->statusbar));

  return FALSE;
}



static void
mousepad_window_menu_set_tooltips (MousepadWindow *window,
                                   GtkWidget      *menu,
                                   GMenuModel     *model,
                                   gint           *offset)
{
  GMenuModel  *section, *submodel;
  GtkWidget   *submenu;
  GVariant    *hidden_when, *action, *tooltip;
  GList       *children, *child;
  const gchar *action_name;
  gint         n_items, n, suboffset = 0;

  /* initialization */
  n_items = g_menu_model_get_n_items (model);
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  child = children;
  if (offset == NULL)
    offset = &suboffset;

  /* attach the GtkMenu and the offset to the GMenuModel for future tooltip updates */
  mousepad_object_set_data (G_OBJECT (model), window->gtkmenu_key, menu);
  mousepad_object_set_data (G_OBJECT (model), window->offset_key, GINT_TO_POINTER (*offset));

  /* move to the right place in the GtkMenu if we are dealing with a section */
  for (n = 0; n < *offset; n++)
    child = child->next;

  /* exit if we have reached the end of the GtkMenu */
  if (child == NULL)
    {
      g_list_free (children);
      return;
    }

  for (n = 0; n < n_items; n++)
    {
      /* skip separators specific to GtkMenu */
      if (GTK_IS_SEPARATOR_MENU_ITEM (child->data))
        {
          child = child->next;
          (*offset)++;
        }

      /* section GMenuItem: one level down in the GMenuModel but same level in the GtkMenu,
       * so go ahead recursively only from GMenuModel point of view */
      if ((section = g_menu_model_get_item_link (model, n, G_MENU_LINK_SECTION)))
        mousepad_window_menu_set_tooltips (window, menu, section, offset);
      /* real GMenuItem */
      else
        {
          /* an hidden GMenuItem doesn't correspond to an hidden GtkMenuItem,
           * but to nothing, so we have to skip it in this case */
          hidden_when = g_menu_model_get_item_attribute_value (model, n, "hidden-when",
                                                               G_VARIANT_TYPE_STRING);
          if (hidden_when)
            {
              action = g_menu_model_get_item_attribute_value (model, n, "action",
                                                              G_VARIANT_TYPE_STRING);

              /* skip the GMenuItem if hidden when action is missing */
              if (g_strcmp0 (g_variant_get_string (hidden_when, NULL), "action-missing") == 0
                  && ! action)
                continue;

              /* skip the GMenuItem if hidden when action is disabled (but not missing) */
              if (g_strcmp0 (g_variant_get_string (hidden_when, NULL), "action-disabled") == 0
                  && action)
                {
                  action_name = g_variant_get_string (action, NULL);

                  /* we have to remove the group prefix if present */
                  if (g_strstr_len (action_name, 4, "win."))
                    action_name += 4;

                  if (! g_action_group_get_action_enabled (G_ACTION_GROUP (window), action_name))
                    continue;
                }

              /* cleanup */
              g_variant_unref (hidden_when);
              if (action)
                g_variant_unref (action);
            }

          /* set the tooltip on the corresponding GtkMenuItem */
          tooltip = g_menu_model_get_item_attribute_value (model, n, "tooltip",
                                                           G_VARIANT_TYPE_STRING);
          if (tooltip)
            {
              gtk_widget_set_tooltip_text (child->data, g_variant_get_string (tooltip, NULL));
              g_variant_unref (tooltip);
            }
          /* set a whitespace instead of NULL or an empty string, otherwise an absence of menu item
           * tooltip could reveal an underlying menu tooltip (e.g. in the case of a new unnamed
           * document in the go-to-tab menu) */
          else
            gtk_widget_set_tooltip_text (child->data, " ");

          /* don't show the tooltip as a tooltip */
          gtk_widget_set_has_tooltip (child->data, FALSE);

          /* connect signals to show the tooltip in the status bar */
          g_signal_connect_object (child->data, "select",
                                   G_CALLBACK (mousepad_window_menu_item_selected),
                                   window, 0);
          g_signal_connect_object (child->data, "deselect",
                                   G_CALLBACK (mousepad_window_menu_item_deselected),
                                   window, 0);

          /* submenu GMenuItem: go ahead recursively */
          if ((submodel = g_menu_model_get_item_link (model, n, G_MENU_LINK_SUBMENU)))
            {
              submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (child->data));
              mousepad_window_menu_set_tooltips (window, submenu, submodel, NULL);
            }

          child = child->next;
          (*offset)++;
        }
    }

  /* cleanup */
  g_list_free (children);
}



/**
 * Mousepad Window Functions
 **/
static gboolean
mousepad_window_open_file (MousepadWindow   *window,
                           const gchar      *filename,
                           MousepadEncoding  encoding)
{
  MousepadDocument *document;
  GError           *error = NULL;
  gint              result;
  gint              npages = 0, i;
  gint              response;
  const gchar      *charset;
  const gchar      *opened_filename;
  GtkWidget        *dialog;
  gboolean          encoding_from_recent = FALSE;
  gchar            *uri;
  GtkRecentInfo    *info;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (filename != NULL && *filename != '\0', FALSE);

  /* check if the file is already openend */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
  for (i = 0; i < npages; i++)
    {
      document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i));

      /* debug check */
      g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

      if (G_LIKELY (document))
        {
          /* get the filename */
          opened_filename = mousepad_file_get_filename (MOUSEPAD_DOCUMENT (document)->file);

          /* see if the file is already opened */
          if (opened_filename && strcmp (filename, opened_filename) == 0)
            {
              /* switch to the tab */
              gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

              /* and we're done */
              return TRUE;
            }
        }
    }

  /* new document */
  document = mousepad_document_new ();

  /* make sure it's not a floating object */
  g_object_ref_sink (G_OBJECT (document));

  /* set the filename */
  mousepad_file_set_filename (document->file, filename, TRUE);

  /* set the passed encoding */
  mousepad_file_set_encoding (document->file, encoding);

  retry:

  /* lock the undo manager */
  gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  /* read the content into the buffer */
  result = mousepad_file_open (document->file, NULL, &error);

  /* release the lock */
  gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  switch (result)
    {
      case 0:
        /* add the document to the window */
        mousepad_window_add (window, document);

        /* insert in the recent history */
        mousepad_window_recent_add (window, document->file);
        break;

      case ERROR_CONVERTING_FAILED:
      case ERROR_NOT_UTF8_VALID:
        /* clear the error */
        g_clear_error (&error);

        /* try to lookup the encoding from the recent history */
        if (encoding_from_recent == FALSE)
          {
            /* we only try this once */
            encoding_from_recent = TRUE;

            /* make sure the recent manager is initialized */
            mousepad_window_recent_manager_init (window);

            /* build uri */
            uri = g_filename_to_uri (filename, NULL, NULL);

            /* try to lookup the recent item */
            if (G_LIKELY (uri))
              {
                info = gtk_recent_manager_lookup_item (window->recent_manager, uri, NULL);

                /* cleanup */
                g_free (uri);

                if (info)
                  {
                    /* try to find the encoding */
                    charset = mousepad_window_recent_get_charset (info);

                    encoding = mousepad_encoding_find (charset);

                    /* set the new encoding */
                    mousepad_file_set_encoding (document->file, encoding);

                    /* release */
                    gtk_recent_info_unref (info);

                    /* try to open again with the last used encoding */
                    if (G_LIKELY (encoding))
                      goto retry;
                  }
              }
          }

        /* run the encoding dialog */
        dialog = mousepad_encoding_dialog_new (GTK_WINDOW (window), document->file);

        /* run the dialog */
        response = gtk_dialog_run (GTK_DIALOG (dialog));

        if (response == GTK_RESPONSE_OK)
          {
            /* set the new encoding */
            encoding = mousepad_encoding_dialog_get_encoding (MOUSEPAD_ENCODING_DIALOG (dialog));

            /* set encoding */
            mousepad_file_set_encoding (document->file, encoding);
          }

        /* destroy the dialog */
        gtk_widget_destroy (dialog);

        /* handle */
        if (response == GTK_RESPONSE_OK)
          goto retry;
        else
          g_object_unref (G_OBJECT (document));

        break;

      default:
        /* something went wrong, release the document */
        g_object_unref (G_OBJECT (document));

        if (G_LIKELY (error))
          {
            /* show the warning */
            mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to open the document"));

            /* cleanup */
            g_error_free (error);
          }

        break;
    }

  return (result == 0);
}



gint
mousepad_window_open_files (MousepadWindow  *window,
                            gchar          **uris)
{
  gchar *filename;
  guint  n;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), 0);
  g_return_val_if_fail (uris != NULL, 0);
  g_return_val_if_fail (*uris != NULL, 0);

  /* block menu updates */
  lock_menu_updates++;

  /* walk through all the filenames */
  for (n = 0; uris[n] != NULL; ++n)
    {
      /* retrieve the filename */
      filename = g_filename_from_uri (uris[n], NULL, NULL);

      /* open a new tab with the file */
      mousepad_window_open_file (window, filename, MOUSEPAD_ENCODING_UTF_8);

      /* cleanup */
      g_free (filename);
    }

  /* allow menu updates again */
  lock_menu_updates--;

  /* return the number of opened documents */
  return gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));
}



void
mousepad_window_add (MousepadWindow   *window,
                     MousepadDocument *document)
{
  GtkWidget        *label;
  gint              page;
  MousepadDocument *prev_active = window->active;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_NOTEBOOK (window->notebook));

  /* receive the document occurrences count */
  g_signal_connect_swapped (document, "search-completed",
                            G_CALLBACK (mousepad_window_search_completed), window);

  /* create the tab label */
  label = mousepad_document_get_tab_label (document);

  /* get active page */
  page = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));

  /* insert the page right of the active tab */
  page = gtk_notebook_insert_page (GTK_NOTEBOOK (window->notebook),
                                   GTK_WIDGET (document), label, page + 1);

  /* set tab child properties */
  gtk_container_child_set (GTK_CONTAINER (window->notebook),
                           GTK_WIDGET (document), "tab-expand", TRUE, NULL);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document), TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document), TRUE);

  /* show the document */
  gtk_widget_show (GTK_WIDGET (document));

  /* don't bother about this when there was no previous active page (startup) */
  if (G_LIKELY (prev_active != NULL))
    {
      /* switch to the new tab */
      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page);

      /* destroy the previous tab if it was not modified, untitled and the new tab is not untitled */
      if (gtk_text_buffer_get_modified (prev_active->buffer) == FALSE
          && mousepad_file_get_filename (prev_active->file) == NULL
          && mousepad_file_get_filename (document->file) != NULL)
        gtk_widget_destroy (GTK_WIDGET (prev_active));
    }

  /* make sure the textview is focused in the new document */
  mousepad_document_focus_textview (document);
}



static gboolean
mousepad_window_close_document (MousepadWindow   *window,
                                MousepadDocument *document)
{
  GVariant *value;
  gboolean  succeed = FALSE, readonly;
  gint      response;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (document), FALSE);

  /* check if the document has been modified */
  if (gtk_text_buffer_get_modified (document->buffer))
    {
      /* whether the file is readonly */
      readonly = mousepad_file_get_read_only (document->file);

      /* run save changes dialog */
      response = mousepad_dialogs_save_changes (GTK_WINDOW (window), readonly);

      switch (response)
        {
          case MOUSEPAD_RESPONSE_DONT_SAVE:
            /* don't save, only destroy the document */
            succeed = TRUE;
            break;

          case MOUSEPAD_RESPONSE_CANCEL:
            /* do nothing */
            break;

          case MOUSEPAD_RESPONSE_SAVE:
            g_action_group_activate_action (G_ACTION_GROUP (window), "file.save", NULL);
            value = g_action_group_get_action_state (G_ACTION_GROUP (window), "file.save");
            succeed = g_variant_get_int32 (value);
            g_variant_unref (value);
            break;

          case MOUSEPAD_RESPONSE_SAVE_AS:
            g_action_group_activate_action (G_ACTION_GROUP (window), "file.save-as", NULL);
            value = g_action_group_get_action_state (G_ACTION_GROUP (window), "file.save-as");
            succeed = g_variant_get_int32 (value);
            g_variant_unref (value);
            break;
        }
    }
  else
    {
      /* no changes in the document, safe to destroy it */
      succeed = TRUE;
    }

  /* destroy the document */
  if (succeed)
    gtk_widget_destroy (GTK_WIDGET (document));

  return succeed;
}



static void
mousepad_window_button_close_tab (MousepadDocument *document,
                                  MousepadWindow   *window)
{
  gint page_num;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* switch to the tab we're going to close */
  page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (document));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page_num);

  /* close the document */
  mousepad_window_close_document (window, document);
}



static void
mousepad_window_set_title (MousepadWindow *window)
{
  gchar            *string;
  const gchar      *title;
  gboolean          show_full_path;
  MousepadDocument *document = window->active;

  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* whether to show the full path */
  show_full_path = MOUSEPAD_SETTING_GET_BOOLEAN (PATH_IN_TITLE);

  /* name we display in the title */
  if (G_UNLIKELY (show_full_path && mousepad_document_get_filename (document)))
    title = mousepad_document_get_filename (document);
  else
    title = mousepad_document_get_basename (document);

  /* build the title */
  if (G_UNLIKELY (mousepad_file_get_read_only (document->file)))
    string = g_strdup_printf ("%s%s [%s] - %s",
                              gtk_text_buffer_get_modified (document->buffer) ? "*" : "",
                              title, _("Read Only"), PACKAGE_NAME);
  else
    string = g_strdup_printf ("%s%s - %s",
                              gtk_text_buffer_get_modified (document->buffer) ? "*" : "",
                              title, PACKAGE_NAME);

  /* set the window title */
  gtk_window_set_title (GTK_WINDOW (window), string);

  /* cleanup */
  g_free (string);
}



/* give the statusbar a languages menu created from the application resources */
GtkWidget *
mousepad_window_get_languages_menu (MousepadWindow *window)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);

  return window->languages_menu;
}



static gboolean
mousepad_window_get_in_fullscreen (MousepadWindow *window)
{
  if (GTK_IS_WIDGET (window) && gtk_widget_get_visible (GTK_WIDGET (window)))
    {
      GdkWindow     *win = gtk_widget_get_window (GTK_WIDGET (window));
      GdkWindowState state = gdk_window_get_state (win);
      return (state & GDK_WINDOW_STATE_FULLSCREEN);
    }

  return FALSE;
}



static gboolean
mousepad_window_hide_menubar_event (MousepadWindow *window)
{
  /* disconnect signals and hide the menubar */
  mousepad_disconnect_by_func (window, mousepad_window_hide_menubar_event, NULL);
  mousepad_disconnect_by_func (window->menubar, mousepad_window_hide_menubar_event, window);
  mousepad_disconnect_by_func (window->notebook, mousepad_window_hide_menubar_event, window);
  gtk_widget_hide (window->menubar);

  return FALSE;
}



static gboolean
mousepad_window_menubar_key_event (MousepadWindow *window,
                                   GdkEventKey    *event,
                                   GList          *mnemonics)
{
  GdkEvent *event_bis;
  static gboolean hidden_last_time = FALSE;

  /* Alt key was pressed (alone or as a GdkModifierType) or released, or Esc key was pressed */
  if (event->state & GDK_MOD1_MASK || event->keyval == GDK_KEY_Alt_L
      || (event->keyval == GDK_KEY_Escape && event->type == GDK_KEY_PRESS))
    {
      /* hide the menubar if Alt/Esc key was pressed */
      if (event->type == GDK_KEY_PRESS
          && (event->keyval == GDK_KEY_Alt_L || event->keyval == GDK_KEY_Escape)
          && gtk_widget_get_visible (window->menubar))
        {
          /* disconnect signals and hide the menubar */
          mousepad_disconnect_by_func (window, mousepad_window_hide_menubar_event, NULL);
          mousepad_disconnect_by_func (window->menubar, mousepad_window_hide_menubar_event, window);
          mousepad_disconnect_by_func (window->notebook, mousepad_window_hide_menubar_event, window);
          gtk_widget_hide (window->menubar);

          /* don't show the menubar when the Alt key is released this time */
          hidden_last_time = TRUE;

          return TRUE;
        }
      /* show the menubar if Alt key was released or if one of its mnemonic keys matched */
      else if (! hidden_last_time && ! gtk_widget_get_visible (window->menubar)
               && ((event->keyval == GDK_KEY_Alt_L && event->type == GDK_KEY_RELEASE)
                   || (event->type == GDK_KEY_PRESS && event->state & GDK_MOD1_MASK
                       && g_list_find (mnemonics, GUINT_TO_POINTER (event->keyval)))))
        {
          /* show the menubar and connect signals to hide it afterwards on user actions */
          gtk_widget_show (window->menubar);
          g_signal_connect (G_OBJECT (window), "button-press-event",
                            G_CALLBACK (mousepad_window_hide_menubar_event), NULL);
          g_signal_connect (G_OBJECT (window), "button-release-event",
                            G_CALLBACK (mousepad_window_hide_menubar_event), NULL);
          g_signal_connect (G_OBJECT (window), "focus-out-event",
                            G_CALLBACK (mousepad_window_hide_menubar_event), NULL);
          g_signal_connect (G_OBJECT (window), "scroll-event",
                            G_CALLBACK (mousepad_window_hide_menubar_event), NULL);
          g_signal_connect_swapped (G_OBJECT (window->menubar), "deactivate",
                                    G_CALLBACK (mousepad_window_hide_menubar_event), window);
          g_signal_connect_swapped (G_OBJECT (window->notebook), "button-press-event",
                                    G_CALLBACK (mousepad_window_hide_menubar_event), window);

          /* in case of a mnemonic key, repeat the same event to make its menu popup */
          if (event->keyval != GDK_KEY_Alt_L)
            {
              event_bis = gdk_event_copy ((GdkEvent*) event);
              gtk_main_do_event (event_bis);
              gdk_event_free (event_bis);
            }

          return TRUE;
        }
    }

  /* show the menubar the next time the Alt key is released */
  hidden_last_time = FALSE;

  return FALSE;
}



static void
mousepad_window_update_bar_visibility (MousepadWindow *window,
                                       const gchar    *hint)
{
  GtkWidget   *bar;
  GVariant    *state;
  const gchar *setting, *setting_fs;
  gboolean     visible, visible_fs;

  /* the hint may or may not contain the whole setting name and/or the fullscreen suffix,
   * but it will always be included in their concatenation */
  if (g_strstr_len (MOUSEPAD_SETTING_MENUBAR_VISIBLE_FULLSCREEN, -1, hint))
    {
      setting = MENUBAR;
      setting_fs = MOUSEPAD_SETTING_MENUBAR_VISIBLE_FULLSCREEN;
      bar = window->menubar;
    }
  else if (g_strstr_len (MOUSEPAD_SETTING_TOOLBAR_VISIBLE_FULLSCREEN, -1, hint))
    {
      setting = TOOLBAR;
      setting_fs = MOUSEPAD_SETTING_TOOLBAR_VISIBLE_FULLSCREEN;
      bar = window->toolbar;
    }
  else if (g_strstr_len (MOUSEPAD_SETTING_STATUSBAR_VISIBLE_FULLSCREEN, -1, hint))
    {
      setting = STATUSBAR;
      setting_fs = MOUSEPAD_SETTING_STATUSBAR_VISIBLE_FULLSCREEN;
      bar = window->statusbar;
    }
  /* should not be reached */
  else
    return;

  /* get visibility setting in window mode */
  visible = mousepad_setting_get_boolean (setting);

  /* deduce the visibility setting if we are in fullscreen mode */
  if (mousepad_window_get_in_fullscreen (window))
    {
      visible_fs = mousepad_setting_get_enum (setting_fs);
      visible = (visible_fs == AUTO) ? visible : (visible_fs == YES);
    }

  /* update the bar visibility */
  gtk_widget_set_visible (bar, visible);

  /* avoid menu actions */
  lock_menu_updates++;

  /* request for the action state to be changed according to the setting */
  state = mousepad_setting_get_variant (setting);
  g_action_group_change_action_state (G_ACTION_GROUP (window), setting, state);
  g_variant_unref (state);

  /* allow menu actions again */
  lock_menu_updates--;
}



/**
 * Notebook Signal Functions
 **/
static void
mousepad_window_notebook_switch_page (GtkNotebook     *notebook,
                                      GtkWidget       *page,
                                      guint            page_num,
                                      MousepadWindow  *window)
{
  MousepadDocument  *document;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* get the new active document */
  document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (notebook, page_num));

  /* only update when really changed */
  if (G_LIKELY (window->active != document))
    {
      /* set new active document */
      window->active = document;

      /* set the window title */
      mousepad_window_set_title (window);

      /* update the menu actions */
      mousepad_window_update_actions (window);

      /* update the statusbar */
      mousepad_document_send_signals (window->active);
    }
}



static void
mousepad_window_notebook_added (GtkNotebook     *notebook,
                                GtkWidget       *page,
                                guint            page_num,
                                MousepadWindow  *window)
{
  MousepadDocument *document = MOUSEPAD_DOCUMENT (page);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* connect signals to the document for this window */
  g_signal_connect (G_OBJECT (page), "close-tab",
                    G_CALLBACK (mousepad_window_button_close_tab), window);
  g_signal_connect (G_OBJECT (page), "cursor-changed",
                    G_CALLBACK (mousepad_window_cursor_changed), window);
  g_signal_connect (G_OBJECT (page), "selection-changed",
                    G_CALLBACK (mousepad_window_selection_changed), window);
  g_signal_connect (G_OBJECT (page), "overwrite-changed",
                    G_CALLBACK (mousepad_window_overwrite_changed), window);
  g_signal_connect (G_OBJECT (page), "language-changed",
                    G_CALLBACK (mousepad_window_buffer_language_changed), window);
  g_signal_connect (G_OBJECT (page), "drag-data-received",
                    G_CALLBACK (mousepad_window_drag_data_received), window);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "notify::can-undo",
                            G_CALLBACK (mousepad_window_can_undo), window);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "notify::can-redo",
                            G_CALLBACK (mousepad_window_can_redo), window);
  g_signal_connect_swapped (G_OBJECT (document->buffer), "modified-changed",
                            G_CALLBACK (mousepad_window_modified_changed), window);
  g_signal_connect_swapped (G_OBJECT (document->file), "readonly-changed",
                            G_CALLBACK (mousepad_window_readonly_changed), window);
  g_signal_connect_swapped (G_OBJECT (document->file), "filename-changed",
                            G_CALLBACK (mousepad_window_set_title), window);
  g_signal_connect (G_OBJECT (document->textview), "populate-popup",
                    G_CALLBACK (mousepad_window_menu_textview_popup), window);

  /* change the visibility of the tabs accordingly */
  mousepad_window_update_tabs (window, NULL, NULL);
}



static void
mousepad_window_notebook_removed (GtkNotebook     *notebook,
                                  GtkWidget       *page,
                                  guint            page_num,
                                  MousepadWindow  *window)
{
  gint              npages;
  MousepadDocument *document = MOUSEPAD_DOCUMENT (page);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  /* disconnect the old document signals */
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_button_close_tab, window);
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_cursor_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_selection_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_overwrite_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_buffer_language_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (page), mousepad_window_drag_data_received, window);
  mousepad_disconnect_by_func (G_OBJECT (document->buffer), mousepad_window_can_undo, window);
  mousepad_disconnect_by_func (G_OBJECT (document->buffer), mousepad_window_can_redo, window);
  mousepad_disconnect_by_func (G_OBJECT (document->buffer),
                               mousepad_window_modified_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (document->file), mousepad_window_set_title, window);
  mousepad_disconnect_by_func (G_OBJECT (document->file),
                               mousepad_window_readonly_changed, window);
  mousepad_disconnect_by_func (G_OBJECT (document->textview),
                               mousepad_window_menu_textview_popup, window);

  /* get the number of pages in this notebook */
  npages = gtk_notebook_get_n_pages (notebook);

  /* update the window */
  if (npages == 0)
    {
      /* window contains no tabs: save geometry and destroy it */
      mousepad_window_configure_event (GTK_WIDGET (window), NULL);
      gtk_widget_destroy (GTK_WIDGET (window));
    }
  else
    {
      /* change the visibility of the tabs accordingly */
      mousepad_window_update_tabs (window, NULL, NULL);

      /* update action entries */
      mousepad_window_update_actions (window);
    }
}



/* stolen from Geany notebook.c */
static gboolean
mousepad_window_is_position_on_tab_bar (GtkNotebook *notebook, GdkEventButton *event)
{
  GtkWidget      *page, *tab, *nb;
  GtkPositionType tab_pos;
  gint            scroll_arrow_hlength, scroll_arrow_vlength;
  gdouble         x, y;

  page = gtk_notebook_get_nth_page (notebook, 0);
  g_return_val_if_fail (page != NULL, FALSE);

  tab = gtk_notebook_get_tab_label (notebook, page);
  g_return_val_if_fail (tab != NULL, FALSE);

  tab_pos = gtk_notebook_get_tab_pos (notebook);
  nb = GTK_WIDGET (notebook);

  gtk_widget_style_get (GTK_WIDGET (notebook),
                        "scroll-arrow-hlength", &scroll_arrow_hlength,
                        "scroll-arrow-vlength", &scroll_arrow_vlength,
                        NULL);

  if (! gdk_event_get_coords ((GdkEvent*) event, &x, &y))
    {
      x = event->x;
      y = event->y;
    }

  switch (tab_pos)
    {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      if (event->y >= 0 && event->y <= gtk_widget_get_allocated_height (tab))
        {
          if (! gtk_notebook_get_scrollable (notebook) || (
            x > scroll_arrow_hlength &&
            x < gtk_widget_get_allocated_width (nb) - scroll_arrow_hlength))
          {
            return TRUE;
          }
        }
      break;
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
        if (event->x >= 0 && event->x <= gtk_widget_get_allocated_width (tab))
          {
            if (! gtk_notebook_get_scrollable (notebook) || (
                y > scroll_arrow_vlength &&
                y < gtk_widget_get_allocated_height (nb) - scroll_arrow_vlength))
              {
                return TRUE;
              }
          }
    }

  return FALSE;
}



static gboolean
mousepad_window_notebook_button_press_event (GtkNotebook    *notebook,
                                             GdkEventButton *event,
                                             MousepadWindow *window)
{
  GtkWidget *page, *label;
  guint      page_num = 0;
  gint       x_root;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  if (event->type == GDK_BUTTON_PRESS && (event->button == 3 || event->button == 2))
    {
      /* walk through the tabs and look for the tab under the cursor */
      while ((page = gtk_notebook_get_nth_page (notebook, page_num)) != NULL)
        {
          GtkAllocation alloc = { 0, 0, 0, 0 };

          label = gtk_notebook_get_tab_label (notebook, page);

          /* get the origin of the label */
          gdk_window_get_origin (gtk_widget_get_window (label), &x_root, NULL);
          gtk_widget_get_allocation (label, &alloc);
          x_root = x_root + alloc.x;

          /* check if the cursor is inside this label */
          if (event->x_root >= x_root && event->x_root <= (x_root + alloc.width))
            {
              /* switch to this tab */
              gtk_notebook_set_current_page (notebook, page_num);

              /* handle the button action */
              if (event->button == 3)
                {
                  /* show the menu */
#if GTK_CHECK_VERSION (3, 22, 0)
                  gtk_menu_popup_at_pointer (GTK_MENU (window->tab_menu), (GdkEvent*) event);
#else

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

                  gtk_menu_popup (GTK_MENU (window->tab_menu), NULL, NULL, NULL, NULL,
                                  event->button, event->time);

G_GNUC_END_IGNORE_DEPRECATIONS

#endif
                }
              /* close the document */
              else if (event->button == 2)
                g_action_group_activate_action (G_ACTION_GROUP (window), "file.close-tab", NULL);

              /* we succeed */
              return TRUE;
            }

          /* try the next tab */
          ++page_num;
        }
    }
  else if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
      GtkWidget   *ev_widget, *nb_child;

      ev_widget = gtk_get_event_widget ((GdkEvent*) event);
      nb_child = gtk_notebook_get_nth_page (notebook,
                                            gtk_notebook_get_current_page (notebook));
      if (ev_widget == NULL || ev_widget == nb_child || gtk_widget_is_ancestor (ev_widget, nb_child))
        return FALSE;

      /* check if the event window is the notebook event window (not a tab) */
      if (mousepad_window_is_position_on_tab_bar (notebook, event))
        {
          /* create new document */
          g_action_group_activate_action (G_ACTION_GROUP (window), "file.new", NULL);

          /* we succeed */
          return TRUE;
        }
    }

  return FALSE;
}



static gboolean
mousepad_window_notebook_button_release_event (GtkNotebook    *notebook,
                                               GdkEventButton *event,
                                               MousepadWindow *window)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (window->active), FALSE);

  /* focus the active textview */
  mousepad_document_focus_textview (window->active);

  return FALSE;
}



static GtkNotebook *
mousepad_window_notebook_create_window (GtkNotebook    *notebook,
                                        GtkWidget      *page,
                                        gint            x,
                                        gint            y,
                                        MousepadWindow *window)
{
  MousepadDocument *document;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);
  g_return_val_if_fail (MOUSEPAD_IS_DOCUMENT (page), NULL);

  /* only create new window when there are more than 2 tabs */
  if (gtk_notebook_get_n_pages (notebook) >= 2)
    {
      /* get the document */
      document = MOUSEPAD_DOCUMENT (page);

      /* take a reference */
      g_object_ref (G_OBJECT (document));

      /* remove the document from the active window */
      gtk_notebook_detach_tab (GTK_NOTEBOOK (window->notebook), page);

      /* emit the new window with document signal */
      g_signal_emit (G_OBJECT (window), window_signals[NEW_WINDOW_WITH_DOCUMENT], 0, document, x, y);

      /* release our reference */
      g_object_unref (G_OBJECT (document));
    }

  return NULL;
}



/**
 * Document Signals Functions
 **/
static void
mousepad_window_modified_changed (MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* update window title */
  mousepad_window_set_title (window);

  /* update document dependent menu items */
  mousepad_window_update_document_menu_items (window);
}



static void
mousepad_window_readonly_changed (MousepadWindow *window)
{
  GAction *action;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* set the save action sensitivity */
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                               ! mousepad_file_get_read_only (window->active->file));
}



static void
mousepad_window_cursor_changed (MousepadDocument *document,
                                gint              line,
                                gint              column,
                                gint              selection,
                                MousepadWindow   *window)
{


  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  if (window->statusbar)
    {
      /* set the new statusbar cursor position and selection length */
      mousepad_statusbar_set_cursor_position (MOUSEPAD_STATUSBAR (window->statusbar), line, column, selection);
    }
}



static void
mousepad_window_selection_changed (MousepadDocument *document,
                                   gint              selection,
                                   MousepadWindow   *window)
{
  GAction     *action;
  guint        i;
  gboolean     sensitive;
  const gchar *action_names_1[] = { "edit.convert.tabs-to-spaces",
                                    "edit.convert.spaces-to-tabs",
                                    "edit.duplicate-line-selection",
                                    "edit.convert.strip-trailing-spaces" };
  const gchar *action_names_2[] = { "edit.move-selection.line-up",
                                    "edit.move-selection.line-down" };
  const gchar *action_names_3[] = { "edit.cut",
                                    "edit.copy",
                                    "edit.delete",
                                    "edit.convert.to-lowercase",
                                    "edit.convert.to-uppercase",
                                    "edit.convert.to-title-case",
                                    "edit.convert.to-opposite-case" };

  /* actions that are unsensitive during a column selection */
  sensitive = (selection == 0 || selection == 1);
  for (i = 0; i < G_N_ELEMENTS (action_names_1); i++)
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (window), action_names_1[i]);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive);
    }

  /* action that are only sensitive for normal selections */
  sensitive = (selection == 1);
  for (i = 0; i < G_N_ELEMENTS (action_names_2); i++)
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (window), action_names_2[i]);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive);
    }

  /* actions that are sensitive for all selections with content */
  sensitive = (selection > 0);
  for (i = 0; i < G_N_ELEMENTS (action_names_3); i++)
    {
      action = g_action_map_lookup_action (G_ACTION_MAP (window), action_names_3[i]);
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), sensitive);
    }
}



static void
mousepad_window_overwrite_changed (MousepadDocument *document,
                                   gboolean          overwrite,
                                   MousepadWindow   *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* set the new overwrite mode in the statusbar */
  if (window->statusbar)
    mousepad_statusbar_set_overwrite (MOUSEPAD_STATUSBAR (window->statusbar), overwrite);
}



static void
mousepad_window_buffer_language_changed (MousepadDocument  *document,
                                         GtkSourceLanguage *language,
                                         MousepadWindow    *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

  /* update the filetype shown in the statusbar */
  mousepad_statusbar_set_language (MOUSEPAD_STATUSBAR (window->statusbar), language);
}



static void
mousepad_window_can_undo (MousepadWindow *window,
                          GParamSpec     *unused,
                          GObject        *buffer)
{
  GAction  *action;
  gboolean  can_undo;

  can_undo = gtk_source_buffer_can_undo (GTK_SOURCE_BUFFER (buffer));

  action = g_action_map_lookup_action (G_ACTION_MAP (window), "edit.undo");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_undo);
}



static void
mousepad_window_can_redo (MousepadWindow *window,
                          GParamSpec     *unused,
                          GObject        *buffer)
{
  GAction  *action;
  gboolean  can_redo;

  can_redo = gtk_source_buffer_can_redo (GTK_SOURCE_BUFFER (buffer));

  action = g_action_map_lookup_action (G_ACTION_MAP (window), "edit.redo");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), can_redo);
}



/**
 * Menu Functions
 **/
static void
mousepad_window_menu_templates_fill (MousepadWindow *window,
                                     GMenu          *menu,
                                     const gchar    *path)
{
  GDir         *dir;
  GSList       *files_list = NULL, *dirs_list = NULL, *li;
  gchar        *absolute_path, *label, *dot, *message,
               *action_name, *filename_utf8, *tooltip;
  const gchar  *name;
  gboolean      files_added = FALSE;
  GIcon        *icon;
  GMenu        *submenu;
  GMenuItem    *item;

  /* open the directory */
  dir = g_dir_open (path, 0, NULL);

  /* read the directory */
  if (G_LIKELY (dir))
    {
      /* walk the directory */
      for (;;)
        {
          /* read the filename of the next file */
          name = g_dir_read_name (dir);

          /* break when we reached the last file */
          if (G_UNLIKELY (name == NULL))
            break;

          /* skip hidden files */
          if (name[0] == '.')
            continue;

          /* build absolute path */
          absolute_path = g_build_path (G_DIR_SEPARATOR_S, path, name, NULL);

          /* check if the file is a regular file or directory */
          if (g_file_test (absolute_path, G_FILE_TEST_IS_DIR))
            dirs_list = g_slist_insert_sorted (dirs_list, absolute_path, (GCompareFunc) strcmp);
          else if (g_file_test (absolute_path, G_FILE_TEST_IS_REGULAR))
            files_list = g_slist_insert_sorted (files_list, absolute_path, (GCompareFunc) strcmp);
          else
            g_free (absolute_path);
        }

      /* close the directory */
      g_dir_close (dir);
    }

  /* append the directories */
  for (li = dirs_list; li != NULL; li = li->next)
    {
      /* create a new submenu for the directory */
      submenu = g_menu_new ();

      /* fill the menu */
      mousepad_window_menu_templates_fill (window, submenu, li->data);

      /* check if the submenu contains items */
      if (g_menu_model_get_n_items (G_MENU_MODEL (submenu)))
        {
          /* append the menu */
          label = g_filename_display_basename (li->data);
          item = g_menu_item_new (label, NULL);
          g_free (label);

          /* set submenu icon */
          /* TODO: is there a way to apply an icon to a submenu?
           * (the documentation says yes, the "folder" icon name is valid, but…) */
          icon = g_icon_new_for_string ("folder", NULL);
          g_menu_item_set_icon (item, icon);
          g_object_unref (G_OBJECT (icon));

          g_menu_item_set_submenu (item, G_MENU_MODEL (submenu));
          g_menu_append_item (menu, item);
          g_object_unref (item);
        }

      /* cleanup */
      g_free (li->data);
    }

  /* append the files */
  for (li = files_list; li != NULL; li = li->next)
    {
      /* create directory label */
      label = g_filename_display_basename (li->data);

      /* strip the extension from the label */
      dot = g_utf8_strrchr (label, -1, '.');
      if (dot != NULL)
        *dot = '\0';

      /* create the menu item */
      action_name = g_strconcat ("win.file.new-from-template.new('", li->data, "')", NULL);
      item = g_menu_item_new (label, action_name);
      g_free (action_name);

      /* create an utf-8 valid version of the filename for the tooltip */
      filename_utf8 = g_filename_to_utf8 (li->data, -1, NULL, NULL, NULL);
      tooltip = g_strdup_printf (_("Use '%s' as template"), filename_utf8);
      g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));
      g_free (filename_utf8);
      g_free (tooltip);

      /* set item icon */
      icon = g_icon_new_for_string ("text-x-generic", NULL);
      g_menu_item_set_icon (item, icon);
      g_object_unref (G_OBJECT (icon));

      /* append item to the menu */
      g_menu_append_item (menu, item);
      g_object_unref (item);

      /* disable the menu item telling the user there's no templates */
      files_added = TRUE;

      /* cleanup */
      g_free (label);
      g_free (li->data);
    }

  /* cleanup */
  g_slist_free (dirs_list);
  g_slist_free (files_list);

  if (! files_added)
    {
      message = g_strdup_printf (_("No template files found in\n'%s'"), path);
      item = g_menu_item_new (message, "win.insensitive");
      g_free (message);
      g_menu_append_item (menu, item);
      g_object_unref (item);
    }
}



static void
mousepad_window_menu_templates (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkApplication *application;
  GtkWidget      *gtkmenu;
  GMenu          *menu;
  GMenuItem      *item;
  const gchar    *homedir;
  gchar          *templates_path, *message;
  gint            offset;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* opening the menu */
  if (g_variant_get_boolean (value))
    {
      /* lock menu updates */
      lock_menu_updates++;

      /* get the templates path */
      templates_path = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES));

      /* check if the templates directory is valid and is not home */
      homedir = g_get_home_dir ();

      if (G_UNLIKELY (templates_path == NULL) || g_strcmp0 (templates_path, homedir) == 0)
        {
          /* if not, fall back to "~/Templates" */
          templates_path = g_build_filename (homedir, "Templates", NULL);
        }

      /* get and empty the "Templates" submenu */
      application = gtk_window_get_application (GTK_WINDOW (window));
      menu = gtk_application_get_menu_by_id (application, "file.new-from-template");
      g_menu_remove_all (menu);

      /* check if the directory exists */
      if (g_file_test (templates_path, G_FILE_TEST_IS_DIR))
        {
          /* fill the menu */
          mousepad_window_menu_templates_fill (window, menu, templates_path);

          /* set the tooltips */
          gtkmenu = mousepad_object_get_data (G_OBJECT (menu), window->gtkmenu_key);
          offset = GPOINTER_TO_INT (mousepad_object_get_data (G_OBJECT (menu), window->offset_key));
          mousepad_window_menu_set_tooltips (window, gtkmenu, G_MENU_MODEL (menu), &offset);
        }
      else
        {
          message = g_strdup_printf (_("Missing Templates directory\n'%s'"), templates_path);
          item = g_menu_item_new (message, "win.insensitive");
          g_free (message);
          g_menu_append_item (menu, item);
          g_object_unref (item);
        }

      /* cleanup */
      g_free (templates_path);

      /* unlock */
      lock_menu_updates--;
    }
}



static void
mousepad_window_menu_tab_sizes_update (MousepadWindow *window)
{
  GtkApplication *application;
  GtkWidget      *gtkmenu;
  GMenuModel     *model;
  GMenuItem      *item;
  gint32          tab_size;
  gint            tab_size_n, nitem, nitems, offset;
  gchar          *text = NULL;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* avoid menu actions */
  lock_menu_updates++;

  /* get tab size of active document */
  tab_size = MOUSEPAD_SETTING_GET_INT (TAB_WIDTH);

  /* get the number of items in the tab-size submenu */
  application = gtk_window_get_application (GTK_WINDOW (window));
  model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "document.tab.tab-size"));
  nitems = g_menu_model_get_n_items (model);

  /* check if there is a default item with this tab size */
  for (nitem = 0; nitem < nitems; nitem++)
    {
      tab_size_n = atoi (g_variant_get_string (
                     g_menu_model_get_item_attribute_value (model, nitem, "label", NULL), NULL));
      if (tab_size_n == tab_size)
        break;
    }

  if (nitem == nitems)
    {
      /* create suitable text label for the "Other" menu */
      text = g_strdup_printf (_("Ot_her (%d)..."), tab_size);
      tab_size = 0;
    }

  /* toggle the action */
  g_action_group_change_action_state (G_ACTION_GROUP (window), "document.tab.tab-size",
                                      g_variant_new_int32 (tab_size));

  /* set the "Other" menu label */
  item = g_menu_item_new_from_model (model, nitems - 1);
  g_menu_item_set_label (item, text ? text : _("Ot_her..."));
  g_menu_remove (G_MENU (model), nitems - 1);
  g_menu_append_item (G_MENU (model), item);
  g_object_unref (item);

  /* set the "Other" menu item tooltip */
  gtkmenu = mousepad_object_get_data (G_OBJECT (model), window->gtkmenu_key);
  offset = GPOINTER_TO_INT (mousepad_object_get_data (G_OBJECT (model), window->offset_key));
  mousepad_window_menu_set_tooltips (window, gtkmenu, model, &offset);

  /* cleanup */
  g_free (text);

  /* allow menu actions again */
  lock_menu_updates--;
}



static void
mousepad_window_menu_textview_shown (GtkMenu        *menu,
                                     MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect this handler */
  mousepad_disconnect_by_func (menu, mousepad_window_menu_textview_shown, window);

  /* empty the original menu */
  mousepad_util_container_clear (GTK_CONTAINER (menu));

  /* move the textview menu children into the other menu */
  mousepad_util_container_move_children (GTK_CONTAINER (window->textview_menu),
                                         GTK_CONTAINER (menu));
}



static void
mousepad_window_menu_textview_deactivate (GtkWidget      *menu,
                                          MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect this handler */
  mousepad_disconnect_by_func (G_OBJECT (menu), mousepad_window_menu_textview_deactivate, window);

  /* copy the menu children back into the textview menu */
  mousepad_util_container_move_children (GTK_CONTAINER (menu),
                                         GTK_CONTAINER (window->textview_menu));
}



static void
mousepad_window_menu_textview_popup (GtkTextView    *textview,
                                     GtkMenu        *menu,
                                     MousepadWindow *window)
{
  g_return_if_fail (GTK_IS_TEXT_VIEW (textview));
  g_return_if_fail (GTK_IS_MENU (menu));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* connect signal */
  g_signal_connect (G_OBJECT (menu), "show",
                    G_CALLBACK (mousepad_window_menu_textview_shown), window);
  g_signal_connect (G_OBJECT (menu), "deactivate",
                    G_CALLBACK (mousepad_window_menu_textview_deactivate), window);
}



static void
mousepad_window_update_actions (MousepadWindow *window)
{
  GAction            *action;
  GtkNotebook        *notebook;
  MousepadDocument   *document;
  GtkSourceLanguage  *language;
  MousepadLineEnding  line_ending;
  gboolean            cycle_tabs, sensitive, value;
  gint                n_pages, page_num;
  const gchar        *language_id;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  notebook = GTK_NOTEBOOK (window->notebook);
  document = window->active;

  /* update the actions for the active document */
  if (G_LIKELY (document))
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* determine the number of pages and the current page number */
      n_pages = gtk_notebook_get_n_pages (notebook);
      page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (document));

      /* whether we cycle tabs */
      cycle_tabs = MOUSEPAD_SETTING_GET_BOOLEAN (CYCLE_TABS);

      /* set the sensitivity of the back and forward buttons in the go menu */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.previous-tab");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   (cycle_tabs && n_pages > 1) || page_num > 0);

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "document.next-tab");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   (cycle_tabs && n_pages > 1 ) || page_num < n_pages - 1);

      /* set the reload, detach and save sensitivity */
      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   ! mousepad_file_get_read_only (document->file));

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.detach-tab");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), n_pages > 1);

      action = g_action_map_lookup_action (G_ACTION_MAP (window), "file.reload");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                   mousepad_file_get_filename (document->file) != NULL);

      /* set the sensitivity of the undo and redo actions */
      mousepad_window_can_undo (window, NULL, G_OBJECT (document->buffer));
      mousepad_window_can_redo (window, NULL, G_OBJECT (document->buffer));

      /* set the current line ending type */
      line_ending = mousepad_file_get_line_ending (document->file);
      g_action_group_change_action_state (G_ACTION_GROUP (window), "document.line-ending",
                                          g_variant_new_int32 (line_ending));

      /* write bom */
      value = mousepad_file_get_write_bom (document->file, &sensitive);
      g_action_group_change_action_state (G_ACTION_GROUP (window), "document.write-unicode-bom",
                                          g_variant_new_boolean (value));

      /* update the currently active language */
      language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (window->active->buffer));
      language_id = language ? gtk_source_language_get_id (language) : "plain-text";
      g_action_group_change_action_state (G_ACTION_GROUP (window), "document.filetype",
                                          g_variant_new_string (language_id));

      /* update document dependent menu items */
      mousepad_window_update_document_menu_items (window);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



void
mousepad_window_update_document_menu_items (MousepadWindow *window)
{
  GtkApplication *application;
  GtkWidget      *gtkmenu;
  GtkToolItem    *tool_item;
  GMenu          *menu;
  GMenuItem      *item;
  GIcon          *icon;
  const gchar    *icon_name, *tooltip;
  gint            nitems, offset;
  gboolean        modified;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* prevent menu updates */
  lock_menu_updates++;

  /* get the save section in the "File" menu */
  application = gtk_window_get_application (GTK_WINDOW (window));
  menu = gtk_application_get_menu_by_id (application, "file.save");
  nitems = g_menu_model_get_n_items (G_MENU_MODEL (menu));

  /* set the "Reload/Revert" menu item */
  item = g_menu_item_new_from_model (G_MENU_MODEL (menu), nitems - 1);
  modified = gtk_text_buffer_get_modified (window->active->buffer);
  g_menu_item_set_label (item, modified ? _("Re_vert") : _("Re_load"));

  icon_name = modified ? "document-revert" : "view-refresh";
  icon = g_icon_new_for_string (icon_name, NULL);
  g_menu_item_set_icon (item, icon);
  g_object_unref (icon);

  tooltip = modified ? _("Revert to the saved version of the file") : _("Reload file from disk");
  g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));

  /* insert menu item in the "File" menu and update tooltip */
  g_menu_remove (menu, nitems - 1);
  g_menu_append_item (menu, item);

  gtkmenu = mousepad_object_get_data (G_OBJECT (menu), window->gtkmenu_key);
  offset = GPOINTER_TO_INT (mousepad_object_get_data (G_OBJECT (menu), window->offset_key));
  mousepad_window_menu_set_tooltips (window, gtkmenu, G_MENU_MODEL (menu), &offset);

  /* insert menu item in the "Tab" menu and update tooltip */
  menu = gtk_application_get_menu_by_id (application, "tab-menu.reload");
  g_menu_remove (menu, 0);
  g_menu_prepend_item (menu, item);
  g_object_unref (item);

  gtkmenu = mousepad_object_get_data (G_OBJECT (menu), window->gtkmenu_key);
  offset = GPOINTER_TO_INT (mousepad_object_get_data (G_OBJECT (menu), window->offset_key));
  mousepad_window_menu_set_tooltips (window, gtkmenu, G_MENU_MODEL (menu), &offset);

  /* allow menu actions again */
  lock_menu_updates--;

  /* update the "Reload/Revert" toolbar item */
  tool_item = gtk_toolbar_get_nth_item (GTK_TOOLBAR (window->toolbar), 4);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (tool_item), icon_name);
  gtk_tool_item_set_tooltip_text (tool_item, tooltip);
}



static void
mousepad_window_update_gomenu (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  MousepadDocument *document;
  GtkApplication   *application;
  GtkWidget        *gtkmenu;
  GMenu            *menu;
  GMenuItem        *item;
  const gchar      *label, *tooltip;
  gchar            *action_name, *accelerator;
  gint              n_pages, n, offset;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* opening the menu */
  if (g_variant_get_boolean (value))
    {
      /* prevent menu updates */
      lock_menu_updates++;

      /* get and empty the "Go to tab" submenu */
      application = gtk_window_get_application (GTK_WINDOW (window));
      menu = gtk_application_get_menu_by_id (application, "document.go-to-tab");
      g_menu_remove_all (menu);

      /* walk through the notebook pages */
      n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

      for (n = 0; n < n_pages; ++n)
        {
          document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), n));

          /* add the item to the "Go to Tab" submenu */
          label = mousepad_document_get_basename (document);
          action_name = g_strdup_printf ("win.document.go-to-tab(%d)", n);
          item = g_menu_item_new (label, action_name);
          tooltip = mousepad_document_get_filename (document);
          if (tooltip)
            g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));
          g_free (action_name);

          if (G_LIKELY (n < 9))
            {
              /* create an accelerator and add it to the menu item */
              accelerator = g_strdup_printf ("<Alt>%d", n + 1);
              g_menu_item_set_attribute_value (item, "accel", g_variant_new_string (accelerator));
              g_free (accelerator);
            }

          /* add the menu item */
          g_menu_append_item (menu, item);
          g_object_unref (item);

          /* select the active entry */
          if (gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook)) == n)
            g_action_group_change_action_state (G_ACTION_GROUP (window), "document.go-to-tab",
                                                g_variant_new_int32 (n));
        }

      /* update the tooltips */
      gtkmenu = mousepad_object_get_data (G_OBJECT (menu), window->gtkmenu_key);
      offset = GPOINTER_TO_INT (mousepad_object_get_data (G_OBJECT (menu), window->offset_key));
      mousepad_window_menu_set_tooltips (window, gtkmenu, G_MENU_MODEL (menu), &offset);

      /* release our lock */
      lock_menu_updates--;
    }
}



/**
 * Funtions for managing the recent files
 **/
static void
mousepad_window_recent_add (MousepadWindow *window,
                            MousepadFile   *file)
{
  GtkRecentData  info;
  gchar         *uri;
  gchar         *description;
  const gchar   *charset;
  static gchar  *groups[] = { (gchar *) PACKAGE_NAME, NULL };

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_FILE (file));

  /* get the charset */
  charset = mousepad_encoding_get_charset (mousepad_file_get_encoding (file));

  /* build description */
  description = g_strdup_printf ("%s: %s", _("Charset"), charset);

  /* create the recent data */
  info.display_name = NULL;
  info.description  = (gchar *) description;
  info.mime_type    = (gchar *) "text/plain";
  info.app_name     = (gchar *) PACKAGE_NAME;
  info.app_exec     = (gchar *) PACKAGE " %u";
  info.groups       = groups;
  info.is_private   = FALSE;

  /* create an uri from the filename */
  uri = mousepad_file_get_uri (file);

  if (G_LIKELY (uri != NULL))
    {
      /* make sure the recent manager is initialized */
      mousepad_window_recent_manager_init (window);

      /* add the new recent info to the recent manager */
      gtk_recent_manager_add_full (window->recent_manager, uri, &info);

      /* cleanup */
      g_free (uri);
    }

  /* cleanup */
  g_free (description);
}



static gint
mousepad_window_recent_sort (GtkRecentInfo *a,
                             GtkRecentInfo *b)
{
  return (gtk_recent_info_get_modified (a) < gtk_recent_info_get_modified (b));
}



static void
mousepad_window_recent_manager_init (MousepadWindow *window)
{
  /* set recent manager if not already done */
  if (G_UNLIKELY (window->recent_manager == NULL))
    window->recent_manager = gtk_recent_manager_get_default ();
}



static void
mousepad_window_recent_menu (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkApplication *application;
  GtkWidget      *gtkmenu;
  GMenu          *menu;
  GMenuItem      *menu_item;
  GAction        *subaction;
  GtkRecentInfo  *info;
  GList          *items, *li, *filtered = NULL;
  const gchar    *uri, *display_name;
  gchar          *label, *filename, *filename_utf8, *tooltip, *action_name;
  gint            n, offset;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* opening the menu */
  if (g_variant_get_boolean (value))
    {
      /* avoid updating the menu */
      lock_menu_updates++;

      /* get and empty the "Recent" submenu */
      application = gtk_window_get_application (GTK_WINDOW (window));
      menu = gtk_application_get_menu_by_id (application, "file.open-recent.list");
      g_menu_remove_all (menu);

      /* make sure the recent manager is initialized */
      mousepad_window_recent_manager_init (window);

      /* get all the items in the manager */
      items = gtk_recent_manager_get_items (window->recent_manager);

      /* walk through the items in the manager and pick the ones that are in the mousepad group */
      for (li = items; li != NULL; li = li->next)
        {
          /* check if the item is in the Mousepad group */
          if (! gtk_recent_info_has_group (li->data, PACKAGE_NAME))
            continue;

          /* insert the list, sorted by date */
          filtered = g_list_insert_sorted (filtered, li->data,
                                           (GCompareFunc) mousepad_window_recent_sort);
        }

      /* get the recent menu limit number */
      n = MOUSEPAD_SETTING_GET_INT (RECENT_MENU_ITEMS);

      /* append the items to the menu */
      for (li = filtered; n > 0 && li != NULL; li = li->next)
        {
          info = li->data;

          /* get the filename */
          uri = gtk_recent_info_get_uri (info);
          filename = g_filename_from_uri (uri, NULL, NULL);

          /* append to the menu if the file exists, else remove it from the history */
          if (filename && g_file_test (filename, G_FILE_TEST_EXISTS))
            {
              /* link the info data to the action via the unique detailed action name */
              action_name = g_strconcat ("win.file.open-recent.new('", filename, "')", NULL);
              subaction = g_action_map_lookup_action (G_ACTION_MAP (window), "file.open-recent.new");
              mousepad_object_set_data_full (G_OBJECT (subaction), g_intern_string (action_name),
                                             gtk_recent_info_ref (info), gtk_recent_info_unref);

              /* get the name of the item and escape the underscores */
              display_name = gtk_recent_info_get_display_name (info);
              label = mousepad_util_escape_underscores (display_name);

              /* create an utf-8 valid version of the filename for the tooltip */
              filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
              tooltip = g_strdup_printf (_("Open '%s'"), filename_utf8);
              g_free (filename_utf8);

              /* append menu item */
              menu_item = g_menu_item_new (label, action_name);
              g_menu_item_set_attribute_value (menu_item, "tooltip", g_variant_new_string (tooltip));
              g_menu_append_item (menu, menu_item);
              g_object_unref (menu_item);
              g_free (label);
              g_free (action_name);
              g_free (tooltip);

              /* update couters */
              n--;
            }
          else
            {
              /* remove the item. don't both the user if this fails */
              gtk_recent_manager_remove_item (window->recent_manager, uri, NULL);
            }

          /* cleanup */
          g_free (filename);
        }

      /* add the "No items found" insensitive menu item */
      if (! filtered)
        {
          menu_item = g_menu_item_new (_("No items found"), "win.insensitive");
          g_menu_append_item (menu, menu_item);
          g_object_unref (menu_item);
        }
      /* set the tooltips */
      else
        {
          gtkmenu = mousepad_object_get_data (G_OBJECT (menu), window->gtkmenu_key);
          offset = GPOINTER_TO_INT (mousepad_object_get_data (G_OBJECT (menu), window->offset_key));
          mousepad_window_menu_set_tooltips (window, gtkmenu, G_MENU_MODEL (menu), &offset);
        }

      /* set the sensitivity of the clear button */
      subaction = g_action_map_lookup_action (G_ACTION_MAP (window),
                                              "file.open-recent.clear-history");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (subaction), filtered != NULL);

      /* cleanup */
      g_list_free_full (items, (GDestroyNotify) gtk_recent_info_unref);
      g_list_free (filtered);

      /* allow menu updates again */
      lock_menu_updates--;
    }
}



static const gchar *
mousepad_window_recent_get_charset (GtkRecentInfo *info)
{
  const gchar *description;
  const gchar *charset = NULL;
  guint        offset;

  /* get the description */
  description = gtk_recent_info_get_description (info);
  if (G_LIKELY (description))
    {
      /* get the offset length: 'Encoding: ' */
      offset = strlen (_("Charset")) + 2;

      /* check if the encoding string looks valid, if so, set it */
      if (G_LIKELY (strlen (description) > offset))
        charset = description + offset;
    }

  return charset;
}



static void
mousepad_window_recent_clear (MousepadWindow *window)
{
  GList         *items, *li;
  const gchar   *uri;
  GError        *error = NULL;
  GtkRecentInfo *info;

  /* make sure the recent manager is initialized */
  mousepad_window_recent_manager_init (window);

  /* get all the items in the manager */
  items = gtk_recent_manager_get_items (window->recent_manager);

  /* walk through the items */
  for (li = items; li != NULL; li = li->next)
    {
      info = li->data;

      /* check if the item is in the Mousepad group */
      if (!gtk_recent_info_has_group (info, PACKAGE_NAME))
        continue;

      /* get the uri of the recent item */
      uri = gtk_recent_info_get_uri (info);

      /* try to remove it, if it fails, break the loop to avoid multiple errors */
      if (G_UNLIKELY (gtk_recent_manager_remove_item (window->recent_manager, uri, &error) == FALSE))
        break;
     }

  /* cleanup */
  g_list_free_full (items, (GDestroyNotify) gtk_recent_info_unref);

  /* print a warning is there is one */
  if (G_UNLIKELY (error != NULL))
    {
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to clear the recent history"));
      g_error_free (error);
    }
}



/**
 * Drag and drop functions
 **/
static void
mousepad_window_drag_data_received (GtkWidget        *widget,
                                    GdkDragContext   *context,
                                    gint              x,
                                    gint              y,
                                    GtkSelectionData *selection_data,
                                    guint             info,
                                    guint             drag_time,
                                    MousepadWindow   *window)
{
  GtkWidget  *notebook, **document;
  GtkWidget  *child, *label;
  gchar     **uris;
  gint        i, n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  /* we only accept text/uri-list drops with format 8 and atleast one byte of data */
  if (info == TARGET_TEXT_URI_LIST
      && gtk_selection_data_get_format (selection_data) == 8
      && gtk_selection_data_get_length (selection_data) > 0)
    {
      /* extract the uris from the data */
      uris = g_uri_list_extract_uris ((const gchar *) gtk_selection_data_get_data (selection_data));

      /* open the files */
      mousepad_window_open_files (window, uris);

      /* cleanup */
      g_strfreev (uris);

      /* finish the drag (copy) */
      gtk_drag_finish (context, TRUE, FALSE, drag_time);
    }
  else if (info == TARGET_GTK_NOTEBOOK_TAB)
    {
      /* get the source notebook */
      notebook = gtk_drag_get_source_widget (context);

      /* get the document that has been dragged */
      document = (GtkWidget **) gtk_selection_data_get_data (selection_data);

      /* check */
      g_return_if_fail (MOUSEPAD_IS_DOCUMENT (*document));

      /* take a reference on the document before we remove it */
      g_object_ref (G_OBJECT (*document));

      /* remove the document from the source window */
      gtk_notebook_detach_tab (GTK_NOTEBOOK (notebook), *document);

      /* get the number of pages in the notebook */
      n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

      /* figure out where to insert the tab in the notebook */
      for (i = 0; i < n_pages; i++)
        {
          GtkAllocation alloc = { 0, 0, 0, 0 };

          /* get the child label */
          child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);
          label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (window->notebook), child);

          gtk_widget_get_allocation (label, &alloc);

          /* break if we have a matching drop position */
          if (x < (alloc.x + alloc.width / 2))
            break;
        }

      /* add the document to the new window */
      mousepad_window_add (window, MOUSEPAD_DOCUMENT (*document));

      /* move the tab to the correct position */
      gtk_notebook_reorder_child (GTK_NOTEBOOK (window->notebook), *document, i);

      /* release our reference on the document */
      g_object_unref (G_OBJECT (*document));

      /* finish the drag (move) */
      gtk_drag_finish (context, TRUE, TRUE, drag_time);
    }
}



/**
 * Find and replace
 **/
static void
mousepad_window_search (MousepadWindow      *window,
                        MousepadSearchFlags  flags,
                        const gchar         *string,
                        const gchar         *replacement)
{
  GtkWidget *document;
  gint       n;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* multi-document mode */
  if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS)
    for (n = 0; n < gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)); n++)
      {
        /* search in the nth document */
        document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), n);
        mousepad_document_search (MOUSEPAD_DOCUMENT (document), string, replacement, flags);
      }
  /* search in the active document */
  else
    mousepad_document_search (window->active, string, replacement, flags);
}



static gboolean
mousepad_window_scroll_to_cursor (MousepadWindow *window)
{
  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), FALSE);

  /* if there is a request to scroll to cursor just before closing, this test could fail */
  if (G_LIKELY (MOUSEPAD_IS_WINDOW (window) && MOUSEPAD_IS_DOCUMENT (window->active)
                && MOUSEPAD_IS_VIEW (window->active->textview)))
    mousepad_view_scroll_to_cursor (window->active->textview);

  return FALSE;
}



static void
mousepad_window_search_completed (MousepadWindow      *window,
                                  gint                 n_matches_doc,
                                  const gchar         *string,
                                  MousepadSearchFlags  flags,
                                  MousepadDocument    *document)
{
  static GList *documents = NULL, *n_matches_docs = NULL;
  static gchar *multi_string = NULL;
  static gint   n_matches = 0, n_documents = 0;
  GList        *up_doc, *up_match;
  gint          index;

  /* always send the active document result, although it will only be relevant for the
   * search bar if the multi-document mode is active */
  if (document == window->active)
    g_signal_emit (window, window_signals[SEARCH_COMPLETED], 0, n_matches_doc, string,
                   flags & (~ MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS));

  /* multi-document mode is active: collect the search results regardless of their origin:
   * replace dialog, search bar or buffer change */
  if (MOUSEPAD_IS_REPLACE_DIALOG (window->replace_dialog)
      && MOUSEPAD_SETTING_GET_BOOLEAN (SEARCH_REPLACE_ALL)
      && MOUSEPAD_SETTING_GET_INT (SEARCH_REPLACE_ALL_LOCATION) == 2)
    {
      /* update the current search string or exit if the search is irrelevant */
      if (flags & MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS)
        {
          g_free (multi_string);
          multi_string = g_strdup (string);
        }
      else if (g_strcmp0 (multi_string, string) != 0)
        return;

      /* remove obsolete document results */
      if (documents != NULL)
        {
          for (up_doc = documents, up_match = n_matches_docs; up_doc != NULL;)
            if (gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook), up_doc->data) == -1)
              {
                /* update data */
                n_documents--;
                n_matches -= GPOINTER_TO_INT (up_match->data);

                /* remove the result from the list */
                up_match->data = GINT_TO_POINTER (-1);
                n_matches_docs = g_list_remove (n_matches_docs, up_match->data);
                up_match = n_matches_docs;

                /* remove the document from the list */
                documents = g_list_remove (documents, up_doc->data);
                up_doc = documents;

                /* complementary exit condition */
                if (documents == NULL)
                  break;
              }
            /* walk the list */
            else
              {
                up_doc = up_doc->next;
                up_match = up_match->next;
              }
        }

      /* update an old document result */
      if (documents != NULL && (index = g_list_index (documents, document)) != -1)
        {
          up_match = g_list_nth (n_matches_docs, index);
          n_matches += n_matches_doc - GPOINTER_TO_INT (up_match->data);
          up_match->data = GINT_TO_POINTER (n_matches_doc);
        }
      /* add a new document result, and wait until all documents have completed their search
       * to send the final result to the replace dialog */
      else
        {
          /* update lists */
          documents = g_list_prepend (documents, document);
          n_matches_docs = g_list_prepend (n_matches_docs, GINT_TO_POINTER (n_matches_doc));
          n_matches += n_matches_doc;

          /* exit if all documents have not yet send their result */
          if (++n_documents < gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)))
            return;
        }

      /* send the final result, only relevant for the replace dialog */
      g_signal_emit (window, window_signals[SEARCH_COMPLETED], 0, n_matches, string,
                     flags | MOUSEPAD_SEARCH_FLAGS_AREA_ALL_DOCUMENTS);
    }

  /* make sure the selection is visible whenever idle */
  if (n_matches_doc > 0)
    g_idle_add (G_SOURCE_FUNC (mousepad_window_scroll_to_cursor), window);
}



/**
 * Paste from History
 **/
static void
mousepad_window_paste_history_add (MousepadWindow *window)
{
  GtkClipboard *clipboard;
  gchar        *text;
  GSList       *li;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the current clipboard text */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  text = gtk_clipboard_wait_for_text (clipboard);

  /* leave when there is no text */
  if (G_UNLIKELY (text == NULL))
    return;

  /* check if the item is already in the history */
  for (li = clipboard_history; li != NULL; li = li->next)
    if (strcmp (li->data, text) == 0)
      break;

  /* append the item or remove it */
  if (G_LIKELY (li == NULL))
    {
      /* add to the list */
      clipboard_history = g_slist_prepend (clipboard_history, text);

      /* get the 10th item from the list and remove it if it exists */
      li = g_slist_nth (clipboard_history, 10);
      if (li != NULL)
        {
          /* cleanup */
          g_free (li->data);
          clipboard_history = g_slist_delete_link (clipboard_history, li);
        }
    }
  else
    {
      /* already in the history, remove it */
      g_free (text);
    }
}



#if !GTK_CHECK_VERSION (3, 22, 0)
static void
mousepad_window_paste_history_menu_position (GtkMenu  *menu,
                                             gint     *x,
                                             gint     *y,
                                             gboolean *push_in,
                                             gpointer  user_data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (user_data);
  MousepadDocument *document = window->active;
  GtkTextIter       iter;
  GtkTextMark      *mark;
  GdkRectangle      location;
  gint              iter_x, iter_y;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));
  g_return_if_fail (GTK_IS_TEXT_VIEW (document->textview));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (document->buffer));

  /* get the root coordinates of the texview widget */
  gdk_window_get_origin (gtk_text_view_get_window (GTK_TEXT_VIEW (document->textview), GTK_TEXT_WINDOW_TEXT), x, y);

  /* get the cursor iter */
  mark = gtk_text_buffer_get_insert (document->buffer);
  gtk_text_buffer_get_iter_at_mark (document->buffer, &iter, mark);

  /* get iter location */
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (document->textview), &iter, &location);

  /* convert to textview coordinates */
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (document->textview), GTK_TEXT_WINDOW_TEXT,
                                         location.x, location.y, &iter_x, &iter_y);

  /* add the iter coordinates to the menu popup position */
  *x += iter_x;
  *y += iter_y + location.height;
}
#endif



static void
mousepad_window_paste_history_activate (GtkMenuItem    *item,
                                        MousepadWindow *window)
{
  const gchar *text;

  g_return_if_fail (GTK_IS_MENU_ITEM (item));
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));
  g_return_if_fail (MOUSEPAD_IS_VIEW (window->active->textview));

  /* get the menu item text */
  text = mousepad_object_get_data (G_OBJECT (item), "history-pointer");

  /* paste the text */
  if (G_LIKELY (text))
    mousepad_view_clipboard_paste (window->active->textview, text, FALSE);
}



static GtkWidget *
mousepad_window_paste_history_menu_item (const gchar *text,
                                         const gchar *mnemonic)
{
  GtkWidget   *item;
  GtkWidget   *label;
  GtkWidget   *hbox;
  const gchar *s;
  gchar       *label_str;
  GString     *string;

  /* create new label string */
  string = g_string_sized_new (PASTE_HISTORY_MENU_LENGTH);

  /* get the first 30 chars of the clipboard text */
  if (g_utf8_strlen (text, -1) > PASTE_HISTORY_MENU_LENGTH)
    {
      /* append the first 30 chars */
      s = g_utf8_offset_to_pointer (text, PASTE_HISTORY_MENU_LENGTH);
      string = g_string_append_len (string, text, s - text);

      /* make it look like a ellipsized string */
      string = g_string_append (string, "...");
    }
  else
    {
      /* append the entire string */
      string = g_string_append (string, text);
    }

  /* get the string */
  label_str = g_string_free (string, FALSE);

  /* replace tab and new lines with spaces */
  label_str = g_strdelimit (label_str, "\n\t\r", ' ');

  /* create a new item */
  item = gtk_menu_item_new ();

  /* create a hbox */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 14);
  gtk_container_add (GTK_CONTAINER (item), hbox);
  gtk_widget_show (hbox);

  /* create the clipboard label */
  label = gtk_label_new (label_str);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_widget_show (label);

  /* create the mnemonic label */
  label = gtk_label_new_with_mnemonic (mnemonic);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), item);
  gtk_widget_show (label);

  /* cleanup */
  g_free (label_str);

  return item;
}



static GtkWidget *
mousepad_window_paste_history_menu (MousepadWindow *window)
{
  GSList       *li;
  gchar        *text;
  gpointer      list_data = NULL;
  GtkWidget    *item;
  GtkWidget    *menu;
  GtkClipboard *clipboard;
  gchar         mnemonic[4];
  gint          n;

  g_return_val_if_fail (MOUSEPAD_IS_WINDOW (window), NULL);

  /* create new menu and set the screen */
  menu = gtk_menu_new ();
  g_object_ref_sink (G_OBJECT (menu));
  g_signal_connect (G_OBJECT (menu), "deactivate", G_CALLBACK (g_object_unref), NULL);
  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (window)));

  /* get the current clipboard text */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window), GDK_SELECTION_CLIPBOARD);
  text = gtk_clipboard_wait_for_text (clipboard);

  /* append the history items */
  for (li = clipboard_history, n = 1; li != NULL; li = li->next)
    {
      /* skip the active clipboard item */
      if (G_UNLIKELY (list_data == NULL && text && strcmp (li->data, text) == 0))
        {
          /* store the pointer so we can attach it at the end of the menu */
          list_data = li->data;
        }
      else
        {
          /* create mnemonic string */
          g_snprintf (mnemonic, sizeof (mnemonic), "_%d", n++);

          /* create menu item */
          item = mousepad_window_paste_history_menu_item (li->data, mnemonic);
          mousepad_object_set_data (G_OBJECT (item), "history-pointer", li->data);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (mousepad_window_paste_history_activate), window);
          gtk_widget_show (item);
        }
    }

  /* cleanup */
  g_free (text);

  if (list_data != NULL)
    {
      /* add separator between history and active menu items */
      if (mousepad_util_container_has_children (GTK_CONTAINER (menu)))
        {
          item = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);
        }

      /* create menu item for current clipboard text */
      item = mousepad_window_paste_history_menu_item (list_data, "_0");
      mousepad_object_set_data (G_OBJECT (item), "history-pointer", list_data);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      g_signal_connect (G_OBJECT (item), "activate",
                        G_CALLBACK (mousepad_window_paste_history_activate), window);
      gtk_widget_show (item);
    }
  else if (! mousepad_util_container_has_children (GTK_CONTAINER (menu)))
    {
      /* create an item to inform the user */
      item = gtk_menu_item_new_with_label (_("No clipboard data"));
      gtk_widget_set_sensitive (item, FALSE);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  return menu;
}



/**
 * Menu Actions
 *
 * All those function should be sorted by the menu structure so it's
 * easy to find a function. The function can always use window->active, since
 * we can assume there is always an active document inside a window.
 **/
static void
mousepad_window_action_new (GSimpleAction *action,
                            GVariant      *value,
                            gpointer       data)
{
  MousepadDocument *document;
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* create new document */
  document = mousepad_document_new ();

  /* add the document to the window */
  mousepad_window_add (window, document);
}



static void
mousepad_window_action_new_window (GSimpleAction *action,
                                   GVariant      *value,
                                   gpointer       data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* emit the new window signal */
  g_signal_emit (data, window_signals[NEW_WINDOW], 0);
}



static void
mousepad_window_action_new_from_template (GSimpleAction *action,
                                          GVariant      *value,
                                          gpointer       data)
{
  MousepadWindow    *window = MOUSEPAD_WINDOW (data);
  MousepadDocument  *document;
  GtkSourceLanguage *language;
  const gchar       *filename, *message;
  GError            *error = NULL;
  gint               result;


  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the language from the filename */
  filename = g_variant_get_string (value, NULL);
  language = gtk_source_language_manager_guess_language (
               gtk_source_language_manager_get_default (), filename, NULL);

  /* test if the file exists */
  if (G_LIKELY (filename))
    {
      /* create new document */
      document = mousepad_document_new ();

      /* sink floating object */
      g_object_ref_sink (G_OBJECT (document));

      /* lock the undo manager */
      gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

      /* try to load the template into the buffer */
      result = mousepad_file_open (document->file, filename, &error);

      /* release the lock */
      gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

      /* handle the result */
      if (G_LIKELY (result == 0))
        {
          /* no errors, insert the document */
          mousepad_window_add (window, document);
          mousepad_file_set_language (window->active->file, language);
        }
      else
        {
          /* release the document */
          g_object_unref (G_OBJECT (document));

          /* handle the error */
          switch (result)
            {
              case ERROR_NOT_UTF8_VALID:
              case ERROR_CONVERTING_FAILED:
                /* set error message */
                message = _("Templates should be UTF-8 valid");
                break;

              case ERROR_READING_FAILED:
                /* set error message */
                message = _("Reading the template failed");
                break;

              default:
                /* set error message */
                message = _("Loading the template failed");
                break;
            }

          /* show the error */
          mousepad_dialogs_show_error (GTK_WINDOW (window), error, message);
          g_clear_error (&error);
        }
    }
}



static void
mousepad_window_action_open (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GSList         *filenames, *li;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* run the dialog */
  if (G_LIKELY (mousepad_dialogs_open (GTK_WINDOW (window),
                                       mousepad_file_get_filename (window->active->file),
                                       &filenames)
                == GTK_RESPONSE_ACCEPT))
    {
      /* lock menu updates */
      lock_menu_updates++;

      /* open all the selected filenames in new tabs */
      for (li = filenames; li != NULL; li = li->next)
        mousepad_window_open_file (window, li->data, MOUSEPAD_ENCODING_UTF_8);

      /* cleanup */
      g_slist_free_full (filenames, g_free);

      /* allow menu updates again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_open_recent (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  const gchar      *uri, *charset;
  MousepadEncoding  encoding;
  GError           *error = NULL;
  gchar            *filename, *action_name;
  gboolean          succeed = FALSE;
  GtkRecentInfo    *info;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get the info */
  action_name = g_strconcat ("win.file.open-recent.new('",
                             g_variant_get_string (value, NULL),
                             "')", NULL);
  info = mousepad_object_get_data (G_OBJECT (action), action_name);
  g_free (action_name);

  if (G_LIKELY (info != NULL))
    {
      /* get the file uri */
      uri = gtk_recent_info_get_uri (info);

      /* build a filename from the uri */
      filename = g_filename_from_uri (uri, NULL, NULL);

      if (G_LIKELY (filename != NULL))
        {
          /* open the file in a new tab if it exists */
          if (g_file_test (filename, G_FILE_TEST_EXISTS))
            {
              /* try to get the charset from the description */
              charset = mousepad_window_recent_get_charset (info);

              /* lookup the encoding */
              encoding = mousepad_encoding_find (charset);

              /* try to open the file */
              succeed = mousepad_window_open_file (window, filename, encoding);
            }
          else
            {
              /* create an error */
              g_set_error (&error,  G_FILE_ERROR, G_FILE_ERROR_IO,
                           _("Failed to open \"%s\" for reading. It will be "
                             "removed from the document history"), filename);

              /* show the warning and cleanup */
              mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to open file"));
              g_error_free (error);
            }

          /* cleanup */
          g_free (filename);

          /* update the document history */
          if (G_LIKELY (succeed))
            gtk_recent_manager_add_item (window->recent_manager, uri);
          else
            gtk_recent_manager_remove_item (window->recent_manager, uri, NULL);
        }
    }
}



static void
mousepad_window_action_clear_recent (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* ask the user if he really want to clear the history */
  if (mousepad_dialogs_clear_recent (GTK_WINDOW (window)))
    {
      /* avoid updating the menu */
      lock_menu_updates++;

      /* clear the document history */
      mousepad_window_recent_clear (window);

      /* allow menu updates again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_save (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  MousepadDocument *document = window->active;
  GError           *error = NULL;
  GVariant         *v_succeed;
  gboolean          succeed = FALSE, modified;
  gint              response;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  if (mousepad_file_get_filename (document->file) == NULL)
    {
      /* file has no filename yet, open the save as dialog */
      g_action_group_activate_action (G_ACTION_GROUP (window), "file.save-as", NULL);
      v_succeed = g_action_group_get_action_state (G_ACTION_GROUP (window), "file.save-as");
      succeed = g_variant_get_int32 (v_succeed);
      g_variant_unref (v_succeed);
    }
  else
    {
      /* check whether the file is externally modified */
      modified = mousepad_file_get_externally_modified (document->file, &error);
      if (G_UNLIKELY (error != NULL))
        goto showerror;

      if (modified)
        {
          /* ask the user what to do */
          response = mousepad_dialogs_externally_modified (GTK_WINDOW (window));
        }
      else
        {
          /* save */
          response = MOUSEPAD_RESPONSE_SAVE;
        }

      switch (response)
        {
          case MOUSEPAD_RESPONSE_CANCEL:
            /* do nothing */
            succeed = FALSE;
            break;

          case MOUSEPAD_RESPONSE_SAVE_AS:
            /* run save as dialog */
            g_action_group_activate_action (G_ACTION_GROUP (window), "file.save-as", NULL);
            v_succeed = g_action_group_get_action_state (G_ACTION_GROUP (window), "file.save-as");
            succeed = g_variant_get_int32 (v_succeed);
            g_variant_unref (v_succeed);
            break;

          case MOUSEPAD_RESPONSE_SAVE:
            /* save the document */
            succeed = mousepad_file_save (document->file, &error);
            break;
        }

      if (G_UNLIKELY (succeed == FALSE) && error != NULL)
        {
          showerror:

          /* show the error */
          mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to save the document"));
          g_error_free (error);
        }
    }

  /* store the save result as the action state */
  g_action_change_state (G_ACTION (action), g_variant_new_int32 (succeed));
}



static void
mousepad_window_action_save_as (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  MousepadDocument *document = window->active;
  GAction          *action_save;
  GVariant         *v_succeed;
  gchar            *current_filename, *filename;
  gboolean          succeed = FALSE;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* store a copy of the current filename to restore it in case of failure */
  current_filename = g_strdup (mousepad_file_get_filename (document->file));

  /* run the dialog */
  if (mousepad_dialogs_save_as (GTK_WINDOW (window),
                                mousepad_file_get_filename (document->file),
                                last_save_location, &filename)
      == GTK_RESPONSE_OK && G_LIKELY (filename))
    {
      /* virtually set the new filename */
      mousepad_file_set_filename (document->file, filename, FALSE);

      /* save the file by an internal call (the save action may be disabled, depending
       * on the file status) */
      action_save = g_action_map_lookup_action (G_ACTION_MAP (window), "file.save");
      mousepad_window_action_save (G_SIMPLE_ACTION (action_save), NULL, window);
      v_succeed = g_action_group_get_action_state (G_ACTION_GROUP (window), "file.save");
      succeed = g_variant_get_int32 (v_succeed);
      g_variant_unref (v_succeed);

      if (G_LIKELY (succeed))
        {
          /* validate filename change */
          mousepad_file_set_filename (document->file, filename, TRUE);

          /* add to the recent history */
          mousepad_window_recent_add (window, document->file);

          /* update last save location */
          g_free (last_save_location);
          last_save_location = g_path_get_dirname (filename);
        }
      /* revert filename change */
      else
        mousepad_file_set_filename (document->file, current_filename, FALSE);
    }

  /* cleanup */
  g_free (current_filename);
  g_free (filename);

  /* store the save result as the action state */
  g_action_change_state (G_ACTION (action), g_variant_new_int32 (succeed));
}



static void
mousepad_window_action_save_all (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  MousepadDocument *document;
  GSList           *li, *documents = NULL;
  GError           *error = NULL;
  gboolean          succeed = TRUE;
  gint              i, current, page_num;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the current active tab */
  current = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));

  /* walk though all the document in the window */
  for (i = 0; i < gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)); i++)
    {
      /* get the document */
      document = MOUSEPAD_DOCUMENT (gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i));

      /* debug check */
      g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

      /* continue if the document is not modified */
      if (! gtk_text_buffer_get_modified (document->buffer))
        continue;

      /* we try to quickly save files, without bothering the user */
      if (mousepad_file_get_filename (document->file) != NULL
          && mousepad_file_get_read_only (document->file) == FALSE
          && mousepad_file_get_externally_modified (document->file, NULL) == FALSE)
        {
          /* try to quickly save the file */
          succeed = mousepad_file_save (document->file, &error);

          /* break on problems */
          if (G_UNLIKELY (!succeed))
            break;
        }
      else
        {
          /* add the document to a queue to bother the user later */
          documents = g_slist_prepend (documents, document);
        }
    }

  if (G_UNLIKELY (succeed == FALSE))
    {
      /* focus the tab that triggered the problem */
      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

      /* show the error */
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to save the document"));

      /* free error */
      if (error != NULL)
        g_error_free (error);
    }
  else
    {
      /* open a save as dialog for all the unnamed files */
      for (li = documents; li != NULL; li = li->next)
        {
          document = MOUSEPAD_DOCUMENT (li->data);

          /* get the documents page number */
          page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook), GTK_WIDGET (li->data));

          if (G_LIKELY (page_num > -1))
            {
              /* focus the tab we're going to save */
              gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page_num);

              if (mousepad_file_get_filename (document->file) == NULL
                  || mousepad_file_get_read_only (document->file))
                {
                  /* trigger the save as function */
                  g_action_group_activate_action (G_ACTION_GROUP (window), "file.save-as", NULL);
                }
              else
                {
                  /* trigger the save function (externally modified document) */
                  g_action_group_activate_action (G_ACTION_GROUP (window), "file.save", NULL);
                }
            }
        }

      /* focus the origional doc if everything went fine */
      if (G_LIKELY (li == NULL))
        gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), current);
    }

  /* cleanup */
  g_slist_free (documents);
}



static void
mousepad_window_action_reload (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       data)
{
  MousepadWindow   *window = MOUSEPAD_WINDOW (data);
  MousepadDocument *document = window->active;
  GVariant         *v_succeed;
  GError           *error = NULL;
  gint              response;
  gboolean          succeed;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* ask the user if he really wants to do this when the file is modified */
  if (gtk_text_buffer_get_modified (document->buffer))
    {
      /* ask the user if he really wants to revert */
      response = mousepad_dialogs_revert (GTK_WINDOW (window));

      if (response == MOUSEPAD_RESPONSE_SAVE_AS)
        {
          /* open the save as dialog */
          g_action_group_activate_action (G_ACTION_GROUP (window), "file.save-as", NULL);
          v_succeed = g_action_group_get_action_state (G_ACTION_GROUP (window), "file.save-as");
          succeed = g_variant_get_int32 (v_succeed);
          g_variant_unref (v_succeed);

          /* exit regardless of save status */
          return;
        }
      /* revert cancelled */
      else if (response == MOUSEPAD_RESPONSE_CANCEL || response < 0)
        return;
    }

  /* lock the undo manager */
  gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  /* reload the file */
  succeed = mousepad_file_reload (document->file, &error);

  /* release the lock */
  gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (document->buffer));

  if (G_UNLIKELY (succeed == FALSE))
    {
      /* show the error */
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to reload the document"));
      g_error_free (error);
    }
}



static void
mousepad_window_action_print (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  MousepadPrint  *print;
  GError         *error = NULL;
  gboolean        succeed;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* create new print operation */
  print = mousepad_print_new ();

  /* print the current document */
  succeed = mousepad_print_document_interactive (print, window->active, GTK_WINDOW (window), &error);

  if (G_UNLIKELY (succeed == FALSE))
    {
      /* show the error */
      mousepad_dialogs_show_error (GTK_WINDOW (window), error, _("Failed to print the document"));
      g_error_free (error);
    }

  /* release the object */
  g_object_unref (G_OBJECT (print));
}



static void
mousepad_window_action_detach (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* invoke function without cooridinates */
  mousepad_window_notebook_create_window (GTK_NOTEBOOK (window->notebook),
                                          GTK_WIDGET (window->active),
                                          -1, -1, window);
}



static void
mousepad_window_action_close (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* close active document */
  mousepad_window_close_document (window, window->active);
}



static void
mousepad_window_action_close_window (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkWidget      *document;
  gint            npages, i;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the number of page in the notebook */
  npages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook)) - 1;

  /* prevent menu updates */
  lock_menu_updates++;

  /* ask what to do with the modified document in this window */
  for (i = npages; i >= 0; --i)
    {
      /* get the document */
      document = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->notebook), i);

      /* check for debug builds */
      g_return_if_fail (MOUSEPAD_IS_DOCUMENT (document));

      /* focus the tab we're going to close */
      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), i);

      /* close each document */
      if (! mousepad_window_close_document (window, MOUSEPAD_DOCUMENT (document)))
        {
          /* closing cancelled, release menu lock */
          lock_menu_updates--;

          /* leave function */
          return;
        }
    }

  /* release lock */
  lock_menu_updates--;
}



static void
mousepad_window_action_undo (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* undo */
  gtk_source_buffer_undo (GTK_SOURCE_BUFFER (window->active->buffer));

  /* scroll to visible area */
  mousepad_view_scroll_to_cursor (window->active->textview);
}



static void
mousepad_window_action_redo (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* redo */
  gtk_source_buffer_redo (GTK_SOURCE_BUFFER (window->active->buffer));

  /* scroll to visible area */
  mousepad_view_scroll_to_cursor (window->active->textview);
}



static void
mousepad_window_action_cut (GSimpleAction *action,
                            GVariant      *value,
                            gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkEditable    *entry;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  /* cut from search bar entry or textview */
  if (G_UNLIKELY (entry))
    gtk_editable_cut_clipboard (entry);
  else
    mousepad_view_clipboard_cut (window->active->textview);

  /* update the history */
  mousepad_window_paste_history_add (window);
}



static void
mousepad_window_action_copy (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkEditable    *entry;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  /* copy from search bar entry or textview */
  if (G_UNLIKELY (entry))
    gtk_editable_copy_clipboard (entry);
  else
    mousepad_view_clipboard_copy (window->active->textview);

  /* update the history */
  mousepad_window_paste_history_add (window);
}



static void
mousepad_window_action_paste (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkEditable    *entry;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  /* paste in search bar entry or textview */
  if (G_UNLIKELY (entry))
    gtk_editable_paste_clipboard (entry);
  else
    mousepad_view_clipboard_paste (window->active->textview, NULL, FALSE);
}



static void
mousepad_window_action_paste_history (GSimpleAction *action,
                                      GVariant      *value,
                                      gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkWidget      *menu;
#if GTK_CHECK_VERSION (3, 22, 0)
  GdkRectangle    location;
#endif

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get the history menu */
  menu = mousepad_window_paste_history_menu (window);

  /* select the first item in the menu */
  gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), TRUE);

#if GTK_CHECK_VERSION (3, 22, 0)
  /* get cursor location in textview coordinates */
  gtk_text_view_get_cursor_locations (GTK_TEXT_VIEW (window->active->textview), NULL, &location, NULL);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (window->active->textview),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         location.x, location.y,
                                         &(location.x), &(location.y));

  /* popup the menu */
  gtk_menu_popup_at_rect (GTK_MENU (menu),
                          gtk_widget_get_parent_window (GTK_WIDGET (window->active->textview)),
                          &location, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
#else

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  /* popup the menu */
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                  mousepad_window_paste_history_menu_position,
                  window, 0, gtk_get_current_event_time ());

G_GNUC_END_IGNORE_DEPRECATIONS

#endif
}



static void
mousepad_window_action_paste_column (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* paste the clipboard into a column */
  mousepad_view_clipboard_paste (window->active->textview, NULL, TRUE);
}



static void
mousepad_window_action_delete (GSimpleAction *action,
                               GVariant      *value,
                               gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkEditable *entry;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* get searchbar entry */
  entry = mousepad_search_bar_entry (MOUSEPAD_SEARCH_BAR (window->search_bar));

  /* delete selection in search bar entry or textview */
  if (G_UNLIKELY (entry))
    gtk_editable_delete_selection (entry);
  else
    mousepad_view_delete_selection (window->active->textview);
}



static void
mousepad_window_action_select_all (GSimpleAction *action,
                                   GVariant      *value,
                                   gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* select everything in the document */
  mousepad_view_select_all (window->active->textview);
}



static void
mousepad_window_action_preferences (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  GtkWindow      *window = GTK_WINDOW (data);
  GtkApplication *application;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  application = gtk_window_get_application (window);
  mousepad_application_show_preferences (MOUSEPAD_APPLICATION (application), window);
}



static void
mousepad_window_action_lowercase (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to lowercase */
  mousepad_view_convert_selection_case (window->active->textview, LOWERCASE);
}



static void
mousepad_window_action_uppercase (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to uppercase */
  mousepad_view_convert_selection_case (window->active->textview, UPPERCASE);
}



static void
mousepad_window_action_titlecase (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to titlecase */
  mousepad_view_convert_selection_case (window->active->textview, TITLECASE);
}



static void
mousepad_window_action_opposite_case (GSimpleAction *action,
                                      GVariant      *value,
                                      gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert selection to opposite case */
  mousepad_view_convert_selection_case (window->active->textview, OPPOSITE_CASE);
}



static void
mousepad_window_action_tabs_to_spaces (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert tabs to spaces */
  mousepad_view_convert_spaces_and_tabs (window->active->textview, TABS_TO_SPACES);
}



static void
mousepad_window_action_spaces_to_tabs (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert spaces to tabs */
  mousepad_view_convert_spaces_and_tabs (window->active->textview, SPACES_TO_TABS);
}



static void
mousepad_window_action_strip_trailing_spaces (GSimpleAction *action,
                                              GVariant      *value,
                                              gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* convert spaces to tabs */
  mousepad_view_strip_trailing_spaces (window->active->textview);
}



static void
mousepad_window_action_transpose (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* transpose */
  mousepad_view_transpose (window->active->textview);
}



static void
mousepad_window_action_move_line_up (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* move the selection on line up */
  mousepad_view_move_selection (window->active->textview, MOVE_LINE_UP);
}



static void
mousepad_window_action_move_line_down (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* move the selection on line down */
  mousepad_view_move_selection (window->active->textview, MOVE_LINE_DOWN);
}



static void
mousepad_window_action_duplicate (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* dupplicate */
  mousepad_view_duplicate (window->active->textview);
}



static void
mousepad_window_action_increase_indent (GSimpleAction *action,
                                        GVariant      *value,
                                        gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* increase the indent */
  mousepad_view_indent (window->active->textview, INCREASE_INDENT);
}



static void
mousepad_window_action_decrease_indent (GSimpleAction *action,
                                        GVariant      *value,
                                        gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* decrease the indent */
  mousepad_view_indent (window->active->textview, DECREASE_INDENT);
}



static void
mousepad_window_hide_search_bar (MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));
  g_return_if_fail (MOUSEPAD_IS_SEARCH_BAR (window->search_bar));

  /* hide the search bar */
  gtk_widget_hide (window->search_bar);

  /* set the window property if no search widget is visible */
  if (! MOUSEPAD_IS_REPLACE_DIALOG (window->replace_dialog)
      || ! gtk_widget_get_visible (window->replace_dialog))
    g_object_set (window, "search-widget-visible", FALSE, NULL);

  /* focus the active document's text view */
  mousepad_document_focus_textview (window->active);
}



static void
mousepad_window_action_find (GSimpleAction *action,
                             GVariant      *value,
                             gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkTextIter     selection_start, selection_end;
  gchar          *selection;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* create a new search bar is needed */
  if (window->search_bar == NULL)
    {
      /* create a new toolbar and pack it into the box */
      window->search_bar = mousepad_search_bar_new ();
      gtk_box_pack_start (GTK_BOX (window->box), window->search_bar, FALSE, FALSE, PADDING);

      /* connect signals */
      g_signal_connect_swapped (window->search_bar, "hide-bar",
                                G_CALLBACK (mousepad_window_hide_search_bar), window);
      g_signal_connect_swapped (window->search_bar, "search",
                                G_CALLBACK (mousepad_window_search), window);
    }

  /* set the search entry text */
  if (gtk_text_buffer_get_has_selection (window->active->buffer) == TRUE)
    {
      gtk_text_buffer_get_selection_bounds (window->active->buffer,
                                            &selection_start, &selection_end);
      selection = gtk_text_buffer_get_text (window->active->buffer,
                                            &selection_start, &selection_end, 0);

      /* selection should be one line */
      if (g_strrstr (selection, "\n") == NULL && g_strrstr (selection, "\r") == NULL)
        mousepad_search_bar_set_text (MOUSEPAD_SEARCH_BAR (window->search_bar), selection);

      g_free (selection);
    }

  if (! gtk_widget_get_visible (window->search_bar))
    {
      /* show the search bar */
      gtk_widget_show (window->search_bar);

      /* set the window property if no search widget was visible */
      if (! MOUSEPAD_IS_REPLACE_DIALOG (window->replace_dialog)
          || ! gtk_widget_get_visible (window->replace_dialog))
        g_object_set (window, "search-widget-visible", TRUE, NULL);
    }

  /* focus the search entry */
  mousepad_search_bar_focus (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_next (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* find the next occurrence */
  if (G_LIKELY (window->search_bar != NULL))
    mousepad_search_bar_find_next (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_action_find_previous (GSimpleAction *action,
                                      GVariant      *value,
                                      gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* find the previous occurrence */
  if (G_LIKELY (window->search_bar != NULL))
    mousepad_search_bar_find_previous (MOUSEPAD_SEARCH_BAR (window->search_bar));
}



static void
mousepad_window_replace_dialog_switch_page (MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_REPLACE_DIALOG (window->replace_dialog));

  mousepad_replace_dialog_page_switched (MOUSEPAD_REPLACE_DIALOG (window->replace_dialog));
}



static void
mousepad_window_replace_dialog_destroy (MousepadWindow *window)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* disconnect tab switch signal */
  mousepad_disconnect_by_func (G_OBJECT (window->notebook),
                               mousepad_window_replace_dialog_switch_page, window);

  /* reset the dialog variable */
  window->replace_dialog = NULL;

  /* set the window property if no search widget is visible */
  if (! MOUSEPAD_IS_SEARCH_BAR (window->search_bar)
      || ! gtk_widget_get_visible (window->search_bar))
    g_object_set (window, "search-widget-visible", FALSE, NULL);
}



static void
mousepad_window_action_replace (GSimpleAction *action,
                                GVariant      *value,
                                gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkTextIter     selection_start, selection_end;
  gchar          *selection;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  if (window->replace_dialog == NULL)
    {
      /* create a new dialog */
      window->replace_dialog = mousepad_replace_dialog_new (window);

      /* connect signals */
      g_signal_connect_swapped (window->replace_dialog, "destroy",
                                G_CALLBACK (mousepad_window_replace_dialog_destroy), window);
      g_signal_connect_swapped (window->replace_dialog, "search",
                                G_CALLBACK (mousepad_window_search), window);
      g_signal_connect_swapped (window->notebook, "switch-page",
                                G_CALLBACK (mousepad_window_replace_dialog_switch_page), window);

      /* set the window property if no search widget was visible */
      if (! MOUSEPAD_IS_SEARCH_BAR (window->search_bar)
          || ! gtk_widget_get_visible (window->search_bar))
        g_object_set (window, "search-widget-visible", TRUE, NULL);
    }
  else
    {
      /* focus the existing dialog */
      gtk_window_present (GTK_WINDOW (window->replace_dialog));
    }

  /* set the search entry text */
  if (gtk_text_buffer_get_has_selection (window->active->buffer) == TRUE)
    {
      gtk_text_buffer_get_selection_bounds (window->active->buffer,
                                            &selection_start, &selection_end);
      selection = gtk_text_buffer_get_text (window->active->buffer,
                                            &selection_start, &selection_end, 0);

      /* selection should be one line */
      if (g_strrstr (selection, "\n") == NULL && g_strrstr (selection, "\r") == NULL)
        mousepad_replace_dialog_set_text (MOUSEPAD_REPLACE_DIALOG (window->replace_dialog),
                                          selection);

      g_free (selection);
    }
}



static void
mousepad_window_action_go_to_position (GSimpleAction *action,
                                       GVariant      *value,
                                       gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (window->active->buffer));

  /* run jump dialog */
  if (mousepad_dialogs_go_to (GTK_WINDOW (window), window->active->buffer))
    {
      /* put the cursor on screen */
      mousepad_view_scroll_to_cursor (window->active->textview);
    }
}



static void
mousepad_window_action_select_font (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkWidget      *dialog;
  gchar          *font_name;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  dialog = gtk_font_chooser_dialog_new (_("Choose Mousepad Font"), GTK_WINDOW (window));

  /* set the current font name */
  font_name = MOUSEPAD_SETTING_GET_STRING (FONT_NAME);

  if (G_LIKELY (font_name))
    {
      gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), font_name);
      g_free (font_name);
    }

  /* run the dialog */
  if (G_LIKELY (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK))
    {
      /* get the selected font from the dialog */
      font_name = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (dialog));

      /* store the font in the preferences */
      MOUSEPAD_SETTING_SET_STRING (FONT_NAME, font_name);
      /* stop using default font */
      MOUSEPAD_SETTING_SET_BOOLEAN (USE_DEFAULT_FONT, FALSE);

      /* cleanup */
      g_free (font_name);
    }

  /* destroy dialog */
  gtk_widget_destroy (dialog);
}



static void
mousepad_window_action_bar_activate (GSimpleAction *action,
                                     GVariant      *value,
                                     gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  gchar          *setting;
  gboolean        state;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  state = ! g_variant_get_boolean (g_action_get_state (G_ACTION (action)));

  if (mousepad_window_get_in_fullscreen (window))
    {
      setting = g_strconcat (g_action_get_name (G_ACTION (action)), "-in-fullscreen", NULL);
      mousepad_setting_set_boolean (setting, state ? YES : NO);
      g_free (setting);
    }
  else
    mousepad_setting_set_boolean (g_action_get_name (G_ACTION (action)), state);
}



static void
mousepad_window_action_textview (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  g_action_group_activate_action (G_ACTION_GROUP (data), MENUBAR, NULL);
}



static void
mousepad_window_action_menubar_state (GSimpleAction *action,
                                      GVariant      *state,
                                      gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkApplication *application;
  GtkWidget      *label;
  GAction        *textview_action;
  GMenuModel     *model;
  GList          *children, *child;
  static GList   *mnemonics = NULL;
  gpointer        mnemonic;
  gboolean        visible;
  gint            offset;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* set the action state */
  g_simple_action_set_state (action, state);

  /* show/hide the "Menubar" item in the text view menu */
  visible = g_variant_get_boolean (state);
  textview_action = g_action_map_lookup_action (G_ACTION_MAP (window), "textview.menubar");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (textview_action), ! visible);

  if (! visible)
    {
      /* set the textview menu last tooltip */
      application = gtk_window_get_application (GTK_WINDOW (window));
      model = G_MENU_MODEL (gtk_application_get_menu_by_id (application, "textview.menubar"));
      offset = GPOINTER_TO_INT (mousepad_object_get_data (G_OBJECT (model), window->offset_key));
      mousepad_window_menu_set_tooltips (window, window->textview_menu, model, &offset);

      /* get the main menubar mnemonic keys */
      children = gtk_container_get_children (GTK_CONTAINER (window->menubar));
      for (child = children; child != NULL; child = child->next)
        {
          label = gtk_bin_get_child (GTK_BIN (child->data));
          mnemonic = GUINT_TO_POINTER (gtk_label_get_mnemonic_keyval (GTK_LABEL (label)));
          mnemonics = g_list_prepend (mnemonics, mnemonic);
        }
      g_list_free (children);

      /* handle key events to show the menubar temporarily when hidden */
      g_signal_connect (G_OBJECT (window), "key-press-event",
                        G_CALLBACK (mousepad_window_menubar_key_event), mnemonics);
      g_signal_connect (G_OBJECT (window), "key-release-event",
                        G_CALLBACK (mousepad_window_menubar_key_event), mnemonics);
    }
  /* disconnect handlers that show the menubar temporarily when hidden */
  else
    {
      mousepad_disconnect_by_func (window, mousepad_window_menubar_key_event, mnemonics);
      mousepad_disconnect_by_func (window, mousepad_window_hide_menubar_event, NULL);
      mousepad_disconnect_by_func (window->menubar, mousepad_window_hide_menubar_event, window);
      mousepad_disconnect_by_func (window->notebook, mousepad_window_hide_menubar_event, window);
    }
}



static void
mousepad_window_action_fullscreen (GSimpleAction *action,
                                   GVariant      *value,
                                   gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  GtkApplication *application;
  GtkToolItem    *tool_item;
  GtkWidget      *gtkmenu;
  GMenu          *menu;
  GMenuItem      *item;
  GIcon          *icon = NULL;
  gboolean        fullscreen;
  const gchar    *icon_name, *tooltip;
  gint            offset;

  /* avoid menu actions */
  lock_menu_updates++;

  fullscreen = ! g_variant_get_boolean (g_action_get_state (G_ACTION (action)));
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (fullscreen));

  /* entering fullscreen mode */
  if (fullscreen)
    {
      gtk_window_fullscreen (GTK_WINDOW (window));
      icon_name = "view-restore";
      tooltip = _("Leave fullscreen mode");
    }
  /* leaving fullscreen mode */
  else
    {
      gtk_window_unfullscreen (GTK_WINDOW (window));
      icon_name = "view-fullscreen";
      tooltip = _("Make the window fullscreen");
    }

  /* update the menu item icon */
  application = gtk_window_get_application (GTK_WINDOW (window));
  menu = gtk_application_get_menu_by_id (application, "view.fullscreen");
  item = g_menu_item_new_from_model (G_MENU_MODEL (menu), 0);

  icon = g_icon_new_for_string (icon_name, NULL);
  g_menu_item_set_icon (item, icon);
  g_menu_item_set_attribute_value (item, "tooltip", g_variant_new_string (tooltip));
  g_object_unref (icon);

  /* append menu item */
  g_menu_remove (menu, 0);
  g_menu_prepend_item (menu, item);
  g_object_unref (item);

  /* update the tooltip */
  gtkmenu = mousepad_object_get_data (G_OBJECT (menu), window->gtkmenu_key);
  offset = GPOINTER_TO_INT (mousepad_object_get_data (G_OBJECT (menu), window->offset_key));
  mousepad_window_menu_set_tooltips (window, gtkmenu, G_MENU_MODEL (menu), &offset);

  /* update the toolbar item */
  tool_item = gtk_toolbar_get_nth_item (GTK_TOOLBAR (window->toolbar),
                                        gtk_toolbar_get_n_items (GTK_TOOLBAR (window->toolbar)) - 1);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (tool_item), icon_name);
  gtk_tool_item_set_tooltip_text (tool_item, tooltip);

  /* allow menu updates again */
  lock_menu_updates--;
}



static void
mousepad_window_action_line_ending (GSimpleAction *action,
                                    GVariant      *value,
                                    gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* set the current state and the new line ending on the file */
      g_action_change_state (G_ACTION (action), value);
      mousepad_file_set_line_ending (window->active->file, g_variant_get_int32 (value));

      /* make buffer as modified to show the user the change is not saved */
      gtk_text_buffer_set_modified (window->active->buffer, TRUE);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_tab_size (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  gint            tab_size;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* get the tab size */
      tab_size = g_variant_get_int32 (value);

      /* whether the other item was clicked */
      if (tab_size == 0)
        {
          /* get tab size from document */
          tab_size = MOUSEPAD_SETTING_GET_INT (TAB_WIDTH);

          /* select other size in dialog */
          tab_size = mousepad_dialogs_other_tab_size (GTK_WINDOW (window), tab_size);
        }

      /* store as last used value */
      MOUSEPAD_SETTING_SET_INT (TAB_WIDTH, tab_size);
    }
}



static void
mousepad_window_action_language (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow    *window = MOUSEPAD_WINDOW (data);
  GtkSourceLanguage *language;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      g_action_change_state (G_ACTION (action), value);
      language = gtk_source_language_manager_get_language (gtk_source_language_manager_get_default (),
                                                           g_variant_get_string (value, NULL));
      mousepad_file_set_language (window->active->file, language);

      /* mark the file as having its language chosen explicitly by the user
       * so we don't clobber their choice by guessing ourselves */
      mousepad_file_set_user_set_language (window->active->file, TRUE);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_write_bom (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  gboolean        state;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));
  g_return_if_fail (MOUSEPAD_IS_DOCUMENT (window->active));

  /* leave when menu updates are locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      /* set the current state */
      state = ! g_variant_get_boolean (g_action_get_state (G_ACTION (action)));
      g_action_change_state (G_ACTION (action), g_variant_new_boolean (state));

      /* set new value */
      mousepad_file_set_write_bom (window->active->file, state);

      /* make buffer as modified to show the user the change is not saved */
      gtk_text_buffer_set_modified (window->active->buffer, TRUE);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_prev_tab (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  gint            page_num, n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get notebook info */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* switch to the previous tab or cycle to the last tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), (page_num - 1) % n_pages);
}



static void
mousepad_window_action_next_tab (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);
  gint            page_num, n_pages;

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* get notebook info */
  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (window->notebook));
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (window->notebook));

  /* switch to the next tab or cycle to the first tab */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), (page_num + 1) % n_pages);
}



static void
mousepad_window_action_go_to_tab (GSimpleAction *action,
                                  GVariant      *value,
                                  gpointer       data)
{
  MousepadWindow *window = MOUSEPAD_WINDOW (data);

  g_return_if_fail (MOUSEPAD_IS_WINDOW (window));

  /* leave when the menu is locked */
  if (lock_menu_updates == 0)
    {
      /* avoid menu actions */
      lock_menu_updates++;

      gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), g_variant_get_int32 (value));
      g_action_change_state (G_ACTION (action), value);

      /* allow menu actions again */
      lock_menu_updates--;
    }
}



static void
mousepad_window_action_contents (GSimpleAction *action,
                                 GVariant      *value,
                                 gpointer       data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* show help */
  mousepad_dialogs_show_help (GTK_WINDOW (data), NULL, NULL);
}



static void
mousepad_window_action_about (GSimpleAction *action,
                              GVariant      *value,
                              gpointer       data)
{
  g_return_if_fail (MOUSEPAD_IS_WINDOW (data));

  /* show about dialog */
  mousepad_dialogs_show_about (GTK_WINDOW (data));
}
