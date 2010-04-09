/*  Audacious
 *  Copyright (C) 2005-2007  Audacious development team.
 *
 *  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
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

/*#define AUD_DEBUG*/
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* TODO: enforce default sizes! */

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "plugin.h"

#include <audacious/plugin.h>
#include "ui_skin.h"
#include "util.h"
#include "ui_main.h"
#include "ui_equalizer.h"
#include "ui_playlist.h"
#include "ui_skinselector.h"
#include "debug.h"

#include "platform/smartinclude.h"

#include "ui_skinned_window.h"
#include "ui_skinned_button.h"
#include "ui_skinned_number.h"
#include "ui_skinned_horizontal_slider.h"
#include "ui_skinned_playstatus.h"

#define EXTENSION_TARGETS 7

static gchar *ext_targets[EXTENSION_TARGETS] =
{ "bmp", "xpm", "png", "svg", "gif", "jpg", "jpeg" };

struct _SkinPixmapIdMapping {
    SkinPixmapId id;
    const gchar *name;
    const gchar *alt_name;
    gint width, height;
};

struct _SkinMaskInfo {
    gint width, height;
    gchar *inistr;
};
/* I know, it's not nice to copy'n'paste stuff, but I wanted it to
   be atleast parially working, before dealing with such things */

typedef struct {
    const gchar *to_match;
    gchar *match;
    gboolean found;
} FindFileContext;

typedef struct _SkinPixmapIdMapping SkinPixmapIdMapping;
typedef struct _SkinMaskInfo SkinMaskInfo;


Skin *aud_active_skin = NULL;

static gint skin_current_num;

static SkinMaskInfo skin_mask_info[] = {
    {275, 116, "Normal"},
    {275, 16,  "WindowShade"},
    {275, 116, "Equalizer"},
    {275, 16,  "EqualizerWS"}
};

static SkinPixmapIdMapping skin_pixmap_id_map[] = {
    {SKIN_MAIN, "main", NULL, 0, 0},
    {SKIN_CBUTTONS, "cbuttons", NULL, 0, 0},
    {SKIN_SHUFREP, "shufrep", NULL, 0, 0},
    {SKIN_TEXT, "text", NULL, 0, 0},
    {SKIN_TITLEBAR, "titlebar", NULL, 0, 0},
    {SKIN_VOLUME, "volume", NULL, 0, 0},
    {SKIN_BALANCE, "balance", "volume", 0, 0},
    {SKIN_MONOSTEREO, "monoster", NULL, 0, 0},
    {SKIN_PLAYPAUSE, "playpaus", NULL, 0, 0},
    {SKIN_NUMBERS, "nums_ex", "numbers", 0, 0},
    {SKIN_POSBAR, "posbar", NULL, 0, 0},
    {SKIN_EQMAIN, "eqmain", NULL, 0, 0},
    {SKIN_PLEDIT, "pledit", NULL, 0, 0},
    {SKIN_EQ_EX, "eq_ex", NULL, 0, 0}
};

static guint skin_pixmap_id_map_size = G_N_ELEMENTS(skin_pixmap_id_map);

static const guchar skin_default_viscolor[24][3] = {
    {9, 34, 53},
    {10, 18, 26},
    {0, 54, 108},
    {0, 58, 116},
    {0, 62, 124},
    {0, 66, 132},
    {0, 70, 140},
    {0, 74, 148},
    {0, 78, 156},
    {0, 82, 164},
    {0, 86, 172},
    {0, 92, 184},
    {0, 98, 196},
    {0, 104, 208},
    {0, 110, 220},
    {0, 116, 232},
    {0, 122, 244},
    {0, 128, 255},
    {0, 128, 255},
    {0, 104, 208},
    {0, 80, 160},
    {0, 56, 112},
    {0, 32, 64},
    {200, 200, 200}
};

static gchar *original_gtk_theme = NULL;

static GdkBitmap *skin_create_transparent_mask(const gchar *,
                                               const gchar *,
                                               const gchar *,
                                               GdkWindow *,
                                               gint, gint, gboolean);

static void skin_set_default_vis_color(Skin * skin);

void
skin_lock(Skin * skin)
{
    g_mutex_lock(skin->lock);
}

void
skin_unlock(Skin * skin)
{
    g_mutex_unlock(skin->lock);
}

gboolean
aud_active_skin_reload(void)
{
    AUDDBG("\n");
    return aud_active_skin_load(aud_active_skin->path);
}

gboolean
aud_active_skin_load(const gchar * path)
{
    AUDDBG("%s\n", path);
    g_return_val_if_fail(aud_active_skin != NULL, FALSE);

    if (!skin_load(aud_active_skin, path)) {
        AUDDBG("loading failed\n");
        return FALSE;
    }

    mainwin_refresh_hints ();
    ui_skinned_window_draw_all(mainwin);
    ui_skinned_window_draw_all(equalizerwin);
    ui_skinned_window_draw_all(playlistwin);

    SkinPixmap *pixmap;
    pixmap = &aud_active_skin->pixmaps[SKIN_POSBAR];
    /* last 59 pixels of SKIN_POSBAR are knobs (normal and selected) */
    gtk_widget_set_size_request(mainwin_position, pixmap->width - 59, pixmap->height);

    return TRUE;
}

static void skin_pixmap_free (SkinPixmap * pixmap)
{
    if (pixmap->pixbuf != NULL)
    {
        g_object_unref (pixmap->pixbuf);
        pixmap->pixbuf = NULL;
    }
}

Skin *
skin_new(void)
{
    Skin *skin;
    skin = g_new0(Skin, 1);
    skin->lock = g_mutex_new();
    return skin;
}

/**
 * Frees the data associated for skin.
 *
 * Does not free skin itself or lock variable so that the skin can immediately
 * populated with new skin data if needed.
 */
void
skin_free(Skin * skin)
{
    gint i;

    g_return_if_fail(skin != NULL);

    for (i = 0; i < SKIN_PIXMAP_COUNT; i++)
        skin_pixmap_free(&skin->pixmaps[i]);

    for (i = 0; i < SKIN_MASK_COUNT; i++) {
        if (skin->masks[i])
            g_object_unref(skin->masks[i]);
        if (skin->scaled_masks[i])
            g_object_unref(skin->scaled_masks[i]);

        skin->masks[i] = NULL;
        skin->scaled_masks[i] = NULL;
    }

    for (i = 0; i < SKIN_COLOR_COUNT; i++) {
        if (skin->colors[i])
            g_free(skin->colors[i]);

        skin->colors[i] = NULL;
    }

    g_free(skin->path);
    skin->path = NULL;

    skin_set_default_vis_color(skin);
}

