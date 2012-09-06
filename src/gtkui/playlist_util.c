/*
 * playlist_util.c
 * Copyright 2010-2011 Michał Lipski and John Lindgren
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

#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/playlist.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/list.h>

#include "playlist_util.h"
#include "ui_playlist_notebook.h"

GtkWidget * playlist_get_treeview (gint playlist)
{
    GtkWidget *page = gtk_notebook_get_nth_page(UI_PLAYLIST_NOTEBOOK, playlist);

    if (!page)
        return NULL;

    return g_object_get_data ((GObject *) page, "treeview");
}

gint playlist_count_selected_in_range (gint list, gint top, gint length)
{
    gint selected = 0;
    gint count;

    for (count = 0; count < length; count ++)
    {
        if (aud_playlist_entry_get_selected (list, top + count))
            selected ++;
    }

    return selected;
}

gint playlist_get_focus (gint list)
{
    GtkWidget * tree = playlist_get_treeview (list);
    g_return_val_if_fail (tree, -1);

    return audgui_list_get_focus (tree);
}

void playlist_song_info (void)
{
    gint list = aud_playlist_get_active ();
    gint focus = playlist_get_focus (list);

    if (focus < 0)
        return;

    audgui_infowin_show (list, focus);
}

void playlist_queue_toggle (void)
{
    gint list = aud_playlist_get_active ();
    gint focus = playlist_get_focus (list);

    if (focus < 0)
        return;

    gint at = aud_playlist_queue_find_entry (list, focus);

    if (at < 0)
        aud_playlist_queue_insert (list, -1, focus);
    else
        aud_playlist_queue_delete (list, at, 1);
}

void playlist_delete_selected (void)
{
    gint list = aud_playlist_get_active ();
    gint focus = playlist_get_focus (list);
    focus -= playlist_count_selected_in_range (list, 0, focus);

    aud_drct_pl_delete_selected (list);

    if (aud_playlist_selected_count (list)) /* song changed? */
        return;

    if (focus == aud_playlist_entry_count (list))
        focus --;
    if (focus >= 0)
    {
        aud_playlist_entry_set_selected (list, focus, TRUE);
        playlist_set_focus (list, focus);
    }
}

void playlist_copy (void)
{
    gchar * text = audgui_urilist_create_from_selected (aud_playlist_get_active ());
    if (! text)
        return;

    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), text, -1);
    g_free (text);
}

void playlist_cut (void)
{
    playlist_copy ();
    playlist_delete_selected ();
}

void playlist_paste (void)
{
    gchar * text = gtk_clipboard_wait_for_text (gtk_clipboard_get
     (GDK_SELECTION_CLIPBOARD));
    if (! text)
        return;

    gint list = aud_playlist_get_active ();
    audgui_urilist_insert (list, playlist_get_focus (list), text);
    g_free (text);
}

void playlist_shift (gint offset)
{
    gint list = aud_playlist_get_active ();
    gint focus = playlist_get_focus (list);

    if (focus < 0 || ! aud_playlist_entry_get_selected (list, focus))
        return;

    focus += aud_playlist_shift (list, focus, offset);
    playlist_set_focus (list, focus);
}
