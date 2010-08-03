/*
 * Copyright (C) Tony Arcieri <bascule@inferno.tusculum.edu>
 * Copyright (C) 2001-2002  Haavard Kvaalen <havardk@xmms.org>
 * Copyright (C) 2007 William Pitcock <nenolod@sacredspiral.co.uk>
 * Copyright (C) 2008 Cristi Măgherușan <majeru@gentoo.ro>
 * Copyright (C) 2008 Eugene Zagidullin <e.asphyx@gmail.com>
 * Copyright (C) 2009 Audacious Developers
 *
 * ReplayGain processing Copyright (C) 2002 Gian-Carlo Pascutto <gcp@sjeng.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "config.h"
/*#define AUD_DEBUG
#define DEBUG*/

#include <glib.h>

#include <stdlib.h>
#include <math.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "vorbis.h"

static size_t ovcb_read (void * buffer, size_t size, size_t count, void * file)
{
    return vfs_fread (buffer, size, count, file);
}

static int ovcb_seek (void * file, ogg_int64_t offset, int whence)
{
    return vfs_fseek (file, offset, whence);
}

static int ovcb_close (void * file)
{
    return 0;
}

static long ovcb_tell (void * file)
{
    return vfs_ftell (file);
}

ov_callbacks vorbis_callbacks = {
    ovcb_read,
    ovcb_seek,
    ovcb_close,
    ovcb_tell
};

ov_callbacks vorbis_callbacks_stream = {
    ovcb_read,
    NULL,
    ovcb_close,
    NULL
};

static volatile gint seek_value = -1;
static GMutex * seek_mutex = NULL;
static GCond * seek_cond = NULL;

gchar **vorbis_tag_encoding_list = NULL;


static gint
vorbis_check_fd(const gchar *filename, VFSFile *stream)
{
    OggVorbis_File vfile;
    gint result;

    /*
     * The open function performs full stream detection and machine
     * initialization.  If it returns zero, the stream *is* Vorbis and
     * we're fully ready to decode.
     */

    memset(&vfile, 0, sizeof(vfile));

    result = ov_test_callbacks (stream, & vfile, NULL, 0, vfs_is_streaming
     (stream) ? vorbis_callbacks_stream : vorbis_callbacks);

    switch (result) {
    case OV_EREAD:
#ifdef DEBUG
        g_message("** vorbis.c: Media read error: %s", filename);
#endif
        return FALSE;
        break;
    case OV_ENOTVORBIS:
#ifdef DEBUG
        g_message("** vorbis.c: Not Vorbis data: %s", filename);
#endif
        return FALSE;
        break;
    case OV_EVERSION:
#ifdef DEBUG
        g_message("** vorbis.c: Version mismatch: %s", filename);
#endif
        return FALSE;
        break;
    case OV_EBADHEADER:
#ifdef DEBUG
        g_message("** vorbis.c: Invalid Vorbis bistream header: %s",
                  filename);
#endif
        return FALSE;
        break;
    case OV_EFAULT:
#ifdef DEBUG
        g_message("** vorbis.c: Internal logic fault while reading %s",
                  filename);
#endif
        return FALSE;
        break;
    case 0:
        break;
    default:
        break;
    }

    ov_clear(&vfile);
    return TRUE;
}

static void
set_tuple_str(Tuple *tuple, const gint nfield, const gchar *field,
    vorbis_comment *comment, gchar *key)
{
    gchar *str = vorbis_comment_query(comment, key, 0);
    if (str != NULL) {
        gchar *tmp = str_to_utf8(str);
        tuple_associate_string(tuple, nfield, field, tmp);
        g_free(tmp);
    }
}

