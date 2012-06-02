/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2007  Audacious development team.
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

#ifndef SKINS_UI_EQUALIZER_H
#define SKINS_UI_EQUALIZER_H

#include <gtk/gtk.h>

void equalizerwin_set_shape (void);
void equalizerwin_create(void);
void equalizerwin_show(gboolean show);
void equalizerwin_set_volume_slider(gint percent);
void equalizerwin_set_balance_slider(gint percent);
void equalizerwin_eq_changed(void);

extern GtkWidget *equalizerwin;

#endif /* SKINS_UI_EQUALIZER_H */
