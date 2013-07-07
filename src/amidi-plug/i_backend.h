/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
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

#ifndef _I_BACKEND_H
#define _I_BACKEND_H 1

#include <gmodule.h>
#include <libaudcore/core.h>

struct amidiplug_cfg_backend_s;
struct midievent_s;

typedef struct
{
    char * desc;
    char * filename;
    char * longname;
    char * name;
    int ppos;
}
amidiplug_sequencer_backend_name_t;

typedef struct
{
    GModule * gmodule;
    int (*init) (struct amidiplug_cfg_backend_s *);
    int (*cleanup) (void);
    int (*audio_info_get) (int *, int *, int *);
    int (*audio_volume_get) (int *, int *);
    int (*audio_volume_set) (int, int);
    int (* seq_start) (const char * filename);
    int (*seq_stop) (void);
    int (*seq_on) (void);
    int (*seq_off) (void);
    int (*seq_queue_tempo) (int, int);
    int (*seq_queue_start) (void);
    int (*seq_queue_stop) (void);
    int (*seq_event_init) (void);
    int (*seq_event_noteon) (struct midievent_s *);
    int (*seq_event_noteoff) (struct midievent_s *);
    int (*seq_event_allnoteoff) (int);
    int (*seq_event_keypress) (struct midievent_s *);
    int (*seq_event_controller) (struct midievent_s *);
    int (*seq_event_pgmchange) (struct midievent_s *);
    int (*seq_event_chanpress) (struct midievent_s *);
    int (*seq_event_pitchbend) (struct midievent_s *);
    int (*seq_event_sysex) (struct midievent_s *);
    int (*seq_event_tempo) (struct midievent_s *);
    int (*seq_event_other) (struct midievent_s *);
    int (*seq_output) (void * *, int *);
    int (*seq_output_shut) (unsigned, int);
    int (*seq_get_port_count) (void);
    bool_t autonomous_audio;
}
amidiplug_sequencer_backend_t;

GSList * i_backend_list_lookup (void);
void i_backend_list_free (GSList *);
amidiplug_sequencer_backend_t * i_backend_load (const char * module_name);
void i_backend_unload (amidiplug_sequencer_backend_t * backend);

#endif /* !_I_BACKEND_H */