void
skin_destroy(Skin * skin)
{
    g_return_if_fail(skin != NULL);
    skin_free(skin);
    g_mutex_free(skin->lock);
    g_free(skin);
}

const SkinPixmapIdMapping *
skin_pixmap_id_lookup(guint id)
{
    guint i;

    for (i = 0; i < skin_pixmap_id_map_size; i++) {
        if (id == skin_pixmap_id_map[i].id) {
            return &skin_pixmap_id_map[i];
        }
    }

    return NULL;
}

const gchar *
skin_pixmap_id_to_name(SkinPixmapId id)
{
    guint i;

    for (i = 0; i < skin_pixmap_id_map_size; i++) {
        if (id == skin_pixmap_id_map[i].id)
            return skin_pixmap_id_map[i].name;
    }
    return NULL;
}

static void
skin_set_default_vis_color(Skin * skin)
{
    memcpy(skin->vis_color, skin_default_viscolor,
           sizeof(skin_default_viscolor));
}

gchar * skin_pixmap_locate (const gchar * dirname, gchar * * basenames)
{
    gchar * filename = NULL;
    gint i;

    for (i = 0; basenames[i] != NULL; i ++)
    {
        if ((filename = find_file_case_path (dirname, basenames[i])) != NULL)
            break;
    }

    return filename;
}

/**
 * Creates possible file names for a pixmap.
 *
 * Basically this makes list of all possible file names that pixmap data
 * can be found from by using the static ext_targets variable to get all
 * possible extensions that pixmap file might have.
 */
static gchar **
skin_pixmap_create_basenames(const SkinPixmapIdMapping * pixmap_id_mapping)
{
    gchar **basenames = g_malloc0(sizeof(gchar*) * (EXTENSION_TARGETS * 2 + 1));
    gint i, y;

    // Create list of all possible image formats that can be loaded
    for (i = 0, y = 0; i < EXTENSION_TARGETS; i++, y++)
    {
        basenames[y] =
            g_strdup_printf("%s.%s", pixmap_id_mapping->name, ext_targets[i]);

        if (pixmap_id_mapping->alt_name)
            basenames[++y] =
                g_strdup_printf("%s.%s", pixmap_id_mapping->alt_name,
                                ext_targets[i]);
    }

    return basenames;
}

/**
 * Frees the data allocated by skin_pixmap_create_basenames
 */
static void
skin_pixmap_free_basenames(gchar ** basenames)
{
    int i;
    for (i = 0; basenames[i] != NULL; i++)
    {
        g_free(basenames[i]);
        basenames[i] = NULL;
    }
    g_free(basenames);
}

/**
 * Locates a pixmap file for skin.
 */
static gchar *
skin_pixmap_locate_basenames(const Skin * skin,
                             const SkinPixmapIdMapping * pixmap_id_mapping,
                             const gchar * path_p)
{
    gchar *filename = NULL;
    const gchar *path = path_p ? path_p : skin->path;
    gchar **basenames = skin_pixmap_create_basenames(pixmap_id_mapping);

    filename = skin_pixmap_locate(path, basenames);

    skin_pixmap_free_basenames(basenames);

    return filename;
}


static gboolean
skin_load_pixmap_id(Skin * skin, SkinPixmapId id, const gchar * path_p)
{
    const SkinPixmapIdMapping *pixmap_id_mapping;
    gchar *filename;
    SkinPixmap *pm = NULL;

    g_return_val_if_fail(skin != NULL, FALSE);
    g_return_val_if_fail(id < SKIN_PIXMAP_COUNT, FALSE);

    pixmap_id_mapping = skin_pixmap_id_lookup(id);
    g_return_val_if_fail(pixmap_id_mapping != NULL, FALSE);

    filename = skin_pixmap_locate_basenames(skin, pixmap_id_mapping, path_p);

    if (filename == NULL)
        return FALSE;

    AUDDBG("loaded %s\n", filename);

    pm = &skin->pixmaps[id];
    GdkPixbuf *pix = gdk_pixbuf_new_from_file(filename, NULL);

    if (pix == NULL)
        return FALSE;

    if (config.colorize_r == 255 && config.colorize_g == 255 &&
     config.colorize_b == 255)
        pm->pixbuf = pix;
    else
    {
        pm->pixbuf = audacious_create_colorized_pixbuf(pix, config.colorize_r,
         config.colorize_g, config.colorize_b);
        g_object_unref(pix);
    }

    pm->width = gdk_pixbuf_get_width(pm->pixbuf);
    pm->height = gdk_pixbuf_get_height(pm->pixbuf);
    pm->current_width = pm->width;
    pm->current_height = pm->height;

    g_free(filename);

    return TRUE;
}

void
skin_mask_create(Skin * skin,
                 const gchar * path,
                 gint id,
                 GdkWindow * window)
{
    skin->masks[id] =
        skin_create_transparent_mask(path, "region.txt",
                                     skin_mask_info[id].inistr, window,
                                     skin_mask_info[id].width,
                                     skin_mask_info[id].height, FALSE);

    skin->scaled_masks[id] =
        skin_create_transparent_mask(path, "region.txt",
                                     skin_mask_info[id].inistr, window,
                                     skin_mask_info[id].width * 2,
                                     skin_mask_info[id].height * 2, TRUE);
}

static GdkBitmap *
create_default_mask(GdkWindow * parent, gint w, gint h)
{
    GdkBitmap *ret;
    GdkGC *gc;
    GdkColor pattern;

    ret = gdk_pixmap_new(parent, w, h, 1);
    gc = gdk_gc_new(ret);
    pattern.pixel = 1;
    gdk_gc_set_foreground(gc, &pattern);
    gdk_draw_rectangle(ret, gc, TRUE, 0, 0, w, h);
    g_object_unref(gc);

    return ret;
}

static void
skin_query_color(GdkColormap * cm, GdkColor * c)
{
#ifdef GDK_WINDOWING_X11
    XColor xc = { 0,0,0,0,0,0 };

    xc.pixel = c->pixel;
    XQueryColor(GDK_COLORMAP_XDISPLAY(cm), GDK_COLORMAP_XCOLORMAP(cm), &xc);
    c->red = xc.red;
    c->green = xc.green;
    c->blue = xc.blue;
#else
    /* do nothing. see what breaks? */
#endif
}

