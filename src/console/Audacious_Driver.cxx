/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2005-2009 Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 */

#include "config.h"
#include <math.h>
extern "C" {
#include <audacious/plugin.h>
}
#include "configure.h"
#include "Music_Emu.h"
#include "Gzip_Reader.h"

static GMutex *seek_mutex = NULL;
static GCond *seek_cond = NULL;
static gint seek_value = -1;

static const gint fade_threshold = 10 * 1000;
static const gint fade_length    = 8 * 1000;

static blargg_err_t log_err(blargg_err_t err)
{
    if (err) g_critical("console: %s\n", err);
    return err;
}

static void log_warning(Music_Emu * emu)
{
    const gchar *str = emu->warning();
    if (str != NULL)
        g_warning("console: %s\n", str);
}

/* Handles URL parsing, file opening and identification, and file
 * loading. Keeps file header around when loading rest of file to
 * avoid seeking and re-reading.
 */
class ConsoleFileHandler {
public:
    gchar *m_path;            // path without track number specification
    gint m_track;             // track number (0 = first track)
    Music_Emu* m_emu;         // set to 0 to take ownership
    gme_type_t m_type;
    
    // Parses path and identifies file type
    ConsoleFileHandler(const gchar* path, VFSFile *fd = NULL);
    
    // Creates emulator and returns 0. If this wasn't a music file or
    // emulator couldn't be created, returns 1.
    gint load(gint sample_rate);
    
    // Deletes owned emu and closes file
    ~ConsoleFileHandler();

private:
    gchar m_header[4];
    Vfs_File_Reader vfs_in;
    Gzip_Reader gzip_in;
};

ConsoleFileHandler::ConsoleFileHandler(const gchar *path, VFSFile *fd)
{
    m_emu   = NULL;
    m_type  = 0;
    m_track = -1;

    m_path = aud_filename_split_subtune(path, &m_track);
    if (m_path == NULL)
        return;
    
    m_track -= 1;

    // open vfs
    if (fd != NULL)
        vfs_in.reset(fd);
    else if (log_err(vfs_in.open(m_path)))
        return;
    
    // now open gzip_reader on top of vfs
    if (log_err(gzip_in.open(&vfs_in)))
        return;
    
    // read and identify header
    if (!log_err(gzip_in.read(m_header, sizeof(m_header))))
    {
        m_type = gme_identify_extension(gme_identify_header(m_header));
        if (!m_type)
        {
            m_type = gme_identify_extension(m_path);
            if (m_type != gme_gym_type) // only trust file extension for headerless .gym files
                m_type = 0;
        }
    }
}

ConsoleFileHandler::~ConsoleFileHandler()
{
    gme_delete(m_emu);
    g_free(m_path);
}

gint ConsoleFileHandler::load(gint sample_rate)
{
    if (!m_type)
        return 1;
    
    m_emu = gme_new_emu(m_type, sample_rate);
    if (m_emu == NULL)
    {
        log_err("Out of memory allocating emulator engine. Fatal error.");
        return 1;
    }
    
    // combine header with remaining file data
    Remaining_Reader reader(m_header, sizeof(m_header), &gzip_in);
    if (log_err(m_emu->load(reader)))
        return 1;
    
    // files can be closed now
    gzip_in.close();
    vfs_in.close();
    
    log_warning(m_emu);

#if 0
    // load .m3u from same directory( replace/add extension with ".m3u")
    gchar *m3u_path = g_strdup(m_path);
    gchar *ext = strrchr(m3u_path, '.');
    if (ext == NULL)
    {
        ext = g_strdup_printf("%s.m3u", m3u_path);
        g_free(m3u_path);
        m3u_path = ext;
    }
    
    Vfs_File_Reader m3u;
    if (!m3u.open(m3u_path))
    {
        if (log_err(m_emu->load_m3u(m3u))) // TODO: fail if m3u can't be loaded?
            log_warning(m_emu); // this will log line number of first problem in m3u
    }
#endif

    return 0;
}

static Tuple * get_track_ti(const gchar *path, const track_info_t *info, const gint track)
{
    Tuple *ti = aud_tuple_new_from_filename(path);

    g_print("file: %s\n", path);

    if (ti != NULL)
    {
        gint length;
        aud_tuple_associate_string(ti, FIELD_ARTIST, NULL, info->author);
        aud_tuple_associate_string(ti, FIELD_ALBUM, NULL, info->game);
        aud_tuple_associate_string(ti, -1, "game", info->game);
        aud_tuple_associate_string(ti, FIELD_TITLE, NULL, info->song ? info->song : g_path_get_basename(path));
        aud_tuple_associate_string(ti, FIELD_COPYRIGHT, NULL, info->copyright);
        aud_tuple_associate_string(ti, -1, "console", info->system);
        aud_tuple_associate_string(ti, FIELD_CODEC, NULL, info->system);
        aud_tuple_associate_string(ti, FIELD_QUALITY, NULL, "sequenced");
        aud_tuple_associate_string(ti, -1, "dumper", info->dumper);
        aud_tuple_associate_string(ti, FIELD_COMMENT, NULL, info->comment);
        if (track >= 0)
        {
            aud_tuple_associate_int(ti, FIELD_TRACK_NUMBER, NULL, track + 1);
            aud_tuple_associate_int(ti, FIELD_SUBSONG_ID, NULL, track + 1);
            aud_tuple_associate_int(ti, FIELD_SUBSONG_NUM, NULL, info->track_count);
        }
        else
        {
            ti->nsubtunes = info->track_count;
        }

        length = info->length;
        if (length <= 0)
            length = info->intro_length + 2 * info->loop_length;
        if (length <= 0)
            length = audcfg.loop_length * 1000;
        else if (length >= fade_threshold)
            length += fade_length;
        aud_tuple_associate_int(ti, FIELD_LENGTH, NULL, length);
    }

    return ti;
}

