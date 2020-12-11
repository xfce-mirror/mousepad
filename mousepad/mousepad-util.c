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
#include <mousepad/mousepad-util.h>

#include <xfconf/xfconf.h>

#ifdef HAVE_MATH_H
#include <math.h>
#endif



#define DEFAULT_FONT "Monospace 10"
#define FONT_FAMILY  "font-family"
#define FONT_STYLE   "font-style"
#define FONT_VARIANT "font-variant"
#define FONT_STRETCH "font-stretch"
#define FONT_WEIGHT  "font-weight"
#define FONT_SIZE    "font-size"



static gboolean
mousepad_util_iter_word_characters (const GtkTextIter *iter)
{
  gunichar c;

  /* get the characters */
  c = gtk_text_iter_get_char (iter);

  /* character we'd like to see in a word */
  if (g_unichar_isalnum (c) || c == '_')
    return TRUE;

  return FALSE;
}



static gboolean
mousepad_util_iter_starts_word (const GtkTextIter *iter)
{
  GtkTextIter prev;

  /* normal gtk word start */
  if (!gtk_text_iter_starts_word (iter))
    return FALSE;

  /* init iter for previous char */
  prev = *iter;

  /* return true when we could not step backwards (start of buffer) */
  if (!gtk_text_iter_backward_char (&prev))
    return TRUE;

  /* check if the previous char also belongs to the word */
  if (mousepad_util_iter_word_characters (&prev))
    return FALSE;

  return TRUE;
}



static gboolean
mousepad_util_iter_ends_word (const GtkTextIter *iter)
{
  if (!gtk_text_iter_ends_word (iter))
    return FALSE;

  /* check if it's a character we'd like to see in a word */
  if (mousepad_util_iter_word_characters (iter))
    return FALSE;

  return TRUE;
}



gboolean
mousepad_util_iter_inside_word (const GtkTextIter *iter)
{
  GtkTextIter prev;

  /* not inside a word when at beginning or end of a word */
  if (mousepad_util_iter_starts_word (iter) || mousepad_util_iter_ends_word (iter))
    return FALSE;

  /* normal gtk function */
  if (gtk_text_iter_inside_word (iter))
    return TRUE;

  /* check if the character is a word char */
  if (!mousepad_util_iter_word_characters (iter))
    return FALSE;

  /* initialize previous iter */
  prev = *iter;

  /* get one character backwards */
  if (!gtk_text_iter_backward_char (&prev))
    return FALSE;

  return mousepad_util_iter_word_characters (&prev);
}



gboolean
mousepad_util_iter_forward_word_end (GtkTextIter *iter)
{
  if (mousepad_util_iter_ends_word (iter))
    return TRUE;

  while (gtk_text_iter_forward_char (iter))
    if (mousepad_util_iter_ends_word (iter))
      return TRUE;

  return mousepad_util_iter_ends_word (iter);
}



gboolean
mousepad_util_iter_backward_word_start (GtkTextIter *iter)
{
  /* return true if the iter already starts a word */
  if (mousepad_util_iter_starts_word (iter))
    return TRUE;

  /* move backwards until we find a word start */
  while (gtk_text_iter_backward_char (iter))
    if (mousepad_util_iter_starts_word (iter))
      return TRUE;

  /* while stops when we hit the first char in the buffer */
  return mousepad_util_iter_starts_word (iter);
}



gboolean
mousepad_util_iter_forward_text_start (GtkTextIter *iter)
{
  g_return_val_if_fail (!mousepad_util_iter_inside_word (iter), FALSE);

  /* keep until we hit text or a line end */
  while (g_unichar_isspace (gtk_text_iter_get_char (iter)))
    if (gtk_text_iter_ends_line (iter) || !gtk_text_iter_forward_char (iter))
      break;

  return TRUE;
}



