#include "config.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "neaacdec.h"
#include "mp4ff.h"
#include "tagging.h"

#include <audacious/debug.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <libaudcore/audstrings.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#define MP4_VERSION VERSION
#define SBR_DEC

/*
 * BUFFER_SIZE is the highest amount of memory that can be pulled.
 * We use this for sanity checks, among other things, as mp4ff needs
 * a labotomy sometimes.
 */
#define BUFFER_SIZE (FAAD_MIN_STREAMSIZE * 16)

static void mp4_about (void);
static void mp4_cleanup (void);
static gint mp4_is_our_fd (const char *, VFSFile *);

static const gchar *fmts[] = { "m4a", "mp4", "aac", NULL };

static GMutex *seek_mutex;
static GCond *seek_cond;
static gint seek_value;
static gboolean stop_flag;

typedef struct _mp4cfg
{
#define FILE_OTHER      0
#define FILE_MP4        1
#define FILE_AAC        2
    gshort file_type;
} Mp4Config;

static Mp4Config mp4cfg;

void getMP4info (char *);
int getAACTrack (mp4ff_t *);

static guint32 mp4_read_callback (void *data, void *buffer, guint32 len)
{
    if (data == NULL || buffer == NULL)
        return -1;

    return vfs_fread (buffer, 1, len, (VFSFile *) data);
}

static guint32 mp4_seek_callback (void *data, guint64 pos)
{
    g_return_val_if_fail (data != NULL, -1);
    g_return_val_if_fail (pos <= G_MAXINT64, -1);

    return vfs_fseek ((VFSFile *) data, pos, SEEK_SET);
}

static gboolean mp4_init (void)
{
    mp4cfg.file_type = FILE_OTHER;

    seek_mutex = g_mutex_new ();
    seek_cond = g_cond_new ();
    return TRUE;
}

static void mp4_stop (InputPlayback * p)
{
    g_mutex_lock (seek_mutex);

    if (! stop_flag)
    {
        stop_flag = TRUE;
        p->output->abort_write ();
        g_cond_signal (seek_cond);
        g_cond_wait (seek_cond, seek_mutex);
    }

    g_mutex_unlock (seek_mutex);
}

static void mp4_pause (InputPlayback * p, gboolean pause)
{
    g_mutex_lock (seek_mutex);

    if (! stop_flag)
        p->output->pause (pause);

    g_mutex_unlock (seek_mutex);
}

static void mp4_seek (InputPlayback * p, gint time)
{
    g_mutex_lock (seek_mutex);

    if (! stop_flag)
    {
        seek_value = time;
        p->output->abort_write();
        g_cond_signal (seek_cond);
        g_cond_wait (seek_cond, seek_mutex);
    }

    g_mutex_unlock (seek_mutex);
}

/*
 * These routines are derived from MPlayer.
 */

/// \param srate (out) sample rate
/// \param num (out) number of audio frames in this ADTS frame
/// \return size of the ADTS frame in bytes
/// aac_parse_frames needs a buffer at least 8 bytes long
int aac_parse_frame (guchar * buf, int *srate, int *num)
{
    int i = 0, sr, fl = 0;
    static int srates[] =
     { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000,
         11025, 8000, 0, 0, 0 };

    if ((buf[i] != 0xFF) || ((buf[i + 1] & 0xF6) != 0xF0))
        return 0;

/* We currently have no use for the id below.
        id = (buf[i+1] >> 3) & 0x01;    //id=1 mpeg2, 0: mpeg4
*/
    sr = (buf[i + 2] >> 2) & 0x0F;
    if (sr > 11)
        return 0;
    *srate = srates[sr];

    fl =
     ((buf[i + 3] & 0x03) << 11) | (buf[i + 4] << 3) | ((buf[i +
     5] >> 5) & 0x07);
    *num = (buf[i + 6] & 0x02) + 1;

    return fl;
}

#define PROBE_DEBUG(...)

/* Searches <length> bytes of data for an ADTS header.  Returns the offset of
 * the first header or -1 if none is found.  Sets <size> to the length of the
 * frame. */
