/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2010 Audacious development team
 *
 *  Based on the xmms_sndfile input plugin:
 *  Copyright (C) 2000, 2002 Erik de Castro Lopo
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

/*
 * Rewritten 17-Feb-2007 (nenolod):
 *   - now uses conditional variables to ensure that sndfile mutex is
 *     entirely protected.
 *   - pausing works now
 *   - fixed some potential race conditions when dealing with NFS.
 *   - TITLE_LEN removed
 *
 * Re-cleaned up 09-Aug-2009 (ccr):
 *   - removed threading madness.
 *   - improved locking.
 *   - misc. cleanups.
 *
 * Play loop rewritten 14-Nov-2009 (jlindgren):
 *   - decode in floating point
 *   - drain audio buffer before closing
 *   - handle seeking/stopping while paused
 */

#include "config.h"

#include <glib.h>
#include <math.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include <sndfile.h>

static GMutex * control_mutex;
static GCond * control_cond;
static gboolean pause_flag;
static glong seek_value;


/* Virtual file access wrappers for libsndfile
 */
static sf_count_t
sf_get_filelen (void *user_data)
{
    return vfs_fsize (user_data);
}

static sf_count_t
sf_vseek (sf_count_t offset, int whence, void *user_data)
{
    return vfs_fseek(user_data, offset, whence);
}

static sf_count_t
sf_vread (void *ptr, sf_count_t count, void *user_data)
{
    return vfs_fread(ptr, 1, count, user_data);
}

static sf_count_t
sf_vwrite (const void *ptr, sf_count_t count, void *user_data)
{
    return vfs_fwrite(ptr, 1, count, user_data);
}

static sf_count_t
sf_tell (void *user_data)
{
    return vfs_ftell(user_data);
}

static SF_VIRTUAL_IO sf_virtual_io =
{
    sf_get_filelen,
    sf_vseek,
    sf_vread,
    sf_vwrite,
    sf_tell
};


static SNDFILE *
open_sndfile_from_uri(const gchar *filename, VFSFile **vfsfile, SF_INFO *sfinfo)
{
    SNDFILE *snd_file = NULL;
    *vfsfile = vfs_fopen(filename, "rb");

    if (*vfsfile == NULL)
        return NULL;

    snd_file = sf_open_virtual (&sf_virtual_io, SFM_READ, sfinfo, *vfsfile);
    if (snd_file == NULL)
        vfs_fclose(*vfsfile);

    return snd_file;
}

static void
close_sndfile(SNDFILE *snd_file, VFSFile *vfsfile)
{
    if (snd_file != NULL)
        sf_close(snd_file);
    if (vfsfile != NULL)
        vfs_fclose(vfsfile);
}


/* Plugin initialization
 */
static void plugin_init (void)
{
    control_mutex = g_mutex_new ();
    control_cond = g_cond_new ();
}

static void plugin_cleanup (void)
{
    g_cond_free (control_cond);
    g_mutex_free (control_mutex);
}