gboolean
mousepad_util_iter_backward_text_start (GtkTextIter *iter)
{
  GtkTextIter prev = *iter;

  g_return_val_if_fail (!mousepad_util_iter_inside_word (iter), FALSE);

  while (!gtk_text_iter_starts_line (&prev) && gtk_text_iter_backward_char (&prev))
    {
      if (g_unichar_isspace (gtk_text_iter_get_char (&prev)))
        *iter = prev;
      else
        break;
    }

  return TRUE;
}



gchar *
mousepad_util_config_name (const gchar *name)
{
  const gchar *s;
  gchar       *config, *t;
  gboolean     upper = TRUE;

  /* allocate string */
  config = g_new (gchar, strlen (name) + 1);

  /* convert name */
  for (s = name, t = config; *s != '\0'; ++s)
    {
      if (*s == '-')
        {
          upper = TRUE;
        }
      else if (upper)
        {
          *t++ = g_ascii_toupper (*s);
          upper = FALSE;
        }
      else
        {
          *t++ = g_ascii_tolower (*s);
        }
    }

  /* zerro terminate string */
  *t = '\0';

  return config;
}



gchar *
mousepad_util_key_name (const gchar *name)
{
  const gchar *s;
  gchar       *key, *t;

  /* allocate string (max of 9 extra - chars) */
  key = g_new (gchar, strlen (name) + 10);

  /* convert name */
  for (s = name, t = key; *s != '\0'; ++s)
    {
      if (g_ascii_isupper (*s) && s != name)
        *t++ = '-';

      *t++ = g_ascii_tolower (*s);
    }

  /* zerro terminate string */
  *t = '\0';

  return key;
}



gchar *
mousepad_util_utf8_strcapital (const gchar *str)
{
  gunichar     c;
  const gchar *p;
  gchar       *buf;
  GString     *result;
  gboolean     upper = TRUE;

  g_return_val_if_fail (g_utf8_validate (str, -1, NULL), NULL);

  /* create a new string */
  result = g_string_sized_new (strlen (str));

  /* walk though the string */
  for (p = str; *p != '\0'; p = g_utf8_next_char (p))
    {
      /* get the unicode char */
      c = g_utf8_get_char (p);

      /* only change the case of alpha chars */
      if (g_unichar_isalpha (c))
        {
          /* check case */
          if (upper ? g_unichar_isupper (c) : g_unichar_islower (c))
            {
              /* currect case is already correct */
              g_string_append_unichar (result, c);
            }
          else
            {
              /* convert the case of the char and append it */
              buf = upper ? g_utf8_strup (p, 1) : g_utf8_strdown (p, 1);
              g_string_append (result, buf);
              g_free (buf);
            }

          /* next char must be lowercase */
          upper = FALSE;
        }
      else
        {
          /* append the char */
          g_string_append_unichar (result, c);

          /* next alpha char uppercase after a space */
          upper = g_unichar_isspace (c);
        }
    }

  /* return the result */
  return g_string_free (result, FALSE);
}



gchar *
mousepad_util_utf8_stropposite (const gchar *str)
{
  gunichar     c;
  const gchar *p;
  gchar       *buf;
  GString     *result;

  g_return_val_if_fail (g_utf8_validate (str, -1, NULL), NULL);

  /* create a new string */
  result = g_string_sized_new (strlen (str));

  /* walk though the string */
  for (p = str; *p != '\0'; p = g_utf8_next_char (p))
    {
      /* get the unicode char */
      c = g_utf8_get_char (p);

      /* only change the case of alpha chars */
      if (g_unichar_isalpha (c))
        {
          /* get the opposite case of the char */
          if (g_unichar_isupper (c))
            buf = g_utf8_strdown (p, 1);
          else
            buf = g_utf8_strup (p, 1);

          /* append to the buffer */
          g_string_append (result, buf);
          g_free (buf);
        }
      else
        {
          /* append the char */
          g_string_append_unichar (result, c);
        }
    }

  /* return the result */
  return g_string_free (result, FALSE);
}



