/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team.
 *
 *  BMP - Cross-platform multimedia player
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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>

#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "dnd.h"
#include "plugin.h"
#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_hints.h"
#include "ui_main.h"
#include "ui_main_evlisteners.h"
#include "ui_manager.h"
#include "ui_playlist.h"
#include "ui_skinned_button.h"
#include "ui_skinned_horizontal_slider.h"
#include "ui_skinned_menurow.h"
#include "ui_skinned_monostereo.h"
#include "ui_skinned_number.h"
#include "ui_skinned_playlist.h"
#include "ui_skinned_playstatus.h"
#include "ui_skinned_textbox.h"
#include "ui_skinned_window.h"
#include "ui_vis.h"
#include "util.h"

#define SEEK_THRESHOLD 200 /* milliseconds */
#define SEEK_TIMEOUT 100 /* milliseconds */
#define SEEK_SPEED 50 /* milliseconds per pixel */

GtkWidget *mainwin = NULL;

static gint balance;
static gint seek_source = 0, seek_start, seek_time;

static GtkWidget *mainwin_menubtn, *mainwin_minimize, *mainwin_shade, *mainwin_close;
static GtkWidget *mainwin_shaded_menubtn, *mainwin_shaded_minimize, *mainwin_shaded_shade, *mainwin_shaded_close;

static GtkWidget *mainwin_rew, *mainwin_fwd;
static GtkWidget *mainwin_eject;
static GtkWidget *mainwin_play, *mainwin_pause, *mainwin_stop;

static GtkWidget *mainwin_shuffle, *mainwin_repeat;
GtkWidget *mainwin_eq, *mainwin_pl;

GtkWidget *mainwin_info;
GtkWidget *mainwin_stime_min, *mainwin_stime_sec;

static GtkWidget *mainwin_rate_text, *mainwin_freq_text, *mainwin_othertext;

GtkWidget *mainwin_playstatus;

GtkWidget *mainwin_minus_num, *mainwin_10min_num, *mainwin_min_num;
GtkWidget *mainwin_10sec_num, *mainwin_sec_num;

GtkWidget *mainwin_vis;
GtkWidget *mainwin_svis;

GtkWidget *mainwin_sposition = NULL;

static GtkWidget *mainwin_menurow;
static GtkWidget *mainwin_volume, *mainwin_balance;
GtkWidget *mainwin_position;

static GtkWidget *mainwin_monostereo;
static GtkWidget *mainwin_srew, *mainwin_splay, *mainwin_spause;
static GtkWidget *mainwin_sstop, *mainwin_sfwd, *mainwin_seject, *mainwin_about;

static gboolean mainwin_info_text_locked = FALSE;
static guint mainwin_volume_release_timeout = 0;

static void change_timer_mode(void);
static void mainwin_position_motion_cb (void);
static void mainwin_position_release_cb (void);
static void mainwin_set_volume_diff (gint diff);

static void format_time (gchar buf[7], gint time, gint length)
{
    if (config.timer_mode == TIMER_REMAINING && length > 0)
    {
        if (length - time < 60000)         /* " -0:SS" */
            snprintf (buf, 7, " -0:%02d", (length - time) / 1000);
        else if (length - time < 6000000)  /* "-MM:SS" */
            snprintf (buf, 7, "%3d:%02d", (time - length) / 60000, (length - time) / 1000 % 60);
        else                               /* "-HH:MM" */
            snprintf (buf, 7, "%3d:%02d", (time - length) / 3600000, (length - time) / 60000 % 60);
    }
    else
    {
        if (time < 60000000)  /* MMM:SS */
            snprintf (buf, 7, "%3d:%02d", time / 60000, time / 1000 % 60);
        else                  /* HHH:MM */
            snprintf (buf, 7, "%3d:%02d", time / 3600000, time / 60000 % 60);
    }

    buf[3] = 0;
}

static void
mainwin_set_shade(gboolean shaded)
{
    GtkAction *action = gtk_action_group_get_action(toggleaction_group_others,
                                                    "roll up player");
    gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(action) , shaded );
}

void mainwin_set_shape (void)
{
    gint id = config.player_shaded ? SKIN_MASK_MAIN_SHADE : SKIN_MASK_MAIN;
    gtk_widget_shape_combine_region (mainwin, active_skin->masks[id]);
}

static void mainwin_vis_set_type (VisType mode)
{
    GtkAction *action;

    switch ( mode )
    {
        case VIS_ANALYZER:
            action = gtk_action_group_get_action(radioaction_group_vismode,
                                                 "vismode analyzer");
            break;
        case VIS_SCOPE:
            action = gtk_action_group_get_action(radioaction_group_vismode,
                                                 "vismode scope");
            break;
        case VIS_VOICEPRINT:
            action = gtk_action_group_get_action(radioaction_group_vismode,
                                                 "vismode voiceprint");
            break;
        case VIS_OFF:
        default:
            action = gtk_action_group_get_action(radioaction_group_vismode,
                                                 "vismode off");
            break;
    }

    gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(action) , TRUE );
}

static void
mainwin_menubtn_cb(void)
{
    gint x, y;
    gtk_window_get_position(GTK_WINDOW(mainwin), &x, &y);
    ui_popup_menu_show (UI_MENU_MAIN, x + 6, y + MAINWIN_SHADED_HEIGHT, FALSE,
     FALSE, 1, GDK_CURRENT_TIME);
}

static void mainwin_minimize_cb (void)
{
    if (!mainwin)
        return;

    gtk_window_iconify(GTK_WINDOW(mainwin));
}

static void
mainwin_shade_toggle(void)
{
    mainwin_set_shade(!config.player_shaded);
}

static gboolean mainwin_vis_cb (GtkWidget * widget, GdkEventButton * event)
{
    if (event->button == 1) {
        config.vis_type++;

        if (config.vis_type > VIS_OFF)
            config.vis_type = VIS_ANALYZER;

        ui_vis_clear_data (mainwin_vis);
        ui_svis_clear_data (mainwin_svis);

        mainwin_vis_set_type(config.vis_type);
    }
    else if (event->button == 3)
        ui_popup_menu_show(UI_MENU_VISUALIZATION, event->x_root, event->y_root,
         FALSE, FALSE, 3, event->time);

    return TRUE;
}

static void show_main_menu (GdkEventButton * event, void * unused)
{
    GdkScreen * screen = gdk_event_get_screen ((GdkEvent *) event);
    gint width = gdk_screen_get_width (screen);
    gint height = gdk_screen_get_height (screen);

    ui_popup_menu_show (UI_MENU_MAIN, event->x_root, event->y_root,
     event->x_root > width / 2, event->y_root > height / 2, event->button,
     event->time);
}

static gchar *mainwin_tb_old_text = NULL;

static void mainwin_lock_info_text (const gchar * text)
{
    if (mainwin_info_text_locked != TRUE)
        mainwin_tb_old_text = g_strdup
         (active_skin->properties.mainwin_othertext_is_status ?
         textbox_get_text (mainwin_othertext) : textbox_get_text (mainwin_info));

    mainwin_info_text_locked = TRUE;
    if (active_skin->properties.mainwin_othertext_is_status)
        textbox_set_text (mainwin_othertext, text);
    else
        textbox_set_text (mainwin_info, text);
}

static void mainwin_release_info_text (void)
{
    mainwin_info_text_locked = FALSE;

    if (mainwin_tb_old_text != NULL)
    {
        if (active_skin->properties.mainwin_othertext_is_status)
            textbox_set_text (mainwin_othertext, mainwin_tb_old_text);
        else
            textbox_set_text (mainwin_info, mainwin_tb_old_text);
        g_free(mainwin_tb_old_text);
        mainwin_tb_old_text = NULL;
    }
}

