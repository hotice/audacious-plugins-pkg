/*
 * UNIX Transport Plugin for Audacious
 * Copyright 2009-2011 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>

#include <audacious/plugin.h>
#include <libaudcore/audstrings.h>

/* in gtk.c */
void unix_error (const gchar * format, ...);
void unix_about (void);

static void * unix_fopen (const gchar * uri, const gchar * mode)
{
    gboolean update;
    mode_t mode_flag;
    gint handle;

    update = (strchr (mode, '+') != NULL);

    switch (mode[0])
    {
      case 'r':
        mode_flag = update ? O_RDWR : O_RDONLY;
        break;
      case 'w':
        mode_flag = (update ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
        break;
      case 'a':
        mode_flag = (update ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
        break;
      default:
        return NULL;
    }

#ifdef _WIN32
    mode_flag |= O_BINARY;
#endif

    gchar * filename = uri_to_filename (uri);
    if (! filename)
        return NULL;

    if (mode_flag & O_CREAT)
#ifdef S_IRGRP
        handle = open (filename, mode_flag, S_IRUSR | S_IWUSR | S_IRGRP |
         S_IROTH);
#else
        handle = open (filename, mode_flag, S_IRUSR | S_IWUSR);
#endif
    else
        handle = open (filename, mode_flag);

    if (handle < 0)
    {
        unix_error ("Cannot open %s: %s.", filename, strerror (errno));
        g_free (filename);
        return NULL;
    }

#ifdef HAVE_FCNTL
    fcntl (handle, F_SETFD, FD_CLOEXEC);
#endif

    g_free (filename);
    return GINT_TO_POINTER (handle);
}

static gint unix_fclose (VFSFile * file)
{
    gint handle = GPOINTER_TO_INT (vfs_get_handle (file));
    gint result = 0;

#ifdef HAVE_FSYNC
    if (fsync (handle) < 0)
    {
        unix_error ("fsync failed: %s.", strerror (errno));
        result = -1;
    }
#endif

    if (close (handle) < 0)
    {
        unix_error ("close failed: %s.", strerror (errno));
        result = -1;
    }

    return result;
}

static gint64 unix_fread (void * ptr, gint64 size, gint64 nitems, VFSFile * file)
{
    gint handle = GPOINTER_TO_INT (vfs_get_handle (file));
    gint64 goal = size * nitems;
    gint64 total = 0;

    while (total < goal)
    {
        gint64 readed = read (handle, (gchar *) ptr + total, goal - total);

        if (readed < 0)
        {
            unix_error ("read failed: %s.", strerror (errno));
            break;
        }

        if (! readed)
            break;

        total += readed;
    }

    return (size > 0) ? total / size : 0;
}

static gint64 unix_fwrite (const void * ptr, gint64 size, gint64 nitems,
 VFSFile * file)
{
    gint handle = GPOINTER_TO_INT (vfs_get_handle (file));
    gint64 goal = size * nitems;
    gint64 total = 0;

    while (total < goal)
    {
        gint64 written = write (handle, (gchar *) ptr + total, goal - total);

        if (written < 0)
        {
            unix_error ("write failed: %s.", strerror (errno));
            break;
        }

        total += written;
    }

    return (size > 0) ? total / size : 0;
}

static gint unix_fseek (VFSFile * file, gint64 offset, gint whence)
{
    gint handle = GPOINTER_TO_INT (vfs_get_handle (file));

    if (lseek (handle, offset, whence) < 0)
    {
        unix_error ("lseek failed: %s.", strerror (errno));
        return -1;
    }

    return 0;
}

static gint64 unix_ftell (VFSFile * file)
{
    gint handle = GPOINTER_TO_INT (vfs_get_handle (file));
    gint64 result = lseek (handle, 0, SEEK_CUR);

    if (result < 0)
        unix_error ("lseek failed: %s.", strerror (errno));

    return result;
}

static gint unix_getc (VFSFile * file)
{
    guchar c;

    return (unix_fread (& c, 1, 1, file) == 1) ? c : -1;
}

static gint unix_ungetc (gint c, VFSFile * file)
{
    return (! unix_fseek (file, -1, SEEK_CUR)) ? c : -1;
}

static void unix_rewind (VFSFile * file)
{
    unix_fseek (file, 0, SEEK_SET);
}

static gboolean unix_feof (VFSFile * file)
{
    gint test = unix_getc (file);

    if (test < 0)
        return TRUE;

    unix_ungetc (test, file);
    return FALSE;
}

static gint unix_ftruncate (VFSFile * file, gint64 length)
{
    gint handle = GPOINTER_TO_INT (vfs_get_handle (file));

    gint result = ftruncate (handle, length);

    if (result < 0)
        unix_error ("ftruncate failed: %s.", strerror (errno));

    return result;
}

static gint64 unix_fsize (VFSFile * file)
{
    gint64 position, length;

    position = unix_ftell (file);

    if (position < 0)
        return -1;

    unix_fseek (file, 0, SEEK_END);
    length = unix_ftell (file);

    if (length < 0)
        return -1;

    unix_fseek (file, position, SEEK_SET);

    return length;
}

static const gchar * const unix_schemes[] = {"file", NULL};

static VFSConstructor constructor = {
 .vfs_fopen_impl = unix_fopen,
 .vfs_fclose_impl = unix_fclose,
 .vfs_fread_impl = unix_fread,
 .vfs_fwrite_impl = unix_fwrite,
 .vfs_getc_impl = unix_getc,
 .vfs_ungetc_impl = unix_ungetc,
 .vfs_fseek_impl = unix_fseek,
 .vfs_rewind_impl = unix_rewind,
 .vfs_ftell_impl = unix_ftell,
 .vfs_feof_impl = unix_feof,
 .vfs_ftruncate_impl = unix_ftruncate,
 .vfs_fsize_impl = unix_fsize
};

AUD_TRANSPORT_PLUGIN
(
 .name = "File I/O",
 .about = unix_about,
 .schemes = unix_schemes,
 .vtable = & constructor
)
