/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
 *  Copyright (C) 2010-2012 Michał Lipski
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

#include <pthread.h>
#include <string.h>

#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>

#include "flacng.h"

static FLAC__StreamDecoder *decoder;
static callback_info *info;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int seek_value;
static bool_t stop_flag = FALSE;

static bool_t flac_init (void)
{
    FLAC__StreamDecoderInitStatus ret;

    /* Callback structure and decoder for main decoding loop */

    if ((info = init_callback_info()) == NULL)
    {
        FLACNG_ERROR("Could not initialize the main callback structure!\n");
        return FALSE;
    }

    if ((decoder = FLAC__stream_decoder_new()) == NULL)
    {
        FLACNG_ERROR("Could not create the main FLAC decoder instance!\n");
        return FALSE;
    }

    if (FLAC__STREAM_DECODER_INIT_STATUS_OK != (ret = FLAC__stream_decoder_init_stream(
        decoder,
        read_callback,
        seek_callback,
        tell_callback,
        length_callback,
        eof_callback,
        write_callback,
        metadata_callback,
        error_callback,
        info)))
    {
        FLACNG_ERROR("Could not initialize the main FLAC decoder: %s(%d)\n",
            FLAC__StreamDecoderInitStatusString[ret], ret);
        return FALSE;
    }

    AUDDBG("Plugin initialized.\n");
    return TRUE;
}

static void flac_cleanup(void)
{
    FLAC__stream_decoder_delete(decoder);
    clean_callback_info(info);
}

bool_t flac_is_our_fd(const char *filename, VFSFile *fd)
{
    AUDDBG("Probe for FLAC.\n");

    if (!fd)
        return FALSE;

    char buf[4];
    if (vfs_fread (buf, 1, sizeof buf, fd) != sizeof buf)
        return FALSE;

    return ! strncmp (buf, "fLaC", sizeof buf);
}

static void squeeze_audio(int32_t* src, void* dst, unsigned count, unsigned res)
{
    int i;
    int32_t* rp = src;
    int8_t*  wp = dst;
    int16_t* wp2 = dst;
    int32_t* wp4 = dst;

    switch (res)
    {
        case 8:
            for (i = 0; i < count; i++, wp++, rp++)
                *wp = *rp & 0xff;
            break;

        case 16:
            for (i = 0; i < count; i++, wp2++, rp++)
                *wp2 = *rp & 0xffff;
            break;

        case 24:
        case 32:
            for (i = 0; i < count; i++, wp4++, rp++)
                *wp4 = *rp;
            break;

        default:
            FLACNG_ERROR("Can not convert to %d bps\n", res);
    }
}

static bool_t flac_play (InputPlayback * playback, const char * filename,
 VFSFile * file, int start_time, int stop_time, bool_t pause)
{
    if (!file)
        return FALSE;

    void * play_buffer = NULL;
    bool_t error = FALSE;

    info->fd = file;

    if (read_metadata(decoder, info) == FALSE)
    {
        FLACNG_ERROR("Could not prepare file for playing!\n");
        error = TRUE;
        goto ERR_NO_CLOSE;
    }

    if ((play_buffer = malloc (BUFFER_SIZE_BYTE)) == NULL)
    {
        FLACNG_ERROR("Could not allocate conversion buffer\n");
        error = TRUE;
        goto ERR_NO_CLOSE;
    }

    if (! playback->output->open_audio (SAMPLE_FMT (info->bits_per_sample),
        info->sample_rate, info->channels))
    {
        error = TRUE;
        goto ERR_NO_CLOSE;
    }

    if (pause)
        playback->output->pause (TRUE);

    seek_value = (start_time > 0) ? start_time : -1;
    stop_flag = FALSE;

    playback->set_params(playback, info->bitrate, info->sample_rate, info->channels);
    playback->set_pb_ready(playback);
    playback->set_gain_from_playlist(playback);

    int64_t samples_remaining = INT64_MAX;
    if (start_time >= 0 && stop_time >= 0)
        samples_remaining = (int64_t) (stop_time - start_time) *
         info->sample_rate / 1000 * info->channels;

    while (samples_remaining && FLAC__stream_decoder_get_state(decoder) !=
     FLAC__STREAM_DECODER_END_OF_STREAM)
    {
        pthread_mutex_lock (& mutex);

        if (stop_flag)
        {
            pthread_mutex_unlock (& mutex);
            break;
        }

        if (seek_value >= 0)
        {
            playback->output->flush (seek_value);
            FLAC__stream_decoder_seek_absolute (decoder, (int64_t)
             seek_value * info->sample_rate / 1000);

            if (stop_time >= 0)
                samples_remaining = (int64_t) (stop_time - seek_value) *
                 info->sample_rate / 1000 * info->channels;

            seek_value = -1;
        }

        pthread_mutex_unlock (& mutex);

        /* Try to decode a single frame of audio */
        if (FLAC__stream_decoder_process_single(decoder) == FALSE)
        {
            FLACNG_ERROR("Error while decoding!\n");
            error = TRUE;
            break;
        }

        if (info->buffer_used >= samples_remaining)
            info->buffer_used = samples_remaining;

        squeeze_audio(info->output_buffer, play_buffer, info->buffer_used, info->bits_per_sample);
        playback->output->write_audio(play_buffer, info->buffer_used * SAMPLE_SIZE(info->bits_per_sample));

        samples_remaining -= info->buffer_used;

        reset_info(info);
    }

    pthread_mutex_lock (& mutex);
    stop_flag = TRUE;
    pthread_mutex_unlock (& mutex);

ERR_NO_CLOSE:
    free (play_buffer);
    reset_info(info);

    if (FLAC__stream_decoder_flush(decoder) == FALSE)
        FLACNG_ERROR("Could not flush decoder state!\n");

    return ! error;
}

static void flac_stop(InputPlayback *playback)
{
    pthread_mutex_lock (& mutex);

    if (!stop_flag)
    {
        stop_flag = TRUE;
        playback->output->abort_write();
    }

    pthread_mutex_unlock (& mutex);
}

static void flac_pause(InputPlayback *playback, bool_t pause)
{
    pthread_mutex_lock (& mutex);

    if (!stop_flag)
        playback->output->pause(pause);

    pthread_mutex_unlock (& mutex);
}

static void flac_seek (InputPlayback * playback, int time)
{
    pthread_mutex_lock (& mutex);

    if (!stop_flag)
    {
        seek_value = time;
        playback->output->abort_write();
    }

    pthread_mutex_unlock (& mutex);
}

static const char flac_about[] =
 N_("Original code by\n"
    "Ralf Ertzinger <ralf@skytale.net>\n\n"
    "http://www.skytale.net/projects/bmp-flac2/");

static const char *flac_fmts[] = { "flac", "fla", NULL };

AUD_INPUT_PLUGIN
(
    .name = N_("FLAC Decoder"),
    .domain = PACKAGE,
    .about_text = flac_about,
    .init = flac_init,
    .cleanup = flac_cleanup,
    .play = flac_play,
    .stop = flac_stop,
    .pause = flac_pause,
    .mseek = flac_seek,
    .probe_for_tuple = flac_probe_for_tuple,
    .is_our_file_from_vfs = flac_is_our_fd,
    .extensions = flac_fmts,
    .update_song_tuple = flac_update_song_tuple,
    .get_song_image = flac_get_image,
    .priority = 1
)
