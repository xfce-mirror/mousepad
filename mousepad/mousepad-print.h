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

#ifndef __MOUSEPAD_PRINT_H__
#define __MOUSEPAD_PRINT_H__

#include "mousepad-document.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_PRINT (mousepad_print_get_type ())
G_DECLARE_FINAL_TYPE (MousepadPrint, mousepad_print, MOUSEPAD, PRINT, GtkPrintOperation)

MousepadPrint *
mousepad_print_new (void);

gboolean
mousepad_print_document_interactive (MousepadPrint *print,
                                     MousepadDocument *document,
                                     GtkWindow *parent,
                                     GError **error);

G_END_DECLS

#endif /* !__MOUSEPAD_PRINT_H__ */