static gint find_aac_header (guchar * data, gint length, gint * size)
{
    gint offset, a, b;

    for (offset = 0; offset <= length - 8; offset++)
    {
        if (data[offset] != 255)
            continue;

        *size = aac_parse_frame (data + offset, &a, &b);

        if (*size < 8)
            continue;

        return offset;
    }

    return -1;
}

static gboolean parse_aac_stream (VFSFile * stream)
{
    guchar data[8192];
    gint offset, found, inner, size;

    size = 0;                   /* avoid bogus uninitialized variable warning */

    if (vfs_fread (data, 1, sizeof data, stream) != sizeof data)
    {
        PROBE_DEBUG ("Read failed.\n");
        return FALSE;
    }

    offset = 0;

    for (found = 0; found < 3; found++)
    {
        inner = find_aac_header (data + offset, sizeof data - offset, &size);

        if (!(inner == 0 || (found == 0 && inner > 0)))
        {
            PROBE_DEBUG ("Only %d ADTS headers.\n", found);
            return FALSE;
        }

        offset += inner + size;
    }

    PROBE_DEBUG ("Accepted.\n");
    return TRUE;
}

/* Quick search for an ADTS or ADIF header in the first <len> bytes of <buf>.
 * Returns the byte offset of the header or <len> if none is found. */

static gint aac_probe (guchar * buf, gint len)
{
    for (gint i = 0; i <= len - 4; i ++)
    {
        if ((buf[i] == 0xff && (buf[i + 1] & 0xf6) == 0xf0) || ! strncmp
         ((gchar *) buf + i, "ADIF", 4))
            return i;
    }

    return len;
}

static gboolean is_mp4_aac_file (VFSFile * handle)
{
    mp4ff_callback_t mp4_data = {.read = mp4_read_callback,.seek =
         mp4_seek_callback,.user_data = handle
    };
    mp4ff_t *mp4_handle = mp4ff_open_read (&mp4_data);
    gboolean success;

    if (mp4_handle == NULL)
        return FALSE;

    success = (getAACTrack (mp4_handle) != -1);

    mp4ff_close (mp4_handle);
    return success;
}

static gboolean mp4_is_our_fd (const gchar * filename, VFSFile * file)
{
    if (is_mp4_aac_file (file))
        return TRUE;

    if (vfs_fseek (file, 0, SEEK_SET))
        return FALSE;
    return parse_aac_stream (file);
}

static void mp4_about (void)
{
    static GtkWidget *aboutbox = NULL;

    if (aboutbox == NULL)
    {
        gchar *about_text =
         g_strdup_printf (_("Using libfaad2-%s for decoding.\n"
         "FAAD2 AAC/HE-AAC/HE-AACv2/DRM decoder (c) Nero AG, www.nero.com\n"
         "Copyright (c) 2005-2006 Audacious team"), FAAD2_VERSION);

        audgui_simple_message (&aboutbox, GTK_MESSAGE_INFO,
         _("About MP4 AAC decoder plugin"), about_text);

        g_free (about_text);
    }
}

static void mp4_cleanup (void)
{
    g_mutex_free (seek_mutex);
    g_cond_free (seek_cond);
}

/* Gets info (some approximated) from an AAC/ADTS file.  <length> is
 * milliseconds, <bitrate> is kilobits per second.  Any parameters that cannot
 * be read are set to -1. */
