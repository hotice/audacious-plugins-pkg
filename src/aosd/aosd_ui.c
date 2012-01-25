/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2007
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#include <gtk/gtk.h>
#include <audacious/gtk-compat.h>
#include <audacious/i18n.h>

#include "aosd_ui.h"
#include "aosd_style.h"
#include "aosd_trigger.h"
#include "aosd_cfg.h"
#include "aosd_osd.h"
#include "aosd_common.h"

extern aosd_cfg_t * global_config;
extern gboolean plugin_is_active;


/*************************************************************/
/* small callback system used by the configuration interface */
typedef void (*aosd_ui_cb_func_t)( GtkWidget * , aosd_cfg_t * );

typedef struct
{
  aosd_ui_cb_func_t func;
  GtkWidget * widget;
}
aosd_ui_cb_t;

static void
aosd_callback_list_add ( GList ** list , GtkWidget * widget , aosd_ui_cb_func_t func )
{
  aosd_ui_cb_t *cb = g_malloc(sizeof(aosd_ui_cb_t));
  cb->widget = widget;
  cb->func = func;
  *list = g_list_append( *list , cb );
  return;
}

static void
aosd_callback_list_run ( GList * list , aosd_cfg_t * cfg )
{
  while ( list != NULL )
  {
    aosd_ui_cb_t *cb = (aosd_ui_cb_t*)list->data;
    cb->func( cb->widget , cfg );
    list = g_list_next( list );
  }
  return;
}

static void
aosd_callback_list_free ( GList * list )
{
  GList *list_top = list;
  while ( list != NULL )
  {
    g_free( (aosd_ui_cb_t*)list->data );
    list = g_list_next( list );
  }
  g_list_free( list_top );
  return;
}
/*************************************************************/



static gboolean
aosd_cb_configure_position_expose ( GtkWidget * darea ,
#if GTK_CHECK_VERSION (3, 0, 0)
                                    cairo_t * cr ,
#else
                                    GdkEventExpose * event ,
#endif
                                    gpointer coord_gp )
{
  gint coord = GPOINTER_TO_INT(coord_gp);

#if GTK_CHECK_VERSION (3, 0, 0)
  cairo_set_source_rgb ( cr , 0 , 0 , 0 );
  cairo_rectangle ( cr , (coord % 3) * 10 , (coord / 3) * 16 , 20 , 8 );
  cairo_fill ( cr );
#else
  gdk_draw_rectangle( GDK_DRAWABLE(gtk_widget_get_window (darea)) ,
                      gtk_widget_get_style (darea)->black_gc , TRUE ,
                      (coord % 3) * 10 , (coord / 3) * 16 , 20 , 8 );
#endif

  return FALSE;
}


static void
aosd_cb_configure_position_placement_commit ( GtkWidget * table , aosd_cfg_t * cfg )
{
  GList *placbt_list = gtk_container_get_children( GTK_CONTAINER(table) );
  GList *list_iter = placbt_list;

  while ( list_iter != NULL )
  {
    GtkWidget *placbt = list_iter->data;
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(placbt) ) == TRUE )
    {
      cfg->osd->position.placement = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(placbt),"value"));
      break;
    }
    list_iter = g_list_next( list_iter );
  }

  g_list_free( placbt_list );
  return;
}


static void
aosd_cb_configure_position_offset_commit ( GtkWidget * table , aosd_cfg_t * cfg )
{
  cfg->osd->position.offset_x = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(table),"offx")) );
  cfg->osd->position.offset_y = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(table),"offy")) );
  return;
}


static void
aosd_cb_configure_position_maxsize_commit ( GtkWidget * table , aosd_cfg_t * cfg )
{
  cfg->osd->position.maxsize_width = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(table),"maxsize_width")) );
  return;
}


static void
aosd_cb_configure_position_multimon_commit ( GtkWidget * combo , aosd_cfg_t * cfg )
{
  gint active = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
  cfg->osd->position.multimon_id = ( active > -1 ) ? (active - 1) : -1;
  return;
}


static GtkWidget *
aosd_ui_configure_position ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *pos_vbox;
  GtkWidget *pos_placement_frame, *pos_placement_hbox, *pos_placement_table;
  GtkWidget *pos_placement_bt[9], *pos_placement_bt_darea[9];
  GtkWidget *pos_offset_table, *pos_offset_x_label, *pos_offset_x_spinbt;
  GtkWidget *pos_offset_y_label, *pos_offset_y_spinbt;
  GtkWidget *pos_maxsize_width_label, *pos_maxsize_width_spinbt;
  GtkWidget *pos_multimon_frame, *pos_multimon_hbox;
  GtkWidget *pos_multimon_label;
  GtkWidget *pos_multimon_combobox;
  gint monitors_num = gdk_screen_get_n_monitors( gdk_screen_get_default() );
  gint i = 0;

  pos_vbox = gtk_vbox_new( FALSE , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(pos_vbox) , 6 );

  pos_placement_frame = gtk_frame_new( _("Placement") );
  pos_placement_hbox = gtk_hbox_new( FALSE , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(pos_placement_hbox) , 6 );
  gtk_container_add( GTK_CONTAINER(pos_placement_frame) , pos_placement_hbox );
  gtk_box_pack_start( GTK_BOX(pos_vbox) , pos_placement_frame , FALSE , FALSE , 0 );

  pos_placement_table = gtk_table_new( 3 , 3 , TRUE );
  for ( i = 0 ; i < 9 ; i++ )
  {
    if ( i == 0 )
      pos_placement_bt[i] = gtk_radio_button_new( NULL );
    else
      pos_placement_bt[i] = gtk_radio_button_new_from_widget( GTK_RADIO_BUTTON(pos_placement_bt[0]) );
    gtk_toggle_button_set_mode( GTK_TOGGLE_BUTTON(pos_placement_bt[i]) , FALSE );
    pos_placement_bt_darea[i] = gtk_drawing_area_new();
    gtk_widget_set_size_request( pos_placement_bt_darea[i] , 40 , 40 );
    gtk_container_add( GTK_CONTAINER(pos_placement_bt[i]) , pos_placement_bt_darea[i] );
#if GTK_CHECK_VERSION (3, 0, 0)
    g_signal_connect( G_OBJECT(pos_placement_bt_darea[i]) , "draw" ,
                      G_CALLBACK(aosd_cb_configure_position_expose) , GINT_TO_POINTER(i) );
#else
    g_signal_connect( G_OBJECT(pos_placement_bt_darea[i]) , "expose-event" ,
                      G_CALLBACK(aosd_cb_configure_position_expose) , GINT_TO_POINTER(i) );
#endif
    gtk_table_attach( GTK_TABLE(pos_placement_table) , pos_placement_bt[i] ,
                      (i % 3) , (i % 3) + 1 , (i / 3) , (i / 3) + 1 ,
                      GTK_FILL , GTK_FILL , 0 , 0 );
    g_object_set_data( G_OBJECT(pos_placement_bt[i]) , "value" , GINT_TO_POINTER(i+1) );
    if ( cfg->osd->position.placement == (i+1) )
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(pos_placement_bt[i]) , TRUE );
  }
  gtk_box_pack_start( GTK_BOX(pos_placement_hbox) , pos_placement_table , FALSE , FALSE , 0 );
  aosd_callback_list_add( cb_list , pos_placement_table , aosd_cb_configure_position_placement_commit );

  gtk_box_pack_start( GTK_BOX(pos_placement_hbox) , gtk_vseparator_new() , FALSE , FALSE , 6 );

  pos_offset_table = gtk_table_new( 3 , 2 , FALSE );
  gtk_table_set_row_spacings( GTK_TABLE(pos_offset_table) , 4 );
  gtk_table_set_col_spacings( GTK_TABLE(pos_offset_table) , 4 );
  pos_offset_x_label = gtk_label_new( _( "Relative X offset:" ) );
  gtk_misc_set_alignment( GTK_MISC(pos_offset_x_label) , 0 , 0.5 );
  gtk_table_attach( GTK_TABLE(pos_offset_table) , pos_offset_x_label ,
                    0 , 1 , 0 , 1 , GTK_FILL , GTK_FILL , 0 , 0 );
  pos_offset_x_spinbt = gtk_spin_button_new_with_range( -9999 , 9999 , 1 );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(pos_offset_x_spinbt) , cfg->osd->position.offset_x );
  gtk_table_attach( GTK_TABLE(pos_offset_table) , pos_offset_x_spinbt ,
                    1 , 2 , 0 , 1 , GTK_FILL , GTK_FILL , 0 , 0 );
  g_object_set_data( G_OBJECT(pos_offset_table) , "offx" , pos_offset_x_spinbt );
  pos_offset_y_label = gtk_label_new( _( "Relative Y offset:" ) );
  gtk_misc_set_alignment( GTK_MISC(pos_offset_y_label) , 0 , 0.5 );
  gtk_table_attach( GTK_TABLE(pos_offset_table) , pos_offset_y_label ,
                    0 , 1 , 1 , 2 , GTK_FILL , GTK_FILL , 0 , 0 );
  pos_offset_y_spinbt = gtk_spin_button_new_with_range( -9999 , 9999 , 1 );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(pos_offset_y_spinbt) , cfg->osd->position.offset_y );
  gtk_table_attach( GTK_TABLE(pos_offset_table) , pos_offset_y_spinbt ,
                    1 , 2 , 1 , 2 , GTK_FILL , GTK_FILL , 0 , 0 );
  g_object_set_data( G_OBJECT(pos_offset_table) , "offy" , pos_offset_y_spinbt );
  pos_maxsize_width_label = gtk_label_new( _("Max OSD width:") );
  gtk_misc_set_alignment( GTK_MISC(pos_maxsize_width_label) , 0 , 0.5 );
  gtk_table_attach( GTK_TABLE(pos_offset_table) , pos_maxsize_width_label ,
                    0 , 1 , 2 , 3 , GTK_FILL , GTK_FILL , 0 , 0 );
  pos_maxsize_width_spinbt = gtk_spin_button_new_with_range( 0 , 99999 , 1 );
  g_object_set_data( G_OBJECT(pos_offset_table) , "maxsize_width" , pos_maxsize_width_spinbt );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(pos_maxsize_width_spinbt) , cfg->osd->position.maxsize_width );
  gtk_table_attach( GTK_TABLE(pos_offset_table) , pos_maxsize_width_spinbt ,
                    1 , 2 , 2 , 3 , GTK_FILL , GTK_FILL , 0 , 0 );
  gtk_box_pack_start( GTK_BOX(pos_placement_hbox) , pos_offset_table , FALSE , FALSE , 0 );
  aosd_callback_list_add( cb_list , pos_offset_table , aosd_cb_configure_position_offset_commit );
  aosd_callback_list_add( cb_list , pos_offset_table , aosd_cb_configure_position_maxsize_commit );

  pos_multimon_frame = gtk_frame_new( _("Multi-Monitor options") );
  pos_multimon_hbox = gtk_hbox_new( FALSE , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(pos_multimon_hbox) , 6 );
  gtk_container_add( GTK_CONTAINER(pos_multimon_frame), pos_multimon_hbox );
  pos_multimon_label = gtk_label_new( _("Display OSD using:") );
