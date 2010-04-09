/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 * XMMS:
 * Copyright (C) 1998-2003  XMMS development team.
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
 * along with this program;  If not, see <http://www.gnu.org/licenses>.
 */

#include "ui_skin.h"
#include "ui_skinned_equalizer_graph.h"
#include "skins_cfg.h"
#include "util.h"

#include <audacious/plugin.h>

#define UI_TYPE_SKINNED_EQUALIZER_GRAPH           (ui_skinned_equalizer_graph_get_type())

enum {
    DOUBLED,
    LAST_SIGNAL
};

static void ui_skinned_equalizer_graph_class_init         (UiSkinnedEqualizerGraphClass *klass);
static void ui_skinned_equalizer_graph_init               (UiSkinnedEqualizerGraph *equalizer_graph);
static void ui_skinned_equalizer_graph_destroy            (GtkObject *object);
static void ui_skinned_equalizer_graph_realize            (GtkWidget *widget);
static void ui_skinned_equalizer_graph_size_request       (GtkWidget *widget, GtkRequisition *requisition);
static void ui_skinned_equalizer_graph_size_allocate      (GtkWidget *widget, GtkAllocation *allocation);
static gboolean ui_skinned_equalizer_graph_expose         (GtkWidget *widget, GdkEventExpose *event);
static void ui_skinned_equalizer_graph_toggle_scaled  (UiSkinnedEqualizerGraph *equalizer_graph);

static GtkWidgetClass *parent_class = NULL;
static guint equalizer_graph_signals[LAST_SIGNAL] = { 0 };

GType ui_skinned_equalizer_graph_get_type() {
    static GType equalizer_graph_type = 0;
    if (!equalizer_graph_type) {
        static const GTypeInfo equalizer_graph_info = {
            sizeof (UiSkinnedEqualizerGraphClass),
            NULL,
            NULL,
            (GClassInitFunc) ui_skinned_equalizer_graph_class_init,
            NULL,
            NULL,
            sizeof (UiSkinnedEqualizerGraph),
            0,
            (GInstanceInitFunc) ui_skinned_equalizer_graph_init,
        };
        equalizer_graph_type = g_type_register_static (GTK_TYPE_WIDGET, "UiSkinnedEqualizerGraph", &equalizer_graph_info, 0);
    }

    return equalizer_graph_type;
}

