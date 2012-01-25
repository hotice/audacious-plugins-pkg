/*  BMP - Cross-platform multimedia player
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

#ifndef SKINS_UI_PLAYLIST_H
#define SKINS_UI_PLAYLIST_H

#include <gtk/gtk.h>

void playlistwin_update (void);
void playlistwin_create(void);
void playlistwin_unhook (void);
void playlistwin_hide_timer(void);
void playlistwin_set_time (const gchar * minutes, const gchar * seconds);
void playlistwin_show (char show);

extern gint active_playlist;
extern gchar * active_title;
extern glong active_length;
extern GtkWidget * playlistwin, * playlistwin_list, * playlistwin_sinfo;

#endif /* SKINS_UI_PLAYLIST_H */
