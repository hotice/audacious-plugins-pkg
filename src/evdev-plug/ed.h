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

#ifndef _I_ED_H
#define _I_ED_H 1

#include "ed_common.h"
#include <glib.h>
#include <audacious/plugin.h>

void ed_init ( void );
void ed_cleanup ( void );
void ed_config ( void );
void ed_about ( void );

GeneralPlugin ed_gp =
{
    .description = "EvDev-Plug " ED_VERSION_PLUGIN,
    .init = ed_init,
    .about = ed_about,
    .configure = ed_config,	
    .cleanup = ed_cleanup
};

#endif /* !_I_ED_H */