static Tuple *
get_tuple_for_vorbisfile(OggVorbis_File * vorbisfile, const gchar *filename)
{
    Tuple *tuple;
    gint length;
    vorbis_comment *comment = NULL;

    tuple = tuple_new_from_filename(filename);

    length = vfs_is_streaming (vorbisfile->datasource) ? -1 : ov_time_total
     (vorbisfile, -1) * 1000;

    /* associate with tuple */
    tuple_associate_int(tuple, FIELD_LENGTH, NULL, length);
    /* maybe, it would be better to display nominal bitrate (like in main win), not average? --eugene */
    tuple_associate_int(tuple, FIELD_BITRATE, NULL, ov_bitrate(vorbisfile, -1) / 1000);

    if ((comment = ov_comment(vorbisfile, -1)) != NULL) {
        gchar *tmps;
        set_tuple_str(tuple, FIELD_TITLE, NULL, comment, "title");
        set_tuple_str(tuple, FIELD_ARTIST, NULL, comment, "artist");
        set_tuple_str(tuple, FIELD_ALBUM, NULL, comment, "album");
        set_tuple_str(tuple, FIELD_GENRE, NULL, comment, "genre");
        set_tuple_str(tuple, FIELD_COMMENT, NULL, comment, "comment");

        if ((tmps = vorbis_comment_query(comment, "tracknumber", 0)) != NULL)
            tuple_associate_int(tuple, FIELD_TRACK_NUMBER, NULL, atoi(tmps));

        if ((tmps = vorbis_comment_query (comment, "date", 0)) != NULL)
            tuple_associate_int (tuple, FIELD_YEAR, NULL, atoi (tmps));
    }

    tuple_associate_string(tuple, FIELD_QUALITY, NULL, "lossy");

    if (comment != NULL && comment->vendor != NULL)
    {
        gchar *codec = g_strdup_printf("Ogg Vorbis [%s]", comment->vendor);
        tuple_associate_string(tuple, FIELD_CODEC, NULL, codec);
        g_free(codec);
    }
    else
        tuple_associate_string(tuple, FIELD_CODEC, NULL, "Ogg Vorbis");

    tuple_associate_string(tuple, FIELD_MIMETYPE, NULL, "application/ogg");

    return tuple;
}

static gfloat atof_no_locale (const gchar * string)
{
    gfloat result = 0;
    gboolean negative = FALSE;

    if (* string == '+')
        string ++;
    else if (* string == '-')
    {
        negative = TRUE;
        string ++;
    }

    while (* string >= '0' && * string <= '9')
        result = 10 * result + (* string ++ - '0');

    if (* string == '.')
    {
        gfloat place = 0.1;

        string ++;

        while (* string >= '0' && * string <= '9')
        {
            result += (* string ++ - '0') * place;
            place *= 0.1;
        }
    }

    return negative ? -result : result;
}

static gboolean
vorbis_update_replaygain(OggVorbis_File *vf, ReplayGainInfo *rg_info)
{
    vorbis_comment *comment;
    gchar *rg_gain, *rg_peak;

    if (vf == NULL || rg_info == NULL || (comment = ov_comment(vf, -1)) == NULL)
    {
#ifdef DEBUG
        printf ("No replay gain info.\n");
#endif
        return FALSE;
    }

    rg_gain = vorbis_comment_query(comment, "replaygain_album_gain", 0);
    if (!rg_gain) rg_gain = vorbis_comment_query(comment, "rg_audiophile", 0);    /* Old */
    rg_info->album_gain = (rg_gain != NULL) ? atof_no_locale (rg_gain) : 0.0;
#ifdef DEBUG
    printf ("Album gain: %s (%f)\n", rg_gain, rg_info->album_gain);
#endif

    rg_gain = vorbis_comment_query(comment, "replaygain_track_gain", 0);
    if (!rg_gain) rg_gain = vorbis_comment_query(comment, "rg_radio", 0);    /* Old */
    rg_info->track_gain = (rg_gain != NULL) ? atof_no_locale (rg_gain) : 0.0;
#ifdef DEBUG
    printf ("Track gain: %s (%f)\n", rg_gain, rg_info->track_gain);
#endif

    rg_peak = vorbis_comment_query(comment, "replaygain_album_peak", 0);
    rg_info->album_peak = rg_peak != NULL ? atof_no_locale (rg_peak) : 0.0;
#ifdef DEBUG
    printf ("Album peak: %s (%f)\n", rg_peak, rg_info->album_peak);
#endif

    rg_peak = vorbis_comment_query(comment, "replaygain_track_peak", 0);
    if (!rg_peak) rg_peak = vorbis_comment_query(comment, "rg_peak", 0);  /* Old */
    rg_info->track_peak = rg_peak != NULL ? atof_no_locale (rg_peak) : 0.0;
#ifdef DEBUG
    printf ("Track peak: %s (%f)\n", rg_peak, rg_info->track_peak);
#endif

    return TRUE;
}