static void calc_aac_info (VFSFile * handle, gint * length, gint * bitrate,
 gint * samplerate, gint * channels)
{
    NeAACDecHandle decoder;
    NeAACDecFrameInfo frame;
    gboolean initted = FALSE;
    gint size = vfs_fsize (handle);
    guchar buffer[BUFFER_SIZE];
    gint offset = 0, filled = 0;
    gint found, bytes_used = 0, time_used = 0;

    decoder = NULL;             /* avoid bogus uninitialized variable warning */

    *length = -1;
    *bitrate = -1;
    *samplerate = -1;
    *channels = -1;

    /* look for a representative bitrate in the middle of the file */
    if (size > 0 && vfs_fseek (handle, size / 2, SEEK_SET))
        goto DONE;

    for (found = 0; found < 32; found++)
    {
        if (filled < BUFFER_SIZE / 2)
        {
            memmove (buffer, buffer + offset, filled);
            offset = 0;

            if (vfs_fread (buffer + filled, 1, BUFFER_SIZE - filled, handle)
             != BUFFER_SIZE - filled)
            {
                PROBE_DEBUG ("Read failed.\n");
                goto DONE;
            }

            filled = BUFFER_SIZE;
        }

        if (!initted)
        {
            gint inner, a;
            gulong r;
            guchar ch;

            inner = find_aac_header (buffer + offset, filled, &a);

            if (inner < 0)
            {
                PROBE_DEBUG ("No ADTS header.\n");
                goto DONE;
            }

            offset += inner;
            filled -= inner;

            decoder = NeAACDecOpen ();
            inner = NeAACDecInit (decoder, buffer + offset, filled, &r, &ch);

            if (inner < 0)
            {
                PROBE_DEBUG ("Decoder init failed.\n");
                NeAACDecClose (decoder);
                goto DONE;
            }

            offset += inner;
            filled -= inner;
            bytes_used += inner;

            *samplerate = r;
            *channels = ch;
            initted = TRUE;
        }

        if (NeAACDecDecode (decoder, &frame, buffer + offset, filled) == NULL)
        {
            PROBE_DEBUG ("Decode failed.\n");
            goto DONE;
        }

        if (frame.samplerate != *samplerate || frame.channels != *channels)
        {
            PROBE_DEBUG ("Parameter mismatch.\n");
            goto DONE;
        }

        offset += frame.bytesconsumed;
        filled -= frame.bytesconsumed;
        bytes_used += frame.bytesconsumed;
        time_used += frame.samples / frame.channels * (gint64) 1000 /
         frame.samplerate;
    }

    /* bits per millisecond = kilobits per second */
    *bitrate = bytes_used * 8 / time_used;

    if (size > 0)
        *length = size * (gint64) time_used / bytes_used;

  DONE:
    if (initted)
        NeAACDecClose (decoder);
}

static Tuple *aac_get_tuple (const gchar * filename, VFSFile * handle)
{
    Tuple *tuple = tuple_new_from_filename (filename);
    gchar *temp;
    gint length, bitrate, samplerate, channels;

    tuple_set_str (tuple, FIELD_CODEC, NULL, "MPEG-2/4 AAC");

    if (!vfs_is_remote (filename))
    {
        calc_aac_info (handle, &length, &bitrate, &samplerate, &channels);

        if (length > 0)
            tuple_set_int (tuple, FIELD_LENGTH, NULL, length);

        if (bitrate > 0)
            tuple_set_int (tuple, FIELD_BITRATE, NULL, bitrate);
    }

    temp = vfs_get_metadata (handle, "track-name");
    if (temp != NULL)
    {
        tuple_set_str (tuple, FIELD_TITLE, NULL, temp);
        g_free (temp);
    }

    temp = vfs_get_metadata (handle, "stream-name");
    if (temp != NULL)
    {
        tuple_set_str (tuple, FIELD_ALBUM, NULL, temp);
        g_free (temp);
    }

    temp = vfs_get_metadata (handle, "content-bitrate");
    if (temp != NULL)
    {
        tuple_set_int (tuple, FIELD_BITRATE, NULL, atoi (temp) / 1000);
        g_free (temp);
    }

    return tuple;
}

static gboolean aac_title_changed (const gchar * filename, VFSFile * handle,
 Tuple * tuple)
{
    gchar *old = tuple_get_str (tuple, FIELD_TITLE, NULL);
    gchar *new = vfs_get_metadata (handle, "track-name");
    gboolean changed = FALSE;

    changed = (new != NULL && (old == NULL || strcmp (old, new)));
    if (changed)
        tuple_set_str (tuple, FIELD_TITLE, NULL, new);

    g_free (new);
    str_unref(old);
    return changed;
}