#if GTK_CHECK_VERSION (3, 0, 0)
  #define CBT_NEW gtk_combo_box_text_new
  #define CBT_ADD(widget, text) \
    gtk_combo_box_text_append( GTK_COMBO_BOX_TEXT(widget) , NULL , text )
#else
  #define CBT_NEW gtk_combo_box_new_text
  #define CBT_ADD(widget, text) \
    gtk_combo_box_append_text( GTK_COMBO_BOX(widget) , text )
#endif
  pos_multimon_combobox = CBT_NEW();
  CBT_ADD( pos_multimon_combobox , _("all monitors") );
  for ( i = 0 ; i < monitors_num ; i++ )
  {
    gchar *mon_str = g_strdup_printf( _("monitor %i") , i + 1 );
    CBT_ADD( pos_multimon_combobox , mon_str );
    g_free( mon_str );
  }
#undef CBT_NEW
#undef CBT_ADD
  gtk_combo_box_set_active( GTK_COMBO_BOX(pos_multimon_combobox) , (cfg->osd->position.multimon_id + 1) );
  aosd_callback_list_add( cb_list , pos_multimon_combobox , aosd_cb_configure_position_multimon_commit );
  gtk_box_pack_start( GTK_BOX(pos_multimon_hbox) , pos_multimon_label , FALSE , FALSE , 0 );
  gtk_box_pack_start( GTK_BOX(pos_multimon_hbox) , pos_multimon_combobox , FALSE , FALSE , 0 );
  gtk_box_pack_start( GTK_BOX(pos_vbox) , pos_multimon_frame , FALSE , FALSE , 0 );

  return pos_vbox;
}


static GtkWidget *
aosd_ui_configure_animation_timing ( gchar * label_string )
{
  GtkWidget *hbox, *desc_label, *spinbt;
  hbox = gtk_hbox_new( FALSE , 4 );
  desc_label = gtk_label_new( label_string );
  spinbt = gtk_spin_button_new_with_range( 0 , 99999 , 1 );
  gtk_box_pack_start( GTK_BOX(hbox) , desc_label , FALSE , FALSE , 0 );
  gtk_box_pack_start( GTK_BOX(hbox) , spinbt , FALSE , FALSE , 0 );
  g_object_set_data( G_OBJECT(hbox) , "spinbt" , spinbt );
  return hbox;
}


static void
aosd_cb_configure_animation_timing_commit ( GtkWidget * timing_hbox , aosd_cfg_t * cfg )
{
  cfg->osd->animation.timing_display = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(timing_hbox),"display")) );
  cfg->osd->animation.timing_fadein = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(timing_hbox),"fadein")) );
  cfg->osd->animation.timing_fadeout = gtk_spin_button_get_value_as_int(
    GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(timing_hbox),"fadeout")) );
  return;
}


static GtkWidget *
aosd_ui_configure_animation ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *ani_vbox;
  GtkWidget *ani_timing_frame, *ani_timing_hbox;
  GtkWidget *ani_timing_fadein_widget, *ani_timing_fadeout_widget, *ani_timing_stay_widget;
  GtkSizeGroup *sizegroup;

  ani_vbox = gtk_vbox_new( FALSE , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(ani_vbox) , 6 );

  ani_timing_hbox = gtk_hbox_new( FALSE , 0 );
  ani_timing_frame = gtk_frame_new( _("Timing (ms)") );
  gtk_container_set_border_width( GTK_CONTAINER(ani_timing_hbox) , 6 );
  gtk_container_add( GTK_CONTAINER(ani_timing_frame) , ani_timing_hbox );
  gtk_box_pack_start( GTK_BOX(ani_vbox) , ani_timing_frame , FALSE , FALSE , 0 );

  ani_timing_stay_widget = aosd_ui_configure_animation_timing( _("Display:") );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_object_get_data(
    G_OBJECT(ani_timing_stay_widget),"spinbt")) , cfg->osd->animation.timing_display );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , ani_timing_stay_widget , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , gtk_vseparator_new() , FALSE , FALSE , 4 );
  ani_timing_fadein_widget = aosd_ui_configure_animation_timing( _("Fade in:") );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_object_get_data(
    G_OBJECT(ani_timing_fadein_widget),"spinbt")) , cfg->osd->animation.timing_fadein );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , ani_timing_fadein_widget , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , gtk_vseparator_new() , FALSE , FALSE , 4 );
  ani_timing_fadeout_widget = aosd_ui_configure_animation_timing( _("Fade out:") );
  gtk_spin_button_set_value( GTK_SPIN_BUTTON(g_object_get_data(
    G_OBJECT(ani_timing_fadeout_widget),"spinbt")) , cfg->osd->animation.timing_fadeout );
  gtk_box_pack_start( GTK_BOX(ani_timing_hbox) , ani_timing_fadeout_widget , TRUE , TRUE , 0 );
  g_object_set_data( G_OBJECT(ani_timing_hbox) , "display" ,
    g_object_get_data(G_OBJECT(ani_timing_stay_widget),"spinbt") );
  g_object_set_data( G_OBJECT(ani_timing_hbox) , "fadein" ,
    g_object_get_data(G_OBJECT(ani_timing_fadein_widget),"spinbt") );
  g_object_set_data( G_OBJECT(ani_timing_hbox) , "fadeout" ,
    g_object_get_data(G_OBJECT(ani_timing_fadeout_widget),"spinbt") );
  sizegroup = gtk_size_group_new( GTK_SIZE_GROUP_HORIZONTAL );
  gtk_size_group_add_widget( sizegroup , ani_timing_stay_widget );
  gtk_size_group_add_widget( sizegroup , ani_timing_fadein_widget );
  gtk_size_group_add_widget( sizegroup , ani_timing_fadeout_widget );
  aosd_callback_list_add( cb_list , ani_timing_hbox , aosd_cb_configure_animation_timing_commit );

  return ani_vbox;
}