static void mainwin_set_info_text (const gchar * text)
{
    if (mainwin_info_text_locked)
    {
        g_free (mainwin_tb_old_text);
        mainwin_tb_old_text = g_strdup (text);
    }
    else
        textbox_set_text (mainwin_info, text);
}

static gboolean status_message_enabled;

void mainwin_enable_status_message (gboolean enable)
{
    status_message_enabled = enable;
}

static gint status_message_source = 0;

static gboolean clear_status_message (void * unused)
{
    mainwin_release_info_text ();
    status_message_source = 0;
    return FALSE;
}

void mainwin_show_status_message (const gchar * message)
{
    if (! status_message_enabled)
        return;

    if (status_message_source)
        g_source_remove (status_message_source);

    mainwin_lock_info_text (message);
    status_message_source = g_timeout_add (1000, clear_status_message, NULL);
}

static gchar *
make_mainwin_title(const gchar * title)
{
    if (title != NULL)
        return g_strdup_printf(_("%s - Audacious"), title);
    else
        return g_strdup(_("Audacious"));
}

void
mainwin_set_song_title(const gchar * title)
{
    gchar *mainwin_title_text = make_mainwin_title(title);
    gtk_window_set_title(GTK_WINDOW(mainwin), mainwin_title_text);
    g_free(mainwin_title_text);

    mainwin_set_info_text (title ? title : "");
}

static void setup_widget (GtkWidget * widget, gint x, gint y, gboolean show)
{
    GtkRequisition size;
    gtk_widget_get_preferred_size (widget, & size, NULL);

    /* leave no-show-all widgets alone (they are shown/hidden elsewhere) */
    if (! gtk_widget_get_no_show_all (widget))
    {
        /* hide widgets that are outside the window boundary */
        if (x < 0 || x + size.width > active_skin->properties.mainwin_width ||
         y < 0 || y + size.height > active_skin->properties.mainwin_height)
            show = FALSE;

        gtk_widget_set_visible (widget, show);
    }

    window_move_widget (mainwin, FALSE, widget, x, y);
}

void mainwin_refresh_hints (void)
{
    const SkinProperties * p = & active_skin->properties;

    gtk_widget_set_visible (mainwin_menurow, p->mainwin_menurow_visible);
    gtk_widget_set_visible (mainwin_rate_text, p->mainwin_streaminfo_visible);
    gtk_widget_set_visible (mainwin_freq_text, p->mainwin_streaminfo_visible);
    gtk_widget_set_visible (mainwin_monostereo, p->mainwin_streaminfo_visible);

    textbox_set_width (mainwin_info, p->mainwin_text_width);

    setup_widget (mainwin_vis, p->mainwin_vis_x, p->mainwin_vis_y, p->mainwin_vis_visible);
    setup_widget (mainwin_info, p->mainwin_text_x, p->mainwin_text_y, p->mainwin_text_visible);
    setup_widget (mainwin_othertext, p->mainwin_infobar_x, p->mainwin_infobar_y, p->mainwin_othertext_visible);

    setup_widget (mainwin_minus_num, p->mainwin_number_0_x, p->mainwin_number_0_y, TRUE);
    setup_widget (mainwin_10min_num, p->mainwin_number_1_x, p->mainwin_number_1_y, TRUE);
    setup_widget (mainwin_min_num, p->mainwin_number_2_x, p->mainwin_number_2_y, TRUE);
    setup_widget (mainwin_10sec_num, p->mainwin_number_3_x, p->mainwin_number_3_y, TRUE);
    setup_widget (mainwin_sec_num, p->mainwin_number_4_x, p->mainwin_number_4_y, TRUE);
    setup_widget (mainwin_position, p->mainwin_position_x, p->mainwin_position_y, TRUE);

    setup_widget (mainwin_playstatus, p->mainwin_playstatus_x, p->mainwin_playstatus_y, TRUE);
    setup_widget (mainwin_volume, p->mainwin_volume_x, p->mainwin_volume_y, TRUE);
    setup_widget (mainwin_balance, p->mainwin_balance_x, p->mainwin_balance_y, TRUE);
    setup_widget (mainwin_rew, p->mainwin_previous_x, p->mainwin_previous_y, TRUE);
    setup_widget (mainwin_play, p->mainwin_play_x, p->mainwin_play_y, TRUE);
    setup_widget (mainwin_pause, p->mainwin_pause_x, p->mainwin_pause_y, TRUE);
    setup_widget (mainwin_stop, p->mainwin_stop_x, p->mainwin_stop_y, TRUE);
    setup_widget (mainwin_fwd, p->mainwin_next_x, p->mainwin_next_y, TRUE);
    setup_widget (mainwin_eject, p->mainwin_eject_x, p->mainwin_eject_y, TRUE);
    setup_widget (mainwin_eq, p->mainwin_eqbutton_x, p->mainwin_eqbutton_y, TRUE);
    setup_widget (mainwin_pl, p->mainwin_plbutton_x, p->mainwin_plbutton_y, TRUE);
    setup_widget (mainwin_shuffle, p->mainwin_shuffle_x, p->mainwin_shuffle_y, TRUE);
    setup_widget (mainwin_repeat, p->mainwin_repeat_x, p->mainwin_repeat_y, TRUE);
    setup_widget (mainwin_about, p->mainwin_about_x, p->mainwin_about_y, TRUE);
    setup_widget (mainwin_minimize, p->mainwin_minimize_x, p->mainwin_minimize_y, TRUE);
    setup_widget (mainwin_shade, p->mainwin_shade_x, p->mainwin_shade_y, TRUE);
    setup_widget (mainwin_close, p->mainwin_close_x, p->mainwin_close_y, TRUE);

    if (config.player_shaded)
        window_set_size (mainwin, MAINWIN_SHADED_WIDTH, MAINWIN_SHADED_HEIGHT);
    else
        window_set_size (mainwin, p->mainwin_width, p->mainwin_height);
}

void mainwin_set_song_info (gint bitrate, gint samplerate, gint channels)
{
    gchar scratch[32];
    gint length;

    if (bitrate > 0)
    {
        if (bitrate < 1000000)
            snprintf (scratch, sizeof scratch, "%3d", bitrate / 1000);
        else
            snprintf (scratch, sizeof scratch, "%2dH", bitrate / 100000);

        textbox_set_text (mainwin_rate_text, scratch);
    }
    else
        textbox_set_text (mainwin_rate_text, "");

    if (samplerate > 0)
    {
        snprintf (scratch, sizeof scratch, "%2d", samplerate / 1000);
        textbox_set_text (mainwin_freq_text, scratch);
    }
    else
        textbox_set_text (mainwin_freq_text, "");

    ui_skinned_monostereo_set_num_channels (mainwin_monostereo, channels);

    if (bitrate > 0)
        snprintf (scratch, sizeof scratch, "%d %s", bitrate / 1000, _("kbps"));
    else
        scratch[0] = 0;

    if (samplerate > 0)
    {
        length = strlen (scratch);
        snprintf (scratch + length, sizeof scratch - length, "%s%d %s", length ?
         ", " : "", samplerate / 1000, _("kHz"));
    }

    if (channels > 0)
    {
        length = strlen (scratch);
        snprintf (scratch + length, sizeof scratch - length, "%s%s", length ?
         ", " : "", channels > 2 ? _("surround") : channels > 1 ? _("stereo") :
         _("mono"));
    }

    textbox_set_text (mainwin_othertext, scratch);
}

