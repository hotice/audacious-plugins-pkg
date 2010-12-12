/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
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
#include <FLAC/all.h>
#include <glib.h>

#include <audacious/debug.h>

#include "flacng.h"
#include "tools.h"
#include "seekable_stream_callbacks.h"

FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
    callback_info* info = (callback_info*) client_data;
    size_t read;

    if (info->fd == NULL)
    {
        ERROR("Trying to read data from an uninitialized file!\n");
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    if (*bytes == 0)
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

    read = vfs_fread(buffer, 1, *bytes, info->fd);
    *bytes = read;

    switch (read)
    {
        case -1:
            ERROR("Error while reading from stream!\n");
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

        case 0:
            AUDDBG("Stream reached EOF\n");
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

        default:
            return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
}

FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 offset, void *client_data)
{
    callback_info *info = (callback_info*) client_data;

    if (vfs_fseek(info->fd, offset, SEEK_SET) != 0)
    {
        ERROR("Could not seek to %lld!\n", (long long)offset);
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }

    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *offset, void *client_data)
{
    callback_info *info = (callback_info*) client_data;

    if ((*offset = vfs_ftell(info->fd)) == -1)
    {
        ERROR("Could not tell current position!\n");
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    }

    AUDDBG ("Current position: %d\n", (gint) * offset);

    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data)
{
    callback_info *info = (callback_info*) client_data;
    return vfs_feof(info->fd);
}

FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *length, void *client_data)
{
    callback_info *info = (callback_info*) client_data;

    if ((*length = vfs_fsize(info->fd)) == -1)
    {
        /*
         * Could not get the stream size. This is not necessarily an
         * error, maybe the stream has no fixed size (think streaming
         * audio)
         */
        AUDDBG("Stream length is unknown.\n");
        *length = 0;
        return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
    }

    AUDDBG ("Stream length is %d bytes\n", (gint) * length);

    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
    glong sample;
    gshort channel;
    callback_info *info = (callback_info*) client_data;

    /*
     * Check if there is more data decoded than we have space
     * for. This _should_ not happen given how our buffer is sized,
     * but you never know.
     */
    if (info->buffer_free < (frame->header.blocksize * frame->header.channels))
    {
        ERROR("BUG! Too much data decoded from stream!\n");
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    if (frame->header.bits_per_sample != 8  &&
        frame->header.bits_per_sample != 16 &&
        frame->header.bits_per_sample != 24 &&
        frame->header.bits_per_sample != 32)
    {
        ERROR("Unsupported bitrate found in stream: %d!\n", frame->header.bits_per_sample);
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    /*
     * Copy the frame metadata, will be compared to stream
     * metadata later
     * This also describes the format of the current buffer content.
     */
    info->frame.channels = frame->header.channels;
    info->frame.samplerate = frame->header.sample_rate;
    info->frame.bits_per_sample = frame->header.bits_per_sample;

    for (sample = 0; sample < frame->header.blocksize; sample++)
    {
        for (channel = 0; channel < frame->header.channels; channel++)
        {
            *(info->write_pointer++) = buffer[channel][sample];
            info->buffer_free -= 1;
            info->buffer_used += 1;
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
    ERROR("FLAC decoder error callback was called: %d\n", status);
}

void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
    callback_info *info = (callback_info*) client_data;
    gint i;
    FLAC__StreamMetadata *metadata_copy;
    FLAC__StreamMetadata_VorbisComment_Entry *entry;
    gchar *key;
    gchar *value;
    gsize size;

    switch (metadata->type)
    {
        case FLAC__METADATA_TYPE_STREAMINFO:
            /* Basic stream information. Sample rate, channels and stuff  */
            AUDDBG("FLAC__METADATA_TYPE_STREAMINFO found.\n");

            info->stream.samples = metadata->data.stream_info.total_samples;
            AUDDBG("total_samples=%lld\n", (long long) metadata->data.stream_info.total_samples);

            info->stream.bits_per_sample = metadata->data.stream_info.bits_per_sample;
            AUDDBG("bits_per_sample=%d\n", metadata->data.stream_info.bits_per_sample);

            info->stream.channels = metadata->data.stream_info.channels;
            AUDDBG("channels=%d\n", metadata->data.stream_info.channels);

            info->stream.samplerate = metadata->data.stream_info.sample_rate;
            AUDDBG("sample_rate=%d\n", metadata->data.stream_info.sample_rate);

            size = vfs_fsize(info->fd);

            if (size == -1 || info->stream.samples == 0)
                info->bitrate = 0;
            else
                info->bitrate = 8 * size * (gint64) info->stream.samplerate / info->stream.samples;

            AUDDBG("bitrate=%d\n", info->bitrate);

            info->metadata_changed = TRUE;
            break;

        case FLAC__METADATA_TYPE_VORBIS_COMMENT:
            /*
             * We will possibly need to modify some of the entries
             * in the metadata field, so we make a copy of it
             * first.
             * The original structure must not be modified.
             */
            metadata_copy = FLAC__metadata_object_clone(metadata);

            /* A vorbis type comment field. */
            AUDDBG("FLAC__METADATA_TYPE_VORBIS_COMMENT found.\n");
            AUDDBG("Vorbis comment contains %d fields\n", metadata_copy->data.vorbis_comment.num_comments);
            AUDDBG("Vendor string: %s\n", metadata_copy->data.vorbis_comment.vendor_string.entry);

            /* Enumerate the comment entries */
            entry = metadata_copy->data.vorbis_comment.comments;

            for (i = 0; i < metadata_copy->data.vorbis_comment.num_comments; i++, entry++)
            {
                /*
                 * Try and parse the comment.
                 * If FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair() succeeds,
                 * it allocates memory for the key and value which we have to take
                 * care of.
                 */
                if (FLAC__metadata_object_vorbiscomment_entry_to_name_value_pair(*entry, &key, &value) == false)
                    AUDDBG("Could not parse comment\n");
                else
                {
                    add_comment(info, key, value);
                    g_free(key);
                    g_free(value);
                }
            }

            /* Free our metadata copy */
            FLAC__metadata_object_delete(metadata_copy);

            info->metadata_changed = TRUE;
            break;

        default:
            break;
    }
    return;
}