gchar *
mousepad_util_escape_underscores (const gchar *str)
{
  GString     *result;
  const gchar *s;

  /* allocate a new string */
  result = g_string_sized_new (strlen (str));

  /* escape all underscores */
  for (s = str; *s != '\0'; ++s)
    {
      if (G_UNLIKELY (*s == '_'))
        g_string_append (result, "__");
      else
        g_string_append_c (result, *s);
    }

  return g_string_free (result, FALSE);
}



GtkWidget *
mousepad_util_image_button (const gchar *icon_name,
                            const gchar *label)
{
  GtkWidget *button, *image;

  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);

  button = gtk_button_new_with_mnemonic (label);
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_widget_show (button);

  return button;
}



void
mousepad_util_entry_error (GtkWidget *widget,
                           gboolean   error)
{
  gpointer pointer;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* get the current error state */
  pointer = mousepad_object_get_data (widget, "error-state");

  /* only change the state when really needed to avoid multiple widget calls */
  if (GPOINTER_TO_INT (pointer) != error)
    {
      /* set the widget style */
      /* see http://gtk.10911.n7.nabble.com/set-custom-entry-background-td88472.html#a88489 */
      if (error)
        gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_ERROR);
      else
        gtk_style_context_remove_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_ERROR);

      /* set the new state */
      mousepad_object_set_data (widget, "error-state", GINT_TO_POINTER (error));
    }
}



void
mousepad_util_dialog_create_header (GtkDialog   *dialog,
                                    const gchar *title,
                                    const gchar *subtitle,
                                    const gchar *icon_name)
{
  gchar     *formated_title, *full_title;
  GtkWidget *vbox, *hbox;
  GtkWidget *icon, *label, *line;
  GtkWidget *dialog_vbox;

  /* remove the main vbox */
  dialog_vbox = gtk_bin_get_child (GTK_BIN (dialog));
  g_object_ref (dialog_vbox);
  gtk_container_remove (GTK_CONTAINER (dialog), dialog_vbox);

  /* add a new vbox to the main window */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (dialog), vbox);
  gtk_widget_show (vbox);

  /* create a hbox */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_show (hbox);

  /* title icon */
  icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
  gtk_widget_show (icon);

  /* create the title */
  formated_title = g_strdup_printf ("<b><big>%s</big></b>", title);
  if (subtitle)
    {
      full_title = g_strconcat (formated_title, "\n", subtitle, NULL);
      g_free (formated_title);
    }
  else
    full_title = formated_title;

  /* title label */
  label = gtk_label_new (full_title);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  /* cleanup */
  g_free (full_title);

  /* add the separator between header and content */
  line = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (vbox), line, FALSE, FALSE, 0);
  gtk_widget_show (line);

  /* add the main dialog box to the new vbox */
  gtk_box_pack_start (GTK_BOX (vbox), dialog_vbox, TRUE, TRUE, 0);
  g_object_unref (dialog_vbox);
}



void
mousepad_util_dialog_update_header (GtkDialog   *dialog,
                                    const gchar *title,
                                    const gchar *subtitle,
                                    const gchar *icon_name)
{
  gchar     *formated_title, *full_title;
  GtkWidget *vbox, *hbox;
  GtkWidget *icon, *label;
  GList     *children, *child;

  /* retrieve the hbox */
  vbox = gtk_bin_get_child (GTK_BIN (dialog));
  children = gtk_container_get_children (GTK_CONTAINER (vbox));
  hbox = children->data;
  g_list_free (children);

  /* title icon */
  children = gtk_container_get_children (GTK_CONTAINER (hbox));
  child = children;
  icon = child->data;
  gtk_image_set_from_icon_name (GTK_IMAGE (icon), icon_name, GTK_ICON_SIZE_DIALOG);

  /* title label */
  formated_title = g_strdup_printf ("<b><big>%s</big></b>", title);
  if (subtitle)
    {
      full_title = g_strconcat (formated_title, "\n", subtitle, NULL);
      g_free (formated_title);
    }
  else
    full_title = formated_title;

  child = child->next;
  label = child->data;
  gtk_label_set_markup (GTK_LABEL (label), full_title);

  /* cleanup */
  g_free (full_title);
  g_list_free (children);
}