void
mainwin_clear_song_info(void)
{
    mainwin_set_song_title (NULL);

    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);

    gtk_widget_hide (mainwin_minus_num);
    gtk_widget_hide (mainwin_10min_num);
    gtk_widget_hide (mainwin_min_num);
    gtk_widget_hide (mainwin_10sec_num);
    gtk_widget_hide (mainwin_sec_num);
    gtk_widget_hide (mainwin_stime_min);
    gtk_widget_hide (mainwin_stime_sec);
    gtk_widget_hide (mainwin_position);
    gtk_widget_hide (mainwin_sposition);

    hslider_set_pressed (mainwin_position, FALSE);
    hslider_set_pressed (mainwin_sposition, FALSE);

    /* clear sampling parameter displays */
    textbox_set_text (mainwin_rate_text, "   ");
    textbox_set_text (mainwin_freq_text, "  ");
    ui_skinned_monostereo_set_num_channels(mainwin_monostereo, 0);
    textbox_set_text (mainwin_othertext, "");

    if (mainwin_playstatus != NULL)
        ui_skinned_playstatus_set_status(mainwin_playstatus, STATUS_STOP);

    playlistwin_hide_timer();
}

void
mainwin_disable_seekbar(void)
{
    if (!mainwin)
        return;

    gtk_widget_hide(mainwin_position);
    gtk_widget_hide(mainwin_sposition);
}

static void mainwin_scrolled (GtkWidget * widget, GdkEventScroll * event, void *
 unused)
{
    switch (event->direction) {
        case GDK_SCROLL_UP:
            mainwin_set_volume_diff (5);
            break;
        case GDK_SCROLL_DOWN:
            mainwin_set_volume_diff (-5);
            break;
        case GDK_SCROLL_LEFT:
            aud_drct_seek (aud_drct_get_time () - 5000);
            break;
        case GDK_SCROLL_RIGHT:
            aud_drct_seek (aud_drct_get_time () + 5000);
            break;
        default:
            break;
    }
}

static gboolean
mainwin_mouse_button_press(GtkWidget * widget,
                           GdkEventButton * event,
                           gpointer callback_data)
{
    if (event->button == 1 && event->type == GDK_2BUTTON_PRESS && event->y < 14)
    {
        mainwin_set_shade(!config.player_shaded);
        return TRUE;
    }

    if (event->button == 3)
    {
        ui_popup_menu_show (UI_MENU_MAIN, event->x_root, event->y_root, FALSE,
         FALSE, event->button, event->time);
        return TRUE;
    }

    return FALSE;
}

static void mainwin_playback_rpress (GtkWidget * button, GdkEventButton * event)
{
    ui_popup_menu_show (UI_MENU_PLAYBACK, event->x_root, event->y_root,
     FALSE, FALSE, event->button, event->time);
}

gboolean mainwin_keypress (GtkWidget * widget, GdkEventKey * event,
 void * unused)
{
    if (ui_skinned_playlist_key (playlistwin_list, event))
        return 1;

    switch (event->keyval)
    {
        case GDK_KEY_minus:
            mainwin_set_volume_diff (-5);
            break;
        case GDK_KEY_plus:
            mainwin_set_volume_diff (5);
            break;
        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
        case GDK_KEY_KP_7:
            aud_drct_seek (aud_drct_get_time () - 5000);
            break;
        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
        case GDK_KEY_KP_9:
            aud_drct_seek (aud_drct_get_time () + 5000);
            break;
        case GDK_KEY_KP_4:
            aud_drct_pl_prev ();
            break;
        case GDK_KEY_KP_6:
            aud_drct_pl_next ();
            break;
        case GDK_KEY_KP_Insert:
            audgui_jump_to_track ();
            break;
        case GDK_KEY_space:
            aud_drct_pause();
            break;
        case GDK_KEY_Tab: /* GtkUIManager does not handle tab, apparently. */
            if (event->state & GDK_SHIFT_MASK)
                action_playlist_prev ();
            else
                action_playlist_next ();

            break;
        case GDK_KEY_ISO_Left_Tab:
            action_playlist_prev ();
            break;
        default:
            return FALSE;
    }

    return TRUE;
}

/*
 * Rewritten 09/13/06:
 *
 * Remove all of this flaky iter/sourcelist/strsplit stuff.
 * All we care about is the filepath.
 *
 * We can figure this out and easily pass it to g_filename_from_uri().
 *   - nenolod
 */
void
mainwin_drag_data_received(GtkWidget * widget,
                           GdkDragContext * context,
                           gint x,
                           gint y,
                           GtkSelectionData * selection_data,
                           guint info,
                           guint time,
                           gpointer user_data)
{
    g_return_if_fail(selection_data != NULL);

    const gchar * data = (const gchar *) gtk_selection_data_get_data
     (selection_data);
    g_return_if_fail (data);

    if (str_has_prefix_nocase (data, "fonts:///"))
    {
        const gchar * path = data;
        gchar *decoded = g_filename_from_uri(path, NULL, NULL);

        if (decoded == NULL)
            return;

        config.playlist_font = g_strconcat(decoded, strrchr(config.playlist_font, ' '), NULL);
        ui_skinned_playlist_set_font (playlistwin_list, config.playlist_font);

        g_free(decoded);

        return;
    }

    if (str_has_prefix_nocase (data, "file:///"))
    {
        if (str_has_suffix_nocase (data, ".wsz\r\n") || str_has_suffix_nocase
         (data, ".zip\r\n"))
        {
            on_skin_view_drag_data_received (0, context, x, y, selection_data, info, time, 0);
            return;
        }
    }

    audgui_urilist_open (data);
}

static gint time_now (void)
{
    struct timeval tv;
    gettimeofday (& tv, NULL);
    return (tv.tv_sec % (24 * 3600) * 1000 + tv.tv_usec / 1000);
}

static gint time_diff (gint a, gint b)
{
    if (a > 18 * 3600 * 1000 && b < 6 * 3600 * 1000) /* detect midnight */
        b += 24 * 3600 * 1000;
    return (b > a) ? b - a : 0;
}

static gboolean seek_timeout (void * rewind)
{
    if (! aud_drct_get_playing ())
    {
        seek_source = 0;
        return FALSE;
    }

    gint held = time_diff (seek_time, time_now ());
    if (held < SEEK_THRESHOLD)
        return TRUE;

    gint position;
    if (GPOINTER_TO_INT (rewind))
        position = seek_start - held / SEEK_SPEED;
    else
        position = seek_start + held / SEEK_SPEED;

    position = CLAMP (position, 0, 219);
    hslider_set_pos (mainwin_position, position);
    mainwin_position_motion_cb ();

    return TRUE;
}

static gboolean seek_press (GtkWidget * widget, GdkEventButton * event,
 gboolean rewind)
{
    if (event->button != 1 || seek_source)
        return FALSE;

    seek_start = hslider_get_pos (mainwin_position);
    seek_time = time_now ();
    seek_source = g_timeout_add (SEEK_TIMEOUT, seek_timeout, GINT_TO_POINTER
     (rewind));
    return FALSE;
}

static gboolean seek_release (GtkWidget * widget, GdkEventButton * event,
 gboolean rewind)
{
    if (event->button != 1 || ! seek_source)
        return FALSE;

    if (! aud_drct_get_playing () || time_diff (seek_time, time_now ()) <
     SEEK_THRESHOLD)
    {
        if (rewind)
            aud_drct_pl_prev ();
        else
            aud_drct_pl_next ();
    }
    else
        mainwin_position_release_cb ();

    g_source_remove (seek_source);
    seek_source = 0;
    return FALSE;
}