static glong
skin_calc_luminance(GdkColor * c)
{
    return (0.212671 * c->red + 0.715160 * c->green + 0.072169 * c->blue);
}

static void
skin_get_textcolors(GdkPixbuf * pix, GdkColor * bgc, GdkColor * fgc)
{
    /*
     * Try to extract reasonable background and foreground colors
     * from the font pixmap
     */

    GdkImage *gi;
    GdkColormap *cm;
    gint i;

    g_return_if_fail(pix != NULL);

    GdkPixmap *text = gdk_pixmap_new(NULL, gdk_pixbuf_get_width(pix), gdk_pixbuf_get_height(pix), gdk_rgb_get_visual()->depth);
    gdk_draw_pixbuf(text, NULL, pix, 0, 0, 0, 0, gdk_pixbuf_get_width(pix), gdk_pixbuf_get_height(pix),
                    GDK_RGB_DITHER_NONE, 0, 0);
    /* Get the first line of text */
    gi = gdk_drawable_get_image(text, 0, 0, 152, 6);
    cm = gdk_colormap_get_system();

    for (i = 0; i < 6; i++) {
        GdkColor c;
        gint x;
        glong d, max_d;

        /* Get a pixel from the middle of the space character */
        bgc[i].pixel = gdk_image_get_pixel(gi, 151, i);
        skin_query_color(cm, &bgc[i]);

        max_d = 0;
        for (x = 1; x < 150; x++) {
            c.pixel = gdk_image_get_pixel(gi, x, i);
            skin_query_color(cm, &c);

            d = labs(skin_calc_luminance(&c) - skin_calc_luminance(&bgc[i]));
            if (d > max_d) {
                memcpy(&fgc[i], &c, sizeof(GdkColor));
                max_d = d;
            }
        }
    }
    g_object_unref(gi);
    g_object_unref(text);
}

gboolean
init_skins(const gchar * path)
{
    aud_active_skin = skin_new();

    skin_parse_hints(aud_active_skin, NULL);

    /* create the windows if they haven't been created yet, needed for bootstrapping */
    if (mainwin == NULL)
    {
        mainwin_create();
        equalizerwin_create();
        playlistwin_create();
    }

    if (!aud_active_skin_load(path)) {
        if (path != NULL)
            AUDDBG("Unable to load skin (%s), trying default...\n", path);
        else
            AUDDBG("Skin not defined: trying default...\n");

        /* can't load configured skin, retry with default */
        if (!aud_active_skin_load(BMP_DEFAULT_SKIN_PATH)) {
            AUDDBG("Unable to load default skin (%s)! Giving up.\n",
                      BMP_DEFAULT_SKIN_PATH);
            return FALSE;
        }
    }

    if (config.random_skin_on_play)
        skinlist_update();

    return TRUE;
}

void cleanup_skins()
{
    skin_destroy(aud_active_skin);
    aud_active_skin = NULL;
}


/*
 * Opens and parses a skin's hints file.
 * Hints files are somewhat like "scripts" in Winamp3/5.
 * We'll probably add scripts to it next.
 */
