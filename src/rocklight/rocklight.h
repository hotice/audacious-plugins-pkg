/*
 *  Copyright (C) 2004 Benedikt 'Hunz' Heinz <rocklight@hunz.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
 
#ifndef _ROCKLIGHT_H
#define _ROCKLIGHT_H

int thinklight_set(int fd, int state);
int thinklight_get(int fd);

int sysled_set(int fd, int state);
int sysled_get(int fd);

int b43led_set(int fd, int state);
int b43led_get(int fd);

#endif
