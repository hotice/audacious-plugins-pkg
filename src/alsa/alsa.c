/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2010 John Lindgren
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

/*
 * Because ALSA is not thread-safe (despite claims to the contrary) we use non-
 * blocking output in the pump thread with the mutex locked, then unlock the
 * mutex and wait for more room in the buffer with poll() while other threads
 * lock the mutex and read the output time.  We poll a pipe of our own as well
 * as the ALSA file descriptors so that we can wake up the pump thread when
 * needed.
 *
 * When paused, or when it comes to the end of the data given it, the pump will
 * wait on alsa_cond for the signal to continue.  When it has more data waiting,
 * however, it will be sitting in poll() waiting for ALSA's signal that more
 * data can be written.
 *
 * * After adding more data to the buffer, and after resuming from pause,
 *   signal on alsa_cond to wake the pump.  (There is no need to signal when
 *   entering pause.)
 * * After setting the pump_quit flag, signal on alsa_cond AND the poll_pipe
 *   before joining the thread.
 */

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <glib.h>

#include <audacious/audconfig.h>
#include <audacious/debug.h>

#include "alsa.h"

#define CHECK_VAL_RECOVER(value, function, ...) \
do { \
    (value) = function (__VA_ARGS__); \
    if ((value) < 0) { \
        CHECK (snd_pcm_recover, alsa_handle, (value), 0); \
        CHECK_VAL ((value), function, __VA_ARGS__); \
    } \
} while (0)

#define CHECK_RECOVER(function, ...) \
do { \
    int error2; \
    CHECK_VAL_RECOVER (error2, function, __VA_ARGS__); \
} while (0)

static snd_pcm_t * alsa_handle;
static char alsa_initted;
pthread_mutex_t alsa_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alsa_cond = PTHREAD_COND_INITIALIZER;

static snd_pcm_format_t alsa_format;
static int alsa_channels, alsa_rate;

static void * alsa_buffer;
static int alsa_buffer_length, alsa_buffer_data_start, alsa_buffer_data_length;

static int64_t alsa_time; /* microseconds */
static char alsa_prebuffer, alsa_paused;
static int alsa_paused_time;

static int poll_pipe[2];
static int poll_count;
static struct pollfd * poll_handles;

static char pump_quit;
static pthread_t pump_thread;

static snd_mixer_t * alsa_mixer;
static snd_mixer_elem_t * alsa_mixer_element;

static char poll_setup (void)
{
    if (pipe (poll_pipe))
    {
        ERROR ("Failed to create pipe: %s.\n", strerror (errno));
        return 0;
    }

    if (fcntl (poll_pipe[0], F_SETFL, O_NONBLOCK))
    {
        ERROR ("Failed to set O_NONBLOCK on pipe: %s.\n", strerror (errno));
        close (poll_pipe[0]);
        close (poll_pipe[1]);
        return 0;
    }

    poll_count = 1 + snd_pcm_poll_descriptors_count (alsa_handle);
    poll_handles = malloc (sizeof (struct pollfd) * poll_count);
    poll_handles[0].fd = poll_pipe[0];
    poll_handles[0].events = POLLIN;
    poll_count = 1 + snd_pcm_poll_descriptors (alsa_handle, poll_handles + 1,
     poll_count - 1);

    return 1;
}

static void poll_sleep (void)
{
    char c;

    poll (poll_handles, poll_count, -1);

    if (poll_handles[0].revents & POLLIN)
    {
        while (read (poll_pipe[0], & c, 1) == 1)
            ;
    }
}

static void poll_wake (void)
{
    const char c = 0;

    write (poll_pipe[1], & c, 1);
}

static void poll_cleanup (void)
{
    close (poll_pipe[0]);
    close (poll_pipe[1]);
    free (poll_handles);
}