void
skin_parse_hints(Skin * skin, gchar *path_p)
{
    gchar *filename, *tmp;
    INIFile *inifile;

    path_p = path_p ? path_p : skin->path;

    skin->properties.mainwin_othertext = FALSE;
    skin->properties.mainwin_vis_x = 24;
    skin->properties.mainwin_vis_y = 43;
    skin->properties.mainwin_vis_width = 76;
    skin->properties.mainwin_text_x = 112;
    skin->properties.mainwin_text_y = 27;
    skin->properties.mainwin_text_width = 153;
    skin->properties.mainwin_infobar_x = 112;
    skin->properties.mainwin_infobar_y = 43;
    skin->properties.mainwin_number_0_x = 36;
    skin->properties.mainwin_number_0_y = 26;
    skin->properties.mainwin_number_1_x = 48;
    skin->properties.mainwin_number_1_y = 26;
    skin->properties.mainwin_number_2_x = 60;
    skin->properties.mainwin_number_2_y = 26;
    skin->properties.mainwin_number_3_x = 78;
    skin->properties.mainwin_number_3_y = 26;
    skin->properties.mainwin_number_4_x = 90;
    skin->properties.mainwin_number_4_y = 26;
    skin->properties.mainwin_playstatus_x = 24;
    skin->properties.mainwin_playstatus_y = 28;
    skin->properties.mainwin_menurow_visible = TRUE;
    skin->properties.mainwin_volume_x = 107;
    skin->properties.mainwin_volume_y = 57;
    skin->properties.mainwin_balance_x = 177;
    skin->properties.mainwin_balance_y = 57;
    skin->properties.mainwin_position_x = 16;
    skin->properties.mainwin_position_y = 72;
    skin->properties.mainwin_othertext_is_status = FALSE;
    skin->properties.mainwin_othertext_visible = skin->properties.mainwin_othertext;
    skin->properties.mainwin_text_visible = TRUE;
    skin->properties.mainwin_vis_visible = TRUE;
    skin->properties.mainwin_previous_x = 16;
    skin->properties.mainwin_previous_y = 88;
    skin->properties.mainwin_play_x = 39;
    skin->properties.mainwin_play_y = 88;
    skin->properties.mainwin_pause_x = 62;
    skin->properties.mainwin_pause_y = 88;
    skin->properties.mainwin_stop_x = 85;
    skin->properties.mainwin_stop_y = 88;
    skin->properties.mainwin_next_x = 108;
    skin->properties.mainwin_next_y = 88;
    skin->properties.mainwin_eject_x = 136;
    skin->properties.mainwin_eject_y = 89;
    skin->properties.mainwin_width = 275;
    skin_mask_info[0].width = skin->properties.mainwin_width;
    skin->properties.mainwin_height = 116;
    skin_mask_info[0].height = skin->properties.mainwin_height;
    skin->properties.mainwin_about_x = 247;
    skin->properties.mainwin_about_y = 83;
    skin->properties.mainwin_shuffle_x = 164;
    skin->properties.mainwin_shuffle_y = 89;
    skin->properties.mainwin_repeat_x = 210;
    skin->properties.mainwin_repeat_y = 89;
    skin->properties.mainwin_eqbutton_x = 219;
    skin->properties.mainwin_eqbutton_y = 58;
    skin->properties.mainwin_plbutton_x = 242;
    skin->properties.mainwin_plbutton_y = 58;
    skin->properties.textbox_bitmap_font_width = 5;
    skin->properties.textbox_bitmap_font_height = 6;
    skin->properties.mainwin_minimize_x = 244;
    skin->properties.mainwin_minimize_y = 3;
    skin->properties.mainwin_shade_x = 254;
    skin->properties.mainwin_shade_y = 3;
    skin->properties.mainwin_close_x = 264;
    skin->properties.mainwin_close_y = 3;

    if (path_p == NULL)
        return;

    filename = find_file_case_uri (path_p, "skin.hints");

    if (filename == NULL)
        return;

    inifile = aud_open_ini_file(filename);
    if (!inifile)
        return;

    tmp = aud_read_ini_string(inifile, "skin", "mainwinOthertext");

    if (tmp != NULL)
    {
        skin->properties.mainwin_othertext = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinVisX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_vis_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinVisY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_vis_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinVisWidth");

    if (tmp != NULL)
    {
        skin->properties.mainwin_vis_width = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinTextX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_text_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinTextY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_text_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinTextWidth");

    if (tmp != NULL)
    {
        skin->properties.mainwin_text_width = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinInfoBarX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_infobar_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinInfoBarY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_infobar_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNumber0X");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_0_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNumber0Y");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_0_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNumber1X");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_1_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNumber1Y");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_1_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNumber2X");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_2_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNumber2Y");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_2_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNumber3X");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_3_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNumber3Y");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_3_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNumber4X");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_4_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNumber4Y");

    if (tmp != NULL)
    {
        skin->properties.mainwin_number_4_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPlayStatusX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_playstatus_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPlayStatusY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_playstatus_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinMenurowVisible");

    if (tmp != NULL)
    {
        skin->properties.mainwin_menurow_visible = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinVolumeX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_volume_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinVolumeY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_volume_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinBalanceX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_balance_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinBalanceY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_balance_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPositionX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_position_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPositionY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_position_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinOthertextIsStatus");

    if (tmp != NULL)
    {
        skin->properties.mainwin_othertext_is_status = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinOthertextVisible");

    if (tmp != NULL)
    {
        skin->properties.mainwin_othertext_visible = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinTextVisible");

    if (tmp != NULL)
    {
        skin->properties.mainwin_text_visible = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinVisVisible");

    if (tmp != NULL)
    {
        skin->properties.mainwin_vis_visible = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPreviousX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_previous_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPreviousY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_previous_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPlayX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_play_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPlayY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_play_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPauseX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_pause_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPauseY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_pause_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinStopX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_stop_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinStopY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_stop_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNextX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_next_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinNextY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_next_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinEjectX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_eject_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinEjectY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_eject_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinWidth");

    if (tmp != NULL)
    {
        skin->properties.mainwin_width = atoi(tmp);
        g_free(tmp);
    }

    skin_mask_info[0].width = skin->properties.mainwin_width;

    tmp = aud_read_ini_string(inifile, "skin", "mainwinHeight");

    if (tmp != NULL)
    {
        skin->properties.mainwin_height = atoi(tmp);
        g_free(tmp);
    }

    skin_mask_info[0].height = skin->properties.mainwin_height;

    tmp = aud_read_ini_string(inifile, "skin", "mainwinAboutX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_about_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinAboutY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_about_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinShuffleX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_shuffle_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinShuffleY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_shuffle_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinRepeatX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_repeat_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinRepeatY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_repeat_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinEQButtonX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_eqbutton_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinEQButtonY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_eqbutton_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPLButtonX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_plbutton_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinPLButtonY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_plbutton_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "textboxBitmapFontWidth");

    if (tmp != NULL)
    {
        skin->properties.textbox_bitmap_font_width = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "textboxBitmapFontHeight");

    if (tmp != NULL)
    {
        skin->properties.textbox_bitmap_font_height = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinMinimizeX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_minimize_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinMinimizeY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_minimize_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinShadeX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_shade_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinShadeY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_shade_y = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinCloseX");

    if (tmp != NULL)
    {
        skin->properties.mainwin_close_x = atoi(tmp);
        g_free(tmp);
    }

    tmp = aud_read_ini_string(inifile, "skin", "mainwinCloseY");

    if (tmp != NULL)
    {
        skin->properties.mainwin_close_y = atoi(tmp);
        g_free(tmp);
    }

    if (filename != NULL)
        g_free(filename);

    aud_close_ini_file(inifile);
}

static guint
hex_chars_to_int(gchar hi, gchar lo)
{
    /*
     * Converts a value in the range 0x00-0xFF
     * to a integer in the range 0-65535
     */
    gchar str[3];

    str[0] = hi;
    str[1] = lo;
    str[2] = 0;

    return (CLAMP(strtol(str, NULL, 16), 0, 0xFF) << 8);
}

static GdkColor *
skin_load_color(INIFile *inifile,
                const gchar * section, const gchar * key,
                gchar * default_hex)
{
    gchar *value;
    GdkColor *color = NULL;

    if (inifile || default_hex) {
        if (inifile) {
            value = aud_read_ini_string(inifile, section, key);
            if (value == NULL) {
                value = g_strdup(default_hex);
            }
        } else {
            value = g_strdup(default_hex);
        }
        if (value) {
            gchar *ptr = value;
            gint len;

            color = g_new0(GdkColor, 1);
            g_strstrip(value);

            if (value[0] == '#')
                ptr++;
            len = strlen(ptr);
            /*
             * The handling of incomplete values is done this way
             * to maximize winamp compatibility
             */
            if (len >= 6) {
                color->red = hex_chars_to_int(*ptr, *(ptr + 1));
                ptr += 2;
            }
            if (len >= 4) {
                color->green = hex_chars_to_int(*ptr, *(ptr + 1));
                ptr += 2;
            }
            if (len >= 2)
                color->blue = hex_chars_to_int(*ptr, *(ptr + 1));

            g_free(value);
        }
    }
    return color;
}



GdkBitmap *
skin_create_transparent_mask(const gchar * path,
                             const gchar * file,
                             const gchar * section,
                             GdkWindow * window,
                             gint width,
                             gint height, gboolean scale)
{
    GdkBitmap *mask = NULL;
    GdkGC *gc = NULL;
    GdkColor pattern;
    GdkPoint *gpoints;

    gchar *filename = NULL;
    INIFile *inifile = NULL;
    gboolean created_mask = FALSE;
    GArray *num, *point;
    guint i, j;
    gint k;

    if (path != NULL)
        filename = find_file_case_uri (path, file);

    /* filename will be null if path wasn't set */
    if (!filename)
        return create_default_mask(window, width, height);

    inifile = aud_open_ini_file(filename);

    if ((num = aud_read_ini_array(inifile, section, "NumPoints")) == NULL) {
        g_free(filename);
        aud_close_ini_file(inifile);
        return NULL;
    }

    if ((point = aud_read_ini_array(inifile, section, "PointList")) == NULL) {
        g_array_free(num, TRUE);
        g_free(filename);
        aud_close_ini_file(inifile);
        return NULL;
    }

    aud_close_ini_file(inifile);

    mask = gdk_pixmap_new(window, width, height, 1);
    gc = gdk_gc_new(mask);

    pattern.pixel = 0;
    gdk_gc_set_foreground(gc, &pattern);
    gdk_draw_rectangle(mask, gc, TRUE, 0, 0, width, height);
    pattern.pixel = 1;
    gdk_gc_set_foreground(gc, &pattern);

    j = 0;
    for (i = 0; i < num->len; i++) {
        if ((int)(point->len - j) >= (g_array_index(num, gint, i) * 2)) {
            created_mask = TRUE;
            gpoints = g_new(GdkPoint, g_array_index(num, gint, i));
            for (k = 0; k < g_array_index(num, gint, i); k++) {
                gpoints[k].x =
                    g_array_index(point, gint, j + k * 2) * (scale ? config.scale_factor : 1 );
                gpoints[k].y =
                    g_array_index(point, gint,
                                  j + k * 2 + 1) * (scale ? config.scale_factor : 1);
            }
            j += k * 2;
            gdk_draw_polygon(mask, gc, TRUE, gpoints,
                             g_array_index(num, gint, i));
            g_free(gpoints);
        }
    }
    g_array_free(num, TRUE);
    g_array_free(point, TRUE);
    g_free(filename);

    if (!created_mask)
        gdk_draw_rectangle(mask, gc, TRUE, 0, 0, width, height);

    g_object_unref(gc);

    return mask;
}

static void skin_load_viscolor (Skin * skin, const gchar * path, const gchar *
 basename)
{
    gchar * filename, * buffer, * string, * next;
    gint line;

    skin_set_default_vis_color (skin);

    filename = find_file_case_uri (path, basename);
    if (filename == NULL)
        return;

    buffer = load_text_file (filename);
    string = buffer;

    for (line = 0; string != NULL && line < 24; line ++)
    {
        GArray * array;
        gint column;

        next = text_parse_line (string);
        array = string_to_garray (string);

        if (array->len >= 3)
        {
            for (column = 0; column < 3; column ++)
                skin->vis_color[line][column] = g_array_index (array, gint,
                 column);
        }

        g_array_free (array, TRUE);
        string = next;
    }

    g_free (buffer);
}

static void
skin_numbers_generate_dash(Skin * skin)
{
    GdkPixbuf *pixbuf;
    SkinPixmap *numbers;

    g_return_if_fail(skin != NULL);

    numbers = &skin->pixmaps[SKIN_NUMBERS];
    if (!numbers->pixbuf || numbers->current_width < 99)
        return;

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
                            108, numbers->current_height);

    skin_draw_pixbuf(NULL, skin, pixbuf, SKIN_NUMBERS, 0, 0, 0, 0, 99, numbers->current_height);
    skin_draw_pixbuf(NULL, skin, pixbuf, SKIN_NUMBERS, 90, 0, 99, 0, 9, numbers->current_height);
    skin_draw_pixbuf(NULL, skin, pixbuf, SKIN_NUMBERS, 20, 6, 101, 6, 5, 1);

    g_object_unref(numbers->pixbuf);

    numbers->pixbuf = pixbuf;
    numbers->current_width = 108;
    numbers->width = 108;
}

static gboolean
skin_load_pixmaps(Skin * skin, const gchar * path)
{
    GdkPixbuf *text_pb;
    guint i;
    gchar *filename;
    INIFile *inifile;

    if(!skin) return FALSE;
    if(!path) return FALSE;

    AUDDBG("Loading pixmaps in %s\n", path);

    for (i = 0; i < SKIN_PIXMAP_COUNT; i++)
        if (!skin_load_pixmap_id(skin, i, path) && !config.allow_broken_skins)
            return FALSE;

    text_pb = skin->pixmaps[SKIN_TEXT].pixbuf;

    if (text_pb)
        skin_get_textcolors(text_pb, skin->textbg, skin->textfg);

    if (skin->pixmaps[SKIN_NUMBERS].pixbuf &&
        skin->pixmaps[SKIN_NUMBERS].width < 108 )
        skin_numbers_generate_dash(skin);

    filename = find_file_case_uri (path, "pledit.txt");
    inifile = (filename != NULL) ? aud_open_ini_file (filename) : NULL;

    skin->colors[SKIN_PLEDIT_NORMAL] =
        skin_load_color(inifile, "Text", "Normal", "#2499ff");
    skin->colors[SKIN_PLEDIT_CURRENT] =
        skin_load_color(inifile, "Text", "Current", "#ffeeff");
    skin->colors[SKIN_PLEDIT_NORMALBG] =
        skin_load_color(inifile, "Text", "NormalBG", "#0a120a");
    skin->colors[SKIN_PLEDIT_SELECTEDBG] =
        skin_load_color(inifile, "Text", "SelectedBG", "#0a124a");

    if (inifile)
        aud_close_ini_file(inifile);

    if (filename)
        g_free(filename);

    skin_mask_create(skin, path, SKIN_MASK_MAIN, mainwin->window);
    skin_mask_create(skin, path, SKIN_MASK_MAIN_SHADE, mainwin->window);

    skin_mask_create(skin, path, SKIN_MASK_EQ, equalizerwin->window);
    skin_mask_create(skin, path, SKIN_MASK_EQ_SHADE, equalizerwin->window);

    skin_load_viscolor(skin, path, "viscolor.txt");

    return TRUE;
}

static void
skin_set_gtk_theme(GtkSettings * settings, Skin * skin)
{
    if (original_gtk_theme == NULL)
         g_object_get(settings, "gtk-theme-name", &original_gtk_theme, NULL);

    /* the way GTK does things can be very broken. --nenolod */

    gchar *tmp = g_strdup_printf("%s/.themes/aud-%s", g_get_home_dir(),
                                 basename(skin->path));

    gchar *troot = g_strdup_printf("%s/.themes", g_get_home_dir());
    g_mkdir_with_parents(troot, 0755);
    g_free(troot);

    symlink(skin->path, tmp);
    gtk_settings_set_string_property(settings, "gtk-theme-name",
                                     basename(tmp), "audacious");
    g_free(tmp);
}

/**
 * Checks if all pixmap files exist that skin needs.
 */
static gboolean
skin_check_pixmaps(const Skin * skin, const gchar * skin_path)
{
    guint i;
    for (i = 0; i < SKIN_PIXMAP_COUNT; i++)
    {
        gchar *filename = skin_pixmap_locate_basenames(skin,
                                                       skin_pixmap_id_lookup(i),
                                                       skin_path);
        if (!filename)
            return FALSE;
        g_free(filename);
    }
    return TRUE;
}

static gboolean
skin_load_nolock(Skin * skin, const gchar * path, gboolean force)
{
    GtkSettings *settings;
    gchar *gtkrcpath;
    gchar *newpath, *skin_path;
    int archive = 0;

    AUDDBG("Attempt to load skin \"%s\"\n", path);

    g_return_val_if_fail(skin != NULL, FALSE);
    g_return_val_if_fail(path != NULL, FALSE);
    REQUIRE_LOCK(skin->lock);

    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_DIR))
        return FALSE;

    if(force) AUDDBG("reloading forced!\n");
    if (!force && skin->path && !strcmp(skin->path, path)) {
        AUDDBG("skin %s already loaded\n", path);
        return FALSE;
    }

    if (file_is_archive(path)) {
        AUDDBG("Attempt to load archive\n");
        if (!(skin_path = archive_decompress(path))) {
            AUDDBG("Unable to extract skin archive (%s)\n", path);
            return FALSE;
        }
        archive = 1;
    } else {
        skin_path = g_strdup(path);
    }

    // Check if skin path has all necessary files.
    if (!config.allow_broken_skins && !skin_check_pixmaps(skin, skin_path)) {
        if(archive) del_directory(skin_path);
        g_free(skin_path);
        AUDDBG("Skin path (%s) doesn't have all wanted pixmaps\n", skin_path);
        return FALSE;
    }

    // skin_free() frees skin->path and variable path can actually be skin->path
    // and we want to get the path before possibly freeing it.
    newpath = g_strdup(path);
    skin_free(skin);
    skin->path = newpath;

    memset(&(skin->properties), 0, sizeof(SkinProperties)); /* do it only if all tests above passed! --asphyx */

    skin_current_num++;

    /* Parse the hints for this skin. */
    skin_parse_hints(skin, skin_path);

    if (!skin_load_pixmaps(skin, skin_path)) {
        if(archive) del_directory(skin_path);
        g_free(skin_path);
        AUDDBG("Skin loading failed\n");
        return FALSE;
    }

    /* restore gtk theme if changed by previous skin */
    settings = gtk_settings_get_default();

    if (original_gtk_theme != NULL) {
        gtk_settings_set_string_property(settings, "gtk-theme-name",
                                              original_gtk_theme, "audacious");
        g_free(original_gtk_theme);
        original_gtk_theme = NULL;
    }

#ifndef _WIN32
    if (! config.disable_inline_gtk && ! archive)
    {
        gtkrcpath = g_strdup_printf ("%s/gtk-2.0/gtkrc", skin->path);

        if (g_file_test (gtkrcpath, G_FILE_TEST_IS_REGULAR))
            skin_set_gtk_theme (settings, skin);

        g_free (gtkrcpath);
    }
#endif

    if(archive) del_directory(skin_path);
    g_free(skin_path);

    mainwin_set_shape ();
    equalizerwin_set_shape ();

    return TRUE;
}

void
skin_install_skin(const gchar * path)
{
    gchar *command;

    g_return_if_fail(path != NULL);

    command = g_strdup_printf("cp %s %s",
                              path, skins_paths[SKINS_PATH_USER_SKIN_DIR]);
    if (system(command)) {
        AUDDBG("Unable to install skin (%s) into user directory (%s)\n",
                  path, skins_paths[SKINS_PATH_USER_SKIN_DIR]);
    }
    g_free(command);
}

static SkinPixmap *
skin_get_pixmap(Skin * skin, SkinPixmapId map_id)
{
    g_return_val_if_fail(skin != NULL, NULL);
    g_return_val_if_fail(map_id < SKIN_PIXMAP_COUNT, NULL);

    return &skin->pixmaps[map_id];
}

gboolean
skin_load(Skin * skin, const gchar * path)
{
    gboolean ret;

    g_return_val_if_fail(skin != NULL, FALSE);

    if (!path)
        return FALSE;

    skin_lock(skin);
    ret = skin_load_nolock(skin, path, FALSE);
    skin_unlock(skin);

    if(!ret) {
        AUDDBG("loading failed\n");
        return FALSE; /* don't try to update anything if loading failed --asphyx */
    }

    SkinPixmap *pixmap = NULL;
    pixmap = skin_get_pixmap(skin, SKIN_NUMBERS);
    if (pixmap) {
        ui_skinned_number_set_size(mainwin_minus_num, 9, pixmap->height);
        ui_skinned_number_set_size(mainwin_10min_num, 9, pixmap->height);
        ui_skinned_number_set_size(mainwin_min_num, 9, pixmap->height);
        ui_skinned_number_set_size(mainwin_10sec_num, 9, pixmap->height);
        ui_skinned_number_set_size(mainwin_sec_num, 9, pixmap->height);
    }

    pixmap = skin_get_pixmap(skin, SKIN_MAIN);
    if (pixmap && skin->properties.mainwin_height > pixmap->height)
        skin->properties.mainwin_height = pixmap->height;

    pixmap = skin_get_pixmap(skin, SKIN_PLAYPAUSE);
    if (pixmap)
        ui_skinned_playstatus_set_size(mainwin_playstatus, 11, pixmap->height);

    pixmap = skin_get_pixmap(skin, SKIN_EQMAIN);
    if (pixmap->height >= 313)
        gtk_widget_show(equalizerwin_graph);

    return TRUE;
}

gboolean
skin_reload_forced(void)
{
   gboolean error;
   AUDDBG("\n");

   skin_lock(aud_active_skin);
   error = skin_load_nolock(aud_active_skin, aud_active_skin->path, TRUE);
   skin_unlock(aud_active_skin);

   return error;
}

void
skin_reload(Skin * skin)
{
    AUDDBG("\n");
    g_return_if_fail(skin != NULL);
    skin_load_nolock(skin, skin->path, TRUE);
}

GdkBitmap *
skin_get_mask(Skin * skin, SkinMaskId mi)
{
    GdkBitmap **masks;

    g_return_val_if_fail(skin != NULL, NULL);
    g_return_val_if_fail(mi < SKIN_PIXMAP_COUNT, NULL);

    masks = config.scaled ? skin->scaled_masks : skin->masks;
    return masks[mi];
}

GdkColor *
skin_get_color(Skin * skin, SkinColorId color_id)
{
    GdkColor *ret = NULL;

    g_return_val_if_fail(skin != NULL, NULL);

    switch (color_id) {
    case SKIN_TEXTBG:
        if (skin->pixmaps[SKIN_TEXT].pixbuf)
            ret = skin->textbg;
        else
            ret = skin->def_textbg;
        break;
    case SKIN_TEXTFG:
        if (skin->pixmaps[SKIN_TEXT].pixbuf)
            ret = skin->textfg;
        else
            ret = skin->def_textfg;
        break;
    default:
        if (color_id < SKIN_COLOR_COUNT)
            ret = skin->colors[color_id];
        break;
    }
    return ret;
}

void
skin_get_viscolor(Skin * skin, guchar vis_color[24][3])
{
    gint i;

    g_return_if_fail(skin != NULL);

    for (i = 0; i < 24; i++) {
        vis_color[i][0] = skin->vis_color[i][0];
        vis_color[i][1] = skin->vis_color[i][1];
        vis_color[i][2] = skin->vis_color[i][2];
    }
}

gint
skin_get_id(void)
{
    return skin_current_num;
}

void
skin_draw_pixbuf(GtkWidget *widget, Skin * skin, GdkPixbuf * pix,
                 SkinPixmapId pixmap_id,
                 gint xsrc, gint ysrc, gint xdest, gint ydest,
                 gint width, gint height)
{
    SkinPixmap *pixmap;

    g_return_if_fail(skin != NULL);

    pixmap = skin_get_pixmap(skin, pixmap_id);
    g_return_if_fail(pixmap != NULL);
    g_return_if_fail(pixmap->pixbuf != NULL);

    /* perhaps we should use transparency or resize widget? */
    if (xsrc+width > pixmap->width || ysrc+height > pixmap->height) {
        if (widget) {
            /* it's better to hide widget using SKIN_PLAYPAUSE/SKIN_POSBAR than display mess */
            if ((pixmap_id == SKIN_PLAYPAUSE && pixmap->width != 42) || pixmap_id == SKIN_POSBAR) {
                gtk_widget_hide(widget);
                return;
            }

            /* Some skins include SKIN_VOLUME and/or SKIN_BALANCE without knobs */
            if (pixmap_id == SKIN_VOLUME || pixmap_id == SKIN_BALANCE) {
                if (ysrc+height > 421 && xsrc+width <= pixmap->width)
                    return;
            }

            /* XMMS skins seems to have SKIN_MONOSTEREO with size 58x20 instead of 58x24 */
            if (pixmap_id == SKIN_MONOSTEREO)
                height = pixmap->height/2;

            if (gtk_widget_get_parent(widget) == SKINNED_WINDOW(equalizerwin)->normal) {
                if (!(pixmap_id == SKIN_EQMAIN && ysrc == 314)) /* equalizer preamp on equalizer graph */
                    gtk_widget_hide(widget);
            }

            if (gtk_widget_get_parent(widget) == SKINNED_WINDOW(playlistwin)->normal) {
                /* I haven't seen any skin with substandard playlist */
                gtk_widget_hide(widget);
            }
        } else
            return;
    }

    width = MIN(width, pixmap->width - xsrc);
    height = MIN(height, pixmap->height - ysrc);
    gdk_pixbuf_copy_area(pixmap->pixbuf, xsrc, ysrc, width, height,
                         pix, xdest, ydest);
}

void
skin_get_eq_spline_colors(Skin * skin, guint32 colors[19])
{
    gint i;
    GdkPixbuf *pixbuf;
    SkinPixmap *eqmainpm;
    guchar* pixels,*p;
    guint rowstride, n_channels;
    g_return_if_fail(skin != NULL);

    eqmainpm = &skin->pixmaps[SKIN_EQMAIN];
    if (eqmainpm->pixbuf &&
            eqmainpm->current_width >= 116 && eqmainpm->current_height >= 313)
        pixbuf = eqmainpm->pixbuf;
    else
        return;

    if (!GDK_IS_PIXBUF(pixbuf))
        return;

    pixels = gdk_pixbuf_get_pixels (pixbuf);
    rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    n_channels = gdk_pixbuf_get_n_channels (pixbuf);
    for (i = 0; i < 19; i++)
    {
        p = pixels + rowstride * (i + 294) + 115 * n_channels;
        colors[i] = (p[0] << 16) | (p[1] << 8) | p[2];
        /* should we really treat the Alpha channel? */
        /*if (n_channels == 4)
            colors[i] = (colors[i] << 8) | p[3];*/
    }
}


static void
skin_draw_playlistwin_frame_top(Skin * skin, GdkPixbuf * pix,
                                gint width, gint height, gboolean focus)
{
    /* The title bar skin consists of 2 sets of 4 images, 1 set
     * for focused state and the other for unfocused. The 4 images
     * are:
     *
     * a. right corner (25,20)
     * b. left corner  (25,20)
     * c. tiler        (25,20)
     * d. title        (100,20)
     *
     * min allowed width = 100+25+25 = 150
     */

    gint i, y, c;

    /* get y offset of the pixmap set to use */
    if (focus)
        y = 0;
    else
        y = 21;

    /* left corner */
    skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 0, y, 0, 0, 25, 20);

    /* titlebar title */
    skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 26, y,
                     (width - 100) / 2, 0, 100, 20);

    /* titlebar right corner  */
    skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 153, y,
                     width - 25, 0, 25, 20);

    /* tile draw the remaining frame */

    /* compute tile count */
    c = (width - (100 + 25 + 25)) / 25;

    for (i = 0; i < c / 2; i++) {
        /* left of title */
        skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 127, y,
                         25 + i * 25, 0, 25, 20);

        /* right of title */
        skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 127, y,
                         (width + 100) / 2 + i * 25, 0, 25, 20);
    }

    if (c & 1) {
        /* Odd tile count, so one remaining to draw. Here we split
         * it into two and draw half on either side of the title */
        skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 127, y,
                         ((c / 2) * 25) + 25, 0, 12, 20);
        skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 127, y,
                         (width / 2) + ((c / 2) * 25) + 50, 0, 13, 20);
    }
}