static Tuple * get_song_tuple (const gchar * filename)
{
    VFSFile *vfsfile = NULL;
    SNDFILE *sndfile;
    SF_INFO sfinfo;
    gboolean lossy = FALSE;
    gchar *codec, *format, *subformat;
    Tuple * ti;

    sndfile = open_sndfile_from_uri(filename, &vfsfile, &sfinfo);

    if (sndfile == NULL)
        return NULL;

    ti = tuple_new_from_filename (filename);

    if (sf_get_string(sndfile, SF_STR_TITLE) != NULL)
        tuple_associate_string(ti, FIELD_TITLE, NULL, sf_get_string(sndfile, SF_STR_TITLE));

    tuple_associate_string(ti, FIELD_ARTIST, NULL, sf_get_string(sndfile, SF_STR_ARTIST));
    tuple_associate_string(ti, FIELD_COMMENT, NULL, sf_get_string(sndfile, SF_STR_COMMENT));
    tuple_associate_string(ti, FIELD_DATE, NULL, sf_get_string(sndfile, SF_STR_DATE));
    tuple_associate_string(ti, -1, "software", sf_get_string(sndfile, SF_STR_SOFTWARE));

    close_sndfile (sndfile, vfsfile);

    if (sfinfo.samplerate > 0)
    {
        tuple_associate_int(ti, FIELD_LENGTH, NULL,
        (gint) ceil (1000.0 * sfinfo.frames / sfinfo.samplerate));
    }

    switch (sfinfo.format & SF_FORMAT_TYPEMASK)
    {
        case SF_FORMAT_WAV:
        case SF_FORMAT_WAVEX:
            format = "Microsoft WAV";
            break;
        case SF_FORMAT_AIFF:
            format = "Apple/SGI AIFF";
            break;
        case SF_FORMAT_AU:
            format = "Sun/NeXT AU";
            break;
        case SF_FORMAT_RAW:
            format = "Raw PCM data";
            break;
        case SF_FORMAT_PAF:
            format = "Ensoniq PARIS";
            break;
        case SF_FORMAT_SVX:
            format = "Amiga IFF / SVX8 / SV16";
            break;
        case SF_FORMAT_NIST:
            format = "Sphere NIST";
            break;
        case SF_FORMAT_VOC:
            format = "Creative VOC";
            break;
        case SF_FORMAT_IRCAM:
            format = "Berkeley/IRCAM/CARL";
            break;
        case SF_FORMAT_W64:
            format = "Sonic Foundry's 64 bit RIFF/WAV";
            break;
        case SF_FORMAT_MAT4:
            format = "Matlab (tm) V4.2 / GNU Octave 2.0";
            break;
        case SF_FORMAT_MAT5:
            format = "Matlab (tm) V5.0 / GNU Octave 2.1";
            break;
        case SF_FORMAT_PVF:
            format = "Portable Voice Format";
            break;
        case SF_FORMAT_XI:
            format = "Fasttracker 2 Extended Instrument";
            break;
        case SF_FORMAT_HTK:
            format = "HMM Tool Kit";
            break;
        case SF_FORMAT_SDS:
            format = "Midi Sample Dump Standard";
            break;
        case SF_FORMAT_AVR:
            format = "Audio Visual Research";
            break;
        case SF_FORMAT_SD2:
            format = "Sound Designer 2";
            break;
        case SF_FORMAT_FLAC:
            format = "Free Lossless Audio Codec";
            break;
        case SF_FORMAT_CAF:
            format = "Core Audio File";
            break;
        default:
            format = "Unknown sndfile";
    }

    switch (sfinfo.format & SF_FORMAT_SUBMASK)
    {
        case SF_FORMAT_PCM_S8:
            subformat = "signed 8 bit";
            break;
        case SF_FORMAT_PCM_16:
            subformat = "signed 16 bit";
            break;
        case SF_FORMAT_PCM_24:
            subformat = "signed 24 bit";
            break;
        case SF_FORMAT_PCM_32:
            subformat = "signed 32 bit";
            break;
        case SF_FORMAT_PCM_U8:
            subformat = "unsigned 8 bit";
            break;
        case SF_FORMAT_FLOAT:
            subformat = "32 bit float";
            break;
        case SF_FORMAT_DOUBLE:
            subformat = "64 bit float";
            break;
        case SF_FORMAT_ULAW:
            subformat = "U-Law";
            lossy = TRUE;
            break;
        case SF_FORMAT_ALAW:
            subformat = "A-Law";
            lossy = TRUE;
            break;
        case SF_FORMAT_IMA_ADPCM:
            subformat = "IMA ADPCM";
            lossy = TRUE;
            break;
        case SF_FORMAT_MS_ADPCM:
            subformat = "MS ADPCM";
            lossy = TRUE;
            break;
        case SF_FORMAT_GSM610:
            subformat = "GSM 6.10";
            lossy = TRUE;
            break;
        case SF_FORMAT_VOX_ADPCM:
            subformat = "Oki Dialogic ADPCM";
            lossy = TRUE;
            break;
        case SF_FORMAT_G721_32:
            subformat = "32kbs G721 ADPCM";
            lossy = TRUE;
            break;
        case SF_FORMAT_G723_24:
            subformat = "24kbs G723 ADPCM";
            lossy = TRUE;
            break;
        case SF_FORMAT_G723_40:
            subformat = "40kbs G723 ADPCM";
            lossy = TRUE;
            break;
        case SF_FORMAT_DWVW_12:
            subformat = "12 bit Delta Width Variable Word";
            lossy = TRUE;
            break;
        case SF_FORMAT_DWVW_16:
            subformat = "16 bit Delta Width Variable Word";
            lossy = TRUE;
            break;
        case SF_FORMAT_DWVW_24:
            subformat = "24 bit Delta Width Variable Word";
            lossy = TRUE;
            break;
        case SF_FORMAT_DWVW_N:
            subformat = "N bit Delta Width Variable Word";
            lossy = TRUE;
            break;
        case SF_FORMAT_DPCM_8:
            subformat = "8 bit differential PCM";
            break;
        case SF_FORMAT_DPCM_16:
            subformat = "16 bit differential PCM";
            break;
        default:
            subformat = NULL;
    }

    if (subformat != NULL)
        codec = g_strdup_printf("%s (%s)", format, subformat);
    else
        codec = g_strdup_printf("%s", format);

    tuple_associate_string(ti, FIELD_CODEC, NULL, codec);
    g_free(codec);

    tuple_associate_string(ti, FIELD_QUALITY, NULL, lossy ? "lossy" : "lossless");

    return ti;
}

