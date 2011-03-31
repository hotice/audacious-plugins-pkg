/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Main source file

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2009 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "xmms-sid.h"

#include <stdlib.h>
#include <stdarg.h>

#include <libaudcore/audstrings.h>

#include "xs_config.h"
#include "xs_length.h"
#include "xs_stil.h"
#include "xs_filter.h"
#include "xs_fileinfo.h"
#include "xs_interface.h"
#include "xs_glade.h"
#include "xs_player.h"
#include "xs_slsup.h"


/*
 * Global variables
 */
xs_status_t xs_status;
XS_MUTEX(xs_status);

static void xs_get_song_tuple_info(Tuple *pResult, xs_tuneinfo_t *pInfo, gint subTune);

/*
 * Error messages
 */
void xs_verror(const gchar *fmt, va_list ap)
{
    g_logv("AUD-SID", G_LOG_LEVEL_WARNING, fmt, ap);
}

void xs_error(const gchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    xs_verror(fmt, ap);
    va_end(ap);
}

#ifdef DEBUG
void XSDEBUG(const gchar *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    g_logv("AUD-SID", G_LOG_LEVEL_DEBUG, fmt, ap);
    va_end(ap);
}
#endif


/*
 * Initialization functions
 */
void xs_reinit(void)
{
    XSDEBUG("xs_reinit() thread = %p\n", g_thread_self());

    /* Stop playing, if we are */
    xs_stop(NULL);

    XS_MUTEX_LOCK(xs_status);
    XS_MUTEX_LOCK(xs_cfg);

    /* Initialize status and sanitize configuration */
    memset(&xs_status, 0, sizeof(xs_status));

    if (xs_cfg.audioFrequency < 8000)
        xs_cfg.audioFrequency = 8000;

    if (xs_cfg.oversampleFactor < XS_MIN_OVERSAMPLE)
        xs_cfg.oversampleFactor = XS_MIN_OVERSAMPLE;
    else if (xs_cfg.oversampleFactor > XS_MAX_OVERSAMPLE)
        xs_cfg.oversampleFactor = XS_MAX_OVERSAMPLE;

    if (xs_cfg.audioChannels != XS_CHN_MONO)
        xs_cfg.oversampleEnable = FALSE;

    xs_status.audioFrequency = xs_cfg.audioFrequency;
    xs_status.audioBitsPerSample = xs_cfg.audioBitsPerSample;
    xs_status.audioChannels = xs_cfg.audioChannels;
    xs_status.audioFormat = -1;
    xs_status.oversampleEnable = xs_cfg.oversampleEnable;
    xs_status.oversampleFactor = xs_cfg.oversampleFactor;

    /* Try to initialize emulator engine */
    xs_init_emu_engine(&xs_cfg.playerEngine, &xs_status);

    /* Get settings back, in case the chosen emulator backend changed them */
    xs_cfg.audioFrequency = xs_status.audioFrequency;
    xs_cfg.audioBitsPerSample = xs_status.audioBitsPerSample;
    xs_cfg.audioChannels = xs_status.audioChannels;
    xs_cfg.oversampleEnable = xs_status.oversampleEnable;

    XS_MUTEX_UNLOCK(xs_status);
    XS_MUTEX_UNLOCK(xs_cfg);

    /* Initialize song-length database */
    xs_songlen_close();
    if (xs_cfg.songlenDBEnable && (xs_songlen_init() != 0)) {
        xs_error("Error initializing song-length database!\n");
    }

    /* Initialize STIL database */
    xs_stil_close();
    if (xs_cfg.stilDBEnable && (xs_stil_init() != 0)) {
        xs_error("Error initializing STIL database!\n");
    }

}


/*
 * Initialize XMMS-SID
 */
void xs_init(void)
{
    XSDEBUG("xs_init()\n");

    /* Initialize and get configuration */
    xs_init_configuration();
    xs_read_configuration();

    /* Initialize subsystems */
    xs_reinit();

    XSDEBUG("OK\n");
}


/*
 * Shut down XMMS-SID
 */
void xs_close(void)
{
    XSDEBUG("xs_close(): shutting down...\n");

    xs_tuneinfo_free(xs_status.tuneInfo);
    xs_status.tuneInfo = NULL;
    xs_status.sidPlayer->plrDeleteSID(&xs_status);
    xs_status.sidPlayer->plrClose(&xs_status);

    xs_songlen_close();
    xs_stil_close();

    XSDEBUG("shutdown finished.\n");
}


/*
 * Start playing the given file
 */