static void mainwin_rew_press (GtkWidget * button, GdkEventButton * event)
 {seek_press (button, event, TRUE); }
static void mainwin_rew_release (GtkWidget * button, GdkEventButton * event)
 {seek_release (button, event, TRUE); }
static void mainwin_fwd_press (GtkWidget * button, GdkEventButton * event)
 {seek_press (button, event, FALSE); }
static void mainwin_fwd_release (GtkWidget * button, GdkEventButton * event)
 {seek_release (button, event, FALSE); }

static void mainwin_shuffle_cb (GtkWidget * button, GdkEventButton * event)
 {check_set (toggleaction_group_others, "playback shuffle", button_get_active (button)); }
static void mainwin_repeat_cb (GtkWidget * button, GdkEventButton * event)
 {check_set (toggleaction_group_others, "playback repeat", button_get_active (button)); }
static void mainwin_eq_cb (GtkWidget * button, GdkEventButton * event)
 {equalizerwin_show (button_get_active (button)); }
static void mainwin_pl_cb (GtkWidget * button, GdkEventButton * event)
 {playlistwin_show (button_get_active (button)); }

static void mainwin_spos_set_knob (void)
{
    gint pos = hslider_get_pos (mainwin_sposition);

    gint x;
    if (pos < 6)
        x = 17;
    else if (pos < 9)
        x = 20;
    else
        x = 23;

    hslider_set_knob (mainwin_sposition, x, 36, x, 36);
}

static void mainwin_spos_motion_cb (void)
{
    mainwin_spos_set_knob ();

    gint pos = hslider_get_pos (mainwin_sposition);
    gint length = aud_drct_get_length ();
    gint time = (pos - 1) * length / 12;

    gchar buf[7];
    format_time (buf, time, length);

    textbox_set_text (mainwin_stime_min, buf);
    textbox_set_text (mainwin_stime_sec, buf + 4);
}

static void mainwin_spos_release_cb (void)
{
    mainwin_spos_set_knob ();

    gint pos = hslider_get_pos (mainwin_sposition);
    aud_drct_seek (aud_drct_get_length () * (pos - 1) / 12);
}

static void mainwin_position_motion_cb (void)
{
    gint length = aud_drct_get_length () / 1000;
    gint pos = hslider_get_pos (mainwin_position);
    gint time = pos * length / 219;

    gchar * seek_msg = g_strdup_printf (_("Seek to %d:%-2.2d / %d:%-2.2d"), time
     / 60, time % 60, length / 60, length % 60);
    mainwin_lock_info_text(seek_msg);
    g_free(seek_msg);
}

static void mainwin_position_release_cb (void)
{
    gint length = aud_drct_get_length ();
    gint pos = hslider_get_pos (mainwin_position);
    gint time = (gint64) pos * length / 219;

    aud_drct_seek(time);
    mainwin_release_info_text();
}

void
mainwin_adjust_volume_motion(gint v)
{
    gchar *volume_msg;

    volume_msg = g_strdup_printf(_("Volume: %d%%"), v);
    mainwin_lock_info_text(volume_msg);
    g_free(volume_msg);

    aud_drct_set_volume_main (v);
    aud_drct_set_volume_balance (balance);
}

void
mainwin_adjust_volume_release(void)
{
    mainwin_release_info_text();
}

void
mainwin_adjust_balance_motion(gint b)
{
    gchar *balance_msg;

    balance = b;
    aud_drct_set_volume_balance (b);

    if (b < 0)
        balance_msg = g_strdup_printf(_("Balance: %d%% left"), -b);
    else if (b == 0)
        balance_msg = g_strdup_printf(_("Balance: center"));
    else
        balance_msg = g_strdup_printf(_("Balance: %d%% right"), b);

    mainwin_lock_info_text(balance_msg);
    g_free(balance_msg);
}

void
mainwin_adjust_balance_release(void)
{
    mainwin_release_info_text();
}

static void mainwin_volume_set_frame (void)
{
    gint pos = hslider_get_pos (mainwin_volume);
    gint frame = (pos * 27 + 25) / 51;
    hslider_set_frame (mainwin_volume, 0, 15 * frame);
}

void mainwin_set_volume_slider (gint percent)
{
    hslider_set_pos (mainwin_volume, (percent * 51 + 50) / 100);
    mainwin_volume_set_frame ();
}

static void mainwin_volume_motion_cb (void)
{
    mainwin_volume_set_frame ();
    gint pos = hslider_get_pos (mainwin_volume);
    gint vol = (pos * 100 + 25) / 51;

    mainwin_adjust_volume_motion(vol);
    equalizerwin_set_volume_slider(vol);
}

static void mainwin_volume_release_cb (void)
{
    mainwin_volume_set_frame ();
    mainwin_adjust_volume_release();
}

static void mainwin_balance_set_frame (void)
{
    gint pos = hslider_get_pos (mainwin_balance);
    gint frame = (abs (pos - 12) * 27 + 6) / 12;
    hslider_set_frame (mainwin_balance, 9, 15 * frame);
}

void mainwin_set_balance_slider (gint percent)
{
    if (percent > 0)
        hslider_set_pos (mainwin_balance, 12 + (percent * 12 + 50) / 100);
    else
        hslider_set_pos (mainwin_balance, 12 + (percent * 12 - 50) / 100);

    mainwin_balance_set_frame ();
}

static void mainwin_balance_motion_cb (void)
{
    mainwin_balance_set_frame ();
    gint pos = hslider_get_pos (mainwin_balance);

    gint bal;
    if (pos > 12)
        bal = ((pos - 12) * 100 + 6) / 12;
    else
        bal = ((pos - 12) * 100 - 6) / 12;

    mainwin_adjust_balance_motion(bal);
    equalizerwin_set_balance_slider(bal);
}

static void mainwin_balance_release_cb (void)
{
    mainwin_balance_set_frame ();
    mainwin_adjust_volume_release();
}

static void mainwin_set_volume_diff (gint diff)
{
    gint vol;

    aud_drct_get_volume_main (& vol);
    vol = CLAMP (vol + diff, 0, 100);
    mainwin_adjust_volume_motion(vol);
    mainwin_set_volume_slider(vol);
    equalizerwin_set_volume_slider(vol);

    if (mainwin_volume_release_timeout)
        g_source_remove(mainwin_volume_release_timeout);
    mainwin_volume_release_timeout =
        g_timeout_add(700, (GSourceFunc)(mainwin_volume_release_cb), NULL);
}

static void mainwin_real_show (gboolean show)
{
    if (show)
        gtk_window_present ((GtkWindow *) mainwin);
    else
        gtk_widget_hide (mainwin);
}

void mainwin_show (gboolean show)
{
    GtkAction * a;

    a = gtk_action_group_get_action (toggleaction_group_others, "show player");

    if (a && gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (a)) != show)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (a), show);
    else
    {
        mainwin_real_show (show);
        equalizerwin_show (config.equalizer_visible);
        playlistwin_show (config.playlist_visible);
        start_stop_visual (FALSE);
    }
}

void mainwin_mr_change (MenuRowItem i)
{
    switch (i) {
        case MENUROW_OPTIONS:
            mainwin_lock_info_text(_("Options Menu"));
            break;
        case MENUROW_ALWAYS:
            if (config.always_on_top)
                mainwin_lock_info_text(_("Disable 'Always On Top'"));
            else
                mainwin_lock_info_text(_("Enable 'Always On Top'"));
            break;
        case MENUROW_FILEINFOBOX:
            mainwin_lock_info_text(_("File Info Box"));
            break;
        case MENUROW_SCALE:
            break;
        case MENUROW_VISUALIZATION:
            mainwin_lock_info_text(_("Visualization Menu"));
            break;
        case MENUROW_NONE:
            break;
    }
}