static long
vorbis_interleave_buffer(float **pcm, int samples, int ch, float *pcmout)
{
    int i, j;
    for (i = 0; i < samples; i++)
        for (j = 0; j < ch; j++)
            *pcmout++ = pcm[j][i];

    return ch * samples * sizeof(float);
}


#define PCM_FRAMES 1024
#define PCM_BUFSIZE (PCM_FRAMES * 2)

static gboolean vorbis_play (InputPlayback * playback, const gchar * filename,
 VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
    if (file == NULL)
        return FALSE;

    vorbis_info *vi;
    OggVorbis_File vf;
    gint last_section = -1;
    ReplayGainInfo rg_info;
    gfloat pcmout[PCM_BUFSIZE*sizeof(float)], **pcm;
    gint bytes, channels, samplerate, br;
    gchar * title = NULL;

    playback->error = FALSE;
    seek_value = (start_time > 0) ? start_time : -1;
    memset(&vf, 0, sizeof(vf));

    if (ov_open_callbacks (file, & vf, NULL, 0, vfs_is_streaming (file) ?
     vorbis_callbacks_stream : vorbis_callbacks) < 0)
    {
        playback->error = TRUE;
        goto play_cleanup;
    }

    vi = ov_info(&vf, -1);

    if (vi->channels > 2)
    {
        playback->eof = TRUE;
        goto play_cleanup;
    }

    br = vi->bitrate_nominal;
    channels = vi->channels;
    samplerate = vi->rate;

    playback->set_params (playback, NULL, 0, br, samplerate, channels);

    if (!playback->output->open_audio(FMT_FLOAT, samplerate, channels)) {
        playback->error = TRUE;
        goto play_cleanup;
    }

    if (pause)
        playback->output->pause (TRUE);

    vorbis_update_replaygain(&vf, &rg_info);
    playback->output->set_replaygain_info (& rg_info);

    g_mutex_lock (seek_mutex);

    playback->playing = TRUE;
    playback->eof = FALSE;
    playback->set_pb_ready(playback);

    g_mutex_unlock (seek_mutex);

    /*
     * Note that chaining changes things here; A vorbis file may
     * be a mix of different channels, bitrates and sample rates.
     * You can fetch the information for any section of the file
     * using the ov_ interface.
     */

    while (1)
    {
        if (stop_time >= 0 && playback->output->written_time () >= stop_time)
            goto DRAIN;

        g_mutex_lock (seek_mutex);

        if (! playback->playing)
        {
            g_mutex_unlock (seek_mutex);
            break;
        }

        if (seek_value >= 0)
        {
            ov_time_seek (& vf, (double) seek_value / 1000);
            playback->output->flush (seek_value);
            seek_value = -1;
            g_cond_signal (seek_cond);
        }

        g_mutex_unlock (seek_mutex);

        gint current_section = last_section;
        bytes = ov_read_float(&vf, &pcm, PCM_FRAMES, &current_section);
        if (bytes == OV_HOLE)
            continue;

        if (bytes <= 0)
        {
DRAIN:
            while (playback->output->buffer_playing ())
                g_usleep (10000);

            playback->eof = 1;
            break;
        }

        bytes = vorbis_interleave_buffer (pcm, bytes, channels, pcmout);

        { /* try to detect when metadata has changed */
            vorbis_comment * comment = ov_comment (& vf, -1);
            const gchar * new_title = (comment == NULL) ? NULL :
             vorbis_comment_query (comment, "title", 0);

            if (new_title != NULL && (title == NULL || strcmp (title, new_title)))
            {
                g_free (title);
                title = g_strdup (new_title);

                playback->set_tuple (playback, get_tuple_for_vorbisfile
                 (& vf, playback->filename));
            }
        }

        if (current_section <= last_section) {
            /*
             * The info struct is different in each section.  vf
             * holds them all for the given bitstream.  This
             * requests the current one
             */
            vi = ov_info(&vf, -1);

            if (vi->channels > 2) {
                playback->eof = TRUE;
                goto stop_processing;
            }

            if (vi->rate != samplerate || vi->channels != channels) {
                samplerate = vi->rate;
                channels = vi->channels;
                while (playback->output->buffer_playing())
                    g_usleep(1000);

                playback->output->close_audio();

                if (!playback->output->open_audio(FMT_FLOAT, vi->rate, vi->channels)) {
                    playback->error = TRUE;
                    playback->eof = TRUE;
                    goto stop_processing;
                }

                playback->output->flush(ov_time_tell(&vf) * 1000);
                vorbis_update_replaygain(&vf, &rg_info);
                playback->output->set_replaygain_info (& rg_info); /* audio reopened */
            }
        }

        playback->output->write_audio (pcmout, bytes);

stop_processing:

        if (current_section <= last_section) {
            /*
             * set total play time, bitrate, rate, and channels of
             * current section
             */
            playback->set_params (playback, NULL, 0, br, samplerate, channels);

            last_section = current_section;

        }
    } /* main loop */

    g_mutex_lock (seek_mutex);
    playback->playing = FALSE;
    g_cond_signal (seek_cond); /* wake up any waiting request */
    g_mutex_unlock (seek_mutex);

    playback->output->close_audio ();

play_cleanup:

    ov_clear(&vf);
    g_free (title);
    return ! playback->error;
}