static void * pump (void * unused)
{
    int length, written;

    pthread_mutex_lock (& alsa_mutex);
    pthread_cond_broadcast (& alsa_cond); /* signal thread started */

    while (! pump_quit)
    {
        if (alsa_prebuffer || alsa_paused || ! alsa_buffer_data_length)
        {
            pthread_cond_wait (& alsa_cond, & alsa_mutex);
            continue;
        }

        CHECK_VAL_RECOVER (length, snd_pcm_avail_update, alsa_handle);

        if (! length)
            goto WAIT;

        length = snd_pcm_frames_to_bytes (alsa_handle, length);
        length = MIN (length, alsa_buffer_data_length);
        length = MIN (length, alsa_buffer_length - alsa_buffer_data_start);
        length = snd_pcm_bytes_to_frames (alsa_handle, length);

        CHECK_VAL_RECOVER (written, snd_pcm_writei, alsa_handle, (char *)
         alsa_buffer + alsa_buffer_data_start, length);
        written = snd_pcm_frames_to_bytes (alsa_handle, written);
        alsa_buffer_data_start += written;
        alsa_buffer_data_length -= written;

        pthread_cond_broadcast (& alsa_cond); /* signal write complete */

        if (alsa_buffer_data_start == alsa_buffer_length)
        {
            alsa_buffer_data_start = 0;
            continue;
        }

    WAIT:
        pthread_mutex_unlock (& alsa_mutex);
        poll_sleep ();
        pthread_mutex_lock (& alsa_mutex);
    }

FAILED:
    pthread_mutex_unlock (& alsa_mutex);
    return NULL;
}

static void pump_start (void)
{
    AUDDBG ("Starting pump.\n");
    pthread_create (& pump_thread, NULL, pump, NULL);
    pthread_cond_wait (& alsa_cond, & alsa_mutex);
}

static void pump_stop (void)
{
    AUDDBG ("Stopping pump.\n");
    pump_quit = 1;
    pthread_cond_broadcast (& alsa_cond);
    poll_wake ();
    pthread_mutex_unlock (& alsa_mutex);
    pthread_join (pump_thread, NULL);
    pthread_mutex_lock (& alsa_mutex);
    pump_quit = 0;
}

static void start_playback (void)
{
    AUDDBG ("Starting playback.\n");
    CHECK (snd_pcm_prepare, alsa_handle);

FAILED:
    alsa_prebuffer = 0;
    pthread_cond_broadcast (& alsa_cond);
}

static int get_delay (void)
{
    snd_pcm_sframes_t delay = 0;

    CHECK_RECOVER (snd_pcm_delay, alsa_handle, & delay);

FAILED:
    return delay;
}

static int get_output_time (void)
{
    return (alsa_time - (int64_t) (snd_pcm_bytes_to_frames (alsa_handle,
     alsa_buffer_data_length) + get_delay ()) * 1000000 / alsa_rate) / 1000;
}

OutputPluginInitStatus alsa_init (void)
{
    alsa_handle = NULL;
    alsa_initted = 0;

    return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
}

void alsa_soft_init (void)
{
    if (! alsa_initted)
    {
        AUDDBG ("Initialize.\n");
        alsa_config_load ();
        alsa_open_mixer ();
        alsa_initted = 1;
    }
}

void alsa_cleanup (void)
{
    if (alsa_initted)
    {
        AUDDBG ("Cleanup.\n");
        alsa_close_mixer ();
        alsa_config_save ();
    }
}

static int convert_aud_format (int aud_format)
{
    const struct
    {
        int aud_format, format;
    }
    table[] =
    {
        {FMT_FLOAT, SND_PCM_FORMAT_FLOAT},
        {FMT_S8, SND_PCM_FORMAT_S8},
        {FMT_U8, SND_PCM_FORMAT_U8},
        {FMT_S16_LE, SND_PCM_FORMAT_S16_LE},
        {FMT_S16_BE, SND_PCM_FORMAT_S16_BE},
        {FMT_U16_LE, SND_PCM_FORMAT_U16_LE},
        {FMT_U16_BE, SND_PCM_FORMAT_U16_BE},
        {FMT_S24_LE, SND_PCM_FORMAT_S24_LE},
        {FMT_S24_BE, SND_PCM_FORMAT_S24_BE},
        {FMT_U24_LE, SND_PCM_FORMAT_U24_LE},
        {FMT_U24_BE, SND_PCM_FORMAT_U24_BE},
        {FMT_S32_LE, SND_PCM_FORMAT_S32_LE},
        {FMT_S32_BE, SND_PCM_FORMAT_S32_BE},
        {FMT_U32_LE, SND_PCM_FORMAT_U32_LE},
        {FMT_U32_BE, SND_PCM_FORMAT_U32_BE},
    };

    int count;

    for (count = 0; count < G_N_ELEMENTS (table); count ++)
    {
         if (table[count].aud_format == aud_format)
             return table[count].format;
    }

    return SND_PCM_FORMAT_UNKNOWN;
}

