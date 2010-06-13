/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007  Audacious development team.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#ifndef UIVIS_H
#define UIVIS_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_VIS(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, ui_vis_get_type (), UiVis)
#define UI_VIS_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, ui_vis_get_type (), UiVisClass)
#define UI_IS_VIS(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, ui_vis_get_type ())

typedef enum {
    VIS_ANALYZER, VIS_SCOPE, VIS_VOICEPRINT, VIS_OFF
} VisType;

typedef enum {
    ANALYZER_NORMAL, ANALYZER_FIRE, ANALYZER_VLINES
} AnalyzerMode;

typedef enum {
    ANALYZER_LINES, ANALYZER_BARS
} AnalyzerType;

typedef enum {
    SCOPE_DOT, SCOPE_LINE, SCOPE_SOLID
} ScopeMode;
typedef enum {
  VOICEPRINT_NORMAL, VOICEPRINT_FIRE, VOICEPRINT_ICE
} VoiceprintMode;


typedef enum {
    VU_NORMAL, VU_SMOOTH
} VUMode;

typedef enum {
    REFRESH_FULL, REFRESH_HALF, REFRESH_QUARTER, REFRESH_EIGTH
} RefreshRate;

typedef enum {
    FALLOFF_SLOWEST, FALLOFF_SLOW, FALLOFF_MEDIUM, FALLOFF_FAST,
    FALLOFF_FASTEST
} FalloffSpeed;

typedef struct _UiVis        UiVis;
typedef struct _UiVisClass   UiVisClass;

struct _UiVis {
    GtkWidget        widget;

    gint             x, y, width, height;
    gfloat           data[75], peak[75], peak_speed[75];
    gboolean         scaled;
    GtkWidget        *fixed;
    gboolean         visible_window;
    GdkWindow        *event_window;
    GdkGC * gc;
};

struct _UiVisClass {
    GtkWidgetClass          parent_class;
    void (* doubled)        (UiVis *vis);
};

GtkWidget* ui_vis_new (GtkWidget *fixed, gint x, gint y, gint width);
GType ui_vis_get_type(void);
void ui_vis_set_colors (void);
void ui_vis_clear_data(GtkWidget *widget);
void ui_vis_timeout_func(GtkWidget *widget, guchar * data);

#ifdef __cplusplus
}
#endif

#endif
