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

#ifndef __MOUSEPAD_FILE_H__
#define __MOUSEPAD_FILE_H__

G_BEGIN_DECLS

#include <mousepad/mousepad-encoding.h>

#include <gtksourceview/gtksource.h>


typedef struct _MousepadFileClass  MousepadFileClass;
typedef struct _MousepadFile       MousepadFile;

#define MOUSEPAD_TYPE_FILE            (mousepad_file_get_type ())
#define MOUSEPAD_FILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_FILE, MousepadFile))
#define MOUSEPAD_FILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_FILE, MousepadFileClass))
#define MOUSEPAD_IS_FILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_FILE))
#define MOUSEPAD_IS_FILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_FILE))
#define MOUSEPAD_FILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_FILE, MousepadFileClass))

#define MOUSEPAD_LANGUAGE_NONE "plain-text"

/* I/O errors */
enum
{
  ERROR_READING_FAILED     = -1,
  ERROR_CONVERTING_FAILED  = -2,
  ERROR_ENCODING_NOT_VALID = -3,
  ERROR_FILE_STATUS_FAILED = -4
};

/* line endings */
typedef enum
{
  MOUSEPAD_EOL_UNIX,
  MOUSEPAD_EOL_MAC,
  MOUSEPAD_EOL_DOS
}
MousepadLineEnding;

GType               mousepad_file_get_type                 (void) G_GNUC_CONST;

MousepadFile       *mousepad_file_new                      (GtkTextBuffer       *buffer);

void                mousepad_file_set_location             (MousepadFile        *file,
                                                            GFile               *location,
                                                            gboolean             real);

GFile              *mousepad_file_get_location             (MousepadFile        *file);

gboolean            mousepad_file_location_is_set          (MousepadFile        *file);

const gchar        *mousepad_file_get_path                 (MousepadFile        *file);

gchar              *mousepad_file_get_uri                  (MousepadFile        *file);

gboolean            mousepad_file_get_read_only            (MousepadFile        *file);

gboolean            mousepad_file_is_savable               (MousepadFile        *file);

void                mousepad_file_set_encoding             (MousepadFile        *file,
                                                            MousepadEncoding     encoding);

MousepadEncoding    mousepad_file_get_encoding             (MousepadFile        *file);

void                mousepad_file_set_write_bom            (MousepadFile        *file,
                                                            gboolean             write_bom);

gboolean            mousepad_file_get_write_bom            (MousepadFile        *file);

GtkTextBuffer      *mousepad_file_get_buffer               (MousepadFile        *file);

void                mousepad_file_set_line_ending          (MousepadFile        *file,
                                                            MousepadLineEnding   line_ending);

MousepadLineEnding  mousepad_file_get_line_ending          (MousepadFile        *file);

void                mousepad_file_set_language             (MousepadFile        *file,
                                                            const gchar         *language_id);

const gchar        *mousepad_file_get_language             (MousepadFile        *file);

gboolean            mousepad_file_get_user_set_language    (MousepadFile        *file);

gint                mousepad_file_open                     (MousepadFile        *file,
                                                            gint                 line,
                                                            gint                 column,
                                                            gboolean             must_exist,
                                                            gboolean             ignore_bom,
                                                            gboolean             make_valid,
                                                            GError             **error);

gboolean            mousepad_file_save                     (MousepadFile        *file,
                                                            gboolean             forced,
                                                            GError             **error);

gboolean            mousepad_file_autosave_location_is_set (MousepadFile        *file);

gchar              *mousepad_file_autosave_get_uri         (MousepadFile        *file);

G_END_DECLS

#endif /* !__MOUSEPAD_FILE_H__ */