int alsa_open_audio (gint aud_format, int rate, int channels)
{
    int format = convert_aud_format (aud_format);
    snd_pcm_hw_params_t * params;
    unsigned int useconds;
    int direction;
    snd_pcm_uframes_t frames, period;
    int hard_buffer, soft_buffer;

    pthread_mutex_lock (& alsa_mutex);
    alsa_soft_init ();

    AUDDBG ("Opening PCM device %s for %s, %d channels, %d Hz.\n",
     alsa_config_pcm, snd_pcm_format_name (format), channels, rate);
    CHECK_NOISY (snd_pcm_open, & alsa_handle, alsa_config_pcm,
     SND_PCM_STREAM_PLAYBACK, 0);

    snd_pcm_hw_params_alloca (& params);
    CHECK_NOISY (snd_pcm_hw_params_any, alsa_handle, params);
    CHECK_NOISY (snd_pcm_hw_params_set_access, alsa_handle, params,
     SND_PCM_ACCESS_RW_INTERLEAVED);
    CHECK_NOISY (snd_pcm_hw_params_set_format, alsa_handle, params, format);
    CHECK_NOISY (snd_pcm_hw_params_set_channels, alsa_handle, params, channels);
    CHECK_NOISY (snd_pcm_hw_params_set_rate, alsa_handle, params, rate, 0);
    useconds = 1000 * aud_cfg->output_buffer_size / 2;
    direction = 0;
    CHECK_NOISY (snd_pcm_hw_params_set_buffer_time_max, alsa_handle, params,
     & useconds, & direction);
    CHECK_NOISY (snd_pcm_hw_params, alsa_handle, params);

    alsa_format = format;
    alsa_channels = channels;
    alsa_rate = rate;

    CHECK_NOISY (snd_pcm_get_params, alsa_handle, & frames, & period);
    hard_buffer = (int64_t) frames * 1000 / rate;
    soft_buffer = MAX (aud_cfg->output_buffer_size / 2,
     aud_cfg->output_buffer_size - hard_buffer);
    AUDDBG ("Hardware buffer %d ms, software buffer %d ms.\n", hard_buffer,
     soft_buffer);

    alsa_buffer_length = snd_pcm_frames_to_bytes (alsa_handle, (int64_t)
     soft_buffer * rate / 1000);
    alsa_buffer = malloc (alsa_buffer_length);
    alsa_buffer_data_start = 0;
    alsa_buffer_data_length = 0;

    alsa_time = 0;
    alsa_prebuffer = 1;
    alsa_paused = 0;
    alsa_paused_time = 0;

    if (! poll_setup ())
        goto FAILED;

    pump_start ();

    pthread_mutex_unlock (& alsa_mutex);
    return 1;

FAILED:
    if (alsa_handle != NULL)
    {
        snd_pcm_close (alsa_handle);
        alsa_handle = NULL;
    }

    pthread_mutex_unlock (& alsa_mutex);
    return 0;
}

void alsa_close_audio (void)
{
    AUDDBG ("Closing audio.\n");
    pthread_mutex_lock (& alsa_mutex);

    pump_stop ();
    CHECK (snd_pcm_drop, alsa_handle);

FAILED:
    free (alsa_buffer);
    poll_cleanup ();
    snd_pcm_close (alsa_handle);
    alsa_handle = NULL;

    pthread_mutex_unlock (& alsa_mutex);
}