extern "C" Tuple * console_probe_for_tuple(const gchar *filename, VFSFile *fd)
{
    ConsoleFileHandler fh(filename, fd);
    
    if (!fh.m_type)
        return NULL;

    if (!fh.load(gme_info_only))
    {
        track_info_t info;
        if (!log_err(fh.m_emu->track_info(&info, fh.m_track < 0 ? 0 : fh.m_track)))
            return get_track_ti(fh.m_path, &info, fh.m_track);
    }

    return NULL;
}

extern "C" Tuple * console_get_file_tuple(const gchar *filename)
{
    ConsoleFileHandler fh(filename);
    
    if (!fh.m_type)
        return NULL;

    if (!fh.load(gme_info_only))
    {
        track_info_t info;
        if (!log_err(fh.m_emu->track_info(&info, fh.m_track < 0 ? 0 : fh.m_track)))
            return get_track_ti(fh.m_path, &info, fh.m_track);
    }

    return NULL;
}

extern "C" void console_play_file(InputPlayback *playback)
{
    gint length, end_delay, sample_rate;
    track_info_t info;

    // identify file
    ConsoleFileHandler fh(playback->filename);
    if (!fh.m_type)
        return;
    
    if (fh.m_track < 0)
        fh.m_track = 0;
    
    // select sample rate
    sample_rate = 0;
    if (fh.m_type == gme_spc_type)
        sample_rate = 32000;
    if (audcfg.resample)
        sample_rate = audcfg.resample_rate;
    if (sample_rate == 0)
        sample_rate = 44100;
    
    // create emulator and load file
    if (fh.load(sample_rate))
        return;
    
    // stereo echo depth
    gme_set_stereo_depth(fh.m_emu, 1.0 / 100 * audcfg.echo);
    
    // set equalizer
    if (audcfg.treble || audcfg.bass)
    {
        Music_Emu::equalizer_t eq;
        
        // bass - logarithmic, 2 to 8194 Hz
        double bass = 1.0 - (audcfg.bass / 200.0 + 0.5);
        eq.bass = (long) (2.0 + pow( 2.0, bass * 13 ));
        
        // treble - -50 to 0 to +5 dB
        double treble = audcfg.treble / 100.0;
        eq.treble = treble * (treble < 0 ? 50.0 : 5.0);
        
        fh.m_emu->set_equalizer(eq);
    }
    
    // get info
    length = -1;
    if (!log_err(fh.m_emu->track_info(&info, fh.m_track)))
    {
        if (fh.m_type == gme_spc_type && audcfg.ignore_spc_length)
            info.length = -1;
        
        Tuple *ti = get_track_ti(fh.m_path, &info, fh.m_track);
        if (ti != NULL)
        {
            length = aud_tuple_get_int(ti, FIELD_LENGTH, NULL);
            tuple_free(ti);
            playback->set_params(playback, NULL, NULL, fh.m_emu->voice_count() * 1000, sample_rate, 2);
        }
    }
    
    // start track
    if (log_err(fh.m_emu->start_track(fh.m_track)))
        return;

    log_warning(fh.m_emu);

    if (!playback->output->open_audio(FMT_S16_NE, sample_rate, 2))
        return;
    
    // set fade time
    if (length <= 0)
        length = audcfg.loop_length * 1000;
    if (length >= fade_threshold + fade_length)
        length -= fade_length / 2;
    fh.m_emu->set_fade(length, fade_length);
    
    playback->playing = 1;
    end_delay = 0;
    playback->set_pb_ready(playback);
    
    while (playback->playing)
    {
        /* Perform seek, if requested */
        g_mutex_lock(seek_mutex);
        if (seek_value >= 0)
        {
            playback->output->flush(seek_value * 1000);
            fh.m_emu->seek(seek_value * 1000);
            seek_value = -1;
            g_cond_signal(seek_cond);
        }
        g_mutex_unlock(seek_mutex);
        
        /* Fill and play buffer of audio */
        gint const buf_size = 1024;
        Music_Emu::sample_t buf[buf_size];
        if (end_delay)
        {
            // TODO: remove delay once host doesn't cut the end of track off
            if (!--end_delay)
                playback->playing = 0;
            memset(buf, 0, sizeof(buf));
        }
        else
        {
            fh.m_emu->play(buf_size, buf);
            if (fh.m_emu->track_ended())
            {
                end_delay = (fh.m_emu->sample_rate() * 3 * 2) / buf_size;
            }
        }
        
        playback->pass_audio(playback, FMT_S16_NE, 2, sizeof(buf), buf, NULL);
    }
    
    // stop playing
    playback->output->close_audio();
    playback->playing = 0;
}

extern "C" void console_seek(InputPlayback *data, gint time)
{
    g_mutex_lock(seek_mutex);
    seek_value = time;
    g_cond_wait(seek_cond, seek_mutex);
    g_mutex_unlock(seek_mutex);
}

extern "C" void console_stop(InputPlayback *playback)
{
    playback->playing = 0;
}

extern "C" void console_pause(InputPlayback * playback, gshort p)
{
    playback->output->pause(p);
}

extern "C" void console_init(void)
{
    console_cfg_load();
    seek_mutex = g_mutex_new();
    seek_cond = g_cond_new();
}

extern "C" void console_cleanup(void)
{
    g_mutex_free(seek_mutex);
    g_cond_free(seek_cond);
}