gboolean xs_play_file(InputPlayback *pb, const gchar *filename, VFSFile *file, gint start_time, gint stop_time, gboolean pause)
{
    xs_tuneinfo_t *tmpTune;
    gboolean audioOpen = FALSE;
    gint audioBufSize, bufRemaining, tmpLength, subTune = -1;
    gchar *tmpFilename, *audioBuffer = NULL, *oversampleBuffer = NULL;
    Tuple *tmpTuple;

    assert(pb);
    assert(xs_status.sidPlayer != NULL);

    XSDEBUG("play '%s'\n", pb->filename);

    tmpFilename = filename_split_subtune(pb->filename, &subTune);
    if (tmpFilename == NULL) return TRUE;

    /* Get tune information */
    XS_MUTEX_LOCK(xs_status);
    if ((xs_status.tuneInfo = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename)) == NULL) {
        XS_MUTEX_UNLOCK(xs_status);
        g_free(tmpFilename);
        return TRUE;
    }

    /* Initialize the tune */
    if (!xs_status.sidPlayer->plrLoadSID(&xs_status, tmpFilename)) {
        XS_MUTEX_UNLOCK(xs_status);
        g_free(tmpFilename);
        xs_tuneinfo_free(xs_status.tuneInfo);
        xs_status.tuneInfo = NULL;
        return TRUE;
    }

    g_free(tmpFilename);
    tmpFilename = NULL;

    XSDEBUG("load ok\n");

    /* Set general status information */
    pb->playing = TRUE;
    pb->error = FALSE;
    pb->eof = FALSE;
    tmpTune = xs_status.tuneInfo;

    if (subTune < 1 || subTune > xs_status.tuneInfo->nsubTunes)
        xs_status.currSong = xs_status.tuneInfo->startTune;
    else
        xs_status.currSong = subTune;

    XSDEBUG("subtune #%i selected (#%d wanted), initializing...\n", xs_status.currSong, subTune);

    gint channels = (xs_status.audioChannels == XS_CHN_AUTOPAN) ? 2 :
     xs_status.audioChannels;

    /* Allocate audio buffer */
    audioBufSize = xs_status.audioFrequency * channels *
     xs_status.audioBitsPerSample / (8 * 4);
    if (audioBufSize < 512) audioBufSize = 512;

    audioBuffer = (gchar *) g_malloc(audioBufSize);
    if (audioBuffer == NULL) {
        xs_error("Couldn't allocate memory for audio data buffer!\n");
        XS_MUTEX_UNLOCK(xs_status);
        goto xs_err_exit;
    }

    if (xs_status.oversampleEnable) {
        oversampleBuffer = (gchar *) g_malloc(audioBufSize * xs_status.oversampleFactor);
        if (oversampleBuffer == NULL) {
            xs_error("Couldn't allocate memory for audio oversampling buffer!\n");
            XS_MUTEX_UNLOCK(xs_status);
            goto xs_err_exit;
        }
    }


    /* Check minimum playtime */
    tmpLength = tmpTune->subTunes[xs_status.currSong - 1].tuneLength;
    if (xs_cfg.playMinTimeEnable && (tmpLength >= 0)) {
        if (tmpLength < xs_cfg.playMinTime)
            tmpLength = xs_cfg.playMinTime;
    }

    /* Initialize song */
    if (!xs_status.sidPlayer->plrInitSong(&xs_status)) {
        xs_error("Couldn't initialize SID-tune '%s' (sub-tune #%i)!\n",
            tmpTune->sidFilename, xs_status.currSong);
        XS_MUTEX_UNLOCK(xs_status);
        goto xs_err_exit;
    }

    /* Open the audio output */
    XSDEBUG("open audio output (%d, %d, %d)\n",
        xs_status.audioFormat, xs_status.audioFrequency, channels);

    if (!pb->output->open_audio(xs_status.audioFormat, xs_status.audioFrequency,
     channels))
    {
        xs_error("Couldn't open audio output (fmt=%x, freq=%i, nchan=%i)!\n",
            xs_status.audioFormat,
            xs_status.audioFrequency,
            channels);

        pb->error = TRUE;
        XS_MUTEX_UNLOCK(xs_status);
        goto xs_err_exit;
    }

    audioOpen = TRUE;

    /* Set song information for current subtune */
    XSDEBUG("foobar #1\n");
    xs_status.sidPlayer->plrUpdateSIDInfo(&xs_status);
    tmpTuple = tuple_new_from_filename(tmpTune->sidFilename);
    xs_get_song_tuple_info(tmpTuple, tmpTune, xs_status.currSong);
    XS_MUTEX_UNLOCK(xs_status);
    pb->set_tuple(pb, tmpTuple);
    pb->set_params(pb, NULL, 0, -1, xs_status.audioFrequency, channels);
    pb->set_pb_ready(pb);

    XS_MUTEX_UNLOCK(xs_status);
    XSDEBUG("playing\n");
    while (pb->playing) {
        /* Render audio data */
        if (xs_status.oversampleEnable) {
            /* Perform oversampled rendering */
            bufRemaining = xs_status.sidPlayer->plrFillBuffer(
                &xs_status,
                oversampleBuffer,
                (audioBufSize * xs_status.oversampleFactor));

            bufRemaining /= xs_status.oversampleFactor;

            /* Execute rate-conversion with filtering */
            if (xs_filter_rateconv(audioBuffer, oversampleBuffer,
                xs_status.audioFormat, xs_status.oversampleFactor, bufRemaining) < 0) {
                xs_error("Oversampling rate-conversion pass failed.\n");
                pb->error = TRUE;
                goto xs_err_exit;
            }
        } else {
            bufRemaining = xs_status.sidPlayer->plrFillBuffer(
                &xs_status, audioBuffer, audioBufSize);
        }

        pb->output->write_audio (audioBuffer, bufRemaining);

        /* Check if we have played enough */
        if (xs_cfg.playMaxTimeEnable) {
            if (xs_cfg.playMaxTimeUnknown) {
                if (tmpLength < 0 &&
                    pb->output->written_time() >= xs_cfg.playMaxTime * 1000)
                    pb->playing = FALSE;
            } else {
                if (pb->output->written_time() >= xs_cfg.playMaxTime * 1000)
                    pb->playing = FALSE;
            }
        }

        if (tmpLength >= 0) {
            if (pb->output->written_time() >= tmpLength * 1000)
                pb->playing = FALSE;
        }
    }

