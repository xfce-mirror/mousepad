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

#include <mousepad/mousepad-app-activatable.h>
#include <mousepad/mousepad-application.h>



G_DEFINE_INTERFACE(MousepadAppActivatable, mousepad_app_activatable, G_TYPE_OBJECT)



static void
mousepad_app_activatable_default_init (MousepadAppActivatableInterface *iface)
{
  g_object_interface_install_property (iface,
    g_param_spec_object ("application",
                         "Application",
                         "The Mousepad application",
                         MOUSEPAD_TYPE_APPLICATION,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));
}



void
mousepad_app_activatable_activate (MousepadAppActivatable *activatable)
{
  MousepadAppActivatableInterface *iface;

  g_return_if_fail (MOUSEPAD_IS_APP_ACTIVATABLE (activatable));

  iface = MOUSEPAD_APP_ACTIVATABLE_GET_IFACE (activatable);
  if (iface->activate != NULL)
    iface->activate (activatable);
}



void
mousepad_app_activatable_deactivate (MousepadAppActivatable *activatable)
{
  MousepadAppActivatableInterface *iface;

  g_return_if_fail (MOUSEPAD_IS_APP_ACTIVATABLE (activatable));

  iface = MOUSEPAD_APP_ACTIVATABLE_GET_IFACE (activatable);
  if (iface->deactivate != NULL)
    iface->deactivate (activatable);
}
