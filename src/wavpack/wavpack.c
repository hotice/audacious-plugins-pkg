#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <wavpack/wavpack.h>

#include <audacious/audtag.h>
#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>

#include "config.h"

#define BUFFER_SIZE 256 /* read buffer size, in samples / frames */
#define SAMPLE_SIZE(a) (a == 8 ? sizeof(uint8_t) : (a == 16 ? sizeof(uint16_t) : sizeof(uint32_t)))
#define SAMPLE_FMT(a) (a == 8 ? FMT_S8 : (a == 16 ? FMT_S16_NE : (a == 24 ? FMT_S24_NE : FMT_S32_NE)))


/* Global mutexes etc.
 */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int64_t seek_value = -1;
static bool_t stop_flag = FALSE;

/* Audacious VFS wrappers for Wavpack stream reading
 */

static int32_t
wv_read_bytes(void *id, void *data, int32_t bcount)
{
    return vfs_fread(data, 1, bcount, (VFSFile *) id);
}

static uint32_t
wv_get_pos(void *id)
{
    return vfs_ftell((VFSFile *) id);
}

static int
wv_set_pos_abs(void *id, uint32_t pos)
{
    return vfs_fseek((VFSFile *) id, pos, SEEK_SET);
}

static int
wv_set_pos_rel(void *id, int32_t delta, int mode)
{
    return vfs_fseek((VFSFile *) id, delta, mode);
}

static int
wv_push_back_byte(void *id, int c)
{
    return vfs_ungetc(c, (VFSFile *) id);
}

static uint32_t
wv_get_length(void *id)
{
    VFSFile *file = (VFSFile *) id;

    if (file == NULL)
        return 0;

    return vfs_fsize(file);
}

static int wv_can_seek(void *id)
{
    return (vfs_is_streaming((VFSFile *) id) == FALSE);
}

static int32_t wv_write_bytes(void *id, void *data, int32_t bcount)
{
    return vfs_fwrite(data, 1, bcount, (VFSFile *) id);
}

WavpackStreamReader wv_readers = {
    wv_read_bytes,
    wv_get_pos,
    wv_set_pos_abs,
    wv_set_pos_rel,
    wv_push_back_byte,
    wv_get_length,
    wv_can_seek,
    wv_write_bytes
};

static bool_t wv_attach (const char * filename, VFSFile * wv_input,
 VFSFile * * wvc_input, WavpackContext * * ctx, char * error, int flags)
{
    if (flags & OPEN_WVC)
    {
        SPRINTF (corrFilename, "%sc", filename);
        *wvc_input = vfs_fopen(corrFilename, "rb");
    }

    * ctx = WavpackOpenFileInputEx (& wv_readers, wv_input, * wvc_input, error,
     flags, 0);

    if (ctx == NULL)
        return FALSE;
    else
        return TRUE;
}

static void wv_deattach (VFSFile * wvc_input, WavpackContext * ctx)
{
    if (wvc_input != NULL)
        vfs_fclose(wvc_input);
    WavpackCloseFile(ctx);
}

static bool_t wv_play (InputPlayback * playback, const char * filename,
 VFSFile * file, int start_time, int stop_time, bool_t pause)
{
    if (file == NULL)
        return FALSE;

    int32_t *input = NULL;
    void *output = NULL;
    int sample_rate, num_channels, bits_per_sample;
    unsigned num_samples;
    WavpackContext *ctx = NULL;
    VFSFile *wvc_input = NULL;
    bool_t error = FALSE;

    if (! wv_attach (filename, file, & wvc_input, & ctx, NULL, OPEN_TAGS |
     OPEN_WVC))
    {
        fprintf (stderr, "Error opening Wavpack file '%s'.", filename);
        error = TRUE;
        goto error_exit;
    }

    sample_rate = WavpackGetSampleRate(ctx);
    num_channels = WavpackGetNumChannels(ctx);
    bits_per_sample = WavpackGetBitsPerSample(ctx);
    num_samples = WavpackGetNumSamples(ctx);

    if (!playback->output->open_audio(SAMPLE_FMT(bits_per_sample), sample_rate, num_channels))
    {
        fprintf (stderr, "Error opening audio output.");
        error = TRUE;
        goto error_exit;
    }

    if (pause)
        playback->output->pause(TRUE);

    input = malloc(BUFFER_SIZE * num_channels * sizeof(uint32_t));
    output = malloc(BUFFER_SIZE * num_channels * SAMPLE_SIZE(bits_per_sample));
    if (input == NULL || output == NULL)
        goto error_exit;

    playback->set_gain_from_playlist(playback);

    pthread_mutex_lock (& mutex);

    playback->set_params(playback, (int) WavpackGetAverageBitrate(ctx, num_channels),
        sample_rate, num_channels);

    seek_value = (start_time > 0) ? start_time : -1;
    stop_flag = FALSE;

    playback->set_pb_ready(playback);

    pthread_mutex_unlock (& mutex);

    while (!stop_flag && (stop_time < 0 ||
     playback->output->written_time () < stop_time))
    {
        int ret;
        unsigned samples_left;

        /* Handle seek and pause requests */
        pthread_mutex_lock (& mutex);

        if (seek_value >= 0)
        {
            playback->output->flush (seek_value);
            WavpackSeekSample (ctx, (int64_t) seek_value * sample_rate / 1000);
            seek_value = -1;
        }

        pthread_mutex_unlock (& mutex);

        /* Decode audio data */
        samples_left = num_samples - WavpackGetSampleIndex(ctx);

        ret = WavpackUnpackSamples(ctx, input, BUFFER_SIZE);
        if (samples_left == 0)
            stop_flag = TRUE;
        else if (ret < 0)
        {
            fprintf (stderr, "Error decoding file.\n");
            break;
        }
        else
        {
            /* Perform audio data conversion and output */
            unsigned i;
            int32_t *rp = input;
            int8_t *wp = output;
            int16_t *wp2 = output;
            int32_t *wp4 = output;

            if (bits_per_sample == 8)
            {
                for (i = 0; i < ret * num_channels; i++, wp++, rp++)
                    *wp = *rp & 0xff;
            }
            else if (bits_per_sample == 16)
            {
                for (i = 0; i < ret * num_channels; i++, wp2++, rp++)
                    *wp2 = *rp & 0xffff;
            }
            else if (bits_per_sample == 24 || bits_per_sample == 32)
            {
                for (i = 0; i < ret * num_channels; i++, wp4++, rp++)
                    *wp4 = *rp;
            }

            playback->output->write_audio(output, ret * num_channels * SAMPLE_SIZE(bits_per_sample));
        }
    }

error_exit:

    free(input);
    free(output);
    wv_deattach (wvc_input, ctx);

    stop_flag = TRUE;
    return ! error;
}

