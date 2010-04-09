#include <audacious/plugin.h>

void Init(void);
void ShowAboutBox(void);
void ShowConfigureBox(void);
void PlayFile(InputPlayback *data);
void Stop(InputPlayback *data);
void Pause(InputPlayback *data, gshort aPaused);
void mseek (InputPlayback * playback, gulong time);
void ShowFileInfoBox(const gchar* aFilename);
Tuple* GetSongTuple(const gchar* aFilename);
gint CanPlayFileFromVFS(const gchar* aFilename, VFSFile *VFSFile);

static const gchar *fmts[] =
    { "amf", "ams", "dbm", "dbf", "dsm", "far", "mdl", "stm", "ult", "mt2",
      "mod", "s3m", "dmf", "umx", "it", "669", "xm", "mtm", "psm", "ft2",
      NULL };

InputPlugin gModPlug =
{
    .description = "ModPlug Audio Plugin",
    .init = Init,
    .about = ShowAboutBox,
    .configure = ShowConfigureBox,
    .play_file = PlayFile,
    .stop = Stop,
    .pause = Pause,
    .mseek = mseek,
    .file_info_box = ShowFileInfoBox,
    .get_song_tuple = GetSongTuple,
    .is_our_file_from_vfs = CanPlayFileFromVFS,
    .vfs_extensions = (gchar **)fmts,
    .have_subtune = TRUE,   // to exclude .zip etc which doesn't contain any mod file --yaz
};

InputPlugin *modplug_iplist[] = { &gModPlug, NULL };

SIMPLE_INPUT_PLUGIN(modplug, modplug_iplist);
