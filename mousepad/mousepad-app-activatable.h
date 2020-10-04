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

#ifndef MOUSEPAD_APP_ACTIVATABLE_H
#define MOUSEPAD_APP_ACTIVATABLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_APP_ACTIVATABLE (mousepad_app_activatable_get_type ())

G_DECLARE_INTERFACE (MousepadAppActivatable, mousepad_app_activatable, MOUSEPAD, APP_ACTIVATABLE, GObject)

struct _MousepadAppActivatableInterface
{
  GTypeInterface g_iface;
  
  void (*activate)   (MousepadAppActivatable *activatable);
  void (*deactivate) (MousepadAppActivatable *activatable);
};

void mousepad_app_activatable_activate   (MousepadAppActivatable *activatable);
void mousepad_app_activatable_deactivate (MousepadAppActivatable *activatable);

G_END_DECLS

#endif /* MOUSEPAD_APP_ACTIVATABLE_H */