static void
skin_draw_playlistwin_frame_bottom(Skin * skin, GdkPixbuf * pix,
                                   gint width, gint height, gboolean focus)
{
    /* The bottom frame skin consists of 1 set of 4 images. The 4
     * images are:
     *
     * a. left corner with menu buttons (125,38)
     * b. visualization window (75,38)
     * c. right corner with play buttons (150,38)
     * d. frame tile (25,38)
     *
     * (min allowed width = 125+150+25=300
     */

    gint i, c;

    /* bottom left corner (menu buttons) */
    skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 0, 72,
                     0, height - 38, 125, 38);

    c = (width - 275) / 25;

    /* draw visualization window, if width allows */
    if (c >= 3) {
        c -= 3;
        skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 205, 0,
                         width - (150 + 75), height - 38, 75, 38);
    }

    /* Bottom right corner (playbuttons etc) */
    skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT,
                     126, 72, width - 150, height - 38, 150, 38);

    /* Tile draw the remaining undrawn portions */
    for (i = 0; i < c; i++)
        skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 179, 0,
                         125 + i * 25, height - 38, 25, 38);
}

static void
skin_draw_playlistwin_frame_sides(Skin * skin, GdkPixbuf * pix,
                                  gint width, gint height, gboolean focus)
{
    /* The side frames consist of 2 tile images. 1 for the left, 1 for
     * the right.
     * a. left  (12,29)
     * b. right (19,29)
     */

    gint i;

    /* frame sides */
    for (i = 0; i < (height - (20 + 38)) / 29; i++) {
        /* left */
        skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 0, 42,
                         0, 20 + i * 29, 12, 29);

        /* right */
        skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 32, 42,
                         width - 19, 20 + i * 29, 19, 29);
    }
}


