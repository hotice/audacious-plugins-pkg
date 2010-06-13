/*
 * Crossfade Plugin for Audacious
 * Copyright 2010 John Lindgren
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

#include "config.h"

#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "crossfade.h"

void crossfade_config_load (void)
{
    mcs_handle_t * database = aud_cfg_db_open ();

    aud_cfg_db_get_int (database, "crossfade", "length", & crossfade_length);
    aud_cfg_db_close (database);
}

void crossfade_config_save (void)
{
    mcs_handle_t * database = aud_cfg_db_open ();

    aud_cfg_db_set_int (database, "crossfade", "length", crossfade_length);
    aud_cfg_db_close (database);
}

static void crossfade_about (void)
{
    static GtkWidget * window = NULL;

    audgui_simple_message (& window, GTK_MESSAGE_INFO, _("About Crossfade"),
     "Crossfade Plugin for Audacious\n"
     "Copyright 2010 John Lindgren\n\n"
     "Redistribution and use in source and binary forms, with or without "
     "modification, are permitted provided that the following conditions are "
     "met:\n\n"
     "1. Redistributions of source code must retain the above copyright "
     "notice, this list of conditions, and the following disclaimer.\n\n"
     "2. Redistributions in binary form must reproduce the above copyright "
     "notice, this list of conditions, and the following disclaimer in the "
     "documentation provided with the distribution.\n\n"
     "This software is provided \"as is\" and without any warranty, express or "
     "implied. In no event shall the authors be liable for any damages arising "
     "from the use of this software.");
}

static void value_changed (GtkRange * range, void * data)
{
    * (int *) data = gtk_range_get_value (range);
}

static void crossfade_configure (void)
{
    static GtkWidget * window = NULL;

    if (window == NULL)
    {
        GtkWidget * vbox, * hbox, * slider, * button;

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_type_hint ((GtkWindow *) window,
         GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_resizable ((GtkWindow *) window, FALSE);
        gtk_window_set_title ((GtkWindow *) window, _("Crossfade Preferences"));
        gtk_container_set_border_width ((GtkContainer *) window, 6);
        g_signal_connect (window, "destroy", (GCallback) gtk_widget_destroyed,
         & window);

        vbox = gtk_vbox_new (FALSE, 6);
        gtk_container_add ((GtkContainer *) window, vbox);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (_("Overlap (in "
         "seconds):")), TRUE, FALSE, 0);

        slider = gtk_hscale_new_with_range (1, 10, 1);
        gtk_range_set_value ((GtkRange *) slider, crossfade_length);
        gtk_widget_set_size_request (slider, 100, -1);
        gtk_box_pack_start ((GtkBox *) hbox, slider, FALSE, FALSE, 0);
        g_signal_connect (slider, "value-changed", (GCallback) value_changed,
         & crossfade_length);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
        gtk_box_pack_end ((GtkBox *) hbox, button, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION (2, 18, 0)
        gtk_widget_set_can_default (button, TRUE);
#endif
        gtk_widget_grab_default (button);
        g_signal_connect_swapped (button, "clicked", (GCallback)
         gtk_widget_destroy, window);

        audgui_destroy_on_escape (window);

        gtk_widget_show_all (vbox);
    }

    gtk_window_present ((GtkWindow *) window);
}

void crossfade_show_channels_message (void)
{
    static GtkWidget * window = NULL;

    audgui_simple_message (& window, GTK_MESSAGE_ERROR, _("Crossfade Error"),
     _("Crossfading failed because the songs had a different number of channels."));
}

void crossfade_show_rate_message (void)
{
    static GtkWidget * window = NULL;

    audgui_simple_message (& window, GTK_MESSAGE_ERROR, _("Crossfade Error"),
     _("Crossfading failed because the songs had different sample rates.\n\n"
     "You can use the Sample Rate Converter effect to resample the songs to "
     "the same rate."));
}

EffectPlugin crossfade_plugin =
{
    .description = "Crossfade",
    .init = crossfade_init,
    .cleanup = crossfade_cleanup,
    .about = crossfade_about,
    .configure = crossfade_configure,
    .start = crossfade_start,
    .process = crossfade_process,
    .flush = crossfade_flush,
    .finish = crossfade_finish,
    .decoder_to_output_time = crossfade_decoder_to_output_time,
    .output_to_decoder_time = crossfade_output_to_decoder_time,

    .order = 5, /* must be after resample */
};

EffectPlugin * crossfade_list[] = {& crossfade_plugin, NULL};

SIMPLE_EFFECT_PLUGIN (crossfade, crossfade_list)
