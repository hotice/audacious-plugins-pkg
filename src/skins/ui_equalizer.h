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

#ifndef AUDACIOUS_UI_EQUALIZER_H
#define AUDACIOUS_UI_EQUALIZER_H

#include <glib.h>
#include <gtk/gtk.h>
#include "skins_cfg.h"

#define EQUALIZER_SCALED     (config.scaled && config.eq_scaled_linked)
#define EQUALIZER_SCALE_FACTOR (EQUALIZER_SCALED ? config.scale_factor : 1)

#define EQUALIZER_HEIGHT         ((config.equalizer_shaded ? 14 : 116) * (EQUALIZER_SCALE_FACTOR))
#define EQUALIZER_WIDTH          (275 * EQUALIZER_SCALE_FACTOR)

#define EQUALIZER_DEFAULT_POS_X  20
#define EQUALIZER_DEFAULT_POS_Y  136

#define EQUALIZER_DEFAULT_DIR_PRESET "dir_default.preset"
#define EQUALIZER_DEFAULT_PRESET_EXT "preset"

void eq_init_hooks (void);
void eq_end_hooks (void);

void equalizerwin_set_shape (void);
void equalizerwin_set_scaled(gboolean ds);
void equalizerwin_set_shade_menu_cb(gboolean shaded);
void draw_equalizer_window(gboolean force);
void equalizerwin_create(void);
void equalizerwin_show(gboolean show);
void equalizerwin_set_volume_slider(gint percent);
void equalizerwin_set_balance_slider(gint percent);
void equalizerwin_eq_changed(void);
void equalizerwin_set_preamp(gfloat preamp);
void equalizerwin_set_band(gint band, gfloat value);
gfloat equalizerwin_get_preamp(void);
gfloat equalizerwin_get_band(gint band);

gboolean equalizerwin_has_focus(void);

extern GtkWidget *equalizerwin;
extern GtkWidget *equalizerwin_graph;
extern gboolean equalizerwin_focus;

void equalizerwin_activate(gboolean active);

#endif /* AUDACIOUS_UI_EQUALIZER_H */