static void read_and_set_string (mp4ff_t * mp4, gint (*func) (const mp4ff_t *
 mp4, gchar * *string), Tuple * tuple, gint field)
{
    gchar *string = NULL;

    func (mp4, &string);

    if (string != NULL)
        tuple_set_str (tuple, field, NULL, string);

    free (string);
}

static Tuple *generate_tuple (const gchar * filename, mp4ff_t * mp4, gint track)
{
    Tuple *tuple = tuple_new_from_filename (filename);
    gint64 length;
    gint scale, rate, channels, bitrate;
    gchar *year = NULL, *cd_track = NULL;
    gchar scratch[32];

    tuple_set_str (tuple, FIELD_CODEC, NULL, "MPEG-2/4 AAC");

    length = mp4ff_get_track_duration (mp4, track);
    scale = mp4ff_time_scale (mp4, track);

    if (length > 0 && scale > 0)
        tuple_set_int (tuple, FIELD_LENGTH, NULL, length * 1000 / scale);

    rate = mp4ff_get_sample_rate (mp4, track);
    channels = mp4ff_get_channel_count (mp4, track);

    if (rate > 0 && channels > 0)
    {
        snprintf (scratch, sizeof scratch, "%d kHz, %s", rate / 1000, channels
         == 1 ? "mono" : channels == 2 ? "stereo" : "surround");
        tuple_set_str (tuple, FIELD_QUALITY, NULL, scratch);
    }

    bitrate = mp4ff_get_avg_bitrate (mp4, track);

    if (bitrate > 0)
        tuple_set_int (tuple, FIELD_BITRATE, NULL, bitrate / 1000);

    read_and_set_string (mp4, mp4ff_meta_get_title, tuple, FIELD_TITLE);
    read_and_set_string (mp4, mp4ff_meta_get_album, tuple, FIELD_ALBUM);
    read_and_set_string (mp4, mp4ff_meta_get_artist, tuple, FIELD_ARTIST);
    read_and_set_string (mp4, mp4ff_meta_get_comment, tuple, FIELD_COMMENT);
    read_and_set_string (mp4, mp4ff_meta_get_genre, tuple, FIELD_GENRE);

    mp4ff_meta_get_date (mp4, &year);

    if (year != NULL)
        tuple_set_int (tuple, FIELD_YEAR, NULL, atoi (year));

    free (year);

    mp4ff_meta_get_track (mp4, &cd_track);

    if (cd_track != NULL)
        tuple_set_int (tuple, FIELD_TRACK_NUMBER, NULL, atoi (cd_track));

    free (cd_track);

    return tuple;
}

static Tuple *mp4_get_tuple (const gchar * filename, VFSFile * handle)
{
    mp4ff_callback_t mp4cb;
    mp4ff_t *mp4;
    gint track;
    Tuple *tuple;

    if (parse_aac_stream (handle))
        return aac_get_tuple (filename, handle);

    vfs_rewind (handle);

    mp4cb.read = mp4_read_callback;
    mp4cb.seek = mp4_seek_callback;
    mp4cb.user_data = handle;

    mp4 = mp4ff_open_read (&mp4cb);

    if (mp4 == NULL)
        return NULL;

    track = getAACTrack (mp4);

    if (track < 0)
    {
        mp4ff_close (mp4);
        return NULL;
    }

    tuple = generate_tuple (filename, mp4, track);
    mp4ff_close (mp4);
    return tuple;
}