static gint
is_our_file (const gchar *filename)
{
    VFSFile *vfsfile = NULL;
    SNDFILE *tmp_sndfile;
    SF_INFO tmp_sfinfo;

    /* Have to open the file to see if libsndfile can handle it. */
    tmp_sndfile = open_sndfile_from_uri(filename, &vfsfile, &tmp_sfinfo);

    if (!tmp_sndfile)
        return FALSE;

    /* It can so close file and return TRUE. */
    close_sndfile (tmp_sndfile, vfsfile);
    tmp_sndfile = NULL;

    return TRUE;
}

static gboolean play_start (InputPlayback * playback, const gchar * filename,
 VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
    if (file == NULL)
        return FALSE;

    SF_INFO sfinfo;
    SNDFILE * sndfile = sf_open_virtual (& sf_virtual_io, SFM_READ, & sfinfo,
     file);
    if (sndfile == NULL)
        return FALSE;

    if (! playback->output->open_audio (FMT_FLOAT, sfinfo.samplerate,
     sfinfo.channels))
    {
        sf_close (sndfile);
        return FALSE;
    }

    playback->set_params (playback, NULL, 0, 8 * sizeof (gfloat) *
     sfinfo.channels * sfinfo.samplerate, sfinfo.samplerate, sfinfo.channels);

    playback->playing = TRUE;
    pause_flag = pause;
    seek_value = (start_time > 0) ? start_time : -1;
    playback->set_pb_ready(playback);

    gint size = sfinfo.channels * (sfinfo.samplerate / 50);
    gfloat * buffer = g_malloc (sizeof (gfloat) * size);
    gboolean paused = FALSE, stopped = FALSE;

    while (stop_time < 0 || playback->output->written_time () < stop_time)
    {
        g_mutex_lock (control_mutex);

        if (! playback->playing)
        {
            stopped = TRUE;
            g_mutex_unlock (control_mutex);
            break;
        }

        if (seek_value != -1)
        {
            sf_seek (sndfile, (gint64) seek_value * sfinfo.samplerate / 1000,
             SEEK_SET);
            playback->output->flush (seek_value);
            seek_value = -1;
            g_cond_signal (control_cond);
        }

        if (pause_flag)
        {
            if (! paused)
            {
                playback->output->pause (TRUE);
                paused = TRUE;
                g_cond_signal (control_cond);
            }

            g_cond_wait (control_cond, control_mutex);
            g_mutex_unlock (control_mutex);
            continue;
        }

        if (paused)
        {
            playback->output->pause (FALSE);
            paused = FALSE;
            g_cond_signal (control_cond);
        }

        g_mutex_unlock (control_mutex);

        gint samples = sf_read_float (sndfile, buffer, size);

        if (! samples)
            break;

        playback->output->write_audio (buffer, sizeof (gfloat) * samples);
    }

    sf_close (sndfile);
    g_free (buffer);

    if (! stopped)
    {
        while (playback->output->buffer_playing ())
            g_usleep (20000);
    }

    playback->output->close_audio();

    g_mutex_lock (control_mutex);
    playback->playing = FALSE;
    g_cond_signal (control_cond); /* wake up any waiting request */
    g_mutex_unlock (control_mutex);

    return TRUE;
}