static void
aosd_cb_configure_text_font_shadow_toggle ( GtkToggleButton * shadow_togglebt ,
                                            gpointer shadow_colorbt )
{
  if ( gtk_toggle_button_get_active( shadow_togglebt ) == TRUE )
    gtk_widget_set_sensitive( GTK_WIDGET(shadow_colorbt) , TRUE );
  else
    gtk_widget_set_sensitive( GTK_WIDGET(shadow_colorbt) , FALSE );
  return;
}


static void
aosd_cb_configure_text_font_commit ( GtkWidget * fontbt , aosd_cfg_t * cfg )
{
  gint fontnum = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(fontbt) , "fontnum" ));
  GdkColor color, shadow_color;
  cfg->osd->text.fonts_name[fontnum] = g_strdup( gtk_font_button_get_font_name(GTK_FONT_BUTTON(fontbt)) );
  gtk_color_button_get_color(
    GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(fontbt),"color")) , &color );
  cfg->osd->text.fonts_color[fontnum].red = color.red;
  cfg->osd->text.fonts_color[fontnum].green = color.green;
  cfg->osd->text.fonts_color[fontnum].blue = color.blue;
  cfg->osd->text.fonts_color[fontnum].alpha = gtk_color_button_get_alpha(
    GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(fontbt),"color")) );
  cfg->osd->text.fonts_draw_shadow[fontnum] = gtk_toggle_button_get_active(
    GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(fontbt),"use_shadow")) );
  gtk_color_button_get_color(
    GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(fontbt),"shadow_color")) , &shadow_color );
  cfg->osd->text.fonts_shadow_color[fontnum].red = shadow_color.red;
  cfg->osd->text.fonts_shadow_color[fontnum].green = shadow_color.green;
  cfg->osd->text.fonts_shadow_color[fontnum].blue = shadow_color.blue;
  cfg->osd->text.fonts_shadow_color[fontnum].alpha = gtk_color_button_get_alpha(
    GTK_COLOR_BUTTON(g_object_get_data(G_OBJECT(fontbt),"shadow_color")) );
  return;
}


static void
aosd_cb_configure_text_inte_commit ( GtkWidget *utf8conv_togglebt , aosd_cfg_t * cfg )
{
  cfg->osd->text.utf8conv_disable = gtk_toggle_button_get_active(
    GTK_TOGGLE_BUTTON(utf8conv_togglebt) );
  return;
}


static GtkWidget *
aosd_ui_configure_text ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *tex_vbox;
  GtkWidget *tex_font_table, *tex_font_frame;
  GtkWidget *tex_font_label[3], *tex_font_fontbt[3];
  GtkWidget *tex_font_colorbt[3], *tex_font_shadow_togglebt[3];
  GtkWidget *tex_font_shadow_colorbt[3];
  GtkWidget *tex_inte_frame, *tex_inte_table, *tex_inte_utf8conv_togglebt;
  gint i = 0;

  tex_vbox = gtk_vbox_new( FALSE , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(tex_vbox) , 6 );

  tex_font_frame = gtk_frame_new( _("Fonts") );
  tex_font_table = gtk_table_new( 3 , 5 , FALSE );
  gtk_container_set_border_width( GTK_CONTAINER(tex_font_table) , 6 );
  gtk_table_set_row_spacings( GTK_TABLE(tex_font_table) , 4 );
  gtk_table_set_col_spacings( GTK_TABLE(tex_font_table) , 4 );
  for ( i = 0 ; i < AOSD_TEXT_FONTS_NUM ; i++ )
  {
    GdkColor gcolor = { 0 , 0 , 0 , 0 };
    gchar *label_str = g_strdup_printf( _("Font %i:") , i+1 );
    tex_font_label[i] = gtk_label_new( label_str );
    g_free( label_str );
    tex_font_fontbt[i] = gtk_font_button_new();
    gtk_font_button_set_show_style( GTK_FONT_BUTTON(tex_font_fontbt[i]) , TRUE );
    gtk_font_button_set_show_size( GTK_FONT_BUTTON(tex_font_fontbt[i]) , TRUE );
    gtk_font_button_set_use_font( GTK_FONT_BUTTON(tex_font_fontbt[i]) , FALSE );
    gtk_font_button_set_use_size( GTK_FONT_BUTTON(tex_font_fontbt[i]) , FALSE );
    gtk_font_button_set_font_name( GTK_FONT_BUTTON(tex_font_fontbt[i]) , cfg->osd->text.fonts_name[i] );
    tex_font_colorbt[i] = gtk_color_button_new();
    gcolor.red = cfg->osd->text.fonts_color[i].red;
    gcolor.green = cfg->osd->text.fonts_color[i].green;
    gcolor.blue = cfg->osd->text.fonts_color[i].blue;
    gtk_color_button_set_use_alpha( GTK_COLOR_BUTTON(tex_font_colorbt[i]) , TRUE );
    gtk_color_button_set_color( GTK_COLOR_BUTTON(tex_font_colorbt[i]) , &gcolor );
    gtk_color_button_set_alpha( GTK_COLOR_BUTTON(tex_font_colorbt[i]) ,
      cfg->osd->text.fonts_color[i].alpha );
    tex_font_shadow_togglebt[i] = gtk_toggle_button_new_with_label( _("Shadow") );
    gtk_toggle_button_set_mode( GTK_TOGGLE_BUTTON(tex_font_shadow_togglebt[i]) , FALSE );
    tex_font_shadow_colorbt[i] = gtk_color_button_new();
    gtk_color_button_set_use_alpha( GTK_COLOR_BUTTON(tex_font_shadow_colorbt[i]) , TRUE );
    gcolor.red = cfg->osd->text.fonts_shadow_color[i].red;
    gcolor.green = cfg->osd->text.fonts_shadow_color[i].green;
    gcolor.blue = cfg->osd->text.fonts_shadow_color[i].blue;
    gtk_color_button_set_color( GTK_COLOR_BUTTON(tex_font_shadow_colorbt[i]) , &gcolor );
    gtk_color_button_set_alpha( GTK_COLOR_BUTTON(tex_font_shadow_colorbt[i]) ,
      cfg->osd->text.fonts_shadow_color[i].alpha );
    gtk_widget_set_sensitive( tex_font_shadow_colorbt[i] , FALSE );
    g_signal_connect( G_OBJECT(tex_font_shadow_togglebt[i]) , "toggled" ,
                      G_CALLBACK(aosd_cb_configure_text_font_shadow_toggle) ,
                      tex_font_shadow_colorbt[i] );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(tex_font_shadow_togglebt[i]) ,
      cfg->osd->text.fonts_draw_shadow[i] );
    gtk_table_attach( GTK_TABLE(tex_font_table) , tex_font_label[i] ,
      0 , 1 , i , i + 1 , GTK_FILL , GTK_FILL , 0 , 0 );
    gtk_table_attach( GTK_TABLE(tex_font_table) , tex_font_fontbt[i] ,
      1 , 2 , i , i + 1 , GTK_FILL | GTK_EXPAND , GTK_FILL , 0 , 0 );
    gtk_table_attach( GTK_TABLE(tex_font_table) , tex_font_colorbt[i] ,
      2 , 3 , i , i + 1 , GTK_FILL , GTK_FILL , 0 , 0 );
    gtk_table_attach( GTK_TABLE(tex_font_table) , tex_font_shadow_togglebt[i] ,
      3 , 4 , i , i + 1 , GTK_FILL , GTK_FILL , 0 , 0 );
    gtk_table_attach( GTK_TABLE(tex_font_table) , tex_font_shadow_colorbt[i] ,
      4 , 5 , i , i + 1 , GTK_FILL , GTK_FILL , 0 , 0 );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "fontnum" , GINT_TO_POINTER(i) );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "color" , tex_font_colorbt[i] );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "use_shadow" , tex_font_shadow_togglebt[i] );
    g_object_set_data( G_OBJECT(tex_font_fontbt[i]) , "shadow_color" , tex_font_shadow_colorbt[i] );
    aosd_callback_list_add( cb_list , tex_font_fontbt[i] , aosd_cb_configure_text_font_commit );
  }
  gtk_container_add( GTK_CONTAINER(tex_font_frame) , tex_font_table );
  gtk_box_pack_start( GTK_BOX(tex_vbox) , tex_font_frame , FALSE , FALSE , 0 );

  tex_inte_frame = gtk_frame_new( _("Internationalization") );
  tex_inte_table = gtk_table_new( 1 , 1 , FALSE );
  gtk_container_set_border_width( GTK_CONTAINER(tex_inte_table) , 6 );
  gtk_table_set_row_spacings( GTK_TABLE(tex_inte_table) , 4 );
  gtk_table_set_col_spacings( GTK_TABLE(tex_inte_table) , 4 );
  tex_inte_utf8conv_togglebt = gtk_check_button_new_with_label(
    _("Disable UTF-8 conversion of text (in aosd)") );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(tex_inte_utf8conv_togglebt) ,
    cfg->osd->text.utf8conv_disable );
  gtk_table_attach( GTK_TABLE(tex_inte_table) , tex_inte_utf8conv_togglebt ,
    0 , 1 , 0 , 1 , GTK_FILL , GTK_FILL , 0 , 0 );
  aosd_callback_list_add( cb_list , tex_inte_utf8conv_togglebt , aosd_cb_configure_text_inte_commit );
  gtk_container_add( GTK_CONTAINER(tex_inte_frame) , tex_inte_table );
  gtk_box_pack_start( GTK_BOX(tex_vbox) , tex_inte_frame , FALSE , FALSE , 0 );

  return tex_vbox;
}