gint
mousepad_util_get_real_line_offset (const GtkTextIter *iter,
                                    gint               tab_size)
{
  gint        offset = 0;
  GtkTextIter needle = *iter;

  /* move the needle to the start of the line */
  gtk_text_iter_set_line_offset (&needle, 0);

  /* forward the needle until we hit the iter */
  while (!gtk_text_iter_equal (&needle, iter))
    {
      /* append the real tab offset or 1 */
      if (gtk_text_iter_get_char (&needle) == '\t')
        offset += (tab_size - (offset % tab_size));
      else
        offset++;

      /* next char */
      gtk_text_iter_forward_char (&needle);
    }

  return offset;
}



gboolean
mousepad_util_forward_iter_to_text (GtkTextIter       *iter,
                                    const GtkTextIter *limit)
{
  gunichar c;

  do
    {
      /* get the iter character */
      c = gtk_text_iter_get_char (iter);

      /* break if the character is not a space */
      if (!g_unichar_isspace (c) || c == '\n' || c == '\r')
        break;

      /* break when we reached the limit iter */
      if (limit && gtk_text_iter_equal (iter, limit))
        return FALSE;
    }
  while (gtk_text_iter_forward_char (iter));

  return TRUE;
}



gchar *
mousepad_util_get_save_location (const gchar *relpath,
                                 gboolean     create_parents)
{
  gchar *filename, *dirname;

  g_return_val_if_fail (g_get_user_config_dir () != NULL, NULL);

  /* create the full filename */
  filename = g_build_filename (g_get_user_config_dir (), relpath, NULL);

  /* test if the file exists */
  if (G_UNLIKELY (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE))
    {
      if (create_parents)
        {
          /* get the directory name */
          dirname = g_path_get_dirname (filename);

          /* make the directory with parents */
          if (g_mkdir_with_parents (dirname, 0700) == -1)
            {
              /* show warning to the user */
              g_critical ("Unable to create base directory \"%s\". "
                          "Saving to file \"%s\" will be aborted.", dirname, filename);

              /* don't return a filename, to avoid problems */
              g_free (filename);
              filename = NULL;
            }

          /* cleanup */
          g_free (dirname);
        }
      else
        {
          /* cleanup */
          g_free (filename);
          filename = NULL;
        }
    }

  return filename;
}



void
mousepad_util_save_key_file (GKeyFile    *keyfile,
                             const gchar *filename)
{
  gchar  *contents;
  gsize   length;
  GError *error = NULL;

  /* get the contents of the key file */
  contents = g_key_file_to_data (keyfile, &length, &error);

  if (G_LIKELY (error == NULL))
    {
      /* write the contents to the file */
      if (G_UNLIKELY (g_file_set_contents (filename, contents, length, &error) == FALSE))
        goto print_error;
    }
  else
    {
      print_error:

      /* print error */
      g_critical ("Failed to store the preferences to \"%s\": %s", filename, error->message);

      /* cleanup */
      g_error_free (error);
    }

  /* cleanup */
  g_free (contents);
}



static void
mousepad_util_container_foreach_counter (GtkWidget *widget,
                                         gpointer   data)
{
  guint *n_children = (guint *) data;

  *n_children = *n_children + 1;
}



gboolean
mousepad_util_container_has_children (GtkContainer *container)
{
  guint n_children = 0;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), FALSE);

  gtk_container_foreach (container,
                         mousepad_util_container_foreach_counter,
                         &n_children);

  return (n_children > 0);
}


void
mousepad_util_container_clear (GtkContainer *container)
{
  GList *list, *iter;

  g_return_if_fail (GTK_IS_CONTAINER (container));

  list = gtk_container_get_children (container);

  for (iter = list; iter != NULL; iter = g_list_next (iter))
    gtk_container_remove (container, iter->data);

  g_list_free (list);
}



