/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2007  Audacious development team
 *
 *  Based on BMP:
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#ifndef DOCK_H
#define DOCK_H

#include <glib.h>
#include <gtk/gtk.h>

void dock_set_uposition(GtkWindow * widget, gint x, gint y);
void dock_move_press(GList * window_list, GtkWindow * w,
                     GdkEventButton * event, gboolean move_list);
void dock_move_motion(GtkWindow * w, GdkEventMotion * event);
void dock_move_release(GtkWindow * w);
void dock_get_widget_pos(GtkWindow * w, gint * x, gint * y);
gboolean dock_is_moving(GtkWindow * w);
void dock_shade(GList *window_list, GtkWindow *widget, gint new_height);

void dock_window_set_decorated (GtkWidget * widget);

GList *get_dock_window_list();

#endif
