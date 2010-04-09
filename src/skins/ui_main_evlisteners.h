/*
 * Audacious
 * Copyright (c) 2006-2007 Audacious development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#include <glib.h>

#ifndef AUDACIOUS_UI_MAIN_EVLISTENERS_H
#define AUDACIOUS_UI_MAIN_EVLISTENERS_H

void ui_main_evlistener_init(void);
void ui_main_evlistener_dissociate(void);

void ui_main_evlistener_playback_begin (void * hook_data, void * user_data);
void ui_main_evlistener_playback_pause (void * hook_data, void * user_data);

void start_stop_visual (void);

#endif /* AUDACIOUS_UI_MAIN_EVLISTENERS_H */