#if 0
static void
aosd_ui_configure_decoration_browse ( GtkButton * button , gpointer entry )
{
  GtkWidget *dialog;
  GtkWidget *parent_win = gtk_widget_get_toplevel( GTK_WIDGET(button) );
  dialog = gtk_file_chooser_dialog_new ( _("Select Skin File") ,
                                         ( GTK_WIDGET_TOPLEVEL(parent_win) ? GTK_WINDOW(parent_win) : NULL ) ,
                                         GTK_FILE_CHOOSER_ACTION_OPEN ,
                                         GTK_STOCK_CANCEL , GTK_RESPONSE_CANCEL ,
                                         GTK_STOCK_OPEN , GTK_RESPONSE_ACCEPT , NULL );
  if ( gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT )
  {
    gchar *filename;
    filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
    gtk_entry_set_text( GTK_ENTRY(entry) , filename );
    g_free( filename );
  }
  gtk_widget_destroy( dialog );
  return;
}
#endif


static void
aosd_cb_configure_decoration_style_commit ( GtkWidget * lv , aosd_cfg_t * cfg )
{
  GtkTreeSelection *sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(lv) );
  GtkTreeModel *model;
  GtkTreeIter iter;

  if ( gtk_tree_selection_get_selected( sel , &model , &iter ) == TRUE )
  {
    gint deco_code = 0;
    gtk_tree_model_get( model , &iter , 1 , &deco_code , -1 );
    cfg->osd->decoration.code = deco_code;
  }
  return;
}


static void
aosd_cb_configure_decoration_color_commit ( GtkWidget * colorbt , aosd_cfg_t * cfg )
{
  GdkColor gcolor;
  aosd_color_t color;
  gint colnum = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(colorbt) , "colnum" ) );
  gtk_color_button_get_color( GTK_COLOR_BUTTON(colorbt) , &gcolor );
  color.red = gcolor.red;
  color.green = gcolor.green;
  color.blue = gcolor.blue;
  color.alpha = gtk_color_button_get_alpha( GTK_COLOR_BUTTON(colorbt) );
  g_array_insert_val( cfg->osd->decoration.colors , colnum , color );
  return;
}

#if 0
static void
aosd_cb_configure_decoration_skinfile_commit ( GtkWidget * entry , aosd_cfg_t * cfg )
{
  cfg->osd->decoration.skin_file = g_strdup( gtk_entry_get_text(GTK_ENTRY(entry)) );
  return;
}
#endif

static GtkWidget *
aosd_ui_configure_decoration ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *dec_hbox;
  GtkWidget *dec_rstyle_lv, *dec_rstyle_lv_frame, *dec_rstyle_lv_sw;
  GtkListStore *dec_rstyle_store;
  GtkCellRenderer *dec_rstyle_lv_rndr_text;
  GtkTreeViewColumn *dec_rstyle_lv_col_desc;
  GtkTreeSelection *dec_rstyle_lv_sel;
  GtkTreeIter iter, iter_sel;
  GtkWidget *dec_rstyle_hbox;
  GtkWidget *dec_rstyleopts_frame, *dec_rstyleopts_table;
#if 0
  GtkWidget *dec_rstylecustom_frame, *dec_rstylecustom_table;
  GtkWidget *dec_rstylecustom_label, *dec_rstylecustom_entry, *dec_rstylecustom_browse_bt;