static void vorbis_stop (InputPlayback * playback)
{
    g_mutex_lock (seek_mutex);

    if (playback->playing)
    {
        playback->playing = FALSE;
        playback->output->abort_write ();
        g_cond_signal (seek_cond);
    }

    g_mutex_unlock (seek_mutex);
}

static void vorbis_pause (InputPlayback * playback, gshort pause)
{
    g_mutex_lock (seek_mutex);

    if (playback->playing)
        playback->output->pause (pause);

    g_mutex_unlock (seek_mutex);
}

static void vorbis_mseek (InputPlayback * playback, gulong time)
{
    g_mutex_lock (seek_mutex);

    if (playback->playing)
    {
        seek_value = time;
        playback->output->abort_write ();
        g_cond_signal (seek_cond);
        g_cond_wait (seek_cond, seek_mutex);
    }

    g_mutex_unlock (seek_mutex);
}

static Tuple * get_song_tuple (const gchar * filename, VFSFile * file)
{
    OggVorbis_File vfile;          /* avoid thread interaction */
    Tuple *tuple = NULL;

    /*
     * The open function performs full stream detection and
     * machine initialization.  If it returns zero, the stream
     * *is* Vorbis and we're fully ready to decode.
     */
    if (ov_open_callbacks (file, & vfile, NULL, 0, vfs_is_streaming (file) ?
     vorbis_callbacks_stream : vorbis_callbacks) < 0)
        return NULL;

    tuple = get_tuple_for_vorbisfile(&vfile, filename);
    ov_clear(&vfile);
    return tuple;
}

static void vorbis_aboutbox (void)
{
    static GtkWidget * about_window = NULL;

    audgui_simple_message (& about_window, GTK_MESSAGE_INFO,
     _("About Ogg Vorbis Audio Plugin"),
     /*
      * I18N: UTF-8 Translation: "Haavard Kvaalen" ->
      * "H\303\245vard Kv\303\245len"
      */
     _("Ogg Vorbis Plugin by the Xiph.org Foundation\n\n"
     "Original code by\n"
     "Tony Arcieri <bascule@inferno.tusculum.edu>\n"
     "Contributions from\n"
     "Chris Montgomery <monty@xiph.org>\n"
     "Peter Alm <peter@xmms.org>\n"
     "Michael Smith <msmith@labyrinth.edu.au>\n"
     "Jack Moffitt <jack@icecast.org>\n"
     "Jorn Baayen <jorn@nl.linux.org>\n"
     "Haavard Kvaalen <havardk@xmms.org>\n"
     "Gian-Carlo Pascutto <gcp@sjeng.org>\n"
     "Eugene Zagidullin <e.asphyx@gmail.com>\n\n"
     "Visit the Xiph.org Foundation at http://www.xiph.org/\n"));
}

static InputPlugin vorbis_ip;

