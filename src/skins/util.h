/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2009  Audacious development team
 *
 *  Based on BMP:
 *  Copyright (C) 2003-2004  BMP development team
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team
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

#ifndef UTIL_H
#define UTIL_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS
#include "audacious/plugin.h"
typedef gboolean(*DirForeachFunc) (const gchar *path, const gchar *basename,
                                   gpointer user_data);

gchar * find_file_case (const gchar * folder, const gchar * basename);
gchar * find_file_case_path (const gchar * folder, const gchar * basename);
gchar * find_file_case_uri (const gchar * folder, const gchar * basename);

gchar * load_text_file (const gchar * filename);
gchar * text_parse_line (gchar * text);

void del_directory(const gchar *dirname);
gboolean dir_foreach(const gchar *path, DirForeachFunc function,
                     gpointer user_data, GError **error);


INIFile *open_ini_file(const gchar *filename);
void close_ini_file(INIFile *key_file);
gchar *read_ini_string(INIFile *key_file, const gchar *section,
                       const gchar *key);
GArray *read_ini_array(INIFile *key_file, const gchar *section,
                       const gchar *key);

GArray *string_to_garray(const gchar *str);

gboolean text_get_extents(const gchar *fontname, const gchar *text, gint *width,
                          gint *height, gint *ascent, gint *descent);

gboolean file_is_archive(const gchar *filename);
gchar *archive_decompress(const gchar *path);
gchar *archive_basename(const gchar *path);

guint gint_count_digits(gint n);


GtkWidget *make_filebrowser(const gchar *title, gboolean save);

GdkPixbuf *audacious_create_colorized_pixbuf(GdkPixbuf *src, gint red,
                                             gint green, gint blue);

void resize_window(GtkWidget *window, gint width, gint height);
gboolean widget_really_drawable (GtkWidget * widget);
void widget_destroy_on_escape (GtkWidget * widget);

void check_set (GtkActionGroup * action_group, const gchar * action_name,
 gboolean is_on);
void check_button_toggled (GtkToggleButton * button, void * data);

G_END_DECLS
#endif