static void play_pause (InputPlayback * playback, gshort pause)
{
    g_mutex_lock (control_mutex);

    if (! playback->playing)
    {
        g_mutex_unlock (control_mutex);
        return;
    }

    pause_flag = pause;
    g_cond_signal (control_cond);
    g_cond_wait (control_cond, control_mutex);
    g_mutex_unlock (control_mutex);
}

static void play_stop (InputPlayback * playback)
{
    g_mutex_lock (control_mutex);

    if (! playback->playing)
    {
        g_mutex_unlock (control_mutex);
        return;
    }

    playback->playing = FALSE;
    g_cond_signal (control_cond);
    g_cond_wait (control_cond, control_mutex);
    g_mutex_unlock (control_mutex);
}

static void file_mseek (InputPlayback * playback, gulong time)
{
    g_mutex_lock (control_mutex);

    if (! playback->playing)
    {
        g_mutex_unlock (control_mutex);
        return;
    }

    seek_value = time;
    g_cond_signal (control_cond);
    g_cond_wait (control_cond, control_mutex);
    g_mutex_unlock (control_mutex);
}

static gint
is_our_file_from_vfs(const gchar *filename, VFSFile *fin)
{
    SNDFILE *tmp_sndfile;
    SF_INFO tmp_sfinfo;

    /* Have to open the file to see if libsndfile can handle it. */
    tmp_sndfile = sf_open_virtual (&sf_virtual_io, SFM_READ, &tmp_sfinfo, fin);

    if (!tmp_sndfile)
        return FALSE;

    /* It can so close file and return TRUE. */
    sf_close (tmp_sndfile);
    tmp_sndfile = NULL;

    return TRUE;
}


static void plugin_about (void)
{
    static GtkWidget * aboutbox = NULL;

    audgui_simple_message (& aboutbox, GTK_MESSAGE_INFO,
     _("About sndfile plugin"),
     _("Adapted for Audacious usage by Tony Vroon <chainsaw@gentoo.org>\n"
     "from the xmms_sndfile plugin which is:\n"
     "Copyright (C) 2000, 2002 Erik de Castro Lopo\n\n"
     "This program is free software ; you can redistribute it and/or modify \n"
     "it under the terms of the GNU General Public License as published by \n"
     "the Free Software Foundation ; either version 2 of the License, or \n"
     "(at your option) any later version. \n \n"
     "This program is distributed in the hope that it will be useful, \n"
     "but WITHOUT ANY WARRANTY ; without even the implied warranty of \n"
     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  \n"
     "See the GNU General Public License for more details. \n\n"
     "You should have received a copy of the GNU General Public \n"
     "License along with this program ; if not, write to \n"
     "the Free Software Foundation, Inc., \n"
     "51 Franklin Street, Fifth Floor, \n"
     "Boston, MA  02110-1301  USA"));
}


static gchar *sndfile_fmts[] = { "aiff", "au", "raw", "wav", NULL };

static InputPlugin sndfile_ip = {
    .description = "sndfile plugin",
    .init = plugin_init,
    .about = plugin_about,
    .is_our_file = is_our_file,
    .play = play_start,
    .stop = play_stop,
    .pause = play_pause,
    .cleanup = plugin_cleanup,
    .get_song_tuple = get_song_tuple,
    .is_our_file_from_vfs = is_our_file_from_vfs,
    .vfs_extensions = sndfile_fmts,
    .mseek = file_mseek,
};

static InputPlugin *sndfile_iplist[] = { &sndfile_ip, NULL };

SIMPLE_INPUT_PLUGIN(sndfile, sndfile_iplist)