static gboolean my_decode_mp4 (InputPlayback * playback, const char * filename,
 mp4ff_t * mp4file)
{
    // We are reading an MP4 file
    gint mp4track = getAACTrack (mp4file);
    NeAACDecHandle decoder;
    NeAACDecConfigurationPtr decoder_config;
    guchar *buffer = NULL;
    guint bufferSize = 0;
    gulong samplerate = 0;
    guchar channels = 0;
    guint numSamples;
    gulong sampleID = 1;
    guint framesize = 0;

    if (mp4track < 0)
    {
        g_print ("Unsupported Audio track type\n");
        return TRUE;
    }

    // Open decoder
    decoder = NeAACDecOpen ();

    // Configure for floating point output
    decoder_config = NeAACDecGetCurrentConfiguration (decoder);
    decoder_config->outputFormat = FAAD_FMT_FLOAT;
    NeAACDecSetConfiguration (decoder, decoder_config);

    mp4ff_get_decoder_config (mp4file, mp4track, &buffer, &bufferSize);
    if (!buffer)
    {
        NeAACDecClose (decoder);
        return FALSE;
    }
    if (NeAACDecInit2 (decoder, buffer, bufferSize, &samplerate, &channels) < 0)
    {
        NeAACDecClose (decoder);

        return FALSE;
    }

    g_free (buffer);
    if (!channels)
    {
        NeAACDecClose (decoder);

        return FALSE;
    }
    numSamples = mp4ff_num_samples (mp4file, mp4track);

    if (!playback->output->open_audio (FMT_FLOAT, samplerate, channels))
    {
        NeAACDecClose (decoder);
        return FALSE;
    }

    playback->set_tuple (playback, generate_tuple (filename, mp4file,
     mp4track));
    playback->set_params (playback, mp4ff_get_avg_bitrate (mp4file, mp4track),
     samplerate, channels);
    playback->set_pb_ready (playback);

    while (1)
    {
        void *sampleBuffer;
        NeAACDecFrameInfo frameInfo;
        gint rc;

        buffer = NULL;
        bufferSize = 0;

        /* If we've run to the end of the file, we're done. */
        if (sampleID >= numSamples)
        {
            /* Finish playing before we close the
               output. */
            while (playback->output->buffer_playing ())
            {
                g_usleep (10000);
            }

            break;
        }

        rc = mp4ff_read_sample (mp4file, mp4track,
         sampleID++, &buffer, &bufferSize);

        /*g_print(":: %d/%d\n", sampleID-1, numSamples); */

        /* If we can't read the file, we're done. */
        if ((rc == 0) || (buffer == NULL) || (bufferSize == 0)
         || (bufferSize > BUFFER_SIZE))
        {
            g_print ("MP4: read error\n");
            sampleBuffer = NULL;
            playback->output->close_audio ();

            NeAACDecClose (decoder);

            return FALSE;
        }

/*          g_print(" :: %d/%d\n", bufferSize, BUFFER_SIZE); */

        sampleBuffer = NeAACDecDecode (decoder, &frameInfo, buffer, bufferSize);

        /* If there was an error decoding, we're done. */
        if (frameInfo.error > 0)
        {
            g_print ("MP4: %s\n", NeAACDecGetErrorMessage (frameInfo.error));
            playback->output->close_audio ();
            NeAACDecClose (decoder);

            return FALSE;
        }
        if (buffer)
        {
            g_free (buffer);
            buffer = NULL;
            bufferSize = 0;
        }

        /* Calculate frame size from the first (non-blank) frame.  This needs to
         * be done before we try to seek. */
        if (!framesize)
        {
            framesize = frameInfo.samples / frameInfo.channels;

            if (!framesize)
                continue;
        }

        /* Respond to seek/stop requests.  This needs to be done after we
         * calculate frame size but of course before we write any audio. */
        g_mutex_lock (seek_mutex);

        if (stop_flag)
        {
            g_mutex_unlock (seek_mutex);
            break;
        }

        if (seek_value >= 0)
        {
            sampleID = (gint64) seek_value *samplerate / 1000 / framesize;
            playback->output->flush (seek_value);
            seek_value = -1;
            g_cond_signal (seek_cond);
        }

        g_mutex_unlock (seek_mutex);

        playback->output->write_audio (sampleBuffer,
         sizeof (gfloat) * frameInfo.samples);
    }

    g_mutex_lock (seek_mutex);
    stop_flag = TRUE;
    g_cond_signal (seek_cond);
    g_mutex_unlock (seek_mutex);

    playback->output->close_audio ();
    NeAACDecClose (decoder);

    return TRUE;
}