int alsa_buffer_free (void)
{
    pthread_mutex_lock (& alsa_mutex);
    int avail = alsa_buffer_length - alsa_buffer_data_length;
    pthread_mutex_unlock (& alsa_mutex);
    return avail;
}

void alsa_write_audio (void * data, int length)
{
    pthread_mutex_lock (& alsa_mutex);

    int start = (alsa_buffer_data_start + alsa_buffer_data_length) %
     alsa_buffer_length;

    assert (length <= alsa_buffer_length - alsa_buffer_data_length);

    if (length > alsa_buffer_length - start)
    {
        int part = alsa_buffer_length - start;

        memcpy ((char *) alsa_buffer + start, data, part);
        memcpy (alsa_buffer, (char *) data + part, length - part);
    }
    else
        memcpy ((char *) alsa_buffer + start, data, length);

    alsa_buffer_data_length += length;
    alsa_time += (int64_t) snd_pcm_bytes_to_frames (alsa_handle, length) *
     1000000 / alsa_rate;

    pthread_mutex_unlock (& alsa_mutex);
}

void alsa_period_wait (void)
{
    pthread_mutex_lock (& alsa_mutex);

    while (alsa_buffer_data_length == alsa_buffer_length)
    {
        if (! alsa_paused)
        {
            if (alsa_prebuffer)
                start_playback ();
            else
                pthread_cond_broadcast (& alsa_cond);
        }

        pthread_cond_wait (& alsa_cond, & alsa_mutex);
    }

    pthread_mutex_unlock (& alsa_mutex);
}

void alsa_drain (void)
{
    int state;

    AUDDBG ("Drain.\n");
    pthread_mutex_lock (& alsa_mutex);

    assert (! alsa_paused);

    if (alsa_prebuffer)
        start_playback ();

    while (alsa_buffer_data_length > 0)
        pthread_cond_wait (& alsa_cond, & alsa_mutex);

    pump_stop ();

    if (alsa_config_drain_workaround)
    {
        int d = get_delay () * 1000 / alsa_rate;
        struct timespec delay = {.tv_sec = d / 1000, .tv_nsec = d % 1000 *
         1000000};

        pthread_mutex_unlock (& alsa_mutex);
        nanosleep (& delay, NULL);
        pthread_mutex_lock (& alsa_mutex);
    }
    else
    {
        while (1)
        {
            CHECK_VAL (state, snd_pcm_state, alsa_handle);

            if (state != SND_PCM_STATE_RUNNING && state !=
             SND_PCM_STATE_DRAINING)
                break;

            pthread_mutex_unlock (& alsa_mutex);
            poll_sleep ();
            pthread_mutex_lock (& alsa_mutex);
        }
    }

    pump_start ();

FAILED:
    pthread_mutex_unlock (& alsa_mutex);
    return;
}

void alsa_set_written_time (int time)
{
    AUDDBG ("Setting time counter to %d.\n", time);
    pthread_mutex_lock (& alsa_mutex);
    alsa_time = 1000 * (int64_t) time;
    pthread_mutex_unlock (& alsa_mutex);
}

int alsa_written_time (void)
{
    int time;

    pthread_mutex_lock (& alsa_mutex);
    time = alsa_time / 1000;
    pthread_mutex_unlock (& alsa_mutex);
    return time;
}

int alsa_output_time (void)
{
    int time = 0;

    pthread_mutex_lock (& alsa_mutex);

    if (alsa_prebuffer || alsa_paused)
        time = alsa_paused_time;
    else
        time = get_output_time ();

    pthread_mutex_unlock (& alsa_mutex);
    return time;
}

void alsa_flush (int time)
{
    AUDDBG ("Seek requested; discarding buffer.\n");
    pthread_mutex_lock (& alsa_mutex);

    pump_stop ();
    CHECK (snd_pcm_drop, alsa_handle);

FAILED:
    alsa_buffer_data_start = 0;
    alsa_buffer_data_length = 0;

    alsa_time = (int64_t) time * 1000;
    alsa_prebuffer = 1;
    alsa_paused_time = time;

    pthread_cond_broadcast (& alsa_cond); /* interrupt period wait */

    pump_start ();

    pthread_mutex_unlock (& alsa_mutex);
}

