/*
 * Dynamic Range Compression Plugin for Audacious
 * Copyright 2010 John Lindgren
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

#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compressor.h"

double compressor_center, compressor_range;

/* Response time adjustments.  Maybe this should be adjustable.  Or maybe that
 * would just be confusing.  I don't know. */
#define CHUNK_TIME 0.2 /* seconds */
#define CHUNKS 5
#define DECAY 0.3

static float * buffer, * output, * peaks;
static int output_size;
static int chunk_size, buffer_size;
static int ring_at, buffer_filled, peaks_filled;
static float current_peak;
static int output_filled;

static void buffer_append (float * * data, int * length)
{
    int offset = (chunk_size * ring_at + buffer_filled) % buffer_size;
    int writable = MIN (* length, buffer_size - buffer_filled);

    if (writable <= buffer_size - offset)
        memcpy (buffer + offset, * data, sizeof (float) * writable);
    else
    {
        int part = buffer_size - offset;

        memcpy (buffer + offset, * data, part);
        memcpy (buffer, (* data) + part, writable - part);
    }

    buffer_filled += writable;
    * data += writable;
    * length -= writable;
}

/* I used to find the maximum sample and take that as the peak, but that doesn't
 * work well on badly clipped tracks.  Now, I use the highly sophisticated
 * method of averaging the absolute value of the samples and multiplying by 6, a
 * number proved by experiment (on exactly three files) to best approximate the
 * actual peak. */

static float calc_peak (float * data, int length)
{
    if (! length)
        return 0;

    float sum = 0;

    float * end = data + length;
    while (data < end)
        sum += fabsf (* data ++);

    return sum / length * 6;
}

static void do_ramp (float * data, int length, float peak_a, float peak_b)
{
    float a = powf (peak_a / compressor_center, compressor_range - 1);
    float b = powf (peak_b / compressor_center, compressor_range - 1);
    int count;

    for (count = 0; count < length; count ++)
    {
        * data = (* data) * (a * (length - count) + b * count) / length;
        data ++;
    }
}

static void output_append (float * data, int length)
{
    if (output_size < output_filled + length)
    {
        output_size = output_filled + length;
        output = realloc (output, sizeof (float) * output_size);
    }

    memcpy (output + output_filled, data, sizeof (float) * length);
    output_filled += length;
}

static void reset (void)
{
    ring_at = 0;
    buffer_filled = 0;
    peaks_filled = 0;
    current_peak = 0.0;
}

#define IN_RING(i) ((ring_at + i) % CHUNKS)
#define GET_PEAK(i) peaks[IN_RING (i)]

static inline float FMAX (float a, float b)
{
    return a > b ? a : b;
}

static void do_compress (float * * data, int * samples, char finish)
{
    int count;
    float new_peak;

    output_filled = 0;

    while (1)
    {
        buffer_append (data, samples);

        if (buffer_filled < buffer_size)
            break;

        for (; peaks_filled < CHUNKS; peaks_filled ++)
            GET_PEAK (peaks_filled) = calc_peak (buffer + chunk_size * IN_RING
             (peaks_filled), chunk_size);

        if (current_peak == 0.0)
            current_peak = FMAX (0.01, calc_peak (peaks, CHUNKS));

        new_peak = FMAX (0.01, GET_PEAK (0));
        new_peak = FMAX (new_peak, current_peak * (1.0 - DECAY));

        for (count = 1; count < CHUNKS; count ++)
            new_peak = FMAX (new_peak, current_peak + (GET_PEAK (count) -
             current_peak) / count);

        do_ramp (buffer + chunk_size * ring_at, chunk_size, current_peak,
         new_peak);

        output_append (buffer + chunk_size * ring_at, chunk_size);

        ring_at = IN_RING (1);
        buffer_filled -= chunk_size;
        peaks_filled --;
        current_peak = new_peak;
    }

    if (finish)
    {
        int offset = chunk_size * ring_at;
        int first = MIN (buffer_filled, buffer_size - offset);
        int second = buffer_filled - first;

        if (current_peak == 0.0)
        {
            current_peak = FMAX (0.01, calc_peak (buffer + offset, first));
            current_peak = FMAX (current_peak, calc_peak (buffer, second));
        }

        do_ramp (buffer + offset, first, current_peak, current_peak);
        do_ramp (buffer, second, current_peak, current_peak);

        output_append (buffer + offset, first);
        output_append (buffer, second);

        reset ();
    }

    * data = output;
    * samples = output_filled;
}

int compressor_init (void)
{
    compressor_config_load ();

    buffer = NULL;
    output = NULL;
    output_size = 0;
    peaks = NULL;

    return 1;
}

void compressor_cleanup (void)
{
    free (buffer);
    free (output);
    free (peaks);

    compressor_config_save ();
}

void compressor_start (int * channels, int * rate)
{
    chunk_size = (* channels) * (int) ((* rate) * CHUNK_TIME);
    buffer_size = chunk_size * CHUNKS;
    buffer = realloc (buffer, sizeof (float) * buffer_size);
    peaks = realloc (peaks, sizeof (float) * CHUNKS);

    reset ();
}

void compressor_process (float * * data, int * samples)
{
    do_compress (data, samples, 0);
}

void compressor_flush (void)
{
    reset ();
}

void compressor_finish (float * * data, int * samples)
{
    do_compress (data, samples, 1);
}

int compressor_decoder_to_output_time (int time)
{
    return time;
}

int compressor_output_to_decoder_time (int time)
{
    return time;
}