static void ui_skinned_equalizer_graph_class_init(UiSkinnedEqualizerGraphClass *klass) {
    GObjectClass *gobject_class;
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(klass);
    object_class = (GtkObjectClass*) klass;
    widget_class = (GtkWidgetClass*) klass;
    parent_class = g_type_class_peek_parent(klass);

    object_class->destroy = ui_skinned_equalizer_graph_destroy;

    widget_class->realize = ui_skinned_equalizer_graph_realize;
    widget_class->expose_event = ui_skinned_equalizer_graph_expose;
    widget_class->size_request = ui_skinned_equalizer_graph_size_request;
    widget_class->size_allocate = ui_skinned_equalizer_graph_size_allocate;

    klass->scaled = ui_skinned_equalizer_graph_toggle_scaled;

    equalizer_graph_signals[DOUBLED] =
        g_signal_new ("toggle-scaled", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedEqualizerGraphClass, scaled), NULL, NULL,
                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void ui_skinned_equalizer_graph_init(UiSkinnedEqualizerGraph *equalizer_graph) {
    equalizer_graph->width = 113;
    equalizer_graph->height = 19;

    GTK_WIDGET_SET_FLAGS(equalizer_graph, GTK_NO_WINDOW);
}

GtkWidget* ui_skinned_equalizer_graph_new(GtkWidget *fixed, gint x, gint y) {
    UiSkinnedEqualizerGraph *equalizer_graph = g_object_new (ui_skinned_equalizer_graph_get_type (), NULL);

    equalizer_graph->x = x;
    equalizer_graph->y = y;
    equalizer_graph->skin_index = SKIN_EQMAIN;
    equalizer_graph->scaled = FALSE;

    gtk_fixed_put(GTK_FIXED(fixed), GTK_WIDGET(equalizer_graph), equalizer_graph->x, equalizer_graph->y);

    return GTK_WIDGET(equalizer_graph);
}

static void ui_skinned_equalizer_graph_destroy(GtkObject *object) {
    UiSkinnedEqualizerGraph *equalizer_graph;

    g_return_if_fail (object != NULL);
    g_return_if_fail (UI_SKINNED_IS_EQUALIZER_GRAPH (object));

    equalizer_graph = UI_SKINNED_EQUALIZER_GRAPH (object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void ui_skinned_equalizer_graph_realize(GtkWidget *widget) {
    if (GTK_WIDGET_CLASS (parent_class)->realize)
        (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
}

static void ui_skinned_equalizer_graph_size_request(GtkWidget *widget, GtkRequisition *requisition) {
    UiSkinnedEqualizerGraph *equalizer_graph = UI_SKINNED_EQUALIZER_GRAPH(widget);

    requisition->width = equalizer_graph->width*(equalizer_graph->scaled ? config.scale_factor : 1);
    requisition->height = equalizer_graph->height*(equalizer_graph->scaled ? config.scale_factor : 1);
}

static void ui_skinned_equalizer_graph_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
    UiSkinnedEqualizerGraph *equalizer_graph = UI_SKINNED_EQUALIZER_GRAPH (widget);

    widget->allocation = *allocation;
    widget->allocation.x *= (equalizer_graph->scaled ? config.scale_factor : 1);
    widget->allocation.y *= (equalizer_graph->scaled ? config.scale_factor : 1);

    equalizer_graph->x = widget->allocation.x/(equalizer_graph->scaled ? config.scale_factor : 1);
    equalizer_graph->y = widget->allocation.y/(equalizer_graph->scaled ? config.scale_factor : 1);
}

void
init_spline(gfloat * x, gfloat * y, gint n, gfloat * y2)
{
    gint i, k;
    gfloat p, qn, sig, un, *u;

    u = (gfloat *) g_malloc(n * sizeof(gfloat));

    y2[0] = u[0] = 0.0;

    for (i = 1; i < n - 1; i++) {
        sig = ((gfloat) x[i] - x[i - 1]) / ((gfloat) x[i + 1] - x[i - 1]);
        p = sig * y2[i - 1] + 2.0;
        y2[i] = (sig - 1.0) / p;
        u[i] =
            (((gfloat) y[i + 1] - y[i]) / (x[i + 1] - x[i])) -
            (((gfloat) y[i] - y[i - 1]) / (x[i] - x[i - 1]));
        u[i] = (6.0 * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
    }
    qn = un = 0.0;

    y2[n - 1] = (un - qn * u[n - 2]) / (qn * y2[n - 2] + 1.0);
    for (k = n - 2; k >= 0; k--)
        y2[k] = y2[k] * y2[k + 1] + u[k];
    g_free(u);
}

gfloat
eval_spline(gfloat xa[], gfloat ya[], gfloat y2a[], gint n, gfloat x)
{
    gint klo, khi, k;
    gfloat h, b, a;

    klo = 0;
    khi = n - 1;
    while (khi - klo > 1) {
        k = (khi + klo) >> 1;
        if (xa[k] > x)
            khi = k;
        else
            klo = k;
    }
    h = xa[khi] - xa[klo];
    a = (xa[khi] - x) / h;
    b = (x - xa[klo]) / h;
    return (a * ya[klo] + b * ya[khi] +
            ((a * a * a - a) * y2a[klo] +
             (b * b * b - b) * y2a[khi]) * (h * h) / 6.0);
}

static gboolean ui_skinned_equalizer_graph_expose(GtkWidget *widget, GdkEventExpose *event) {
    UiSkinnedEqualizerGraph *equalizer_graph = UI_SKINNED_EQUALIZER_GRAPH (widget);
    g_return_val_if_fail (equalizer_graph->width > 0 && equalizer_graph->height > 0, FALSE);

    GdkPixbuf *obj = NULL;

    obj = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, equalizer_graph->width, equalizer_graph->height);

    guint32 cols[19], rowstride;
    gint i, y, ymin, ymax, py = 0;
    gfloat x[] = { 0, 11, 23, 35, 47, 59, 71, 83, 97, 109 }, yf[10];
    guchar* pixels, *p;
    gint n_channels;
    /*
     * This avoids the init_spline() function to be inlined.
     * Inlining the function caused troubles when compiling with
     * `-O' (at least on FreeBSD).
     */
    void (*__init_spline) (gfloat *, gfloat *, gint, gfloat *) = init_spline;

    skin_draw_pixbuf(widget, aud_active_skin, obj, equalizer_graph->skin_index, 0, 294, 0, 0,
                     equalizer_graph->width, equalizer_graph->height);
    skin_draw_pixbuf(widget, aud_active_skin, obj, equalizer_graph->skin_index, 0, 314,
                     0, 9 + ((aud_cfg->equalizer_preamp * 9) / 20),
                     equalizer_graph->width, 1);

    skin_get_eq_spline_colors(aud_active_skin, cols);

    __init_spline(x, aud_cfg->equalizer_bands, 10, yf);
    for (i = 0; i < 109; i++) {
        y = 9 -
            (gint) ((eval_spline(x, aud_cfg->equalizer_bands, yf, 10, i) *
                     9.0) / EQUALIZER_MAX_GAIN);
        if (y < 0)
            y = 0;
        if (y > 18)
            y = 18;
        if (!i)
            py = y;
        if (y < py) {
            ymin = y;
            ymax = py;
        }
        else {
            ymin = py;
            ymax = y;
        }
        py = y;

        pixels = gdk_pixbuf_get_pixels(obj);
        rowstride = gdk_pixbuf_get_rowstride(obj);
        n_channels = gdk_pixbuf_get_n_channels(obj);

        for (y = ymin; y <= ymax; y++)
        {
            p = pixels + (y * rowstride) + (( i + 2) * n_channels);
            p[0] = (cols[y] & 0xff0000) >> 16;
            p[1] = (cols[y] & 0x00ff00) >> 8;
            p[2] = (cols[y] & 0x0000ff);
            /* do we really need to treat the alpha channel? */
            /*if (n_channels == 4)
                  p[3] = cols[y] >> 24;*/
        }
    }

    ui_skinned_widget_draw_with_coordinates(widget, obj, equalizer_graph->width, equalizer_graph->height,
                                            widget->allocation.x,
                                            widget->allocation.y,
                                            equalizer_graph->scaled);

    g_object_unref(obj);

    return FALSE;
}

static void ui_skinned_equalizer_graph_toggle_scaled(UiSkinnedEqualizerGraph *equalizer_graph) {
    GtkWidget *widget = GTK_WIDGET (equalizer_graph);

    equalizer_graph->scaled = !equalizer_graph->scaled;
    gtk_widget_set_size_request(widget, equalizer_graph->width*(equalizer_graph->scaled ? config.scale_factor : 1),
                                        equalizer_graph->height*(equalizer_graph->scaled ? config.scale_factor : 1));

    if (widget_really_drawable (widget))
        ui_skinned_equalizer_graph_expose (widget, NULL);
}

void ui_skinned_equalizer_graph_update (GtkWidget * graph)
{
    if (widget_really_drawable (graph))
        ui_skinned_equalizer_graph_expose (graph, NULL);
}