static void aac_seek (VFSFile * file, NeAACDecHandle dec, gint time, gint len,
 void * buf, gint size, gint * buflen)
{
    AUDDBG ("Seeking to millisecond %d of %d.\n", time, len);

    /* == ESTIMATE BYTE OFFSET == */

    gint64 total = vfs_fsize (file);
    if (total < 0)
    {
        fprintf (stderr, "aac: File is not seekable.\n");
        return;
    }

    AUDDBG ("That means byte %d of %d.\n", (gint) (total * time / len), (gint)
     total);

    /* == SEEK == */

    if (vfs_fseek (file, total * time / len, SEEK_SET))
        return;

    * buflen = vfs_fread (buf, 1, size, file);

    /* == FIND FRAME HEADER == */

    gint used = aac_probe (buf, * buflen);

    if (used == * buflen)
    {
        fprintf (stderr, "aac: No valid frame header found.\n");
        * buflen = 0;
        return;
    }

    if (used)
    {
        * buflen -= used;
        memmove (buf, buf + used, * buflen);
        * buflen += vfs_fread (buf + * buflen, 1, size - * buflen, file);
    }

    /* == START DECODING == */

    guchar chan;
    gulong rate;

    if ((used = NeAACDecInit (dec, buf, * buflen, & rate, & chan)))
    {
        * buflen -= used;
        memmove (buf, buf + used, * buflen);
        * buflen += vfs_fread (buf + * buflen, 1, size - * buflen, file);
    }
}

static gboolean my_decode_aac (InputPlayback * playback, const char * filename,
 VFSFile * file)
{
    NeAACDecHandle decoder = 0;
    NeAACDecConfigurationPtr decoder_config;
    gulong samplerate = 0;
    guchar channels = 0;
    Tuple *tuple;
    gint bitrate = 0;

    tuple = aac_get_tuple (filename, file);
    if (tuple != NULL)
    {
        tuple_ref (tuple);
        playback->set_tuple (playback, tuple);

        bitrate = tuple_get_int (tuple, FIELD_BITRATE, NULL);
        bitrate = 1000 * MAX (0, bitrate);
    }

    vfs_rewind (file);
    if ((decoder = NeAACDecOpen ()) == NULL)
    {
        g_print ("AAC: Open Decoder Error\n");
        return FALSE;
    }

    decoder_config = NeAACDecGetCurrentConfiguration (decoder);
    decoder_config->outputFormat = FAAD_FMT_FLOAT;
    NeAACDecSetConfiguration (decoder, decoder_config);

    /* == FILL BUFFER == */

    guchar buf[BUFFER_SIZE];
    gint buflen = vfs_fread (buf, 1, sizeof buf, file);

    /* == SKIP ID3 TAG == */

    if (buflen >= 10 && ! strncmp ((char *) buf, "ID3", 3))
    {
        if (vfs_fseek (file, 10 + (buf[6] << 21) + (buf[7] << 14) + (buf[8] <<
         7) + buf[9], SEEK_SET))
        {
            fprintf (stderr, "aac: Failed to seek past ID3v2 tag.\n");
            goto ERR_CLOSE_DECODER;
        }

        buflen = vfs_fread (buf, 1, sizeof buf, file);
    }

    /* == FIND FRAME HEADER == */

    gint used = aac_probe (buf, buflen);

    if (used == buflen)
    {
        fprintf (stderr, "aac: No valid frame header found.\n");
        goto ERR_CLOSE_DECODER;
    }

    if (used)
    {
        buflen -= used;
        memmove (buf, buf + used, buflen);
        buflen += vfs_fread (buf + buflen, 1, sizeof buf - buflen, file);
    }

    /* == START DECODING == */

    if ((used = NeAACDecInit (decoder, buf, buflen, & samplerate, & channels)))
    {
        buflen -= used;
        memmove (buf, buf + used, buflen);
        buflen += vfs_fread (buf + buflen, 1, sizeof buf - buflen, file);
    }

    /* == CHECK FOR METADATA == */

    if (tuple && aac_title_changed (filename, file, tuple))
    {
        tuple_ref (tuple);
        playback->set_tuple (playback, tuple);
    }

    /* == START PLAYBACK == */

    if (! playback->output->open_audio (FMT_FLOAT, samplerate, channels))
        goto ERR_CLOSE_DECODER;

    playback->set_params (playback, bitrate, samplerate, channels);
    playback->set_pb_ready (playback);

    /* == MAIN LOOP == */

    while (1)
    {
        g_mutex_lock (seek_mutex);

        /* == HANDLE STOP REQUESTS == */

        if (stop_flag)
        {
            g_mutex_unlock (seek_mutex);
            break;
        }

        /* == HANDLE SEEK REQUESTS == */

        if (seek_value >= 0)
        {
            gint length = (tuple != NULL) ? tuple_get_int (tuple, FIELD_LENGTH,
             NULL) : 0;

            if (length > 0)
            {
                aac_seek (file, decoder, seek_value, length, buf, sizeof buf,
                 & buflen);
                playback->output->flush (seek_value);
            }

            seek_value = -1;
            g_cond_signal (seek_cond);
        }

        g_mutex_unlock (seek_mutex);

        /* == CHECK FOR END OF FILE == */

        if (! buflen)
        {
            while (playback->output->buffer_playing ())
                g_usleep (20000);

            break;
        }

        /* == CHECK FOR METADATA == */

        if (tuple && aac_title_changed (filename, file, tuple))
        {
            tuple_ref (tuple);
            playback->set_tuple (playback, tuple);
        }

        /* == DECODE A FRAME == */

        NeAACDecFrameInfo info;
        void * audio = NeAACDecDecode (decoder, & info, buf, buflen);

        if (info.error)
        {
            fprintf (stderr, "aac: %s.\n", NeAACDecGetErrorMessage (info.error));

            if (buflen)
            {
                used = 1 + aac_probe (buf + 1, buflen - 1);
                buflen -= used;
                memmove (buf, buf + used, buflen);
                buflen += vfs_fread (buf + buflen, 1, sizeof buf - buflen, file);
            }

            continue;
        }

        if ((used = info.bytesconsumed))
        {
            buflen -= used;
            memmove (buf, buf + used, buflen);
            buflen += vfs_fread (buf + buflen, 1, sizeof buf - buflen, file);
        }

        /* == PLAY THE SOUND == */

        if (audio && info.samples)
            playback->output->write_audio (audio, sizeof (gfloat) * info.samples);
    }

    g_mutex_lock (seek_mutex);
    stop_flag = TRUE;
    g_cond_signal (seek_cond);
    g_mutex_unlock (seek_mutex);

    playback->output->close_audio ();
    NeAACDecClose (decoder);

    if (tuple)
        tuple_unref (tuple);

    return TRUE;

ERR_CLOSE_DECODER:
    NeAACDecClose (decoder);

    if (tuple)
        tuple_unref (tuple);

    return FALSE;
}