#endif
  gint *deco_code_array, deco_code_array_size;
  gint colors_max_num = 0, i = 0;

  dec_hbox = gtk_hbox_new( FALSE , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(dec_hbox) , 6 );

  /* decoration style model
     ---------------------------------------------
     G_TYPE_STRING -> decoration description
     G_TYPE_INT -> decoration code
     G_TYPE_INT -> number of user-definable colors
     ---------------------------------------------
  */
  dec_rstyle_store = gtk_list_store_new( 3 , G_TYPE_STRING , G_TYPE_INT , G_TYPE_INT );
  aosd_deco_style_get_codes_array ( &deco_code_array , &deco_code_array_size );
  for ( i = 0 ; i < deco_code_array_size ; i++ )
  {
    gint colors_num = aosd_deco_style_get_numcol( deco_code_array[i] );
    if ( colors_num > colors_max_num )
      colors_max_num = colors_num;
    gtk_list_store_append( dec_rstyle_store , &iter );
    gtk_list_store_set( dec_rstyle_store , &iter ,
      0 , _(aosd_deco_style_get_desc( deco_code_array[i] )) ,
      1 , deco_code_array[i] , 2 , colors_num , -1 );
    if ( deco_code_array[i] == cfg->osd->decoration.code )
      iter_sel = iter;
  }

  dec_rstyle_lv_frame = gtk_frame_new( NULL );
  dec_rstyle_lv = gtk_tree_view_new_with_model( GTK_TREE_MODEL(dec_rstyle_store) );
  g_object_unref( dec_rstyle_store );
  dec_rstyle_lv_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(dec_rstyle_lv) );
  gtk_tree_selection_set_mode( dec_rstyle_lv_sel , GTK_SELECTION_BROWSE );

  dec_rstyle_lv_rndr_text = gtk_cell_renderer_text_new();
  dec_rstyle_lv_col_desc = gtk_tree_view_column_new_with_attributes(
    _("Render Style") , dec_rstyle_lv_rndr_text , "text" , 0 , NULL );
  gtk_tree_view_append_column( GTK_TREE_VIEW(dec_rstyle_lv), dec_rstyle_lv_col_desc );
  dec_rstyle_lv_sw = gtk_scrolled_window_new( NULL , NULL );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(dec_rstyle_lv_sw) ,
                                  GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );
  gtk_container_add( GTK_CONTAINER(dec_rstyle_lv_sw) , dec_rstyle_lv );
  gtk_container_add( GTK_CONTAINER(dec_rstyle_lv_frame) , dec_rstyle_lv_sw );

  gtk_tree_selection_select_iter( dec_rstyle_lv_sel , &iter_sel );
  gtk_box_pack_start( GTK_BOX(dec_hbox) , dec_rstyle_lv_frame , FALSE , FALSE , 0 );
  aosd_callback_list_add( cb_list , dec_rstyle_lv , aosd_cb_configure_decoration_style_commit );

  dec_rstyle_hbox = gtk_vbox_new( FALSE , 4 );
  gtk_box_pack_start( GTK_BOX(dec_hbox) , dec_rstyle_hbox , TRUE , TRUE , 0 );

  /* in colors_max_num now there's the maximum number of colors used by decoration styles */
  dec_rstyleopts_frame = gtk_frame_new( _("Colors") );
  dec_rstyleopts_table = gtk_table_new( (colors_max_num / 3) + 1 , 3 , TRUE );
  gtk_container_set_border_width( GTK_CONTAINER(dec_rstyleopts_table) , 6 );
  gtk_table_set_row_spacings( GTK_TABLE(dec_rstyleopts_table) , 4 );
  gtk_table_set_col_spacings( GTK_TABLE(dec_rstyleopts_table) , 8 );
  gtk_container_add( GTK_CONTAINER(dec_rstyleopts_frame) , dec_rstyleopts_table );
  for ( i = 0 ; i < colors_max_num ; i++ )
  {
    GtkWidget *colorbt, *hbox, *label;
    aosd_color_t color = g_array_index( cfg->osd->decoration.colors , aosd_color_t , i );
    GdkColor gcolor = { 0 , 0 , 0 , 0 };
    gchar *label_str = NULL;
    hbox = gtk_hbox_new( FALSE , 4 );
    label_str = g_strdup_printf( _("Color %i:") , i+1 );
    label = gtk_label_new( label_str );
    g_free( label_str );
    colorbt = gtk_color_button_new();
    gtk_color_button_set_use_alpha( GTK_COLOR_BUTTON(colorbt) , TRUE );
    gcolor.red = color.red; gcolor.green = color.green; gcolor.blue = color.blue;
    gtk_color_button_set_color( GTK_COLOR_BUTTON(colorbt) , &gcolor );
    gtk_color_button_set_alpha( GTK_COLOR_BUTTON(colorbt) , color.alpha );
    gtk_box_pack_start( GTK_BOX(hbox) , label , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(hbox) , colorbt , FALSE , FALSE , 0 );
    gtk_table_attach( GTK_TABLE(dec_rstyleopts_table) , hbox ,
      (i % 3) , (i % 3) + 1 , (i / 3) , (i / 3) + 1 ,
      GTK_FILL , GTK_FILL , 0 , 0 );
    g_object_set_data( G_OBJECT(colorbt) , "colnum" , GINT_TO_POINTER(i) );
    aosd_callback_list_add( cb_list , colorbt , aosd_cb_configure_decoration_color_commit );
  }
  gtk_box_pack_start( GTK_BOX(dec_rstyle_hbox) , dec_rstyleopts_frame , FALSE , FALSE , 0 );

#if 0
  /* custom skin entry TODO still working on this */
  dec_rstylecustom_frame = gtk_frame_new( _("Custom Skin") );
  dec_rstylecustom_table = gtk_table_new( 1 , 3 , FALSE );
  gtk_container_set_border_width( GTK_CONTAINER(dec_rstylecustom_table) , 6 );
  gtk_table_set_row_spacings( GTK_TABLE(dec_rstylecustom_table) , 4 );
  gtk_table_set_col_spacings( GTK_TABLE(dec_rstylecustom_table) , 4 );
  gtk_container_add( GTK_CONTAINER(dec_rstylecustom_frame) , dec_rstylecustom_table );
  dec_rstylecustom_label = gtk_label_new( _("Skin file:") );
  dec_rstylecustom_entry = gtk_entry_new();
  gtk_entry_set_text( GTK_ENTRY(dec_rstylecustom_entry) , cfg->osd->decoration.skin_file );
  dec_rstylecustom_browse_bt = gtk_button_new_with_label( _("Browse") );
  g_signal_connect( G_OBJECT(dec_rstylecustom_browse_bt) , "clicked" ,
                    G_CALLBACK(aosd_ui_configure_decoration_browse) , dec_rstylecustom_entry );
  gtk_table_attach( GTK_TABLE(dec_rstylecustom_table) , dec_rstylecustom_label ,
    0 , 1 , 0 , 1 , GTK_FILL , GTK_FILL , 0 , 0 );
  gtk_table_attach( GTK_TABLE(dec_rstylecustom_table) , dec_rstylecustom_entry ,
    1 , 2 , 0 , 1 , GTK_FILL | GTK_EXPAND , GTK_FILL , 0 , 0 );
  gtk_table_attach( GTK_TABLE(dec_rstylecustom_table) , dec_rstylecustom_browse_bt ,
    2 , 3 , 0 , 1 , GTK_FILL , GTK_FILL , 0 , 0 );
  aosd_callback_list_add( cb_list , dec_rstylecustom_entry , aosd_cb_configure_decoration_skinfile_commit );

  gtk_box_pack_start( GTK_BOX(dec_rstyle_hbox) , dec_rstylecustom_frame , FALSE , FALSE , 0 );
#endif

  return dec_hbox;
}


static void
aosd_cb_configure_trigger_lvchanged ( GtkTreeSelection *sel , gpointer nb )
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if ( gtk_tree_selection_get_selected( sel , &model , &iter ) == TRUE )
  {
    gint page_num = 0;
    gtk_tree_model_get( model , &iter , 2 , &page_num , -1 );
    gtk_notebook_set_current_page( GTK_NOTEBOOK(nb) , page_num );
  }
  return;
}


static gboolean
aosd_cb_configure_trigger_findinarr ( GArray * array , gint value )
{
  gint i = 0;
  for ( i = 0 ; i < array->len ; i++ )
  {
    if ( g_array_index( array , gint , i ) == value )
      return TRUE;
  }
  return FALSE;
}


static void
aosd_cb_configure_trigger_commit ( GtkWidget * cbt , aosd_cfg_t * cfg )
{
  if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cbt) ) == TRUE )
  {
    gint value = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(cbt),"code"));
    g_array_append_val( cfg->osd->trigger.active , value );
  }
  return;
}