void
skin_draw_playlistwin_frame(Skin * skin, GdkPixbuf * pix,
                            gint width, gint height, gboolean focus)
{
    skin_draw_playlistwin_frame_top(skin, pix, width, height, focus);
    skin_draw_playlistwin_frame_bottom(skin, pix, width, height, focus);
    skin_draw_playlistwin_frame_sides(skin, pix, width, height, focus);
}


void
skin_draw_playlistwin_shaded(Skin * skin, GdkPixbuf * pix,
                             gint width, gboolean focus)
{
    /* The shade mode titlebar skin consists of 4 images:
     * a) left corner               offset (72,42) size (25,14)
     * b) right corner, focused     offset (99,57) size (50,14)
     * c) right corner, unfocused   offset (99,42) size (50,14)
     * d) bar tile                  offset (72,57) size (25,14)
     */

    gint i;

    /* left corner */
    skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 72, 42, 0, 0, 25, 14);

    /* bar tile */
    for (i = 0; i < (width - 75) / 25; i++)
        skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 72, 57,
                         (i * 25) + 25, 0, 25, 14);

    /* right corner */
    skin_draw_pixbuf(NULL, skin, pix, SKIN_PLEDIT, 99, focus ? 42 : 57,
                     width - 50, 0, 50, 14);
}