void
mousepad_util_container_move_children (GtkContainer *source,
                                       GtkContainer *destination)
{
  GList *list, *iter;

  list = gtk_container_get_children (source);

  for (iter = list; iter != NULL; iter = g_list_next (iter))
    {
      GtkWidget *tmp = g_object_ref (iter->data);

      gtk_container_remove (source, tmp);
      gtk_container_add (destination, tmp);
      g_object_unref (tmp);
    }

  g_list_free (list);
}



static gint
mousepad_util_style_schemes_name_compare (gconstpointer a,
                                          gconstpointer b)
{
  const gchar *name_a, *name_b;

  if (G_UNLIKELY (!GTK_SOURCE_IS_STYLE_SCHEME (a)))
    return - (a != b);
  if (G_UNLIKELY (!GTK_SOURCE_IS_STYLE_SCHEME (b)))
    return a != b;

  name_a = gtk_source_style_scheme_get_name (GTK_SOURCE_STYLE_SCHEME (a));
  name_b = gtk_source_style_scheme_get_name (GTK_SOURCE_STYLE_SCHEME (b));

  return g_utf8_collate (name_a, name_b);
}



static GSList *
mousepad_util_get_style_schemes (void)
{
  GSList               *list = NULL;
  const gchar * const  *schemes;
  GtkSourceStyleScheme *scheme;

  schemes = gtk_source_style_scheme_manager_get_scheme_ids (
              gtk_source_style_scheme_manager_get_default ());

  while (*schemes)
    {
      scheme = gtk_source_style_scheme_manager_get_scheme (
                gtk_source_style_scheme_manager_get_default (), *schemes);
      list = g_slist_prepend (list, scheme);
      schemes++;
    }

  return list;
}



GSList *
mousepad_util_get_sorted_style_schemes (void)
{
  return g_slist_sort (mousepad_util_get_style_schemes (),
                       mousepad_util_style_schemes_name_compare);
}



GSList *
mousepad_util_get_sorted_language_sections (void)
{
  GSList                   *list = NULL;
  const gchar *const       *languages;
  GtkSourceLanguage        *language;
  GtkSourceLanguageManager *manager;

  manager = gtk_source_language_manager_get_default ();
  languages = gtk_source_language_manager_get_language_ids (manager);

  while (*languages)
    {
      language = gtk_source_language_manager_get_language (manager, *languages);
      if (G_LIKELY (GTK_SOURCE_IS_LANGUAGE (language)))
        {
          /* ignore hidden languages */
          if (gtk_source_language_get_hidden (language))
            {
              languages++;
              continue;
            }

          /* ensure no duplicates in list */
          if (!g_slist_find_custom (list,
                                    gtk_source_language_get_section (language),
                                    (GCompareFunc) g_strcmp0))
            {
              list = g_slist_prepend (list, (gchar *) gtk_source_language_get_section (language));
            }
        }
      languages++;
    }

  return g_slist_sort (list, (GCompareFunc) g_utf8_collate);
}



static gint
mousepad_util_languages_name_compare (gconstpointer a,
                                      gconstpointer b)
{
  const gchar *name_a, *name_b;

  if (G_UNLIKELY (!GTK_SOURCE_IS_LANGUAGE (a)))
    return - (a != b);
  if (G_UNLIKELY (!GTK_SOURCE_IS_LANGUAGE (b)))
    return a != b;

  name_a = gtk_source_language_get_name (GTK_SOURCE_LANGUAGE (a));
  name_b = gtk_source_language_get_name (GTK_SOURCE_LANGUAGE (b));

  return g_utf8_collate (name_a, name_b);
}



