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
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#ifndef AUDACIOUS_UI_SKINNED_PLAYSTATUS_H
#define AUDACIOUS_UI_SKINNED_PLAYSTATUS_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_SKINNED_PLAYSTATUS(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, ui_skinned_playstatus_get_type (), UiSkinnedPlaystatus)
#define UI_SKINNED_PLAYSTATUS_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, ui_skinned_playstatus_get_type (), UiSkinnedPlaystatusClass)
#define UI_SKINNED_IS_PLAYSTATUS(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, ui_skinned_playstatus_get_type ())

typedef struct _UiSkinnedPlaystatus        UiSkinnedPlaystatus;
typedef struct _UiSkinnedPlaystatusClass   UiSkinnedPlaystatusClass;

typedef enum {
    STATUS_STOP, STATUS_PAUSE, STATUS_PLAY
} PStatus;

struct _UiSkinnedPlaystatus {
    GtkWidget        widget;

    gint             x, y, width, height;
    gboolean         scaled;
    PStatus          status;
    gboolean         buffering;
};

struct _UiSkinnedPlaystatusClass {
    GtkWidgetClass          parent_class;
    void (* scaled)        (UiSkinnedPlaystatus *menurow);
};

GtkWidget* ui_skinned_playstatus_new (GtkWidget *fixed, gint x, gint y);
GType ui_skinned_playstatus_get_type(void);
void ui_skinned_playstatus_set_status(GtkWidget *widget, PStatus status);
void ui_skinned_playstatus_set_buffering(GtkWidget *widget, gboolean status);
void ui_skinned_playstatus_set_size(GtkWidget *widget, gint width, gint height);

#ifdef __cplusplus
}
#endif

#endif /* AUDACIOUS_UI_SKINNED_PLAYSTATUS_H */
