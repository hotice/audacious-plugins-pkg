/*
 * Channel Mixer Plugin for Audacious
 * Copyright 2011-2012 John Lindgren and Michał Lipski
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

/* TODO: implement more surround converters */

#include <stdio.h>
#include <stdlib.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

#include "config.h"

#define MAX_CHANNELS 8

typedef void (* Converter) (float * * data, int * samples);

static float * mixer_buf;

static void mono_to_stereo (float * * data, int * samples)
{
    int frames = * samples;
    float * get = * data;
    float * set = mixer_buf = realloc (mixer_buf, sizeof (float) * 2 * frames);

    * data = mixer_buf;
    * samples = 2 * frames;

    while (frames --)
    {
        float val = * get ++;
        * set ++ = val;
        * set ++ = val;
    }
}

static void stereo_to_mono (float * * data, int * samples)
{
    int frames = * samples / 2;
    float * get = * data;
    float * set = mixer_buf = realloc (mixer_buf, sizeof (float) * frames);

    * data = mixer_buf;
    * samples = frames;

    while (frames --)
    {
        float val = * get ++;
        val += * get ++;
        * set ++ = val / 2;
    }
}

static void quadro_to_stereo (float * * data, int * samples)
{
    int frames = * samples / 4;
    float * get = * data;
    float * set = mixer_buf = realloc (mixer_buf, sizeof (float) * 2 * frames);

    * data = mixer_buf;
    * samples = 2 * frames;

    while (frames --)
    {
        float front_left  = * get ++;
        float front_right = * get ++;
        float back_left   = * get ++;
        float back_right  = * get ++;
        * set ++ = front_left + (back_left * 0.7);
        * set ++ = front_right + (back_right * 0.7);
    }
}

static void surround_5p1_to_stereo (float * * data, int * samples)
{
    int frames = * samples / 6;
    float * get = * data;
    float * set = mixer_buf = realloc (mixer_buf, sizeof (float) * 2 * frames);

    * data = mixer_buf;
    * samples = 2 * frames;

    while (frames --)
    {
        float front_left  = * get ++;
        float front_right = * get ++;
        float center = * get ++;
        float lfe    = * get ++;
        float rear_left   = * get ++;
        float rear_right  = * get ++;
        * set ++ = front_left + (center * 0.5) + (lfe * 0.5) + (rear_left * 0.5);
        * set ++ = front_right + (center * 0.5) + (lfe * 0.5) + (rear_right * 0.5);
    }
}

static const Converter converters[MAX_CHANNELS + 1][MAX_CHANNELS + 1] = {
 [1][2] = mono_to_stereo,
 [2][1] = stereo_to_mono,
 [4][2] = quadro_to_stereo,
 [6][2] = surround_5p1_to_stereo};

static int input_channels, output_channels;

void mixer_start (int * channels, int * rate)
{
    input_channels = * channels;
    output_channels = aud_get_int ("mixer", "channels");
    output_channels = CLAMP (output_channels, 1, MAX_CHANNELS);

    if (input_channels == output_channels)
        return;

    if (input_channels < 1 || input_channels > MAX_CHANNELS ||
     ! converters[input_channels][output_channels])
    {
        fprintf (stderr, "Converting %d to %d channels is not implemented.\n",
         input_channels, output_channels);
        return;
    }

    * channels = output_channels;
}

void mixer_process (float * * data, int * samples)
{
    if (input_channels == output_channels)
        return;

    if (input_channels < 1 || input_channels > MAX_CHANNELS ||
     ! converters[input_channels][output_channels])
        return;

    converters[input_channels][output_channels] (data, samples);
}

static const char * const mixer_defaults[] = {
 "channels", "2",
  NULL};

static bool_t mixer_init (void)
{
    aud_config_set_defaults ("mixer", mixer_defaults);
    return TRUE;
}

static void mixer_cleanup (void)
{
    free (mixer_buf);
    mixer_buf = 0;
}

static const char mixer_about[] =
 N_("Channel Mixer Plugin for Audacious\n"
    "Copyright 2011-2012 John Lindgren and Michał Lipski");

static const PreferencesWidget mixer_widgets[] = {
 {WIDGET_LABEL, N_("<b>Channel Mixer</b>")},
 {WIDGET_SPIN_BTN, N_("Output channels:"),
  .cfg_type = VALUE_INT, .csect = "mixer", .cname = "channels",
  .data = {.spin_btn = {1, MAX_CHANNELS, 1}}}};

static const PluginPreferences mixer_prefs = {
 .widgets = mixer_widgets,
 .n_widgets = sizeof mixer_widgets / sizeof mixer_widgets[0]};

AUD_EFFECT_PLUGIN
(
    .name = N_("Channel Mixer"),
    .domain = PACKAGE,
    .about_text = mixer_about,
    .prefs = & mixer_prefs,
    .init = mixer_init,
    .cleanup = mixer_cleanup,
    .start = mixer_start,
    .process = mixer_process,
    .finish = mixer_process,
    .order = 2, /* must be before crossfade */
)
