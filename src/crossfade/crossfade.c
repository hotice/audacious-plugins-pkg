/*
 * Crossfade Plugin for Audacious
 * Copyright 2010-2012 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <stdlib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

#include "config.h"

enum
{
    STATE_OFF,
    STATE_PREBUFFER,
    STATE_RUNNING,
    STATE_BETWEEN,
    STATE_STOPPING,
};

static const char * const crossfade_defaults[] = {
 "length", "3",
 NULL};

static char state = STATE_OFF;
static int current_channels = 0, current_rate = 0;
static float * buffer = NULL;
static int buffer_size = 0, buffer_filled = 0;
static int prebuffer_filled = 0;
static float * output = NULL;
static int output_size = 0;

static void reset (void)
{
    state = STATE_OFF;
    current_channels = 0;
    current_rate = 0;
    free (buffer);
    buffer = NULL;
    buffer_size = 0;
    buffer_filled = 0;
    prebuffer_filled = 0;
    free (output);
    output = NULL;
    output_size = 0;
}

static bool_t crossfade_init (void)
{
    aud_config_set_defaults ("crossfade", crossfade_defaults);
    return TRUE;
}

static void crossfade_cleanup (void)
{
    reset ();
}

static void crossfade_start (int * channels, int * rate)
{
    if (state != STATE_BETWEEN)
        reset ();
    else if (* channels != current_channels)
    {
        aud_interface_show_error (_("Crossfading failed because the songs had "
         "a different number of channels.  You can use the Channel Mixer to "
         "convert the songs to the same number of channels."));
        reset ();
    }
    else if (* rate != current_rate)
    {
        aud_interface_show_error (_("Crossfading failed because the songs had "
         "different sample rates.  You can use the Sample Rate Converter to "
         "convert the songs to the same sample rate."));
        reset ();
    }

    state = STATE_PREBUFFER;
    current_channels = * channels;
    current_rate = * rate;
    prebuffer_filled = 0;
}

static void do_ramp (float * data, int length, float a, float b)
{
    int count;

    for (count = 0; count < length; count ++)
    {
        * data = (* data) * (a * (length - count) + b * count) / length;
        data ++;
    }
}

static void mix (float * data, float * new, int length)
{
    while (length --)
        (* data ++) += (* new ++);
}

static void enlarge_buffer (int length)
{
    if (length > buffer_size)
    {
        buffer = realloc (buffer, sizeof (float) * length);
        buffer_size = length;
    }
}

static void add_data (float * data, int length)
{
    if (state == STATE_PREBUFFER)
    {
        int full = current_channels * current_rate * aud_get_int ("crossfade", "length");

        if (prebuffer_filled < full)
        {
            int copy = MIN (length, full - prebuffer_filled);
            float a = (float) prebuffer_filled / full;
            float b = (float) (prebuffer_filled + copy) / full;

            if (prebuffer_filled + copy > buffer_filled)
            {
                enlarge_buffer (prebuffer_filled + copy);
                memset (buffer + buffer_filled, 0, sizeof (float) *
                 (prebuffer_filled + copy - buffer_filled));
                buffer_filled = prebuffer_filled + copy;
            }

            do_ramp (data, copy, a, b);
            mix (buffer + prebuffer_filled, data, copy);
            prebuffer_filled += copy;
            data += copy;
            length -= copy;
        }

        if (prebuffer_filled < full)
            return;

        if (prebuffer_filled < buffer_filled)
        {
            int copy = MIN (length, buffer_filled - prebuffer_filled);

            mix (buffer + prebuffer_filled, data, copy);
            prebuffer_filled += copy;
            data += copy;
            length -= copy;
        }

        if (prebuffer_filled < buffer_filled)
            return;

        state = STATE_RUNNING;
    }

    if (state != STATE_RUNNING)
        return;

    enlarge_buffer (buffer_filled + length);
    memcpy (buffer + buffer_filled, data, sizeof (float) * length);
    buffer_filled += length;
}

static void enlarge_output (int length)
{
    if (length > output_size)
    {
        output = realloc (output, sizeof (float) * length);
        output_size = length;
    }
}

static void return_data (float * * data, int * length)
{
    int full = current_channels * current_rate * aud_get_int ("crossfade", "length");
    int copy = buffer_filled - full;

    /* only return if we have at least 1/2 second -- this reduces memmove's */
    if (state != STATE_RUNNING || copy < current_channels * (current_rate / 2))
    {
        * data = NULL;
        * length = 0;
        return;
    }

    enlarge_output (copy);
    memcpy (output, buffer, sizeof (float) * copy);
    buffer_filled -= copy;
    memmove (buffer, buffer + copy, sizeof (float) * buffer_filled);
    * data = output;
    * length = copy;
}

static void crossfade_process (float * * data, int * samples)
{
    add_data (* data, * samples);
    return_data (data, samples);
}

static void crossfade_flush (void)
{
    if (state == STATE_PREBUFFER || state == STATE_RUNNING)
    {
        state = STATE_RUNNING;
        buffer_filled = 0;
    }
}

static void crossfade_finish (float * * data, int * samples)
{
    if (state == STATE_BETWEEN) /* second call, end of last song */
    {
        enlarge_output (buffer_filled);
        memcpy (output, buffer, sizeof (float) * buffer_filled);
        * data = output;
        * samples = buffer_filled;
        buffer_filled = 0;
        state = STATE_OFF;
        return;
    }

    add_data (* data, * samples);
    return_data (data, samples);

    if (state == STATE_PREBUFFER || state == STATE_RUNNING)
    {
        do_ramp (buffer, buffer_filled, 1.0, 0.0);
        state = STATE_BETWEEN;
    }
}

static int crossfade_adjust_delay (int delay)
{
    return delay + (int64_t) (buffer_filled / current_channels) * 1000 / current_rate;
}

static const char crossfade_about[] =
 "Crossfade Plugin for Audacious\n"
 "Copyright 2010-2012 John Lindgren";

static const PreferencesWidget crossfade_widgets[] = {
 {WIDGET_LABEL, N_("<b>Crossfade</b>")},
 {WIDGET_SPIN_BTN, N_("Overlap:"),
  .cfg_type = VALUE_INT, .csect = "crossfade", .cname = "length",
  .data = {.spin_btn = {1, 10, 1, N_("seconds")}}}};

static const PluginPreferences crossfade_prefs = {
 .widgets = crossfade_widgets,
 .n_widgets = sizeof crossfade_widgets / sizeof crossfade_widgets[0]};

AUD_EFFECT_PLUGIN
(
    .name = N_("Crossfade"),
    .domain = PACKAGE,
    .about_text = crossfade_about,
    .prefs = & crossfade_prefs,
    .init = crossfade_init,
    .cleanup = crossfade_cleanup,
    .start = crossfade_start,
    .process = crossfade_process,
    .flush = crossfade_flush,
    .finish = crossfade_finish,
    .adjust_delay = crossfade_adjust_delay,
    .order = 5, /* must be after resample and mixer */
    .preserves_format = TRUE
)
