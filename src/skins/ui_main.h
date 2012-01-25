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

#ifndef SKINS_UI_MAIN_H
#define SKINS_UI_MAIN_H

#include <gtk/gtk.h>

/* yes, main window size is fixed */
#define MAINWIN_WIDTH            (gint)275
#define MAINWIN_HEIGHT           (gint)116
#define MAINWIN_TITLEBAR_HEIGHT  (gint)14
#define MAINWIN_SHADED_WIDTH     MAINWIN_WIDTH
#define MAINWIN_SHADED_HEIGHT    MAINWIN_TITLEBAR_HEIGHT

typedef enum {
    TIMER_ELAPSED,
    TIMER_REMAINING
} TimerMode;

extern GtkWidget *mainwin;

extern GtkWidget *mainwin_eq, *mainwin_pl;
extern GtkWidget *mainwin_info;

extern GtkWidget *mainwin_stime_min, *mainwin_stime_sec;

extern GtkWidget *mainwin_vis;
extern GtkWidget *mainwin_svis;

extern GtkWidget *mainwin_playstatus;

extern GtkWidget *mainwin_minus_num, *mainwin_10min_num, *mainwin_min_num;
extern GtkWidget *mainwin_10sec_num, *mainwin_sec_num;

extern GtkWidget *mainwin_position, *mainwin_sposition;

void mainwin_create(void);
void mainwin_unhook (void);

void mainwin_play_pushed(void);

void mainwin_adjust_volume_motion(gint v);
void mainwin_adjust_volume_release(void);
void mainwin_adjust_balance_motion(gint b);
void mainwin_adjust_balance_release(void);
void mainwin_set_volume_slider(gint percent);
void mainwin_set_balance_slider(gint percent);

void mainwin_refresh_hints(void);
void mainwin_set_song_title (const gchar * title);
void mainwin_set_song_info(gint rate, gint freq, gint nch);
void mainwin_clear_song_info(void);

void mainwin_set_shape (void);

void mainwin_show(gboolean);
void mainwin_disable_seekbar(void);

void mainwin_update_song_info (void);
void mainwin_show_status_message (const gchar * message);
void mainwin_enable_status_message (gboolean enable);

void mainwin_drag_data_received(GtkWidget * widget,
                                GdkDragContext * context,
                                gint x,
                                gint y,
                                GtkSelectionData * selection_data,
                                guint info,
                                guint time,
                                gpointer user_data);

void mainwin_setup_menus(void);
gboolean change_timer_mode_cb(GtkWidget *widget, GdkEventButton *event);

/* widget should be null if called manually. */
gboolean mainwin_keypress (GtkWidget * widget, GdkEventKey * event,
 void * unused);

#endif /* SKINS_UI_MAIN_H */
