/*
 *  Copyright 2000,2001 Haavard Kvaalen <havardk@sol.no>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include <glib.h>
#include <stdlib.h>
#include <math.h>

#define MIN_FREQ        10
#define MAX_FREQ        20000
#define OUTPUT_FREQ     44100
#define BUF_SAMPLES     512
#define BUF_BYTES       (BUF_SAMPLES * sizeof(float))

#ifndef PI
#define PI              3.14159265358979323846
#endif

static InputPlugin tone_ip;

static void tone_init(void)
{
    aud_uri_set_plugin("tone://", &tone_ip);
}

static void tone_about (void)
{
    static GtkWidget * aboutbox = NULL;

    audgui_simple_message (& aboutbox, GTK_MESSAGE_INFO,
     _("About Tone Generator"),
     /* I18N: UTF-8 Translation: "Haavard Kvaalen" -> "H\303\245vard Kv\303\245len" */
     _("Sinus tone generator by Haavard Kvaalen <havardk@xmms.org>\n"
     "Modified by Daniel J. Peng <danielpeng@bigfoot.com>\n\n"
     "To use it, add a URL: tone://frequency1;frequency2;frequency3;...\n"
     "e.g. tone://2000;2005 to play a 2000Hz tone and a 2005Hz tone"));
}

static gint tone_is_our_file(const gchar * filename)
{
    if (!strncmp(filename, "tone://", 7))
        return TRUE;
    return FALSE;
}

static GArray *tone_filename_parse(const gchar * filename)
{
    GArray *frequencies = g_array_new(FALSE, FALSE, sizeof(double));
    gchar **strings, **ptr;

    if (strncmp(filename, "tone://", 7))
        return NULL;

    strings = g_strsplit(filename + 7, ";", 100);

    for (ptr = strings; *ptr != NULL; ptr++)
    {
        gdouble freq = strtod(*ptr, NULL);
        if (freq >= MIN_FREQ && freq <= MAX_FREQ)
            g_array_append_val(frequencies, freq);
    }
    g_strfreev(strings);

    if (frequencies->len == 0)
    {
        g_array_free(frequencies, TRUE);
        frequencies = NULL;
    }

    return frequencies;
}

static gchar *tone_title(const gchar * filename)
{
    GArray *freqs;
    gchar *title;
    gsize i;

    freqs = tone_filename_parse(filename);
    if (freqs == NULL)
        return NULL;

    title = g_strdup_printf(_("%s %.1f Hz"), _("Tone Generator: "), g_array_index(freqs, double, 0));
    for (i = 1; i < freqs->len; i++)
    {
        gchar *old_title = title;
        title = g_strdup_printf("%s;%.1f Hz", old_title, g_array_index(freqs, double, i));
        g_free(old_title);
    }
    g_array_free(freqs, TRUE);

    return title;
}

static void tone_play(InputPlayback * playback)
{
    GArray *frequencies;
    gchar *name, *filename = playback->filename;
    gfloat data[BUF_SAMPLES];
    gsize i;
    struct
    {
        gdouble wd;
        guint period, t;
    } *tone = NULL;

    frequencies = tone_filename_parse(filename);
    if (frequencies == NULL)
        return;

    if (playback->output->open_audio(FMT_FLOAT, OUTPUT_FREQ, 1) == 0)
    {
        playback->error = TRUE;
        goto error_exit;
    }

    name = tone_title(filename);
    playback->set_params(playback, name, -1, 16 * OUTPUT_FREQ, OUTPUT_FREQ, 1);
    g_free(name);

    tone = g_malloc(frequencies->len * sizeof(*tone));
    for (i = 0; i < frequencies->len; i++)
    {
        gdouble f = g_array_index(frequencies, gdouble, i);
        tone[i].wd = 2 * PI * f / OUTPUT_FREQ;
        tone[i].period = (G_MAXINT * 2U / OUTPUT_FREQ) * (OUTPUT_FREQ / f);
        tone[i].t = 0;
    }

    playback->playing = TRUE;
    playback->error = FALSE;
    playback->set_pb_ready(playback);

    while (playback->playing)
    {
        for (i = 0; i < BUF_SAMPLES; i++)
        {
            gsize j;
            double sum_sines;

            for (sum_sines = 0, j = 0; j < frequencies->len; j++)
            {
                sum_sines += sin(tone[j].wd * tone[j].t);
                if (tone[j].t > tone[j].period)
                    tone[j].t -= tone[j].period;
                tone[j].t++;
            }
            /* dithering can cause a little bit of clipping */
            data[i] = (sum_sines * 0.999 / (gdouble) frequencies->len);
        }

        playback->pass_audio(playback, FMT_FLOAT, 1, BUF_BYTES, data, &playback->playing);
    }

error_exit:
    g_array_free(frequencies, TRUE);
    g_free(tone);

    playback->output->close_audio();
    playback->playing = FALSE;
}

static void tone_stop(InputPlayback * playback)
{
    playback->playing = FALSE;
}

static void tone_pause(InputPlayback * playback, gshort paused)
{
    playback->output->pause(paused);
}

static Tuple *tone_get_song_tuple(const gchar * filename)
{
    Tuple *tuple = tuple_new_from_filename(filename);
    gchar *tmp;

    if (tuple == NULL)
        return NULL;

    if ((tmp = tone_title(filename)) != NULL)
    {
        tuple_associate_string(tuple, FIELD_TITLE, NULL, tmp);
        g_free(tmp);
    }

    return tuple;
}

static InputPlugin tone_ip = {
    .description = "Tone Generator",
    .init = tone_init,
    .about = tone_about,
    .is_our_file = tone_is_our_file,
    .play_file = tone_play,
    .stop = tone_stop,
    .pause = tone_pause,
    .get_song_tuple = tone_get_song_tuple
};

static InputPlugin *tonegen_iplist[] = { &tone_ip, NULL };

DECLARE_PLUGIN(tonegen, NULL, NULL, tonegen_iplist, NULL, NULL, NULL, NULL, NULL);