xs_err_exit:
    XSDEBUG("out of playing loop\n");

    /* Close audio output plugin */
    if (audioOpen) {
        XSDEBUG("close audio #2\n");
        pb->output->close_audio();
        XSDEBUG("closed\n");
    }

    g_free(audioBuffer);
    g_free(oversampleBuffer);

    /* Set playing status to false (stopped), thus when
     * XMMS next calls xs_get_time(), it can return appropriate
     * value "not playing" status and XMMS knows to move to
     * next entry in the playlist .. or whatever it wishes.
     */
    XS_MUTEX_LOCK(xs_status);
    pb->playing = FALSE;
    pb->eof = TRUE;

    /* Free tune information */
    xs_status.sidPlayer->plrDeleteSID(&xs_status);
    xs_tuneinfo_free(xs_status.tuneInfo);
    xs_status.tuneInfo = NULL;
    XS_MUTEX_UNLOCK(xs_status);

    /* Exit the playing thread */
    XSDEBUG("exiting thread, bye.\n");

    return ! pb->error;
}


/*
 * Stop playing
 * Here we set the playing status to stop and wait for playing
 * thread to shut down. In any "correctly" done plugin, this is
 * also the function where you close the output-plugin, but since
 * XMMS-SID has special behaviour (audio opened/closed in the
 * playing thread), we don't do that here.
 *
 * Finally tune and other memory allocations are free'd.
 */
void xs_stop(InputPlayback *pb)
{
    XSDEBUG("stop requested\n");

    /* Lock xs_status and stop playing thread */
    XS_MUTEX_LOCK(xs_status);
    if (pb != NULL && pb->playing) {
        XSDEBUG("stopping...\n");
        pb->playing = FALSE;
        pb->output->abort_write ();
        XS_MUTEX_UNLOCK(xs_status);
    } else {
        XS_MUTEX_UNLOCK(xs_status);
    }

    XSDEBUG("ok\n");
}


/*
 * Pause/unpause the playing
 */
void xs_pause(InputPlayback *pb, short pauseState)
{
    XS_MUTEX_LOCK(xs_status);
    pb->output->pause(pauseState);
    XS_MUTEX_UNLOCK(xs_status);
}


/*
 * Return song information Tuple
 */
