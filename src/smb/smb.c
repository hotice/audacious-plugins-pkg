/*  Audacious
 *  Copyright (c) 2007 Daniel Bradshaw
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

#include "config.h"
#include <audacious/plugin.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libsmbclient.h>

static void smb_auth_fn(const char *srv,
                        const char *shr,
                        char *wg, int wglen,
                        char *un, int unlen,
                        char *pw, int pwlen)
{
  /* Does absolutely nothing :P */
}

typedef struct _SMBFile {
	int fd;
	long length;
} SMBFile;

/* TODO: make writing work. */
VFSFile *smb_vfs_fopen_impl(const gchar * path, const gchar * mode)
{
  VFSFile *file;
  SMBFile *handle;
  struct stat st;

  if (!path || !mode)
    return NULL;

  file = g_new0(VFSFile, 1);
  handle = g_new0(SMBFile, 1);
  handle->fd = smbc_open(path, O_RDONLY, 0);
		
  if (handle->fd < 0) {
    g_free(file);
    file = NULL;
  } else {
    smbc_stat(path,&st);
    handle->length = st.st_size;
    file->handle = (void *)handle;
  }
		
  return file;
}

gint smb_vfs_fclose_impl(VFSFile * file)
{
  gint ret = 0;
  SMBFile *handle;

  if (file == NULL)
    return -1;
	
  if (file->handle)
  {
    handle = (SMBFile *)file->handle;
    if (smbc_close(handle->fd) != 0)
      ret = -1;
    g_free(file->handle);
  }

  return ret;
}

size_t smb_vfs_fread_impl(gpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
  SMBFile *handle;
  if (file == NULL)
    return 0;
  handle = (SMBFile *)file->handle;
  return smbc_read(handle->fd, ptr, size * nmemb);
}

size_t smb_vfs_fwrite_impl(gconstpointer ptr, size_t size, size_t nmemb, VFSFile * file)
{
  return 0;
}

gint smb_vfs_getc_impl(VFSFile *file)
{
  SMBFile *handle;
  char temp;
  handle = (SMBFile *)file->handle;
  smbc_read(handle->fd, &temp, 1);
  return (gint) temp;
}

gint smb_vfs_fseek_impl(VFSFile * file, glong offset, gint whence)
{
  SMBFile *handle;
  glong roffset = offset;
  gint ret = 0;
  if (file == NULL)
     return 0;
	
  handle = (SMBFile *)file->handle;
	
  if (whence == SEEK_END)
  {
    roffset = handle->length + offset;
    if (roffset < 0)
      roffset = 0;

    ret = smbc_lseek(handle->fd, roffset, SEEK_SET);
    //printf("%ld -> %ld = %d\n",offset, roffset, ret);
  }
  else
  {
    if (roffset < 0)
      roffset = 0;
    ret = smbc_lseek(handle->fd, roffset, whence);
    //printf("%ld %d = %d\n",roffset, whence, ret);
  }
	
  return ret;
}

gint smb_vfs_ungetc_impl(gint c, VFSFile *file)
{
  smb_vfs_fseek_impl(file, -1, SEEK_CUR);
  return c;
}

void smb_vfs_rewind_impl(VFSFile * file)
{
  smb_vfs_fseek_impl(file, 0, SEEK_SET);
}

glong
smb_vfs_ftell_impl(VFSFile * file)
{
  SMBFile *handle;
  handle = (SMBFile *)file->handle;
  return smbc_lseek(handle->fd, 0, SEEK_CUR);
}

gboolean
smb_vfs_feof_impl(VFSFile * file)
{
  SMBFile *handle;
  off_t at;

  at = smb_vfs_ftell_impl(file);

  //printf("%d %d %ld %ld\n",sizeof(int), sizeof(off_t), at, handle->length);
  return (gboolean) (at == handle->length) ? TRUE : FALSE;
}

gint
smb_vfs_truncate_impl(VFSFile * file, glong size)
{
  return -1;
}

off_t
smb_vfs_fsize_impl(VFSFile * file)
{
    SMBFile *handle = (SMBFile *)file->handle;

    return handle->length;
}

VFSConstructor smb_const = {
	"smb://",
	smb_vfs_fopen_impl,
	smb_vfs_fclose_impl,
	smb_vfs_fread_impl,
	smb_vfs_fwrite_impl,
	smb_vfs_getc_impl,
	smb_vfs_ungetc_impl,
	smb_vfs_fseek_impl,
	smb_vfs_rewind_impl,
	smb_vfs_ftell_impl,
	smb_vfs_feof_impl,
	smb_vfs_truncate_impl,
	smb_vfs_fsize_impl
};

static void init(void)
{
	int err;

	err = smbc_init(smb_auth_fn, 1);
	if (err < 0)
	{
		g_message("[smb] not starting samba support due to error code %d", err);
		return;
	}

	vfs_register_transport(&smb_const);
}

static void cleanup(void)
{
#if 0
	vfs_unregister_transport(&smb_const);
#endif
}

LowlevelPlugin llp_smb = {
	NULL,
	NULL,
	"smb:// URI Transport",
	init,
	cleanup,
};

LowlevelPlugin *get_lplugin_info(void)
{
        return &llp_smb;
}

