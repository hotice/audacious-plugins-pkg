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

#ifndef OSS4_H
#define OSS4_H

#include "config.h"

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#else
#include <soundcard.h>
#endif

#include <audacious/plugin.h>
#include <glib.h>

#define IS_BIG_ENDIAN (G_BYTE_ORDER == G_BIG_ENDIAN)
#define DEFAULT_MIXER "/dev/mixer"

extern OutputPlugin op;

typedef struct {
    gint audio_device;
    gint buffer_size;
    gint prebuffer;
    gboolean save_volume;
    gboolean use_alt_audio_device;
    gchar *alt_audio_device;
} OSSConfig;

extern OSSConfig oss_cfg;
int vol;
void oss_configure(void);
int oss_hardware_present(void);
void oss_describe_error();
void oss_get_volume(int *l, int *r);
void oss_set_volume(int l, int r);

int oss_playing(void);
int oss_free(void);
void oss_write(void *ptr, int length);
void oss_close(void);
void oss_flush(int time);
void oss_pause(short p);
int oss_open(AFormat fmt, int rate, int nch);
int oss_get_output_time(void);
int oss_get_written_time(void);
void oss_set_audio_params(void);

void oss_free_convert_buffer(void);
int (*oss_get_convert_func(int output, int input)) (void **, int);
int (*oss_get_stereo_convert_func(int output, int input)) (void **, int,
                                                           int);

void oss_tell(AFormat * fmt, gint * rate, gint * nch);

#endif