void mainwin_mr_release (MenuRowItem i, GdkEventButton * event)
{
    switch (i) {
        case MENUROW_OPTIONS:
            ui_popup_menu_show(UI_MENU_VIEW, event->x_root, event->y_root,
             FALSE, FALSE, 1, event->time);
            break;
        case MENUROW_ALWAYS:
            gtk_toggle_action_set_active ((GtkToggleAction *)
             gtk_action_group_get_action (toggleaction_group_others,
             "view always on top"), config.always_on_top);
            break;
        case MENUROW_FILEINFOBOX:
            audgui_infowin_show_current ();
            break;
        case MENUROW_SCALE:
            break;
        case MENUROW_VISUALIZATION:
            ui_popup_menu_show(UI_MENU_VISUALIZATION, event->x_root,
             event->y_root, FALSE, FALSE, 1, event->time);
            break;
        case MENUROW_NONE:
            break;
    }

    mainwin_release_info_text();
}

static void
set_timer_mode(TimerMode mode)
{
    if (mode == TIMER_ELAPSED)
        check_set(radioaction_group_viewtime, "view time elapsed", TRUE);
    else
        check_set(radioaction_group_viewtime, "view time remaining", TRUE);
}

gboolean
change_timer_mode_cb(GtkWidget *widget, GdkEventButton *event)
{
    if (event->button == 1) {
        change_timer_mode();
    } else if (event->button == 3)
        return FALSE;

    return TRUE;
}

static void change_timer_mode(void) {
    if (config.timer_mode == TIMER_ELAPSED)
        set_timer_mode(TIMER_REMAINING);
    else
        set_timer_mode(TIMER_ELAPSED);
    if (aud_drct_get_playing())
        mainwin_update_song_info();
}

void
mainwin_setup_menus(void)
{
    set_timer_mode(config.timer_mode);

    /* View menu */

    check_set(toggleaction_group_others, "view always on top", config.always_on_top);
    check_set(toggleaction_group_others, "view put on all workspaces", config.sticky);
    check_set(toggleaction_group_others, "roll up player", config.player_shaded);
    check_set(toggleaction_group_others, "roll up playlist editor", config.playlist_shaded);
    check_set(toggleaction_group_others, "roll up equalizer", config.equalizer_shaded);

    mainwin_enable_status_message (FALSE);

    /* Songname menu */

    check_set(toggleaction_group_others, "autoscroll songname", config.autoscroll);
    check_set (toggleaction_group_others, "stop after current song",
     aud_get_bool (NULL, "stop_after_current_song"));

    /* Playback menu */

    check_set (toggleaction_group_others, "playback repeat", aud_get_bool (NULL, "repeat"));
    check_set (toggleaction_group_others, "playback shuffle", aud_get_bool (NULL, "shuffle"));
    check_set (toggleaction_group_others, "playback no playlist advance",
     aud_get_bool (NULL, "no_playlist_advance"));

    mainwin_enable_status_message (TRUE);

    /* Visualization menu */

    switch ( config.vis_type )
    {
        case VIS_ANALYZER:
            check_set(radioaction_group_vismode, "vismode analyzer", TRUE);
            break;
        case VIS_SCOPE:
            check_set(radioaction_group_vismode, "vismode scope", TRUE);
            break;
        case VIS_VOICEPRINT:
            check_set(radioaction_group_vismode, "vismode voiceprint", TRUE);
            break;
        case VIS_OFF:
        default:
            check_set(radioaction_group_vismode, "vismode off", TRUE);
            break;
    }

    switch ( config.analyzer_mode )
    {
        case ANALYZER_FIRE:
            check_set(radioaction_group_anamode, "anamode fire", TRUE);
            break;
        case ANALYZER_VLINES:
            check_set(radioaction_group_anamode, "anamode vertical lines", TRUE);
            break;
        case ANALYZER_NORMAL:
        default:
            check_set(radioaction_group_anamode, "anamode normal", TRUE);
            break;
    }

    switch ( config.analyzer_type )
    {
        case ANALYZER_BARS:
            check_set(radioaction_group_anatype, "anatype bars", TRUE);
            break;
        case ANALYZER_LINES:
        default:
            check_set(radioaction_group_anatype, "anatype lines", TRUE);
            break;
    }

    check_set(toggleaction_group_others, "anamode peaks", config.analyzer_peaks );

    switch ( config.scope_mode )
    {
        case SCOPE_LINE:
            check_set(radioaction_group_scomode, "scomode line", TRUE);
            break;
        case SCOPE_SOLID:
            check_set(radioaction_group_scomode, "scomode solid", TRUE);
            break;
        case SCOPE_DOT:
        default:
            check_set(radioaction_group_scomode, "scomode dot", TRUE);
            break;
    }

    switch ( config.voiceprint_mode )
    {
        case VOICEPRINT_FIRE:
            check_set(radioaction_group_vprmode, "vprmode fire", TRUE);
            break;
        case VOICEPRINT_ICE:
            check_set(radioaction_group_vprmode, "vprmode ice", TRUE);
            break;
        case VOICEPRINT_NORMAL:
        default:
            check_set(radioaction_group_vprmode, "vprmode normal", TRUE);
            break;
    }

    switch ( config.vu_mode )
    {
        case VU_SMOOTH:
            check_set(radioaction_group_wshmode, "wshmode smooth", TRUE);
            break;
        case VU_NORMAL:
        default:
            check_set(radioaction_group_wshmode, "wshmode normal", TRUE);
            break;
    }

    switch ( config.analyzer_falloff )
    {
        case FALLOFF_SLOW:
            check_set(radioaction_group_anafoff, "anafoff slow", TRUE);
            break;
        case FALLOFF_MEDIUM:
            check_set(radioaction_group_anafoff, "anafoff medium", TRUE);
            break;
        case FALLOFF_FAST:
            check_set(radioaction_group_anafoff, "anafoff fast", TRUE);
            break;
        case FALLOFF_FASTEST:
            check_set(radioaction_group_anafoff, "anafoff fastest", TRUE);
            break;
        case FALLOFF_SLOWEST:
        default:
            check_set(radioaction_group_anafoff, "anafoff slowest", TRUE);
            break;
    }

    switch ( config.peaks_falloff )
    {
        case FALLOFF_SLOW:
            check_set(radioaction_group_peafoff, "peafoff slow", TRUE);
            break;
        case FALLOFF_MEDIUM:
            check_set(radioaction_group_peafoff, "peafoff medium", TRUE);
            break;
        case FALLOFF_FAST:
            check_set(radioaction_group_peafoff, "peafoff fast", TRUE);
            break;
        case FALLOFF_FASTEST:
            check_set(radioaction_group_peafoff, "peafoff fastest", TRUE);
            break;
        case FALLOFF_SLOWEST:
        default:
            check_set(radioaction_group_peafoff, "peafoff slowest", TRUE);
            break;
    }
}

static gboolean mainwin_info_button_press (GtkWidget * widget, GdkEventButton *
 event)
{
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        ui_popup_menu_show (UI_MENU_SONGNAME, event->x_root, event->y_root,
         FALSE, FALSE, event->button, event->time);
        return TRUE;
    }

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
        audgui_infowin_show_current ();
        return TRUE;
    }

    return FALSE;
}

