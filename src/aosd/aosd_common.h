/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2007
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#ifndef _I_AOSD_COMMON_H
#define _I_AOSD_COMMON_H 1

#ifdef DEBUG
#include <stdio.h>
#define DEBUGMSG(...) { fprintf(stderr, "statusicon(%s:%s:%d): ", __FILE__, __FUNCTION__, (int) __LINE__); fprintf(stderr, __VA_ARGS__); }
#else
#define DEBUGMSG(...)
#endif /* DEBUG */

#include "../../config.h"

#define AOSD_VERSION_PLUGIN "0.1beta5"

#endif /* !_I_AOSD_COMMON_H */
