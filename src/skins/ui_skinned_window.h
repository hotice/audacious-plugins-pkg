/*
 * ui_skinned_window.h
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#ifndef SKINS_UI_SKINNED_WINDOW_H
#define SKINS_UI_SKINNED_WINDOW_H

#include <gtk/gtk.h>

GtkWidget * window_new (gint * x, gint * y, gint w, gint h, gboolean main,
 gboolean shaded, void (* draw) (GtkWidget * window, cairo_t * cr));
void window_set_size (GtkWidget * window, gint w, gint h);
void window_set_shaded (GtkWidget * window, gboolean shaded);
void window_put_widget (GtkWidget * window, gboolean shaded, GtkWidget * widget,
 gint x, gint y);
void window_move_widget (GtkWidget * window, gboolean shaded, GtkWidget *
 widget, gint x, gint y);
void window_show_all (GtkWidget * window);

#endif
