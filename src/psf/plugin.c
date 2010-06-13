/*
	Audio Overload SDK - main driver.  for demonstration only, not user friendly!

	Copyright (c) 2007-2008 R. Belmont and Richard Bannister.

	All rights reserved.

	Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
	* Neither the names of R. Belmont and Richard Bannister nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
	CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <audacious/plugin.h>

#include "ao.h"
#include "corlett.h"
#include "eng_protos.h"

typedef enum {
    ENG_NONE = 0,
    ENG_PSF1,
    ENG_PSF2,
    ENG_SPX,
    ENG_COUNT
} PSFEngine;

typedef struct {
    int32 (*start)(uint8 *buffer, uint32 length);
    int32 (*stop)(void);
    int32 (*seek)(uint32);
    int32 (*execute)(InputPlayback *data);
} PSFEngineFunctors;

static PSFEngineFunctors psf_functor_map[ENG_COUNT] = {
    {NULL, NULL, NULL, NULL},
    {psf_start, psf_stop, psf_seek, psf_execute},
    {psf2_start, psf2_stop, psf2_seek, psf2_execute},
    {spx_start, spx_stop, psf_seek, spx_execute},
};

static PSFEngine psf_probe(uint8 *buffer)
{
	if (!memcmp(buffer, "PSF\x01", 4))
		return ENG_PSF1;

	if (!memcmp(buffer, "PSF\x02", 4))
		return ENG_PSF2;

	if (!memcmp(buffer, "SPU", 3))
		return ENG_SPX;

	if (!memcmp(buffer, "SPX", 3))
		return ENG_SPX;

	return ENG_NONE;
}

/* ao_get_lib: called to load secondary files */
static gchar *path;

int ao_get_lib(char *filename, uint8 **buffer, uint64 *length)
{
	guchar *filebuf;
	gint64 size;
	char buf[PATH_MAX];
	gchar path2[PATH_MAX];

	g_strlcpy(path2, path, PATH_MAX);
	snprintf(buf, PATH_MAX, "%s/%s", dirname(path2), filename);

	aud_vfs_file_get_contents (buf, & filebuf, & size);

	*buffer = filebuf;
	*length = (uint64)size;

	return AO_SUCCESS;
}

static gint seek = 0;

Tuple *psf2_tuple(const gchar *filename)
{
	Tuple *t;
	corlett_t *c;
	guchar *buf;
	gint64 sz;

	aud_vfs_file_get_contents (filename, & buf, & sz);

	if (!buf)
		return NULL;

	if (corlett_decode(buf, sz, NULL, NULL, &c) != AO_SUCCESS)
		return NULL;

	t = aud_tuple_new_from_filename(filename);

	aud_tuple_associate_int(t, FIELD_LENGTH, NULL, c->inf_length ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1);
	aud_tuple_associate_string(t, FIELD_ARTIST, NULL, c->inf_artist);
	aud_tuple_associate_string(t, FIELD_ALBUM, NULL, c->inf_game);
	aud_tuple_associate_string(t, -1, "game", c->inf_game);
	aud_tuple_associate_string(t, FIELD_TITLE, NULL, c->inf_title);
	aud_tuple_associate_string(t, FIELD_COPYRIGHT, NULL, c->inf_copy);
	aud_tuple_associate_string(t, FIELD_QUALITY, NULL, "sequenced");
	aud_tuple_associate_string(t, FIELD_CODEC, NULL, "PlayStation 1/2 Audio");
	aud_tuple_associate_string(t, -1, "console", "PlayStation 1/2");

	free(c);
	g_free(buf);

	return t;
}

gchar *psf2_title(gchar *filename, gint *length)
{
	gchar *title = NULL;
	Tuple *tuple = psf2_tuple(filename);

	if (tuple != NULL)
	{
		title = aud_tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());
		*length = aud_tuple_get_int(tuple, FIELD_LENGTH, NULL);
		aud_tuple_free(tuple);
	}
	else
	{
		title = g_path_get_basename(filename);
		*length = -1;
	}

	return title;
}

void psf2_play(InputPlayback *data)
{
	guchar *buffer;
	gint64 size;
	gint length;
	gchar *title = psf2_title(data->filename, &length);
	PSFEngine eng;
	PSFEngineFunctors *f;

	path = g_strdup(data->filename);
	aud_vfs_file_get_contents (data->filename, & buffer, & size);

	eng = psf_probe(buffer);
	if (eng == ENG_NONE || eng == ENG_COUNT)
	{
		g_free(buffer);
		return;
	}

	f = &psf_functor_map[eng];
	if (f->start(buffer, size) != AO_SUCCESS)
	{
		g_free(buffer);
		return;
	}

	data->output->open_audio(FMT_S16_NE, 44100, 2);

        data->set_params(data, title, length, 44100*2*2*8, 44100, 2);

	data->playing = TRUE;
	data->set_pb_ready(data);

	for (;;)
	{
		f->execute(data);

		if (seek)
		{
			data->eof = FALSE;
			data->output->flush(seek);

			f->stop();

			if (f->start(buffer, size) == AO_SUCCESS)
			{
				f->seek(seek);
				seek = 0;
				continue;
			}
			else
			{
				data->output->close_audio();
				break;
			}
		}

		f->stop();

		while (data->eof && data->output->buffer_playing())
			g_usleep(10000);

		data->output->close_audio();

		break;
	}

	g_free(buffer);
	g_free(path);
        g_free(title);

	data->playing = FALSE;
}

void psf2_update(unsigned char *buffer, long count, InputPlayback *playback)
{
	if (buffer == NULL)
	{
		playback->playing = FALSE;
		playback->eof = TRUE;

		return;
	}

	playback->output->write_audio (buffer, count);

	if (seek)
	{
		if (psf2_seek(seek))
		{
			playback->output->flush(seek);
			seek = 0;
		}
		else
		{
			playback->eof = TRUE;
			return;
		}
	}
}

void psf2_Stop(InputPlayback *playback)
{
	playback->playing = FALSE;
}

void psf2_pause(InputPlayback *playback, short p)
{
	playback->output->pause(p);
}

int psf2_is_our_fd(const gchar *filename, VFSFile *file)
{
	uint8 magic[4];
	aud_vfs_fread(magic, 1, 4, file);

	return (psf_probe(magic) != ENG_NONE);
}

void psf2_Seek(InputPlayback *playback, int time)
{
	seek = time * 1000;
}

gchar *psf2_fmts[] = { "psf", "minipsf", "psf2", "minipsf2", "spu", "spx", NULL };

InputPlugin psf2_ip =
{
	.description = "OpenPSF PSF1/PSF2 Audio Plugin",
	.play_file = psf2_play,
	.stop = psf2_Stop,
	.pause = psf2_pause,
	.seek = psf2_Seek,
	.get_song_tuple = psf2_tuple,
	.is_our_file_from_vfs = psf2_is_our_fd,
	.vfs_extensions = psf2_fmts,
};

InputPlugin *psf2_iplist[] = { &psf2_ip, NULL };

DECLARE_PLUGIN(psf2, NULL, NULL, psf2_iplist, NULL, NULL, NULL, NULL, NULL);