static gboolean mp4_play (InputPlayback * playback, const gchar * filename,
 VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
    g_return_val_if_fail (file != NULL, FALSE);

    mp4ff_callback_t *mp4cb = g_malloc0 (sizeof (mp4ff_callback_t));
    mp4ff_t *mp4file;
    gboolean ret;

    ret = parse_aac_stream (file);
    vfs_rewind (file);

    mp4cb->read = mp4_read_callback;
    mp4cb->seek = mp4_seek_callback;
    mp4cb->user_data = file;

    mp4file = mp4ff_open_read (mp4cb);

    seek_value = (start_time > 0) ? start_time : -1;
    stop_flag = FALSE;

    if (ret == TRUE)
    {
        g_free (mp4cb);
        return my_decode_aac (playback, filename, file);
    }

    return my_decode_mp4 (playback, filename, mp4file);
}

gboolean read_itunes_cover (const gchar * filename, VFSFile * file, void * *
 data, gint64 * size);

AUD_INPUT_PLUGIN
(
    .name = "MP4 AAC decoder",
    .init = mp4_init,
    .about = mp4_about,
    .play = mp4_play,
    .stop = mp4_stop,
    .pause = mp4_pause,
    .mseek = mp4_seek,
    .cleanup = mp4_cleanup,
    .is_our_file_from_vfs = mp4_is_our_fd,
    .probe_for_tuple = mp4_get_tuple,
    .get_song_image = read_itunes_cover,
    .extensions = fmts,
)