static void xs_get_song_tuple_info(Tuple *tuple, xs_tuneinfo_t *info, gint subTune)
{
    gchar *tmpStr, tmpStr2[64];

    tuple_associate_string_rel(tuple, FIELD_TITLE, NULL, str_to_utf8(info->sidName));
    tuple_associate_string_rel(tuple, FIELD_ARTIST, NULL, str_to_utf8(info->sidComposer));
    tuple_associate_string_rel(tuple, FIELD_COPYRIGHT, NULL, str_to_utf8(info->sidCopyright));
    tuple_associate_string(tuple, -1, "sid-format", info->sidFormat);
    tuple_associate_string(tuple, FIELD_CODEC, NULL, "Commodore 64 SID PlaySID/RSID");

    switch (info->sidModel) {
        case XS_SIDMODEL_6581: tmpStr = "6581"; break;
        case XS_SIDMODEL_8580: tmpStr = "8580"; break;
        case XS_SIDMODEL_ANY: tmpStr = "ANY"; break;
        default: tmpStr = "?"; break;
    }
    tuple_associate_string(tuple, -1, "sid-model", tmpStr);

    /* Get sub-tune information, if available */
    if (subTune < 0 || info->startTune > info->nsubTunes)
        subTune = info->startTune;

    if (subTune > 0 && subTune <= info->nsubTunes) {
        gint tmpInt = info->subTunes[subTune - 1].tuneLength;
        tuple_associate_int(tuple, FIELD_LENGTH, NULL, (tmpInt < 0) ? -1 : tmpInt * 1000);

        tmpInt = info->subTunes[subTune - 1].tuneSpeed;
        if (tmpInt > 0) {
            switch (tmpInt) {
            case XS_CLOCK_PAL: tmpStr = "PAL"; break;
            case XS_CLOCK_NTSC: tmpStr = "NTSC"; break;
            case XS_CLOCK_ANY: tmpStr = "ANY"; break;
            case XS_CLOCK_VBI: tmpStr = "VBI"; break;
            case XS_CLOCK_CIA: tmpStr = "CIA"; break;
            default:
                g_snprintf(tmpStr2, sizeof(tmpStr2), "%dHz", tmpInt);
                tmpStr = tmpStr2;
                break;
            }
        } else
            tmpStr = "?";

        tuple_associate_string(tuple, -1, "sid-speed", tmpStr);
    } else
        subTune = 1;

    tuple_associate_int(tuple, FIELD_SUBSONG_NUM, NULL, info->nsubTunes);
    tuple_associate_int(tuple, FIELD_SUBSONG_ID, NULL, subTune);
    tuple_associate_int(tuple, FIELD_TRACK_NUMBER, NULL, subTune);

    if (xs_cfg.titleOverride)
        tuple_associate_string(tuple, FIELD_FORMATTER, NULL, xs_cfg.titleFormat);
}


static void xs_fill_subtunes(Tuple *tuple, xs_tuneinfo_t *info)
{
    gint count, found;

    tuple->subtunes = g_new(gint, info->nsubTunes);

    for (found = count = 0; count < info->nsubTunes; count++) {
        if (count + 1 == info->startTune || !xs_cfg.subAutoMinOnly ||
            info->subTunes[count].tuneLength >= xs_cfg.subAutoMinTime)
            tuple->subtunes[found++] = count + 1;
    }

    tuple->nsubtunes = found;
}


Tuple * xs_get_song_tuple(const gchar *filename)
{
    Tuple *tuple;
    gchar *tmpFilename;
    xs_tuneinfo_t *info;
    gint tune = -1;

    /* Get information from URL */
    tmpFilename = filename_split_subtune(filename, &tune);
    if (tmpFilename == NULL) return NULL;

    tuple = tuple_new_from_filename(tmpFilename);
    if (tuple == NULL) {
        g_free(tmpFilename);
        return NULL;
    }

    if (xs_status.sidPlayer == NULL)
        return tuple;

    /* Get tune information from emulation engine */
    XS_MUTEX_LOCK(xs_status);
    info = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename);
    XS_MUTEX_UNLOCK(xs_status);
    g_free(tmpFilename);

    if (info == NULL)
        return tuple;

    xs_get_song_tuple_info(tuple, info, tune);

    if (xs_cfg.subAutoEnable && info->nsubTunes > 1 && tune < 0)
        xs_fill_subtunes(tuple, info);

    xs_tuneinfo_free(info);

    return tuple;
}


Tuple * xs_probe_for_tuple(const gchar *filename, xs_file_t *fd)
{
    Tuple *tuple;
    gchar *tmpFilename;
    xs_tuneinfo_t *info;
    gint tune = -1;

    if (xs_status.sidPlayer == NULL || filename == NULL)
        return NULL;

    XS_MUTEX_LOCK(xs_status);
    if (!xs_status.sidPlayer->plrProbe(fd)) {
        XS_MUTEX_UNLOCK(xs_status);
        return NULL;
    }
    XS_MUTEX_UNLOCK(xs_status);

    /* Get information from URL */
    tmpFilename = filename_split_subtune(filename, &tune);
    if (tmpFilename == NULL) return NULL;

    tuple = tuple_new_from_filename(tmpFilename);
    if (tuple == NULL) {
        g_free(tmpFilename);
        return NULL;
    }

    /* Get tune information from emulation engine */
    XS_MUTEX_LOCK(xs_status);
    info = xs_status.sidPlayer->plrGetSIDInfo(tmpFilename);
    XS_MUTEX_UNLOCK(xs_status);
    g_free(tmpFilename);

    if (info == NULL)
        return tuple;

    xs_get_song_tuple_info(tuple, info, tune);

    if (xs_cfg.subAutoEnable && info->nsubTunes > 1 && tune < 0)
        xs_fill_subtunes(tuple, info);

    xs_tuneinfo_free(info);

    return tuple;
}