static GtkWidget *
aosd_ui_configure_trigger ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *tri_hbox;
  GtkWidget *tri_event_lv, *tri_event_lv_frame, *tri_event_lv_sw;
  GtkListStore *tri_event_store;
  GtkCellRenderer *tri_event_lv_rndr_text;
  GtkTreeViewColumn *tri_event_lv_col_desc;
  GtkTreeSelection *tri_event_lv_sel;
  GtkTreeIter iter;
  gint *trigger_code_array, trigger_code_array_size;
  GtkWidget *tri_event_nb;
  gint i = 0;

  tri_event_nb = gtk_notebook_new();
  gtk_notebook_set_tab_pos( GTK_NOTEBOOK(tri_event_nb) , GTK_POS_LEFT );
  gtk_notebook_set_show_tabs( GTK_NOTEBOOK(tri_event_nb) , FALSE );
  gtk_notebook_set_show_border( GTK_NOTEBOOK(tri_event_nb) , FALSE );

  tri_hbox = gtk_hbox_new( FALSE , 4 );
  gtk_container_set_border_width( GTK_CONTAINER(tri_hbox) , 6 );

  /* trigger model
     ---------------------------------------------
     G_TYPE_STRING -> trigger description
     G_TYPE_INT -> trigger code
     G_TYPE_INT -> gtk notebook page number
     ---------------------------------------------
  */
  tri_event_store = gtk_list_store_new( 3 , G_TYPE_STRING , G_TYPE_INT , G_TYPE_INT );
  aosd_trigger_get_codes_array ( &trigger_code_array , &trigger_code_array_size );
  for ( i = 0 ; i < trigger_code_array_size ; i ++ )
  {
    GtkWidget *frame, *vbox, *label, *checkbt;
    gtk_list_store_append( tri_event_store , &iter );
    gtk_list_store_set( tri_event_store , &iter ,
      0 , _(aosd_trigger_get_name( trigger_code_array[i] )) ,
      1 , trigger_code_array[i] , 2 , i , -1 );
    vbox = gtk_vbox_new( FALSE , 0 );
    gtk_container_set_border_width( GTK_CONTAINER(vbox) , 6 );
    label = gtk_label_new( _(aosd_trigger_get_desc( trigger_code_array[i] )) );
    gtk_label_set_line_wrap( GTK_LABEL(label) , TRUE );
    gtk_misc_set_alignment( GTK_MISC(label) , 0.0 , 0.0 );
    checkbt = gtk_check_button_new_with_label( _("Enable trigger") );
    if ( aosd_cb_configure_trigger_findinarr( cfg->osd->trigger.active , trigger_code_array[i] ) )
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(checkbt) , TRUE );
    else
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(checkbt) , FALSE );
    gtk_box_pack_start( GTK_BOX(vbox) , checkbt , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(vbox) , gtk_hseparator_new() , FALSE , FALSE , 4 );
    gtk_box_pack_start( GTK_BOX(vbox) , label , FALSE , FALSE , 0 );
    frame = gtk_frame_new( NULL );
    gtk_container_add( GTK_CONTAINER(frame) , vbox );
    gtk_notebook_append_page( GTK_NOTEBOOK(tri_event_nb) , frame , NULL );
    g_object_set_data( G_OBJECT(checkbt) , "code" , GINT_TO_POINTER(trigger_code_array[i]) );
    aosd_callback_list_add( cb_list , checkbt , aosd_cb_configure_trigger_commit );
  }

  tri_event_lv_frame = gtk_frame_new( NULL );
  tri_event_lv = gtk_tree_view_new_with_model( GTK_TREE_MODEL(tri_event_store) );
  g_object_unref( tri_event_store );
  tri_event_lv_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(tri_event_lv) );
  gtk_tree_selection_set_mode( tri_event_lv_sel , GTK_SELECTION_BROWSE );
  g_signal_connect( G_OBJECT(tri_event_lv_sel) , "changed" ,
                    G_CALLBACK(aosd_cb_configure_trigger_lvchanged) , tri_event_nb );
  if ( gtk_tree_model_get_iter_first( GTK_TREE_MODEL(tri_event_store) , &iter ) == TRUE )
    gtk_tree_selection_select_iter( tri_event_lv_sel , &iter );

  tri_event_lv_rndr_text = gtk_cell_renderer_text_new();
  tri_event_lv_col_desc = gtk_tree_view_column_new_with_attributes(
    _("Event") , tri_event_lv_rndr_text , "text" , 0 , NULL );
  gtk_tree_view_append_column( GTK_TREE_VIEW(tri_event_lv), tri_event_lv_col_desc );
  tri_event_lv_sw = gtk_scrolled_window_new( NULL , NULL );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(tri_event_lv_sw) ,
                                  GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );
  gtk_container_add( GTK_CONTAINER(tri_event_lv_sw) , tri_event_lv );
  gtk_container_add( GTK_CONTAINER(tri_event_lv_frame) , tri_event_lv_sw );
  gtk_tree_selection_select_iter( tri_event_lv_sel , &iter );

  gtk_box_pack_start( GTK_BOX(tri_hbox) , tri_event_lv_frame , FALSE , FALSE , 0 );

  gtk_box_pack_start( GTK_BOX(tri_hbox) , tri_event_nb , TRUE , TRUE , 0 );

  return tri_hbox;
}


#ifdef HAVE_XCOMPOSITE
static void
aosd_cb_configure_misc_transp_real_clicked ( GtkToggleButton * real_rbt , gpointer status_hbox )
{
  GtkWidget *img = g_object_get_data( G_OBJECT(status_hbox) , "img" );
  GtkWidget *label = g_object_get_data( G_OBJECT(status_hbox) , "label" );
  if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(real_rbt) ) )
  {
    if ( aosd_osd_check_composite_mgr() )
    {
      gtk_image_set_from_stock( GTK_IMAGE(img) , GTK_STOCK_YES , GTK_ICON_SIZE_MENU );
      gtk_label_set_text( GTK_LABEL(label) , _("Composite manager detected") );
      gtk_widget_set_sensitive( GTK_WIDGET(status_hbox) , TRUE );
    }
    else
    {
      gtk_image_set_from_stock( GTK_IMAGE(img) , GTK_STOCK_DIALOG_WARNING , GTK_ICON_SIZE_MENU );
      gtk_label_set_text( GTK_LABEL(label) ,
        _("Composite manager not detected;\nunless you know that you have one running, "
          "please activate a composite manager otherwise the OSD won't work properly") );
      gtk_widget_set_sensitive( GTK_WIDGET(status_hbox) , TRUE );
    }
  }
  else
  {
    gtk_image_set_from_stock( GTK_IMAGE(img) , GTK_STOCK_DIALOG_INFO , GTK_ICON_SIZE_MENU );
    gtk_label_set_text( GTK_LABEL(label) , _("Composite manager not required for fake transparency") );
    gtk_widget_set_sensitive( GTK_WIDGET(status_hbox) , FALSE );
  }
  return;
}
#endif


static void
aosd_cb_configure_misc_transp_commit ( GtkWidget * mis_transp_vbox , aosd_cfg_t * cfg )
{
  GList *child_list = gtk_container_get_children( GTK_CONTAINER(mis_transp_vbox) );
  while (child_list != NULL)
  {
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(child_list->data) ) )
    {
      cfg->osd->misc.transparency_mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child_list->data),"val"));
      break;
    }
    child_list = g_list_next(child_list);
  }
  return;
}


static GtkWidget *
aosd_ui_configure_misc ( aosd_cfg_t * cfg , GList ** cb_list )
{
  GtkWidget *mis_vbox;
  GtkWidget *mis_transp_frame, *mis_transp_vbox;
  GtkWidget *mis_transp_fake_rbt, *mis_transp_real_rbt;
  GtkWidget *mis_transp_status_frame, *mis_transp_status_hbox;
  GtkWidget *mis_transp_status_img, *mis_transp_status_label;

  mis_vbox = gtk_vbox_new( FALSE , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(mis_vbox) , 6 );

  mis_transp_vbox = gtk_vbox_new( FALSE , 0 );
  mis_transp_frame = gtk_frame_new( _("Transparency") );
  gtk_container_set_border_width( GTK_CONTAINER(mis_transp_vbox) , 6 );
  gtk_container_add( GTK_CONTAINER(mis_transp_frame) , mis_transp_vbox );
  gtk_box_pack_start( GTK_BOX(mis_vbox) , mis_transp_frame , FALSE , FALSE , 0 );

  mis_transp_fake_rbt = gtk_radio_button_new_with_label( NULL ,
                          _("Fake transparency") );
  mis_transp_real_rbt = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(mis_transp_fake_rbt) ,
                          _("Real transparency (requires X Composite Ext.)") );
  g_object_set_data( G_OBJECT(mis_transp_fake_rbt) , "val" ,
                     GINT_TO_POINTER(AOSD_MISC_TRANSPARENCY_FAKE) );
  g_object_set_data( G_OBJECT(mis_transp_real_rbt) , "val" ,
                     GINT_TO_POINTER(AOSD_MISC_TRANSPARENCY_REAL) );
  gtk_box_pack_start( GTK_BOX(mis_transp_vbox) , mis_transp_fake_rbt , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(mis_transp_vbox) , mis_transp_real_rbt , TRUE , TRUE , 0 );

  mis_transp_status_hbox = gtk_hbox_new( FALSE , 4 );
  mis_transp_status_frame = gtk_frame_new( NULL );
  gtk_container_set_border_width( GTK_CONTAINER(mis_transp_status_hbox) , 3 );
  gtk_container_add( GTK_CONTAINER(mis_transp_status_frame) , mis_transp_status_hbox );
  gtk_box_pack_start( GTK_BOX(mis_transp_vbox) , mis_transp_status_frame , TRUE , TRUE , 0 );

  mis_transp_status_img = gtk_image_new();
  gtk_misc_set_alignment( GTK_MISC(mis_transp_status_img) , 0.5 , 0 );
  mis_transp_status_label = gtk_label_new( "" );
  gtk_misc_set_alignment( GTK_MISC(mis_transp_status_label) , 0 , 0.5 );
  gtk_label_set_line_wrap( GTK_LABEL(mis_transp_status_label) , TRUE );
  gtk_box_pack_start( GTK_BOX(mis_transp_status_hbox) , mis_transp_status_img , FALSE , FALSE , 0 );
  gtk_box_pack_start( GTK_BOX(mis_transp_status_hbox) , mis_transp_status_label , TRUE , TRUE , 0 );
  g_object_set_data( G_OBJECT(mis_transp_status_hbox) , "img" , mis_transp_status_img );
  g_object_set_data( G_OBJECT(mis_transp_status_hbox) , "label" , mis_transp_status_label );

