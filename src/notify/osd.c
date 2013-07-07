/*
 * osd.c
 *
 * Copyright (C) 2010 Maximilian Bogner <max@mbogner.de>
 * Copyright (C) 2013 John Lindgren and Jean-Alexandre Anglès d'Auriac
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "osd.h"

#include <libnotify/notify.h>

#include <audacious/i18n.h>
#include <audacious/drct.h>
#include <audacious/misc.h>

static void show_cb (void)
{
    aud_interface_show (TRUE);
}

static void osd_setup (NotifyNotification *notification)
{
    bool_t resident = aud_get_bool ("notify", "resident");

    notify_notification_set_hint (notification, "desktop-entry",
     g_variant_new_string ("audacious"));

    notify_notification_set_hint (notification, "action-icons", g_variant_new_boolean (TRUE));
    notify_notification_set_hint (notification, "resident", g_variant_new_boolean (resident));
    notify_notification_set_hint (notification, "transient", g_variant_new_boolean (! resident));

    notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
    notify_notification_set_timeout (notification, resident ?
     NOTIFY_EXPIRES_NEVER : NOTIFY_EXPIRES_DEFAULT);
}

void osd_setup_buttons (NotifyNotification *notification)
{
    notify_notification_clear_actions (notification);

    if (! aud_get_bool ("notify", "actions"))
        return;

    notify_notification_add_action (notification, "default", _("Show"),
     NOTIFY_ACTION_CALLBACK (show_cb), NULL, NULL);

    bool_t playing = aud_drct_get_playing ();
    bool_t paused = aud_drct_get_paused ();

    if (playing && ! paused)
        notify_notification_add_action (notification, "media-playback-pause",
         _("Pause"), NOTIFY_ACTION_CALLBACK (aud_drct_pause), NULL, NULL);
    else
        notify_notification_add_action (notification, "media-playback-start",
         _("Play"), NOTIFY_ACTION_CALLBACK (aud_drct_play), NULL, NULL);

    if (playing)
        notify_notification_add_action (notification, "media-skip-forward",
         _("Next"), NOTIFY_ACTION_CALLBACK (aud_drct_pl_next), NULL, NULL);
}

static NotifyNotification * notification = NULL;

void osd_show (const char * title, const char * _message, const char * icon,
 GdkPixbuf * pixbuf)
{
    char * message = g_markup_escape_text (_message, -1);

    if (pixbuf)
        icon = NULL;

    if (notification)
        notify_notification_update (notification, title, message, icon);
    else
    {
        notification = notify_notification_new (title, message, icon);
        osd_setup (notification);
    }

    if (pixbuf)
        notify_notification_set_image_from_pixbuf (notification, pixbuf);

    osd_setup_buttons (notification);
    notify_notification_show (notification, NULL);

    g_free (message);
}

void osd_hide (void)
{
    if (! notification)
        return;

    notify_notification_close (notification, NULL);
    g_object_unref (notification);
    notification = NULL;
}