static void
mainwin_create_widgets(void)
{
    mainwin_menubtn = button_new (9, 9, 0, 0, 0, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, FALSE, mainwin_menubtn, 6, 3);
    button_on_release (mainwin_menubtn, (ButtonCB) mainwin_menubtn_cb);

    mainwin_minimize = button_new (9, 9, 9, 0, 9, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, FALSE, mainwin_minimize, 244, 3);
    button_on_release (mainwin_minimize, (ButtonCB) mainwin_minimize_cb);

    mainwin_shade = button_new (9, 9, 0, 18, 9, 18, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, FALSE, mainwin_shade, 254, 3);
    button_on_release (mainwin_shade, (ButtonCB) mainwin_shade_toggle);

    mainwin_close = button_new (9, 9, 18, 0, 18, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, FALSE, mainwin_close, 264, 3);
    button_on_release (mainwin_close, (ButtonCB) handle_window_close);

    mainwin_rew = button_new (23, 18, 0, 0, 0, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_rew, 16, 88);
    button_on_press (mainwin_rew, mainwin_rew_press);
    button_on_release (mainwin_rew, mainwin_rew_release);
    button_on_rpress (mainwin_rew, mainwin_playback_rpress);

    mainwin_fwd = button_new (22, 18, 92, 0, 92, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_fwd, 108, 88);
    button_on_press (mainwin_fwd, mainwin_fwd_press);
    button_on_release (mainwin_fwd, mainwin_fwd_release);
    button_on_rpress (mainwin_fwd, mainwin_playback_rpress);

    mainwin_play = button_new (23, 18, 23, 0, 23, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_play, 39, 88);
    button_on_release (mainwin_play, (ButtonCB) aud_drct_play);
    button_on_rpress (mainwin_play, mainwin_playback_rpress);

    mainwin_pause = button_new (23, 18, 46, 0, 46, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_pause, 62, 88);
    button_on_release (mainwin_pause, (ButtonCB) aud_drct_pause);
    button_on_rpress (mainwin_pause, mainwin_playback_rpress);

    mainwin_stop = button_new (23, 18, 69, 0, 69, 18, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_stop, 85, 88);
    button_on_release (mainwin_stop, (ButtonCB) aud_drct_stop);
    button_on_rpress (mainwin_stop, mainwin_playback_rpress);

    mainwin_eject = button_new (22, 16, 114, 0, 114, 16, SKIN_CBUTTONS, SKIN_CBUTTONS);
    window_put_widget (mainwin, FALSE, mainwin_eject, 136, 89);
    button_on_release (mainwin_eject, (ButtonCB) action_play_file);

    mainwin_shuffle = button_new_toggle (46, 15, 28, 0, 28, 15, 28, 30, 28, 45, SKIN_SHUFREP, SKIN_SHUFREP);
    window_put_widget (mainwin, FALSE, mainwin_shuffle, 164, 89);
    button_on_release (mainwin_shuffle, mainwin_shuffle_cb);

    mainwin_repeat = button_new_toggle (28, 15, 0, 0, 0, 15, 0, 30, 0, 45, SKIN_SHUFREP, SKIN_SHUFREP);
    window_put_widget (mainwin, FALSE, mainwin_repeat, 210, 89);
    button_on_release (mainwin_repeat, mainwin_repeat_cb);

    mainwin_eq = button_new_toggle (23, 12, 0, 61, 46, 61, 0, 73, 46, 73, SKIN_SHUFREP, SKIN_SHUFREP);
    window_put_widget (mainwin, FALSE, mainwin_eq, 219, 58);
    button_on_release (mainwin_eq, mainwin_eq_cb);

    mainwin_pl = button_new_toggle (23, 12, 23, 61, 69, 61, 23, 73, 69, 73, SKIN_SHUFREP, SKIN_SHUFREP);
    window_put_widget (mainwin, FALSE, mainwin_pl, 242, 58);
    button_on_release (mainwin_pl, mainwin_pl_cb);

    mainwin_info = textbox_new (153, "", config.mainwin_use_bitmapfont ? NULL :
     config.mainwin_font, config.autoscroll);
    window_put_widget (mainwin, FALSE, mainwin_info, 112, 27);
    g_signal_connect (mainwin_info, "button-press-event", (GCallback)
     mainwin_info_button_press, NULL);

    mainwin_othertext = textbox_new (153, "", NULL, FALSE);
    window_put_widget (mainwin, FALSE, mainwin_othertext, 112, 43);

    mainwin_rate_text = textbox_new (15, "", NULL, FALSE);
    window_put_widget (mainwin, FALSE, mainwin_rate_text, 111, 43);

    mainwin_freq_text = textbox_new (10, "", NULL, FALSE);
    window_put_widget (mainwin, FALSE, mainwin_freq_text, 156, 43);

    mainwin_menurow = ui_skinned_menurow_new ();
    window_put_widget (mainwin, FALSE, mainwin_menurow, 10, 22);

    mainwin_volume = hslider_new (0, 51, SKIN_VOLUME, 68, 13, 0, 0, 14, 11, 15, 422, 0, 422);
    window_put_widget (mainwin, FALSE, mainwin_volume, 107, 57);
    hslider_on_motion (mainwin_volume, mainwin_volume_motion_cb);
    hslider_on_release (mainwin_volume, mainwin_volume_release_cb);

    mainwin_balance = hslider_new (0, 24, SKIN_BALANCE, 38, 13, 9, 0, 14, 11, 15, 422, 0, 422);
    window_put_widget (mainwin, FALSE, mainwin_balance, 177, 57);
    hslider_on_motion (mainwin_balance, mainwin_balance_motion_cb);
    hslider_on_release (mainwin_balance, mainwin_balance_release_cb);

    mainwin_monostereo = ui_skinned_monostereo_new ();
    window_put_widget (mainwin, FALSE, mainwin_monostereo, 212, 41);

    mainwin_playstatus = ui_skinned_playstatus_new ();
    window_put_widget (mainwin, FALSE, mainwin_playstatus, 24, 28);

    mainwin_minus_num = ui_skinned_number_new ();
    window_put_widget (mainwin, FALSE, mainwin_minus_num, 36, 26);
    g_signal_connect(mainwin_minus_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    mainwin_10min_num = ui_skinned_number_new ();
    window_put_widget (mainwin, FALSE, mainwin_10min_num, 48, 26);
    g_signal_connect(mainwin_10min_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    mainwin_min_num = ui_skinned_number_new ();
    window_put_widget (mainwin, FALSE, mainwin_min_num, 60, 26);
    g_signal_connect(mainwin_min_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    mainwin_10sec_num = ui_skinned_number_new ();
    window_put_widget (mainwin, FALSE, mainwin_10sec_num, 78, 26);
    g_signal_connect(mainwin_10sec_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    mainwin_sec_num = ui_skinned_number_new ();
    window_put_widget (mainwin, FALSE, mainwin_sec_num, 90, 26);
    g_signal_connect(mainwin_sec_num, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);

    mainwin_about = button_new_small (20, 25);
    window_put_widget (mainwin, FALSE, mainwin_about, 247, 83);
    button_on_release (mainwin_about, (ButtonCB) audgui_show_about_window);

    mainwin_vis = ui_vis_new ();
    window_put_widget (mainwin, FALSE, mainwin_vis, 24, 43);
    g_signal_connect(mainwin_vis, "button-press-event", G_CALLBACK(mainwin_vis_cb), NULL);

    mainwin_position = hslider_new (0, 219, SKIN_POSBAR, 248, 10, 0, 0, 29, 10, 248, 0, 278, 0);
    window_put_widget (mainwin, FALSE, mainwin_position, 16, 72);
    hslider_on_motion (mainwin_position, mainwin_position_motion_cb);
    hslider_on_release (mainwin_position, mainwin_position_release_cb);

    /* shaded */

    mainwin_shaded_menubtn = button_new (9, 9, 0, 0, 0, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, TRUE, mainwin_shaded_menubtn, 6, 3);
    button_on_release (mainwin_shaded_menubtn, (ButtonCB) mainwin_menubtn_cb);

    mainwin_shaded_minimize = button_new (9, 9, 9, 0, 9, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, TRUE, mainwin_shaded_minimize, 244, 3);
    button_on_release (mainwin_shaded_minimize, (ButtonCB) mainwin_minimize_cb);

    mainwin_shaded_shade = button_new (9, 9, 0, 27, 9, 27, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, TRUE, mainwin_shaded_shade, 254, 3);
    button_on_release (mainwin_shaded_shade, (ButtonCB) mainwin_shade_toggle);

    mainwin_shaded_close = button_new (9, 9, 18, 0, 18, 9, SKIN_TITLEBAR, SKIN_TITLEBAR);
    window_put_widget (mainwin, TRUE, mainwin_shaded_close, 264, 3);
    button_on_release (mainwin_shaded_close, (ButtonCB) handle_window_close);

    mainwin_srew = button_new_small (8, 7);
    window_put_widget (mainwin, TRUE, mainwin_srew, 169, 4);
    button_on_release (mainwin_srew, (ButtonCB) aud_drct_pl_prev);

    mainwin_splay = button_new_small (10, 7);
    window_put_widget (mainwin, TRUE, mainwin_splay, 177, 4);
    button_on_release (mainwin_splay, (ButtonCB) aud_drct_play);

    mainwin_spause = button_new_small (10, 7);
    window_put_widget (mainwin, TRUE, mainwin_spause, 187, 4);
    button_on_release (mainwin_spause, (ButtonCB) aud_drct_pause);

    mainwin_sstop = button_new_small (9, 7);
    window_put_widget (mainwin, TRUE, mainwin_sstop, 197, 4);
    button_on_release (mainwin_sstop, (ButtonCB) aud_drct_stop);

    mainwin_sfwd = button_new_small (8, 7);
    window_put_widget (mainwin, TRUE, mainwin_sfwd, 206, 4);
    button_on_release (mainwin_sfwd, (ButtonCB) aud_drct_pl_next);

    mainwin_seject = button_new_small (9, 7);
    window_put_widget (mainwin, TRUE, mainwin_seject, 216, 4);
    button_on_release (mainwin_seject, (ButtonCB) action_play_file);

    mainwin_svis = ui_svis_new ();
    window_put_widget (mainwin, TRUE, mainwin_svis, 79, 5);
    g_signal_connect(mainwin_svis, "button-press-event", G_CALLBACK(mainwin_vis_cb), NULL);

    mainwin_sposition = hslider_new (1, 13, SKIN_TITLEBAR, 17, 7, 0, 36, 3, 7, 17, 36, 17, 36);
    window_put_widget (mainwin, TRUE, mainwin_sposition, 226, 4);
    hslider_on_motion (mainwin_sposition, mainwin_spos_motion_cb);
    hslider_on_release (mainwin_sposition, mainwin_spos_release_cb);

    mainwin_stime_min = textbox_new (15, "", NULL, FALSE);
    window_put_widget (mainwin, TRUE, mainwin_stime_min, 130, 4);

    mainwin_stime_sec = textbox_new (10, "", NULL, FALSE);
    window_put_widget (mainwin, TRUE, mainwin_stime_sec, 147, 4);

    g_signal_connect(mainwin_stime_min, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);
    g_signal_connect(mainwin_stime_sec, "button-press-event", G_CALLBACK(change_timer_mode_cb), NULL);
}

static void show_widgets (void)
{
    gtk_widget_set_no_show_all (mainwin_minus_num, TRUE);
    gtk_widget_set_no_show_all (mainwin_10min_num, TRUE);
    gtk_widget_set_no_show_all (mainwin_min_num, TRUE);
    gtk_widget_set_no_show_all (mainwin_10sec_num, TRUE);
    gtk_widget_set_no_show_all (mainwin_sec_num, TRUE);
    gtk_widget_set_no_show_all (mainwin_stime_min, TRUE);
    gtk_widget_set_no_show_all (mainwin_stime_sec, TRUE);
    gtk_widget_set_no_show_all (mainwin_position, TRUE);
    gtk_widget_set_no_show_all (mainwin_sposition, TRUE);

    window_set_shaded (mainwin, config.player_shaded);
    window_show_all (mainwin);
}

static gboolean state_cb (GtkWidget * widget, GdkEventWindowState * event,
 void * unused)
{
    if (event->changed_mask & GDK_WINDOW_STATE_STICKY)
    {
        config.sticky = (event->new_window_state & GDK_WINDOW_STATE_STICKY) ?
         TRUE : FALSE;

        GtkToggleAction * action = (GtkToggleAction *)
         gtk_action_group_get_action (toggleaction_group_others,
         "view put on all workspaces");
        gtk_toggle_action_set_active (action, config.sticky);
    }

    if (event->changed_mask & GDK_WINDOW_STATE_ABOVE)
    {
        config.always_on_top = (event->new_window_state &
         GDK_WINDOW_STATE_ABOVE) ? TRUE : FALSE;

        GtkToggleAction * action = (GtkToggleAction *)
         gtk_action_group_get_action (toggleaction_group_others,
         "view always on top");
        gtk_toggle_action_set_active (action, config.always_on_top);
    }

    return TRUE;
}

static void mainwin_draw (GtkWidget * window, cairo_t * cr)
{
    gint width = config.player_shaded ? MAINWIN_SHADED_WIDTH : active_skin->properties.mainwin_width;
    gint height = config.player_shaded ? MAINWIN_SHADED_HEIGHT : active_skin->properties.mainwin_height;

    skin_draw_pixbuf (cr, SKIN_MAIN, 0, 0, 0, 0, width, height);
    skin_draw_mainwin_titlebar (cr, config.player_shaded, TRUE);
}

static void
mainwin_create_window(void)
{
    gint width = config.player_shaded ? MAINWIN_SHADED_WIDTH : active_skin->properties.mainwin_width;
    gint height = config.player_shaded ? MAINWIN_SHADED_HEIGHT : active_skin->properties.mainwin_height;

    mainwin = window_new (& config.player_x, & config.player_y, width, height,
     TRUE, config.player_shaded, mainwin_draw);

    gtk_window_set_title(GTK_WINDOW(mainwin), _("Audacious"));

    g_signal_connect(mainwin, "button_press_event",
                     G_CALLBACK(mainwin_mouse_button_press), NULL);
    g_signal_connect(mainwin, "scroll_event",
                     G_CALLBACK(mainwin_scrolled), NULL);

    drag_dest_set(mainwin);
    g_signal_connect ((GObject *) mainwin, "drag-data-received", (GCallback)
     mainwin_drag_data_received, 0);

    g_signal_connect(mainwin, "key_press_event",
                     G_CALLBACK(mainwin_keypress), NULL);

    ui_main_evlistener_init();

    g_signal_connect ((GObject *) mainwin, "window-state-event", (GCallback) state_cb, NULL);
    g_signal_connect ((GObject *) mainwin, "delete-event", (GCallback) handle_window_close, NULL);
}

void mainwin_unhook (void)
{
    if (seek_source != 0)
    {
        g_source_remove (seek_source);
        seek_source = 0;
    }

    hook_dissociate ("show main menu", (HookFunction) show_main_menu);
    ui_main_evlistener_dissociate ();
    start_stop_visual (TRUE);
}

void
mainwin_create(void)
{
    mainwin_create_window();

    gtk_window_add_accel_group( GTK_WINDOW(mainwin) , ui_manager_get_accel_group() );

    mainwin_create_widgets();
    show_widgets ();

    hook_associate ("show main menu", (HookFunction) show_main_menu, 0);
    status_message_enabled = TRUE;
}

static void mainwin_update_volume (void)
{
    gint volume, balance;

    aud_drct_get_volume_main (& volume);
    aud_drct_get_volume_balance (& balance);
    mainwin_set_volume_slider (volume);
    mainwin_set_balance_slider (balance);
    equalizerwin_set_volume_slider (volume);
    equalizerwin_set_balance_slider (balance);
}

static void mainwin_update_time_display (gint time, gint length)
{
    gchar scratch[7];
    format_time (scratch, time, length);

    ui_skinned_number_set (mainwin_minus_num, scratch[0]);
    ui_skinned_number_set (mainwin_10min_num, scratch[1]);
    ui_skinned_number_set (mainwin_min_num, scratch[2]);
    ui_skinned_number_set (mainwin_10sec_num, scratch[4]);
    ui_skinned_number_set (mainwin_sec_num, scratch[5]);

    if (! hslider_get_pressed (mainwin_sposition))
    {
        textbox_set_text (mainwin_stime_min, scratch);
        textbox_set_text (mainwin_stime_sec, scratch + 4);
    }

    playlistwin_set_time (scratch, scratch + 4);
}

static void mainwin_update_time_slider (gint time, gint length)
{
    gtk_widget_set_visible (mainwin_position, length > 0);
    gtk_widget_set_visible (mainwin_sposition, length > 0);

    if (length > 0 && seek_source == 0)
    {
        if (time < length)
        {
            hslider_set_pos (mainwin_position, time * (gint64) 219 / length);
            hslider_set_pos (mainwin_sposition, 1 + time * (gint64) 12 / length);
        }
        else
        {
            hslider_set_pos (mainwin_position, 219);
            hslider_set_pos (mainwin_sposition, 13);
        }

        mainwin_spos_set_knob ();
    }
}

void mainwin_update_song_info (void)
{
    mainwin_update_volume ();

    if (! aud_drct_get_playing ())
        return;

    gint time = 0, length = 0;
    if (aud_drct_get_ready ())
    {
        time = aud_drct_get_time ();
        length = aud_drct_get_length ();
    }

    mainwin_update_time_display (time, length);
    mainwin_update_time_slider (time, length);
}

/* toggleactionentries actions */

void action_anamode_peaks (GtkToggleAction * action)
{
    config.analyzer_peaks = gtk_toggle_action_get_active (action);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_autoscroll_songname (GtkToggleAction * action)
{
    config.autoscroll = gtk_toggle_action_get_active (action);
    textbox_set_scroll (mainwin_info, config.autoscroll);
    textbox_set_scroll (playlistwin_sinfo, config.autoscroll);
}

void action_playback_noplaylistadvance (GtkToggleAction * action)
{
    aud_set_bool (NULL, "no_playlist_advance", gtk_toggle_action_get_active (action));

    if (gtk_toggle_action_get_active (action))
        mainwin_show_status_message (_("Single mode."));
    else
        mainwin_show_status_message (_("Playlist mode."));
}

void action_playback_repeat (GtkToggleAction * action)
{
    aud_set_bool (NULL, "repeat", gtk_toggle_action_get_active (action));
    button_set_active (mainwin_repeat, gtk_toggle_action_get_active (action));
}

void action_playback_shuffle (GtkToggleAction * action)
{
    aud_set_bool (NULL, "shuffle", gtk_toggle_action_get_active (action));
    button_set_active (mainwin_shuffle, gtk_toggle_action_get_active (action));
}

void action_stop_after_current_song (GtkToggleAction * action)
{
    gboolean active = gtk_toggle_action_get_active (action);

    if (active != aud_get_bool (NULL, "stop_after_current_song"))
    {
        if (active)
            mainwin_show_status_message (_("Stopping after song."));
        else
            mainwin_show_status_message (_("Not stopping after song."));

        aud_set_bool (NULL, "stop_after_current_song", active);
    }
}

void action_view_always_on_top (GtkToggleAction * action)
{
    gboolean on_top = gtk_toggle_action_get_active (action);

    if (config.always_on_top != on_top)
    {
        config.always_on_top = on_top;
        ui_skinned_menurow_update (mainwin_menurow);
        hint_set_always (config.always_on_top);
    }
}

void action_view_on_all_workspaces (GtkToggleAction * action)
{
    gboolean sticky = gtk_toggle_action_get_active (action);

    if (config.sticky != sticky)
    {
        config.sticky = sticky;
        hint_set_sticky (sticky);
    }
}

void action_roll_up_player (GtkToggleAction * action)
{
    config.player_shaded = gtk_toggle_action_get_active (action);
    window_set_shaded (mainwin, config.player_shaded);

    gint width = config.player_shaded ? MAINWIN_SHADED_WIDTH : active_skin->properties.mainwin_width;
    gint height = config.player_shaded ? MAINWIN_SHADED_HEIGHT : active_skin->properties.mainwin_height;
    window_set_size (mainwin, width, height);
    mainwin_set_shape ();
}

void action_show_player (GtkToggleAction * action)
{
    mainwin_show(gtk_toggle_action_get_active(action));
}


/* radioactionentries actions (one callback for each radio group) */

void action_anafoff (GtkAction * action, GtkRadioAction * current)
{
    config.analyzer_falloff = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_anamode (GtkAction * action, GtkRadioAction * current)
{
    config.analyzer_mode = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_anatype (GtkAction * action, GtkRadioAction * current)
{
    config.analyzer_type = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_peafoff (GtkAction * action, GtkRadioAction * current)
{
    config.peaks_falloff = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_scomode (GtkAction * action, GtkRadioAction * current)
{
    config.scope_mode = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_vismode (GtkAction * action, GtkRadioAction * current)
{
    config.vis_type = gtk_radio_action_get_current_value (current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);

    start_stop_visual (FALSE);
}

void action_vprmode (GtkAction * action, GtkRadioAction * current)
{
    config.voiceprint_mode = gtk_radio_action_get_current_value(current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_wshmode (GtkAction * action, GtkRadioAction * current)
{
    config.vu_mode = gtk_radio_action_get_current_value(current);
    ui_vis_clear_data (mainwin_vis);
    ui_svis_clear_data (mainwin_svis);
}

void action_viewtime (GtkAction * action, GtkRadioAction * current)
{
    config.timer_mode = gtk_radio_action_get_current_value (current);
}


/* actionentries actions */

void action_play_file (void)
{
    audgui_run_filebrowser(TRUE); /* TRUE = PLAY_BUTTON */
}

void action_play_location (void)
{
    audgui_show_add_url_window (TRUE);
}

void action_ab_set (void)
{
    if (aud_drct_get_length () > 0)
    {
        int a, b;
        aud_drct_get_ab_repeat (& a, & b);

        if (a < 0 || b >= 0)
        {
            a = aud_drct_get_time ();
            b = -1;
            mainwin_show_status_message (_("Repeat point A set."));
        }
        else
        {
            b = aud_drct_get_time ();
            mainwin_show_status_message (_("Repeat point B set."));
        }

        aud_drct_set_ab_repeat (a, b);
    }
}

void action_ab_clear (void)
{
    mainwin_show_status_message (_("Repeat points cleared."));
    aud_drct_set_ab_repeat (-1, -1);
}