#ifdef HAVE_XCOMPOSITE
  g_signal_connect( G_OBJECT(mis_transp_real_rbt) , "toggled" ,
    G_CALLBACK(aosd_cb_configure_misc_transp_real_clicked) , mis_transp_status_hbox );

  /* check if the composite extension is loaded */
  if ( aosd_osd_check_composite_ext() )
  {
    if ( cfg->osd->misc.transparency_mode == AOSD_MISC_TRANSPARENCY_FAKE )
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_fake_rbt) , TRUE );
    else
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_real_rbt) , TRUE );
  }
  else
  {
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_fake_rbt) , TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET(mis_transp_real_rbt) , FALSE );
    gtk_image_set_from_stock( GTK_IMAGE(mis_transp_status_img) ,
    GTK_STOCK_DIALOG_ERROR , GTK_ICON_SIZE_MENU );
    gtk_label_set_text( GTK_LABEL(mis_transp_status_label) , _("Composite extension not loaded") );
    gtk_widget_set_sensitive( GTK_WIDGET(mis_transp_status_hbox) , FALSE );
  }
#else
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mis_transp_fake_rbt) , TRUE );
  gtk_widget_set_sensitive( GTK_WIDGET(mis_transp_real_rbt) , FALSE );
  gtk_image_set_from_stock( GTK_IMAGE(mis_transp_status_img) ,
    GTK_STOCK_DIALOG_ERROR , GTK_ICON_SIZE_MENU );
  gtk_label_set_text( GTK_LABEL(mis_transp_status_label) , _("Composite extension not available") );
  gtk_widget_set_sensitive( GTK_WIDGET(mis_transp_status_hbox) , FALSE );
#endif

  aosd_callback_list_add( cb_list , mis_transp_vbox , aosd_cb_configure_misc_transp_commit );

  return mis_vbox;
}


static void
aosd_cb_configure_test ( gpointer cfg_win )
{
  gchar *markup_message = NULL;
  aosd_cfg_t *cfg = aosd_cfg_new();
  GList *cb_list = g_object_get_data( G_OBJECT(cfg_win) , "cblist" );
  aosd_callback_list_run( cb_list , cfg );
  cfg->set = TRUE;
  markup_message = g_markup_printf_escaped(
    _("<span font_desc='%s'>Audacious OSD</span>") , cfg->osd->text.fonts_name[0] );
  aosd_osd_shutdown(); /* stop any displayed osd */
  aosd_osd_cleanup(); /* just in case it's active */
  aosd_osd_init( cfg->osd->misc.transparency_mode );
  aosd_osd_display( markup_message , cfg->osd , TRUE );
  g_free( markup_message );
  aosd_cfg_delete( cfg );
  return;
}


static void
aosd_cb_configure_cancel ( gpointer cfg_win )
{
  GList *cb_list = g_object_get_data( G_OBJECT(cfg_win) , "cblist" );
  aosd_callback_list_free( cb_list );
  aosd_osd_shutdown(); /* stop any displayed osd */
  aosd_osd_cleanup(); /* just in case it's active */
  if ( plugin_is_active == TRUE )
    aosd_osd_init( global_config->osd->misc.transparency_mode );
  gtk_widget_destroy( GTK_WIDGET(cfg_win) );
  return;
}


static void
aosd_cb_configure_ok ( gpointer cfg_win )
{
  //gchar *markup_message = NULL;
  aosd_cfg_t *cfg = aosd_cfg_new();
  GList *cb_list = g_object_get_data( G_OBJECT(cfg_win) , "cblist" );
  aosd_callback_list_run( cb_list , cfg );
  cfg->set = TRUE;
  aosd_osd_shutdown(); /* stop any displayed osd */
  aosd_osd_cleanup(); /* just in case it's active */

  if ( global_config != NULL )
  {
    /* plugin is active */
    aosd_trigger_stop( &global_config->osd->trigger ); /* stop triggers */
    aosd_cfg_delete( global_config ); /* delete old global_config */
    global_config = cfg; /* put the new one */
    aosd_cfg_save( cfg ); /* save the new configuration on config file */
    aosd_osd_init( cfg->osd->misc.transparency_mode ); /* restart osd */
    aosd_trigger_start( &cfg->osd->trigger ); /* restart triggers */
  }
  else
  {
    /* plugin is not active */
    aosd_cfg_save( cfg ); /* save the new configuration on config file */
  }
  aosd_callback_list_free( cb_list );
  gtk_widget_destroy( GTK_WIDGET(cfg_win) );
  return;
}


