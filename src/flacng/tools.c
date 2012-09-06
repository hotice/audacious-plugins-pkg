/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
 *  Copyright (C) 2010 John Lindgren
 *  Copyright (C) 2010-2012 Michał Lipski
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

#include <string.h>
#include <audacious/debug.h>

#include "flacng.h"

callback_info *init_callback_info(void)
{
    callback_info *info;

    if ((info = malloc (sizeof (callback_info))) == NULL)
    {
        FLACNG_ERROR("Could not allocate memory for callback structure!");
        return NULL;
    }

    memset (info, 0, sizeof (callback_info));

    if ((info->output_buffer = malloc (BUFFER_SIZE_BYTE)) == NULL)
    {
        FLACNG_ERROR("Could not allocate memory for output buffer!");
        return NULL;
    }

    reset_info(info);

    AUDDBG("Playback buffer allocated for %d samples, %d bytes\n", BUFFER_SIZE_SAMP, BUFFER_SIZE_BYTE);

    return info;
}

void clean_callback_info(callback_info *info)
{
    free (info->output_buffer);
    free (info);
}

void reset_info(callback_info *info)
{
    info->buffer_used = 0;
    info->write_pointer = info->output_buffer;
}

bool_t read_metadata(FLAC__StreamDecoder *decoder, callback_info *info)
{
    FLAC__StreamDecoderState ret;

    reset_info(info);

    /* Reset the decoder */
    if (FLAC__stream_decoder_reset(decoder) == false)
    {
        FLACNG_ERROR("Could not reset the decoder!\n");
        return FALSE;
    }

    /* Try to decode the metadata */
    if (FLAC__stream_decoder_process_until_end_of_metadata(decoder) == false)
    {
        ret = FLAC__stream_decoder_get_state(decoder);
        AUDDBG("Could not read the metadata: %s(%d)!\n",
            FLAC__StreamDecoderStateString[ret], ret);
        reset_info(info);
        return FALSE;
    }

    return TRUE;
}