static void
wv_stop(InputPlayback * playback)
{
    pthread_mutex_lock (& mutex);

    if (!stop_flag)
    {
        stop_flag = TRUE;
        playback->output->abort_write();
    }

    pthread_mutex_unlock (& mutex);
}

static void
wv_pause(InputPlayback * playback, bool_t pause)
{
    pthread_mutex_lock (& mutex);

    if (!stop_flag)
        playback->output->pause(pause);

    pthread_mutex_unlock (& mutex);
}

static void wv_seek (InputPlayback * playback, int time)
{
    pthread_mutex_lock (& mutex);

    if (!stop_flag)
    {
        seek_value = time;
        playback->output->abort_write();
    }

    pthread_mutex_unlock (& mutex);
}

static char *
wv_get_quality(WavpackContext *ctx)
{
    int mode = WavpackGetMode(ctx);
    const char *quality;

    if (mode & MODE_LOSSLESS)
        quality = _("lossless");
    else if (mode & MODE_HYBRID)
        quality = _("lossy (hybrid)");
    else
        quality = _("lossy");

    return str_printf ("%s%s%s", quality,
        (mode & MODE_WVC) ? " (wvc corrected)" : "",
#ifdef MODE_DNS /* WavPack 4.50 or later */
        (mode & MODE_DNS) ? " (dynamic noise shaped)" :
#endif
        "");
}

static Tuple *
wv_probe_for_tuple(const char * filename, VFSFile * fd)
{
    WavpackContext *ctx;
    Tuple *tu;
    char error[1024];

    ctx = WavpackOpenFileInputEx(&wv_readers, fd, NULL, error, OPEN_TAGS, 0);

    if (ctx == NULL)
        return NULL;

	AUDDBG("starting probe of %p\n", (void *) fd);

	vfs_rewind(fd);
	tu = tuple_new_from_filename(filename);

	vfs_rewind(fd);
	tag_tuple_read(tu, fd);

	tuple_set_int(tu, FIELD_LENGTH, NULL,
        ((uint64_t) WavpackGetNumSamples(ctx) * 1000) / (uint64_t) WavpackGetSampleRate(ctx));
    tuple_set_str(tu, FIELD_CODEC, NULL, "WavPack");

    char * quality = wv_get_quality (ctx);
    tuple_set_str (tu, FIELD_QUALITY, NULL, quality);
    str_unref (quality);

    WavpackCloseFile(ctx);

	AUDDBG("returning tuple %p for file %p\n", (void *) tu, (void *) fd);
	return tu;
}

static bool_t wv_write_tag (const Tuple * tuple, VFSFile * handle)
{
    return tag_tuple_write(tuple, handle, TAG_TYPE_APE);
}

static const char wv_about[] =
 N_("Copyright 2006 William Pitcock <nenolod@nenolod.net>\n\n"
    "Some of the plugin code was by Miles Egan.");

static const char *wv_fmts[] = { "wv", NULL };

AUD_INPUT_PLUGIN
(
    .name = N_("WavPack Decoder"),
    .domain = PACKAGE,
    .about_text = wv_about,
    .play = wv_play,
    .stop = wv_stop,
    .pause = wv_pause,
    .mseek = wv_seek,
    .extensions = wv_fmts,
    .probe_for_tuple = wv_probe_for_tuple,
    .update_song_tuple = wv_write_tag,
)