void
aosd_ui_configure ( aosd_cfg_t * cfg )
{
  static GtkWidget *cfg_win = NULL;
  GtkWidget *cfg_vbox;
  GtkWidget *cfg_nb;
  GtkWidget *cfg_bbar_hbbox;
  GtkWidget *cfg_bbar_bt_ok, *cfg_bbar_bt_test, *cfg_bbar_bt_cancel;
  GtkWidget *cfg_position_widget;
  GtkWidget *cfg_animation_widget;
  GtkWidget *cfg_text_widget;
  GtkWidget *cfg_decoration_widget;
  GtkWidget *cfg_trigger_widget;
  GdkGeometry cfg_win_hints;
  GList *cb_list = NULL; /* list of custom callbacks */

  if ( cfg_win != NULL )
  {
    gtk_window_present( GTK_WINDOW(cfg_win) );
    return;
  }

  cfg_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_type_hint( GTK_WINDOW(cfg_win), GDK_WINDOW_TYPE_HINT_DIALOG );
  gtk_window_set_title( GTK_WINDOW(cfg_win) , _("Audacious OSD - configuration") );
  gtk_container_set_border_width( GTK_CONTAINER(cfg_win), 10 );
  g_signal_connect( G_OBJECT(cfg_win) , "destroy" ,
                    G_CALLBACK(gtk_widget_destroyed) , &cfg_win );
  cfg_win_hints.min_width = -1;
  cfg_win_hints.min_height = 350;
  gtk_window_set_geometry_hints( GTK_WINDOW(cfg_win) , GTK_WIDGET(cfg_win) ,
                                 &cfg_win_hints , GDK_HINT_MIN_SIZE );

  cfg_vbox = gtk_vbox_new( 0 , FALSE );
  gtk_container_add( GTK_CONTAINER(cfg_win) , cfg_vbox );

  cfg_nb = gtk_notebook_new();
  gtk_notebook_set_tab_pos( GTK_NOTEBOOK(cfg_nb) , GTK_POS_TOP );
  gtk_box_pack_start( GTK_BOX(cfg_vbox) , cfg_nb , TRUE , TRUE , 0 );

  gtk_box_pack_start( GTK_BOX(cfg_vbox) , gtk_hseparator_new() , FALSE , FALSE , 4 );

  cfg_bbar_hbbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout( GTK_BUTTON_BOX(cfg_bbar_hbbox) , GTK_BUTTONBOX_START );
  gtk_box_pack_start( GTK_BOX(cfg_vbox) , cfg_bbar_hbbox , FALSE , FALSE , 0 );
  cfg_bbar_bt_test = gtk_button_new_with_label( _("Test") );
  gtk_button_set_image( GTK_BUTTON(cfg_bbar_bt_test) ,
    gtk_image_new_from_stock( GTK_STOCK_MEDIA_PLAY , GTK_ICON_SIZE_BUTTON ) );
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_test );
  gtk_button_box_set_child_secondary( GTK_BUTTON_BOX(cfg_bbar_hbbox) , cfg_bbar_bt_test , FALSE );
  cfg_bbar_bt_cancel = gtk_button_new_from_stock( GTK_STOCK_CANCEL );
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_cancel );
  gtk_button_box_set_child_secondary( GTK_BUTTON_BOX(cfg_bbar_hbbox) , cfg_bbar_bt_cancel , TRUE );
  cfg_bbar_bt_ok = gtk_button_new_from_stock( GTK_STOCK_OK );
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_ok );
  gtk_button_box_set_child_secondary( GTK_BUTTON_BOX(cfg_bbar_hbbox) , cfg_bbar_bt_ok , TRUE );

  /* add POSITION page */
  cfg_position_widget = aosd_ui_configure_position( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_position_widget , gtk_label_new( _("Position") ) );

  /* add ANIMATION page */
  cfg_animation_widget = aosd_ui_configure_animation( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_animation_widget , gtk_label_new( _("Animation") ) );

  /* add TEXT page */
  cfg_text_widget = aosd_ui_configure_text( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_text_widget , gtk_label_new( _("Text") ) );

  /* add DECORATION page */
  cfg_decoration_widget = aosd_ui_configure_decoration( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_decoration_widget , gtk_label_new( _("Decoration") ) );

  /* add TRIGGER page */
  cfg_trigger_widget = aosd_ui_configure_trigger( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_trigger_widget , gtk_label_new( _("Trigger") ) );

  /* add MISC page */
  cfg_trigger_widget = aosd_ui_configure_misc( cfg , &cb_list );
  gtk_notebook_append_page( GTK_NOTEBOOK(cfg_nb) ,
    cfg_trigger_widget , gtk_label_new( _("Misc") ) );

  g_object_set_data( G_OBJECT(cfg_win) , "cblist" , cb_list );

  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_test) , "clicked" ,
                            G_CALLBACK(aosd_cb_configure_test) , cfg_win );
  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_cancel) , "clicked" ,
                            G_CALLBACK(aosd_cb_configure_cancel) , cfg_win );
  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_ok) , "clicked" ,
                            G_CALLBACK(aosd_cb_configure_ok) , cfg_win );

  gtk_widget_show_all( cfg_win );
}


/* about box */
void
aosd_ui_about ( void )
{
  static GtkWidget *about_win = NULL;
  GtkWidget *about_vbox;
  GtkWidget *logoandinfo_vbox;
  GtkWidget *info_tv, *info_tv_sw, *info_tv_frame;
  GtkWidget *bbar_bbox, *bbar_bt_ok;
  GtkTextBuffer *info_tb;
  GdkGeometry abount_win_hints;
  gchar *info_tb_content = NULL;

  if ( about_win != NULL )
  {
    gtk_window_present( GTK_WINDOW(about_win) );
    return;
  }

  about_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_type_hint( GTK_WINDOW(about_win), GDK_WINDOW_TYPE_HINT_DIALOG );
  gtk_window_set_position( GTK_WINDOW(about_win), GTK_WIN_POS_CENTER );
  gtk_window_set_title( GTK_WINDOW(about_win), _("Audacious OSD - about") );
  abount_win_hints.min_width = 420;
  abount_win_hints.min_height = 240;
  gtk_window_set_geometry_hints( GTK_WINDOW(about_win) , GTK_WIDGET(about_win) ,
                                 &abount_win_hints , GDK_HINT_MIN_SIZE );
  /* gtk_window_set_resizable( GTK_WINDOW(about_win) , FALSE ); */
  gtk_container_set_border_width( GTK_CONTAINER(about_win) , 10 );
  g_signal_connect( G_OBJECT(about_win) , "destroy" , G_CALLBACK(gtk_widget_destroyed) , &about_win );

  about_vbox = gtk_vbox_new( FALSE , 0 );
  gtk_container_add( GTK_CONTAINER(about_win) , about_vbox );

  logoandinfo_vbox = gtk_vbox_new( TRUE , 2 );

  /* TODO make a logo or make someone do it! :)
  logo_pixbuf = gdk_pixbuf_new_from_xpm_data( (const gchar **)evdev_plug_logo_xpm );
  logo_image = gtk_image_new_from_pixbuf( logo_pixbuf );
  g_object_unref( logo_pixbuf );

  logo_frame = gtk_frame_new( NULL );
  gtk_container_add( GTK_CONTAINER(logo_frame) , logo_image );
  gtk_box_pack_start( GTK_BOX(logoandinfo_vbox) , logo_frame , TRUE , TRUE , 0 );  */

  info_tv = gtk_text_view_new();
  info_tb = gtk_text_view_get_buffer( GTK_TEXT_VIEW(info_tv) );
  gtk_text_view_set_editable( GTK_TEXT_VIEW(info_tv) , FALSE );
  gtk_text_view_set_cursor_visible( GTK_TEXT_VIEW(info_tv) , FALSE );
  gtk_text_view_set_justification( GTK_TEXT_VIEW(info_tv) , GTK_JUSTIFY_LEFT );
  gtk_text_view_set_left_margin( GTK_TEXT_VIEW(info_tv) , 10 );

  info_tb_content = g_strjoin( NULL , _("\nAudacious OSD ") , AOSD_VERSION_PLUGIN ,
                               _("\nhttp://www.develia.org/projects.php?p=audacious#aosd\n"
                                 "written by Giacomo Lozito\n"
                                 "< james@develia.org >\n\n"
                                 "On-Screen-Display is based on Ghosd library\n"
                                 "written by Evan Martin\n"
                                 "http://neugierig.org/software/ghosd/\n\n") , NULL );
  gtk_text_buffer_set_text( info_tb , info_tb_content , -1 );
  g_free( info_tb_content );

  info_tv_sw = gtk_scrolled_window_new( NULL , NULL );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(info_tv_sw) ,
                                  GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );
  gtk_container_add( GTK_CONTAINER(info_tv_sw) , info_tv );
  info_tv_frame = gtk_frame_new( NULL );
  gtk_container_add( GTK_CONTAINER(info_tv_frame) , info_tv_sw );
  gtk_box_pack_start( GTK_BOX(logoandinfo_vbox) , info_tv_frame , TRUE , TRUE , 0 );

  gtk_box_pack_start( GTK_BOX(about_vbox) , logoandinfo_vbox , TRUE , TRUE , 0 );

  /* horizontal separator and buttons */
  gtk_box_pack_start( GTK_BOX(about_vbox) , gtk_hseparator_new() , FALSE , FALSE , 4 );
  bbar_bbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout( GTK_BUTTON_BOX(bbar_bbox) , GTK_BUTTONBOX_END );
  bbar_bt_ok = gtk_button_new_from_stock( GTK_STOCK_OK );
  g_signal_connect_swapped( G_OBJECT(bbar_bt_ok) , "clicked" ,
                            G_CALLBACK(gtk_widget_destroy) , about_win );
  gtk_container_add( GTK_CONTAINER(bbar_bbox) , bbar_bt_ok );
  gtk_box_pack_start( GTK_BOX(about_vbox) , bbar_bbox , FALSE , FALSE , 0 );

  gtk_widget_show_all( about_win );
}
