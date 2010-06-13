/*  FileWriter-Plugin
 *  (C) copyright 2007 merging of Disk Writer and Out-Lame by Michael Färber
 *
 *  Original Out-Lame-Plugin:
 *  (C) copyright 2002 Lars Siebold <khandha5@gmx.net>
 *  (C) copyright 2006-2007 porting to audacious by Yoshiki Yazawa <yaz@cc.rim.or.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "filewriter.h"
#include "plugins.h"
#include "convert.h"

struct format_info input;

static GtkWidget *configure_win = NULL, *configure_vbox;
static GtkWidget *path_hbox, *path_label, *path_dirbrowser;
static GtkWidget *configure_bbox, *configure_ok, *configure_cancel;

static GtkWidget *fileext_hbox, *fileext_label, *fileext_combo, *plugin_button;

enum fileext_t
{
    WAV = 0,
#ifdef FILEWRITER_MP3
    MP3,
#endif
#ifdef FILEWRITER_VORBIS
    VORBIS,
#endif
#ifdef FILEWRITER_FLAC
    FLAC,
#endif
    FILEEXT_MAX
};

static gint fileext = WAV;
static const gchar *fileext_str[FILEEXT_MAX] =
{
    "wav",
#ifdef FILEWRITER_MP3
    "mp3",
#endif
#ifdef FILEWRITER_VORBIS
    "ogg",
#endif
#ifdef FILEWRITER_FLAC
    "flac"
#endif
};

static FileWriter *plugin;

static GtkWidget *saveplace_hbox, *saveplace;
static gboolean save_original = TRUE;

static GtkWidget *filenamefrom_hbox, *filenamefrom_label, *filenamefrom_toggle;
static gboolean filenamefromtags = TRUE;

static GtkWidget *use_suffix_toggle = NULL;
static gboolean use_suffix = FALSE;

static GtkWidget *prependnumber_toggle;
static gboolean prependnumber = FALSE;

static gchar *file_path = NULL;

VFSFile *output_file = NULL;
Tuple *tuple = NULL;

static gint64 samples_written;

static OutputPluginInitStatus file_init(void);
static void file_about(void);
static gint file_open(AFormat fmt, gint rate, gint nch);
static void file_write(void *ptr, gint length);
static gint file_write_output(void *ptr, gint length);
static void file_close(void);
static void file_flush(gint time);
static void file_pause(short p);
static gint file_playing(void);
static gint file_get_time (void);
static void file_configure(void);

OutputPlugin file_op =
{
    .description = "FileWriter Plugin",
    .probe_priority = 0,
    .init = file_init,
    .about = file_about,
    .configure = file_configure,
    .open_audio = file_open,
    .write_audio = file_write,
    .close_audio = file_close,
    .flush = file_flush,
    .pause = file_pause,
    .buffer_playing = file_playing,
    .output_time = file_get_time,
    .written_time = file_get_time,
};

OutputPlugin *file_oplist[] = { &file_op, NULL };

SIMPLE_OUTPUT_PLUGIN (filewriter, file_oplist)

FileWriter *plugins[FILEEXT_MAX] = {
    &wav_plugin,
#ifdef FILEWRITER_MP3
    &mp3_plugin,
#endif
#ifdef FILEWRITER_VORBIS
    &vorbis_plugin,
#endif
#ifdef FILEWRITER_FLAC
    &flac_plugin,
#endif
};

static void set_plugin(void)
{
    if (fileext < 0 || fileext >= FILEEXT_MAX)
        fileext = 0;

    plugin = plugins[fileext];
}

static OutputPluginInitStatus file_init(void)
{
    ConfigDb *db;

    db = aud_cfg_db_open();
    aud_cfg_db_get_int(db, FILEWRITER_CFGID, "fileext", &fileext);
    aud_cfg_db_get_string(db, FILEWRITER_CFGID, "file_path", &file_path);
    aud_cfg_db_get_bool(db, FILEWRITER_CFGID, "save_original", &save_original);
    aud_cfg_db_get_bool(db, FILEWRITER_CFGID, "use_suffix", &use_suffix);
    aud_cfg_db_get_bool(db, FILEWRITER_CFGID, "filenamefromtags", &filenamefromtags);
    aud_cfg_db_get_bool(db, FILEWRITER_CFGID, "prependnumber", &prependnumber);
    aud_cfg_db_close(db);

    if (file_path == NULL)
    {
        g_return_val_if_fail (getenv ("HOME") != NULL, OUTPUT_PLUGIN_INIT_FAIL);
        file_path = g_filename_to_uri (getenv ("HOME"), NULL, NULL);
        g_return_val_if_fail (file_path != NULL, OUTPUT_PLUGIN_INIT_FAIL);
    }

    set_plugin();
    if (plugin->init)
        plugin->init(&file_write_output);

    return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
}

void file_about (void)
{
    static GtkWidget * dialog;

    audgui_simple_message (& dialog, GTK_MESSAGE_INFO,
     _("About FileWriter-Plugin"),
     _("FileWriter-Plugin\n\n"
     "This program is free software; you can redistribute it and/or modify\n"
     "it under the terms of the GNU General Public License as published by\n"
     "the Free Software Foundation; either version 2 of the License, or\n"
     "(at your option) any later version.\n"
     "\n"
     "This program is distributed in the hope that it will be useful,\n"
     "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
     "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
     "GNU General Public License for more details.\n"
     "\n"
     "You should have received a copy of the GNU General Public License\n"
     "along with this program; if not, write to the Free Software\n"
     "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,\n"
     "USA."));
}

static VFSFile * safe_create (const gchar * filename)
{
    if (! aud_vfs_file_test (filename, G_FILE_TEST_EXISTS))
        return aud_vfs_fopen (filename, "w");

    const gchar * extension = strrchr (filename, '.');
    gint length = strlen (filename);
    gchar scratch[length + 3];
    gint count;

    for (count = 1; count < 100; count ++)
    {
        if (extension == NULL)
            sprintf (scratch, "%s-%d", filename, count);
        else
            sprintf (scratch, "%.*s-%d%s", (gint) (extension - filename),
             filename, count, extension);

        if (! aud_vfs_file_test (scratch, G_FILE_TEST_EXISTS))
            return aud_vfs_fopen (scratch, "w");
    }

    return NULL;
}

static gint file_open(AFormat fmt, gint rate, gint nch)
{
    gchar *filename = NULL, *temp = NULL;
    gchar * directory;
    gint pos;
    gint rv;
    gint playlist;

    input.format = fmt;
    input.frequency = rate;
    input.channels = nch;

    playlist = aud_playlist_get_playing ();
    if (playlist < 0)
        return 0;

    pos = aud_playlist_get_position(playlist);
    tuple = (Tuple*) aud_playlist_entry_get_tuple(playlist, pos);
    if (tuple == NULL)
        return 0;

    if (filenamefromtags)
    {
        gchar *utf8 = aud_tuple_formatter_make_title_string(tuple, aud_get_gentitle_format());

        string_replace_char (utf8, '/', ' ');
        filename = string_encode_percent (utf8, FALSE);
        g_free (utf8);
    }
    else
    {
        const gchar * original = strrchr (aud_playlist_entry_get_filename
         (playlist, pos), '/');

        g_return_val_if_fail (original != NULL, 0);
        filename = g_strdup (original + 1);

        if (!use_suffix)
            if ((temp = strrchr(filename, '.')) != NULL)
                *temp = '\0';
    }

    if (prependnumber)
    {
        gint number = aud_tuple_get_int(tuple, FIELD_TRACK_NUMBER, NULL);
        if (!tuple || !number)
            number = pos + 1;

        temp = g_strdup_printf ("%d%%20%s", number, filename);
        g_free(filename);
        filename = temp;
    }

    if (save_original)
    {
        directory = g_strdup (aud_playlist_entry_get_filename (playlist, pos));
        temp = strrchr (directory, '/');
        g_return_val_if_fail (temp != NULL, 0);
        temp[1] = 0;
    }
    else
    {
        g_return_val_if_fail (file_path[0], 0);
        if (file_path[strlen (file_path) - 1] == '/')
            directory = g_strdup (file_path);
        else
            directory = g_strdup_printf ("%s/", file_path);
    }

    temp = g_strdup_printf ("%s%s.%s", directory, filename, fileext_str[fileext]);
    g_free (directory);
    g_free (filename);
    filename = temp;

    output_file = safe_create (filename);
    g_free (filename);

    if (output_file == NULL)
        return 0;

    convert_init(fmt, plugin->format_required, nch);

    rv = (plugin->open)();

    samples_written = 0;

    return rv;
}

static void file_write(void *ptr, gint length)
{
    int len = convert_process (ptr, length);

    plugin->write(convert_output, len);

    samples_written += length / FMT_SIZEOF (input.format);
}

static gint file_write_output(void *ptr, gint length)
{
    return aud_vfs_fwrite(ptr, 1, length, output_file);
}

static void file_close(void)
{
    plugin->close();
    convert_free();

    if (output_file != NULL)
        aud_vfs_fclose(output_file);
    output_file = NULL;
}

static void file_flush(gint time)
{
    file_close();
    file_open(input.format, input.frequency, input.channels);

    samples_written = time * (gint64) input.channels * input.frequency / 1000;
}

static void file_pause(short p)
{
}

static gint file_playing(void)
{
    return 0;
}

static gint file_get_time (void)
{
    return samples_written * 1000 / (input.channels * input.frequency);
}

static void configure_ok_cb(gpointer data)
{
    ConfigDb *db;

    fileext = gtk_combo_box_get_active(GTK_COMBO_BOX(fileext_combo));

    g_free (file_path);
    file_path = gtk_file_chooser_get_uri ((GtkFileChooser *) path_dirbrowser);

    use_suffix =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(use_suffix_toggle));

    prependnumber =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prependnumber_toggle));

    db = aud_cfg_db_open();
    aud_cfg_db_set_int(db, FILEWRITER_CFGID, "fileext", fileext);
    aud_cfg_db_set_string(db, FILEWRITER_CFGID, "file_path", file_path);
    aud_cfg_db_set_bool(db, FILEWRITER_CFGID, "save_original", save_original);
    aud_cfg_db_set_bool(db, FILEWRITER_CFGID, "filenamefromtags", filenamefromtags);
    aud_cfg_db_set_bool(db, FILEWRITER_CFGID, "use_suffix", use_suffix);
    aud_cfg_db_set_bool(db, FILEWRITER_CFGID, "prependnumber", prependnumber);

    aud_cfg_db_close(db);

    gtk_widget_destroy(configure_win);
    if (path_dirbrowser)
        gtk_widget_destroy(path_dirbrowser);
}

static void fileext_cb(GtkWidget *combo, gpointer data)
{
    fileext = gtk_combo_box_get_active(GTK_COMBO_BOX(fileext_combo));
    set_plugin();
    if (plugin->init)
        plugin->init(&file_write_output);

    gtk_widget_set_sensitive(plugin_button, plugin->configure != NULL);
}

static void plugin_configure_cb(GtkWidget *button, gpointer data)
{
    if (plugin->configure)
        plugin->configure();
}


static void saveplace_original_cb(GtkWidget *button, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(path_hbox, FALSE);
        save_original = TRUE;
    }
}

static void saveplace_custom_cb(GtkWidget *button, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(path_hbox, TRUE);
        save_original = FALSE;
    }
}

static void filenamefromtags_cb(GtkWidget *button, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(use_suffix_toggle, FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_suffix_toggle), FALSE);
        use_suffix = FALSE;
        filenamefromtags = TRUE;
    }
}

static void filenamefromfilename_cb(GtkWidget *button, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        gtk_widget_set_sensitive(use_suffix_toggle, TRUE);
        filenamefromtags = FALSE;
    }
}


static void configure_destroy(void)
{
    if (path_dirbrowser)
        gtk_widget_destroy(path_dirbrowser);
}

static void file_configure(void)
{
    GtkTooltips *use_suffix_tooltips;

    if (!configure_win)
    {
        configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_type_hint(GTK_WINDOW(configure_win), GDK_WINDOW_TYPE_HINT_DIALOG);

        gtk_signal_connect(GTK_OBJECT(configure_win), "destroy",
                           GTK_SIGNAL_FUNC(configure_destroy), NULL);
        gtk_signal_connect(GTK_OBJECT(configure_win), "destroy",
                           GTK_SIGNAL_FUNC(gtk_widget_destroyed),
                           &configure_win);

        gtk_window_set_title(GTK_WINDOW(configure_win),
                             _("File Writer Configuration"));
        gtk_window_set_position(GTK_WINDOW(configure_win), GTK_WIN_POS_MOUSE);

        gtk_container_set_border_width(GTK_CONTAINER(configure_win), 10);

        configure_vbox = gtk_vbox_new(FALSE, 10);
        gtk_container_add(GTK_CONTAINER(configure_win), configure_vbox);


        fileext_hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), fileext_hbox, FALSE, FALSE, 0);

        fileext_label = gtk_label_new(_("Output file format:"));
        gtk_box_pack_start(GTK_BOX(fileext_hbox), fileext_label, FALSE, FALSE, 0);

        fileext_combo = gtk_combo_box_new_text();
        gtk_combo_box_append_text(GTK_COMBO_BOX(fileext_combo), "WAV");
#ifdef FILEWRITER_MP3
        gtk_combo_box_append_text(GTK_COMBO_BOX(fileext_combo), "MP3");
#endif
#ifdef FILEWRITER_VORBIS
        gtk_combo_box_append_text(GTK_COMBO_BOX(fileext_combo), "Vorbis");
#endif
#ifdef FILEWRITER_FLAC
        gtk_combo_box_append_text(GTK_COMBO_BOX(fileext_combo), "FLAC");
#endif
        gtk_box_pack_start(GTK_BOX(fileext_hbox), fileext_combo, FALSE, FALSE, 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(fileext_combo), fileext);
        g_signal_connect(G_OBJECT(fileext_combo), "changed", G_CALLBACK(fileext_cb), NULL);

        plugin_button = gtk_button_new_with_label(_("Configure"));
        gtk_widget_set_sensitive(plugin_button, plugin->configure != NULL);
        g_signal_connect(G_OBJECT(plugin_button), "clicked", G_CALLBACK(plugin_configure_cb), NULL);
        gtk_box_pack_end(GTK_BOX(fileext_hbox), plugin_button, FALSE, FALSE, 0);




        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);



        saveplace_hbox = gtk_hbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(configure_vbox), saveplace_hbox);

        saveplace = gtk_radio_button_new_with_label(NULL, _("Save into original directory"));
        g_signal_connect(G_OBJECT(saveplace), "toggled", G_CALLBACK(saveplace_original_cb), NULL);
        gtk_box_pack_start(GTK_BOX(saveplace_hbox), saveplace, FALSE, FALSE, 0);

        saveplace = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(saveplace),
                                                                _("Save into custom directory"));
        g_signal_connect(G_OBJECT(saveplace), "toggled", G_CALLBACK(saveplace_custom_cb), NULL);
        gtk_box_pack_start(GTK_BOX(saveplace_hbox), saveplace, FALSE, FALSE, 0);

        if (!save_original)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(saveplace), TRUE);



        path_hbox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), path_hbox, FALSE, FALSE, 0);

        path_label = gtk_label_new(_("Output file folder:"));
        gtk_box_pack_start(GTK_BOX(path_hbox), path_label, FALSE, FALSE, 0);

        path_dirbrowser =
            gtk_file_chooser_button_new (_("Pick a folder"),
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        gtk_file_chooser_set_uri ((GtkFileChooser *) path_dirbrowser, file_path);
        gtk_box_pack_start(GTK_BOX(path_hbox), path_dirbrowser, TRUE, TRUE, 0);

        if (save_original)
            gtk_widget_set_sensitive(path_hbox, FALSE);




        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);




        filenamefrom_hbox = gtk_hbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(configure_vbox), filenamefrom_hbox);

        filenamefrom_label = gtk_label_new(_("Get filename from:"));
        gtk_box_pack_start(GTK_BOX(filenamefrom_hbox), filenamefrom_label, FALSE, FALSE, 0);

        filenamefrom_toggle = gtk_radio_button_new_with_label(NULL, _("original file tags"));
        g_signal_connect(G_OBJECT(filenamefrom_toggle), "toggled", G_CALLBACK(filenamefromtags_cb), NULL);
        gtk_box_pack_start(GTK_BOX(filenamefrom_hbox), filenamefrom_toggle, FALSE, FALSE, 0);

        filenamefrom_toggle =
            gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(filenamefrom_toggle),
                                                        _("original filename"));
        g_signal_connect(G_OBJECT(filenamefrom_toggle), "toggled", G_CALLBACK(filenamefromfilename_cb), NULL);
        gtk_box_pack_start(GTK_BOX(filenamefrom_hbox), filenamefrom_toggle, FALSE, FALSE, 0);

        if (!filenamefromtags)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(filenamefrom_toggle), TRUE);




        use_suffix_toggle = gtk_check_button_new_with_label(_("Don't strip file name extension"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_suffix_toggle), use_suffix);
        gtk_box_pack_start(GTK_BOX(configure_vbox), use_suffix_toggle, FALSE, FALSE, 0);
        use_suffix_tooltips = gtk_tooltips_new();
        gtk_tooltips_set_tip(use_suffix_tooltips, use_suffix_toggle, _("If enabled, the extension from the original filename will not be stripped before adding the new file extension to the end."), NULL);
        gtk_tooltips_enable(use_suffix_tooltips);

        if (filenamefromtags)
            gtk_widget_set_sensitive(use_suffix_toggle, FALSE);




        gtk_box_pack_start(GTK_BOX(configure_vbox), gtk_hseparator_new(), FALSE, FALSE, 0);




        prependnumber_toggle = gtk_check_button_new_with_label(_("Prepend track number to filename"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prependnumber_toggle), prependnumber);
        gtk_box_pack_start(GTK_BOX(configure_vbox), prependnumber_toggle, FALSE, FALSE, 0);



        configure_bbox = gtk_hbutton_box_new();
        gtk_button_box_set_layout(GTK_BUTTON_BOX(configure_bbox),
                                  GTK_BUTTONBOX_END);
        gtk_button_box_set_spacing(GTK_BUTTON_BOX(configure_bbox), 5);
        gtk_box_pack_start(GTK_BOX(configure_vbox), configure_bbox,
                           FALSE, FALSE, 0);

        configure_cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
        gtk_signal_connect_object(GTK_OBJECT(configure_cancel), "clicked",
                                  GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                  GTK_OBJECT(configure_win));
        gtk_box_pack_start(GTK_BOX(configure_bbox), configure_cancel,
                           TRUE, TRUE, 0);

        configure_ok = gtk_button_new_from_stock(GTK_STOCK_OK);
        gtk_signal_connect(GTK_OBJECT(configure_ok), "clicked",
                           GTK_SIGNAL_FUNC(configure_ok_cb), NULL);
        gtk_box_pack_start(GTK_BOX(configure_bbox), configure_ok,
                           TRUE, TRUE, 0);

        gtk_widget_show_all(configure_win);
    }
}