GSList *
mousepad_util_get_sorted_languages_for_section (const gchar *section)
{
  GSList                   *list = NULL;
  const gchar *const       *languages;
  GtkSourceLanguage        *language;
  GtkSourceLanguageManager *manager;

  g_return_val_if_fail (section != NULL, NULL);

  manager = gtk_source_language_manager_get_default ();
  languages = gtk_source_language_manager_get_language_ids (manager);

  while (*languages)
    {
      language = gtk_source_language_manager_get_language (manager, *languages);
      if (G_LIKELY (GTK_SOURCE_IS_LANGUAGE (language)))
        {
          /* ignore hidden languages */
          if (gtk_source_language_get_hidden (language))
            {
              languages++;
              continue;
            }

          /* only get languages in the specified section */
          if (g_strcmp0 (gtk_source_language_get_section (language), section) == 0)
            list = g_slist_prepend (list, language);
        }
      languages++;
    }

  return g_slist_sort (list, mousepad_util_languages_name_compare);
}



gchar *
mousepad_util_get_default_font (void)
{
  gchar *font;

  /* get the default font from xfconf */
  font = xfconf_channel_get_string (xfconf_channel_get ("xsettings"),
                                    "/Gtk/MonospaceFontName", NULL);
  if (G_UNLIKELY (font == NULL))
    font = g_strdup (DEFAULT_FONT);

  return font;
}



/*
 * Copied from Gedit 3.38.0 and slightly modified:
 * https://gitlab.gnome.org/GNOME/gedit/-/blob/21fac3f0c87db0db104d7af7eaeb6f63d8216a14/gedit/gedit-pango.c#L37
 */
/**
 * mousepad_util_pango_font_description_to_css:
 *
 * This function will generate CSS suitable for Gtk's CSS engine
 * based on the properties of the #PangoFontDescription.
 *
 * Returns: (transfer full): A newly allocated string containing the
 *    CSS describing the font description.
 */