void
skin_draw_mainwin_titlebar(Skin * skin, GdkPixbuf * pix,
                           gboolean shaded, gboolean focus)
{
    /* The titlebar skin consists of 2 sets of 2 images, one for for
     * shaded and the other for unshaded mode, giving a total of 4.
     * The images are exactly 275x14 pixels, aligned and arranged
     * vertically on each other in the pixmap in the following order:
     *
     * a) unshaded, focused      offset (27, 0)
     * b) unshaded, unfocused    offset (27, 15)
     * c) shaded, focused        offset (27, 29)
     * d) shaded, unfocused      offset (27, 42)
     */

    gint y_offset;

    if (shaded) {
        if (focus)
            y_offset = 29;
        else
            y_offset = 42;
    }
    else {
        if (focus)
            y_offset = 0;
        else
            y_offset = 15;
    }

    skin_draw_pixbuf(NULL, skin, pix, SKIN_TITLEBAR, 27, y_offset,
                     0, 0, aud_active_skin->properties.mainwin_width, MAINWIN_TITLEBAR_HEIGHT);
}


void
skin_set_random_skin(void)
{
    SkinNode *node;
    guint32 randval;

    /* Get a random value to select the skin to use */
    randval = g_random_int_range(0, g_list_length(skinlist));
    node = g_list_nth(skinlist, randval)->data;
    aud_active_skin_load(node->path);
}

void ui_skinned_widget_draw_with_coordinates(GtkWidget *widget, GdkPixbuf *obj, gint width, gint height, gint destx, gint desty, gboolean scale) {
    g_return_if_fail(widget != NULL);
    g_return_if_fail(obj != NULL);

    if (scale) {
        GdkPixbuf *image = gdk_pixbuf_scale_simple(obj, width * config.scale_factor, height* config.scale_factor, GDK_INTERP_NEAREST);
        gdk_draw_pixbuf(widget->window, NULL, image, 0, 0, destx, desty, width * config.scale_factor , height * config.scale_factor, GDK_RGB_DITHER_NONE, 0, 0);
        g_object_unref(image);
    } else {
        gdk_draw_pixbuf(widget->window, NULL, obj, 0, 0, destx, desty, width, height, GDK_RGB_DITHER_NONE, 0, 0);
    }
}

void ui_skinned_widget_draw(GtkWidget *widget, GdkPixbuf *obj, gint width, gint height, gboolean scale) {
    ui_skinned_widget_draw_with_coordinates(widget, obj, width, height, 0, 0, scale);
}
