/************************************************************************
 * libnotify-aosd_common.h						*
 *									*
 * Copyright (C) 2010 Maximilian Bogner	<max@mbogner.de>		*
 *									*
 * This program is free software; you can redistribute it and/or modify	*
 * it under the terms of the GNU General Public License as published by	*
 * the Free Software Foundation; either version 3 of the License,	*
 * or (at your option) any later version.				*
 *									*
 * This program is distributed in the hope that it will be useful,	*
 * but WITHOUT ANY WARRANTY; without even the implied warranty of	*
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the		*
 * GNU General Public License for more details.				*
 *									*
 * You should have received a copy of the GNU General Public License	*
 * along with this program; if not, see <http://www.gnu.org/licenses/>.	*
 ************************************************************************/

#define PLUGIN_NAME		"libnotify-aosd"
#define PLUGIN_VERSION		PACKAGE_VERSION
#define PLUGIN_AUTHORS		"Maximilian Bogner <max@mbogner.de>"
#define PLUGIN_COPYRIGHT	"Copyright (C) 2010 Maximilian Bogner"
#define PLUGIN_WEBSITE		"http://www.mbogner.de/projects/libnotify-aosd/"
#define PLUGIN_LICENSE		"This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.\n\nThis program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.\n\nYou should have received a copy of the GNU General Public License along with this program; if not, see <http://www.gnu.org/licenses/>."

#ifdef DEBUG
	#define DEBUG_PRINT(...)		{ g_print(__VA_ARGS__); }
#else
	#define DEBUG_PRINT(...)		{ }
#endif

#include "libnotify-aosd.h"
#include "libnotify-aosd_event.h"
#include "libnotify-aosd_osd.h"