gchar *
mousepad_util_pango_font_description_to_css (const PangoFontDescription *font_desc)
{
  PangoFontMask  mask;
  GString       *str;

#define ADD_KEYVAL(key,fmt) \
  g_string_append(str,key":"fmt";")
#define ADD_KEYVAL_PRINTF(key,fmt,...) \
  g_string_append_printf(str,key":"fmt";", __VA_ARGS__)

  g_return_val_if_fail (font_desc, NULL);

  str = g_string_new (NULL);

  mask = pango_font_description_get_set_fields (font_desc);

  if (mask & PANGO_FONT_MASK_FAMILY)
    {
      const gchar *family;

      family = pango_font_description_get_family (font_desc);
      ADD_KEYVAL_PRINTF (FONT_FAMILY, "\"%s\"", family);
    }

  if (mask & PANGO_FONT_MASK_STYLE)
    {
      PangoStyle style;

      style = pango_font_description_get_style (font_desc);

      switch (style)
        {
        case PANGO_STYLE_NORMAL:
          ADD_KEYVAL (FONT_STYLE, "normal");
          break;

        case PANGO_STYLE_OBLIQUE:
          ADD_KEYVAL (FONT_STYLE, "oblique");
          break;

        case PANGO_STYLE_ITALIC:
          ADD_KEYVAL (FONT_STYLE, "italic");
          break;

        default:
          break;
        }
    }

  if (mask & PANGO_FONT_MASK_VARIANT)
    {
      PangoVariant variant;

      variant = pango_font_description_get_variant (font_desc);

      switch (variant)
        {
        case PANGO_VARIANT_NORMAL:
          ADD_KEYVAL (FONT_VARIANT, "normal");
          break;

        case PANGO_VARIANT_SMALL_CAPS:
          ADD_KEYVAL (FONT_VARIANT, "small-caps");
          break;

        default:
          break;
        }
    }

  if (mask & PANGO_FONT_MASK_WEIGHT)
    {
      gint weight;

      weight = pango_font_description_get_weight (font_desc);

      /*
       * WORKAROUND:
       *
       * font-weight with numbers does not appear to be working as expected
       * right now. So for the common (bold/normal), let's just use the string
       * and let gtk warn for the other values, which shouldn't really be
       * used for this.
       */

      switch (weight)
        {
        case PANGO_WEIGHT_SEMILIGHT:
          /*
           * 350 is not actually a valid css font-weight, so we will just round
           * up to 400.
           */
        case PANGO_WEIGHT_NORMAL:
          ADD_KEYVAL (FONT_WEIGHT, "normal");
          break;

        case PANGO_WEIGHT_BOLD:
          ADD_KEYVAL (FONT_WEIGHT, "bold");
          break;

        case PANGO_WEIGHT_THIN:
        case PANGO_WEIGHT_ULTRALIGHT:
        case PANGO_WEIGHT_LIGHT:
        case PANGO_WEIGHT_BOOK:
        case PANGO_WEIGHT_MEDIUM:
        case PANGO_WEIGHT_SEMIBOLD:
        case PANGO_WEIGHT_ULTRABOLD:
        case PANGO_WEIGHT_HEAVY:
        case PANGO_WEIGHT_ULTRAHEAVY:
        default:
          /* round to nearest hundred */
          weight = round (weight / 100.0) * 100;
          ADD_KEYVAL_PRINTF (FONT_WEIGHT, "%d", weight);
          break;
        }
    }

#ifndef GDK_WINDOWING_QUARTZ
  /*
   * We seem to get "Condensed" for fonts on the Quartz backend,
   * which is rather annoying as it results in us always hitting
   * fallback (stretch) paths. So let's cheat and just disable
   * stretch support for now on Quartz.
   */
  if (mask & PANGO_FONT_MASK_STRETCH)
    {
      switch (pango_font_description_get_stretch (font_desc))
        {
        case PANGO_STRETCH_ULTRA_CONDENSED:
          ADD_KEYVAL (FONT_STRETCH, "ultra-condensed");
          break;

        case PANGO_STRETCH_EXTRA_CONDENSED:
          ADD_KEYVAL (FONT_STRETCH, "extra-condensed");
          break;

        case PANGO_STRETCH_CONDENSED:
          ADD_KEYVAL (FONT_STRETCH, "condensed");
          break;

        case PANGO_STRETCH_SEMI_CONDENSED:
          ADD_KEYVAL (FONT_STRETCH, "semi-condensed");
          break;

        case PANGO_STRETCH_NORMAL:
          ADD_KEYVAL (FONT_STRETCH, "normal");
          break;

        case PANGO_STRETCH_SEMI_EXPANDED:
          ADD_KEYVAL (FONT_STRETCH, "semi-expanded");
          break;

        case PANGO_STRETCH_EXPANDED:
          ADD_KEYVAL (FONT_STRETCH, "expanded");
          break;

        case PANGO_STRETCH_EXTRA_EXPANDED:
          ADD_KEYVAL (FONT_STRETCH, "extra-expanded");
          break;

        case PANGO_STRETCH_ULTRA_EXPANDED:
          ADD_KEYVAL (FONT_STRETCH, "ultra-expanded");
          break;

        default:
          break;
        }
    }
#endif

  if (mask & PANGO_FONT_MASK_SIZE)
    {
      gint font_size;

      font_size = pango_font_description_get_size (font_desc) / PANGO_SCALE;
      ADD_KEYVAL_PRINTF (FONT_SIZE, "%dpt", font_size);
    }

  return g_string_free (str, FALSE);

#undef ADD_KEYVAL
#undef ADD_KEYVAL_PRINTF
}



/* to be replaced by g_file_peek_path() when bumping GLib minimal version to 2.56 or higher */
const gchar *
mousepad_util_get_path (GFile *file)
{
#if ! GLIB_CHECK_VERSION (2, 56, 0)
  gchar       *path;
  const gchar *intern_path;
#endif

  g_return_val_if_fail (G_IS_FILE (file), NULL);

#if GLIB_CHECK_VERSION (2, 56, 0)
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  return g_file_peek_path (file);

G_GNUC_END_IGNORE_DEPRECATIONS
#else

  path = g_file_get_path (file);
  intern_path = g_intern_string (path);
  g_free (path);

  return intern_path;

#endif
}
