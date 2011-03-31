/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

//open()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//mmap()
#include <unistd.h>
#include <sys/mman.h>
#include <cstdlib>

#include "arch_raw.h"

arch_Raw::arch_Raw(const string& aFileName)
{
	mFileDesc = vfs_fopen(aFileName.c_str(), "r");
	if (!mFileDesc)
	{
		mSize = 0;
		return;
	}

	mSize = vfs_fsize(mFileDesc);
	if (mSize <= 0)
	{
		vfs_fclose(mFileDesc);
		mSize = 0;
		return;
	}

	mMap = malloc(mSize);
	if (vfs_fread(mMap, 1, mSize, mFileDesc) < mSize)
	{
		free(mMap);
		vfs_fclose(mFileDesc);
		mSize = 0;
		return;
	}
}

arch_Raw::~arch_Raw()
{
	if(mSize != 0)
	{
		free(mMap);
		vfs_fclose(mFileDesc);
	}
}

bool arch_Raw::ContainsMod(const string& aFileName)
{
	return IsOurFile(aFileName);
}
