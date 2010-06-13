/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2008  Audacious development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#ifndef __UI_INFOAREA_H
#define __UI_INFOAREA_H

typedef struct {
    GtkWidget *parent;
    Tuple *tu;

    struct {
        gfloat title;
        gfloat artist;
        gfloat album;
        gfloat artwork;
    } alpha;

    gint fadein_timeout;
    gint fadeout_timeout;
    guint8 visdata[20];

    InputPlayback *playback;
    GdkPixbuf *pb;
} UIInfoArea;

extern UIInfoArea *ui_infoarea_new(void);

#endif
