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

#ifndef __MOUSEPAD_CLOSE_BUTTON_H__
#define __MOUSEPAD_CLOSE_BUTTON_H__ 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_CLOSE_BUTTON            (mousepad_close_button_get_type ())
#define MOUSEPAD_CLOSE_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_CLOSE_BUTTON, MousepadCloseButton))
#define MOUSEPAD_CLOSE_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_CLOSE_BUTTON, MousepadCloseButtonClass))
#define MOUSEPAD_IS_CLOSE_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_CLOSE_BUTTON))
#define MOUSEPAD_IS_CLOSE_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_CLOSE_BUTTON))
#define MOUSEPAD_CLOSE_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_CLOSE_BUTTON, MousepadCloseButtonClass))

typedef struct MousepadCloseButton_      MousepadCloseButton;
typedef struct MousepadCloseButtonClass_ MousepadCloseButtonClass;

GType      mousepad_close_button_get_type (void);
GtkWidget *mousepad_close_button_new      (GtkTextBuffer *buffer);

G_END_DECLS

#endif /* __MOUSEPAD_CLOSE_BUTTON_H__ */