static void
vorbis_init(void)
{
    mcs_handle_t *db;
    gchar *tmp = NULL;

    memset(&vorbis_cfg, 0, sizeof(vorbis_config_t));
    vorbis_cfg.http_buffer_size = 128;
    vorbis_cfg.http_prebuffer = 25;
    vorbis_cfg.proxy_port = 8080;
    vorbis_cfg.proxy_use_auth = FALSE;
    vorbis_cfg.proxy_user = NULL;
    vorbis_cfg.proxy_pass = NULL;
    vorbis_cfg.tag_override = FALSE;
    vorbis_cfg.tag_format = NULL;

    db = aud_cfg_db_open();
    aud_cfg_db_get_int(db, "vorbis", "http_buffer_size",
                       &vorbis_cfg.http_buffer_size);
    aud_cfg_db_get_int(db, "vorbis", "http_prebuffer",
                       &vorbis_cfg.http_prebuffer);
    aud_cfg_db_get_bool(db, "vorbis", "save_http_stream",
                        &vorbis_cfg.save_http_stream);
    if (!aud_cfg_db_get_string(db, "vorbis", "save_http_path",
                               &vorbis_cfg.save_http_path))
        vorbis_cfg.save_http_path = g_strdup(g_get_home_dir());

    aud_cfg_db_get_bool(db, "vorbis", "tag_override",
                        &vorbis_cfg.tag_override);
    if (!aud_cfg_db_get_string(db, "vorbis", "tag_format",
                               &vorbis_cfg.tag_format))
        vorbis_cfg.tag_format = g_strdup("%p - %t");

    aud_cfg_db_get_bool(db, NULL, "use_proxy", &vorbis_cfg.use_proxy);
    aud_cfg_db_get_string(db, NULL, "proxy_host", &vorbis_cfg.proxy_host);
    aud_cfg_db_get_string(db, NULL, "proxy_port", &tmp);

    if (tmp != NULL)
        vorbis_cfg.proxy_port = atoi(tmp);

    aud_cfg_db_get_bool(db, NULL, "proxy_use_auth", &vorbis_cfg.proxy_use_auth);
    aud_cfg_db_get_string(db, NULL, "proxy_user", &vorbis_cfg.proxy_user);
    aud_cfg_db_get_string(db, NULL, "proxy_pass", &vorbis_cfg.proxy_pass);

    aud_cfg_db_close(db);

    seek_mutex = g_mutex_new();
    seek_cond = g_cond_new();

    aud_mime_set_plugin("application/ogg", &vorbis_ip);
}

static void
vorbis_cleanup(void)
{
    if (vorbis_cfg.save_http_path) {
        g_free(vorbis_cfg.save_http_path);
        vorbis_cfg.save_http_path = NULL;
    }

    if (vorbis_cfg.proxy_host) {
        g_free(vorbis_cfg.proxy_host);
        vorbis_cfg.proxy_host = NULL;
    }

    if (vorbis_cfg.proxy_user) {
        g_free(vorbis_cfg.proxy_user);
        vorbis_cfg.proxy_user = NULL;
    }

    if (vorbis_cfg.proxy_pass) {
        g_free(vorbis_cfg.proxy_pass);
        vorbis_cfg.proxy_pass = NULL;
    }

    if (vorbis_cfg.tag_format) {
        g_free(vorbis_cfg.tag_format);
        vorbis_cfg.tag_format = NULL;
    }

    if (vorbis_cfg.title_encoding) {
        g_free(vorbis_cfg.title_encoding);
        vorbis_cfg.title_encoding = NULL;
    }

    g_strfreev(vorbis_tag_encoding_list);
    g_mutex_free(seek_mutex);
    g_cond_free(seek_cond);
}

extern PluginPreferences preferences;

static const gchar *vorbis_fmts[] = { "ogg", "ogm", "oga", NULL };

static InputPlugin vorbis_ip = {
    .description = "Ogg Vorbis Audio Plugin",
    .init = vorbis_init,
    .about = vorbis_aboutbox,
    .settings = &preferences,
    .play = vorbis_play,
    .stop = vorbis_stop,
    .pause = vorbis_pause,
    .mseek = vorbis_mseek,
    .cleanup = vorbis_cleanup,
    .probe_for_tuple = get_song_tuple,
    .is_our_file_from_vfs = vorbis_check_fd,
    .vfs_extensions = vorbis_fmts,
    .update_song_tuple = vorbis_update_song_tuple,

    /* Vorbis probing is a bit slow; check for MP3 and AAC first. -jlindgren */
    .priority = 2,
};

static InputPlugin *vorbis_iplist[] = { &vorbis_ip, NULL };

SIMPLE_INPUT_PLUGIN (vorbis, vorbis_iplist)