void alsa_pause (short pause)
{
    AUDDBG ("%sause.\n", pause ? "P" : "Unp");
    pthread_mutex_lock (& alsa_mutex);

    alsa_paused = pause;

    if (! alsa_prebuffer)
    {
        if (pause)
            alsa_paused_time = get_output_time ();

        CHECK (snd_pcm_pause, alsa_handle, pause);
    }

FAILED:
    pthread_cond_broadcast (& alsa_cond);
    pthread_mutex_unlock (& alsa_mutex);
}

void alsa_open_mixer (void)
{
    snd_mixer_selem_id_t * selem_id;

    alsa_mixer = NULL;

    if (alsa_config_mixer_element == NULL)
        goto FAILED;

    AUDDBG ("Opening mixer card %s.\n", alsa_config_mixer);
    CHECK_NOISY (snd_mixer_open, & alsa_mixer, 0);
    CHECK_NOISY (snd_mixer_attach, alsa_mixer, alsa_config_mixer);
    CHECK_NOISY (snd_mixer_selem_register, alsa_mixer, NULL, NULL);
    CHECK_NOISY (snd_mixer_load, alsa_mixer);

    snd_mixer_selem_id_alloca (& selem_id);
    snd_mixer_selem_id_set_name (selem_id, alsa_config_mixer_element);
    alsa_mixer_element = snd_mixer_find_selem (alsa_mixer, selem_id);

    if (alsa_mixer_element == NULL)
    {
        ERROR_NOISY ("snd_mixer_find_selem failed.\n");
        goto FAILED;
    }

    CHECK (snd_mixer_selem_set_playback_volume_range, alsa_mixer_element, 0, 100);
    return;

FAILED:
    if (alsa_mixer != NULL)
    {
        snd_mixer_close (alsa_mixer);
        alsa_mixer = NULL;
    }
}

void alsa_close_mixer (void)
{
    if (alsa_mixer != NULL)
        snd_mixer_close (alsa_mixer);
}

void alsa_get_volume (int * left, int * right)
{
    long left_l = 0, right_l = 0;

    pthread_mutex_lock (& alsa_mutex);
    alsa_soft_init ();

    if (alsa_mixer == NULL)
        goto FAILED;

    CHECK (snd_mixer_handle_events, alsa_mixer);

    if (snd_mixer_selem_is_playback_mono (alsa_mixer_element))
    {
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_MONO, & left_l);
        right_l = left_l;
    }
    else
    {
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_LEFT, & left_l);
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_RIGHT, & right_l);
    }

FAILED:
    pthread_mutex_unlock (& alsa_mutex);

    * left = left_l;
    * right = right_l;
}

void alsa_set_volume (int left, int right)
{
    pthread_mutex_lock (& alsa_mutex);
    alsa_soft_init ();

    if (alsa_mixer == NULL)
        goto FAILED;

    if (snd_mixer_selem_is_playback_mono (alsa_mixer_element))
    {
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_MONO, MAX (left, right));

        if (snd_mixer_selem_has_playback_switch (alsa_mixer_element))
            CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
             SND_MIXER_SCHN_MONO, MAX (left, right) != 0);
    }
    else
    {
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_LEFT, left);
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_element,
         SND_MIXER_SCHN_FRONT_RIGHT, right);

        if (snd_mixer_selem_has_playback_switch (alsa_mixer_element))
        {
            if (snd_mixer_selem_has_playback_switch_joined (alsa_mixer_element))
                CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
                 SND_MIXER_SCHN_FRONT_LEFT, MAX (left, right) != 0);
            else
            {
                CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
                 SND_MIXER_SCHN_FRONT_LEFT, left != 0);
                CHECK (snd_mixer_selem_set_playback_switch, alsa_mixer_element,
                 SND_MIXER_SCHN_FRONT_RIGHT, right != 0);
            }
        }
    }

    CHECK (snd_mixer_handle_events, alsa_mixer);

FAILED:
    pthread_mutex_unlock (& alsa_mutex);
}
