/*
 * ui_playlist_widget.h
 * Copyright 2011-2012 John Lindgren, William Pitcock, and Michał Lipski
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

#ifndef UI_PLAYLIST_WIDGET_H
#define UI_PLAYLIST_WIDGET_H

#include <gtk/gtk.h>

GtkWidget * ui_playlist_widget_new (gint playlist);
gint ui_playlist_widget_get_playlist (GtkWidget * widget);
void ui_playlist_widget_set_playlist (GtkWidget * widget, gint playlist);
void ui_playlist_widget_update (GtkWidget * widget, gint type, gint at,
 gint count);
void ui_playlist_widget_scroll (GtkWidget * widget);

enum {PW_COL_NUMBER, PW_COL_TITLE, PW_COL_ARTIST, PW_COL_YEAR, PW_COL_ALBUM,
 PW_COL_TRACK, PW_COL_GENRE, PW_COL_QUEUED, PW_COL_LENGTH, PW_COL_PATH,
 PW_COL_FILENAME, PW_COL_CUSTOM, PW_COL_BITRATE, PW_COLS};

extern const gchar * const pw_col_names[PW_COLS];

extern gint pw_num_cols;
extern gint pw_cols[PW_COLS];

void pw_col_init (void);
void pw_col_choose (void);
void pw_col_save (void);
void pw_col_cleanup (void);

#endif
