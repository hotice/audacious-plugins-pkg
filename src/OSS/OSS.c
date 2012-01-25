/*  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
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

#include "OSS.h"

#include <glib.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include <stdlib.h>
#include <string.h>

OSSConfig oss_cfg;

static void oss_about (void)
{
    static GtkWidget * about_dialog = NULL;

    audgui_simple_message (& about_dialog, GTK_MESSAGE_INFO,
     _("About OSS Driver"),
     "Audacious OSS Driver\n\n "
     "This program is free software; you can redistribute it and/or modify\n"
     "it under the terms of the GNU General Public License as published by\n"
     "the Free Software Foundation; either version 2 of the License, or\n"
     "(at your option) any later version.\n"
     "\n"
     "This program is distributed in the hope that it will be useful,\n"
     "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
     "GNU General Public License for more details.\n"
     "\n"
     "You should have received a copy of the GNU General Public License\n"
     "along with this program; if not, write to the Free Software\n"
     "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,\n"
     "USA.");
}

static const gchar * const oss_defaults[] = {
 "audio_device", "0",
 "mixer_device", "0",
 "prebuffer", "50",
 "use_master", "FALSE",
 "use_alt_audio_device", "FALSE",
 "use_alt_mixer_device", "FALSE",
 NULL};

static gboolean oss_init (void)
{
    aud_config_set_defaults ("OSS", oss_defaults);

    memset(&oss_cfg, 0, sizeof(OSSConfig));

    oss_cfg.audio_device = aud_get_int ("OSS", "audio_device");
    oss_cfg.mixer_device = aud_get_int ("OSS", "mixer_device");
    oss_cfg.prebuffer = aud_get_int ("OSS", "prebuffer");
    oss_cfg.use_master = aud_get_bool ("OSS", "use_master");
    oss_cfg.use_alt_audio_device = aud_get_bool ("OSS", "use_alt_audio_device");
    oss_cfg.alt_audio_device = aud_get_string ("OSS", "alt_audio_device");
    oss_cfg.use_alt_mixer_device = aud_get_bool ("OSS", "use_alt_mixer_device");
    oss_cfg.alt_mixer_device = aud_get_string ("OSS", "alt_mixer_device");

    return TRUE;
}

static void oss_cleanup(void)
{
    if (oss_cfg.alt_audio_device) {
        g_free(oss_cfg.alt_audio_device);
        oss_cfg.alt_audio_device = NULL;
    }

    if (oss_cfg.alt_mixer_device) {
        g_free(oss_cfg.alt_mixer_device);
        oss_cfg.alt_mixer_device = NULL;
    }
}

AUD_OUTPUT_PLUGIN
(
    .name = "OSS 3",
    .probe_priority = 3,
    .init = oss_init,
    .cleanup = oss_cleanup,
    .about = oss_about,
    .configure = oss_configure,
    .get_volume = oss_get_volume,
    .set_volume = oss_set_volume,
    .open_audio = oss_open,
    .write_audio = oss_write,
    .close_audio = oss_close,
    .flush = oss_flush,
    .pause = oss_pause,
    .buffer_free = oss_free,
    .output_time = oss_get_output_time,
    .written_time = oss_get_written_time,
)
