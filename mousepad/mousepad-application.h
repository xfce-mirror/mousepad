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

#ifndef __MOUSEPAD_APPLICATION_H__
#define __MOUSEPAD_APPLICATION_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* repetitive code whose forgetting causes memory leaks */
static inline gboolean
mousepad_action_get_state_boolean (GAction *action)
{
  GVariant *variant;
  gboolean  value;

  variant = g_action_get_state (action);
  value = g_variant_get_boolean (variant);
  g_variant_unref (variant);

  return value;
}

/* same, but specific to non-toggle actions with boolean state: an int32 state
 * is used so that the action is not interpreted as toggle */
static inline gboolean
mousepad_action_get_state_int32_boolean (GAction *action)
{
  GVariant *variant;
  gboolean  value;

  variant = g_action_get_state (action);
  value = g_variant_get_int32 (variant);
  g_variant_unref (variant);

  return value;
}

typedef struct _MousepadApplicationClass MousepadApplicationClass;
typedef struct _MousepadApplication      MousepadApplication;

#define MOUSEPAD_TYPE_APPLICATION            (mousepad_application_get_type ())
#define MOUSEPAD_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_APPLICATION, MousepadApplication))
#define MOUSEPAD_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_APPLICATION, MousepadApplicationClass))
#define MOUSEPAD_IS_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_APPLICATION))
#define MOUSEPAD_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_APPLICATION))
#define MOUSEPAD_APPLICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_APPLICATION, MousepadApplicationClass))

GType                mousepad_application_get_type                   (void) G_GNUC_CONST;

GList               *mousepad_application_get_providers              (MousepadApplication  *application);

GtkWidget           *mousepad_application_get_prefs_dialog           (MousepadApplication  *application);

G_END_DECLS

#endif /* !__MOUSEPAD_APPLICATION_H__ */
