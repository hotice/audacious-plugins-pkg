/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
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


#include "i_configure-fluidsynth.h"
#include "backend-fluidsynth/b-fluidsynth-config.h"
#include "backend-fluidsynth/backend-fluidsynth-icon.xpm"
#include <glib/gstdio.h>


enum
{
  LISTSFONT_FILENAME_COLUMN = 0,
  LISTSFONT_FILESIZE_COLUMN,
  LISTSFONT_N_COLUMNS
};


void i_configure_ev_toggle_default( GtkToggleButton *togglebutton , gpointer hbox )
{
  if ( gtk_toggle_button_get_active( togglebutton ) )
    gtk_widget_set_sensitive( GTK_WIDGET(hbox) , FALSE );
  else
    gtk_widget_set_sensitive( GTK_WIDGET(hbox) , TRUE );
}


void i_configure_ev_sflist_add( gpointer sfont_lv )
{
  GtkWidget *parent_window = gtk_widget_get_toplevel( sfont_lv );
  if ( GTK_WIDGET_TOPLEVEL(parent_window) )
  {
    GtkTreeSelection *listsel = gtk_tree_view_get_selection( GTK_TREE_VIEW(sfont_lv) );
    GtkTreeIter itersel, iterapp;
    GtkWidget *browse_dialog = gtk_file_chooser_dialog_new( _("AMIDI-Plug - select SoundFont file") ,
                                                            GTK_WINDOW(parent_window) ,
                                                            GTK_FILE_CHOOSER_ACTION_OPEN ,
                                                            GTK_STOCK_CANCEL , GTK_RESPONSE_CANCEL ,
                                                            GTK_STOCK_OPEN , GTK_RESPONSE_ACCEPT , NULL );
    if ( gtk_tree_selection_get_selected( listsel , NULL , &itersel ) )
    {
      gchar *selfilename = NULL, *selfiledir = NULL;
      GtkTreeModel *store = gtk_tree_view_get_model( GTK_TREE_VIEW(sfont_lv) );
      gtk_tree_model_get( GTK_TREE_MODEL(store) , &itersel , LISTSFONT_FILENAME_COLUMN , &selfilename , -1 );
      selfiledir = g_path_get_dirname( selfilename );
      gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(browse_dialog) , selfiledir );
      g_free( selfiledir ); g_free( selfilename );
    }
    if ( gtk_dialog_run(GTK_DIALOG(browse_dialog)) == GTK_RESPONSE_ACCEPT )
    {
      struct stat finfo;
      GtkTreeModel *store = gtk_tree_view_get_model( GTK_TREE_VIEW(sfont_lv) );
      gchar *filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(browse_dialog) );
      gint filesize = -1;
      if ( g_stat( filename , &finfo ) == 0 )
        filesize = finfo.st_size;
      gtk_list_store_append( GTK_LIST_STORE(store) , &iterapp );
      gtk_list_store_set( GTK_LIST_STORE(store) , &iterapp ,
                          LISTSFONT_FILENAME_COLUMN , filename ,
                          LISTSFONT_FILESIZE_COLUMN , filesize , -1 );
      DEBUGMSG( "selected file: %s\n" , filename );
      g_free( filename );
    }
    gtk_widget_destroy( browse_dialog );
  }
}


void i_configure_ev_sflist_rem( gpointer sfont_lv )
{
  GtkTreeModel *store;
  GtkTreeIter iter;
  GtkTreeSelection *listsel = gtk_tree_view_get_selection( GTK_TREE_VIEW(sfont_lv) );
  if ( gtk_tree_selection_get_selected( listsel , &store , &iter ) )
    gtk_list_store_remove( GTK_LIST_STORE(store) , &iter );
}


void i_configure_ev_sflist_swap( GtkWidget * button , gpointer sfont_lv )
{
  GtkTreeModel *store;
  GtkTreeIter iter;
  GtkTreeSelection *listsel = gtk_tree_view_get_selection( GTK_TREE_VIEW(sfont_lv) );

  if ( gtk_tree_selection_get_selected( listsel , &store , &iter ) )
  {
    guint swapdire = GPOINTER_TO_UINT(g_object_get_data( G_OBJECT(button) , "swapdire" ));
    if ( swapdire == 0 ) /* move up */
    {
      GtkTreePath *treepath = gtk_tree_model_get_path( store, &iter );
      if ( gtk_tree_path_prev( treepath ) )
      {
         GtkTreeIter iter_prev;
         if ( gtk_tree_model_get_iter( store , &iter_prev , treepath ) )
           gtk_list_store_swap( GTK_LIST_STORE(store) , &iter , &iter_prev );
      }
      gtk_tree_path_free( treepath );
    }
    else /* move down */
    {
      GtkTreeIter iter_prev = iter;
      if ( gtk_tree_model_iter_next( store , &iter ) )
        gtk_list_store_swap( GTK_LIST_STORE(store) , &iter , &iter_prev );
    }
  }
}


void i_configure_ev_sflist_commit( gpointer sfont_lv )
{
  amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;
  GtkTreeIter iter;
  GtkTreeModel *store = gtk_tree_view_get_model( GTK_TREE_VIEW(sfont_lv) );
  GString *sflist_string = g_string_new( "" );

  if ( gtk_tree_model_get_iter_first( GTK_TREE_MODEL(store) , &iter ) == TRUE )
  {
    gboolean iter_is_valid = FALSE;
    do
    {
      gchar *fname;
      gtk_tree_model_get( GTK_TREE_MODEL(store) , &iter ,
                          LISTSFONT_FILENAME_COLUMN , &fname , -1 );
      g_string_prepend_c( sflist_string , ';' );
      g_string_prepend( sflist_string , fname );
      g_free( fname );
      iter_is_valid = gtk_tree_model_iter_next( GTK_TREE_MODEL(store) , &iter );
    }
    while ( iter_is_valid == TRUE );
  }

  if ( sflist_string->len > 0 )
    g_string_truncate( sflist_string , sflist_string->len - 1 );

  g_free( fsyncfg->fsyn_soundfont_file ); /* free previous */
  fsyncfg->fsyn_soundfont_file = g_strdup( sflist_string->str );
  g_string_free( sflist_string , TRUE );
}


void i_configure_ev_sfload_commit( gpointer sfload_radiobt )
{
  amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;
  GSList *group = gtk_radio_button_get_group( GTK_RADIO_BUTTON(sfload_radiobt) );
  while ( group != NULL )
  {
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(group->data) ) )
      fsyncfg->fsyn_soundfont_load = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(group->data) , "val" ));
    group = group->next;
  }
}


void i_configure_ev_sygain_commit( gpointer gain_spinbt )
{
  amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;
  if ( GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(gain_spinbt)) )
    fsyncfg->fsyn_synth_gain = (gint)(gtk_spin_button_get_value(GTK_SPIN_BUTTON(gain_spinbt)) * 10);
  else
    fsyncfg->fsyn_synth_gain = -1;
}


void i_configure_ev_sypoly_commit( gpointer poly_spinbt )
{
  amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;
  if ( GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(poly_spinbt)) )
    fsyncfg->fsyn_synth_poliphony = (gint)(gtk_spin_button_get_value(GTK_SPIN_BUTTON(poly_spinbt)));
  else
    fsyncfg->fsyn_synth_poliphony = -1;
}


void i_configure_ev_syreverb_commit( gpointer reverb_yes_radiobt )
{
  amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;
  if ( GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(reverb_yes_radiobt)) )
  {
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(reverb_yes_radiobt) ) )
      fsyncfg->fsyn_synth_reverb = 1;
    else
      fsyncfg->fsyn_synth_reverb = 0;
  }
  else
    fsyncfg->fsyn_synth_reverb = -1;
}


void i_configure_ev_sychorus_commit( gpointer chorus_yes_radiobt )
{
  amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;
  if ( GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(chorus_yes_radiobt)) )
  {
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(chorus_yes_radiobt) ) )
      fsyncfg->fsyn_synth_chorus = 1;
    else
      fsyncfg->fsyn_synth_chorus = 0;
  }
  else
    fsyncfg->fsyn_synth_chorus = -1;
}


void i_configure_ev_sysamplerate_togglecustom( GtkToggleButton *custom_radiobt , gpointer custom_entry )
{
  if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(custom_radiobt) ) )
    gtk_widget_set_sensitive( GTK_WIDGET(custom_entry) , TRUE );
  else
    gtk_widget_set_sensitive( GTK_WIDGET(custom_entry) , FALSE );
}


void i_configure_ev_sysamplerate_commit( gpointer samplerate_custom_radiobt )
{
  amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;
  if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(samplerate_custom_radiobt)) )
  {
    GtkWidget *customentry = g_object_get_data( G_OBJECT(samplerate_custom_radiobt) , "customentry" );
    gint customvalue = strtol( gtk_entry_get_text(GTK_ENTRY(customentry)) , NULL , 10 );
    if (( customvalue > 22050 ) && ( customvalue < 96000 ))
      fsyncfg->fsyn_synth_samplerate = customvalue;
    else
      fsyncfg->fsyn_synth_samplerate = 44100;
  }
  else
  {
    GSList *group = gtk_radio_button_get_group( GTK_RADIO_BUTTON(samplerate_custom_radiobt) );
    while ( group != NULL )
    {
      if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(group->data) ) )
        fsyncfg->fsyn_synth_samplerate = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(group->data) , "val" ));
      group = group->next;
    }
  }
}


void i_configure_gui_tab_fsyn( GtkWidget * fsyn_page_alignment ,
                               gpointer backend_list_p ,
                               gpointer commit_button )
{
  GtkWidget *fsyn_page_vbox;
  GtkWidget *title_widget;
  GtkWidget *content_vbox; /* this vbox will contain the various frames for config sections */
  GSList * backend_list = backend_list_p;
  gboolean fsyn_module_ok = FALSE;
  gchar * fsyn_module_pathfilename;

  fsyn_page_vbox = gtk_vbox_new( FALSE , 0 );

  title_widget = i_configure_gui_draw_title( _("FLUIDSYNTH BACKEND CONFIGURATION") );
  gtk_box_pack_start( GTK_BOX(fsyn_page_vbox) , title_widget , FALSE , FALSE , 2 );

  content_vbox = gtk_vbox_new( FALSE , 2 );

  /* check if the FluidSynth module is available */
  while ( backend_list != NULL )
  {
    amidiplug_sequencer_backend_name_t * mn = backend_list->data;
    if ( !strcmp( mn->name , "fluidsynth" ) )
    {
      fsyn_module_ok = TRUE;
      fsyn_module_pathfilename = mn->filename;
      break;
    }
    backend_list = backend_list->next;
  }

  if ( fsyn_module_ok )
  {
    GtkWidget *soundfont_frame, *soundfont_vbox;
    GtkListStore *soundfont_file_store;
    GtkCellRenderer *soundfont_file_lv_text_rndr;
    GtkTreeViewColumn *soundfont_file_lv_fname_col, *soundfont_file_lv_fsize_col;
    GtkWidget *soundfont_file_hbox, *soundfont_file_lv, *soundfont_file_lv_sw, *soundfont_file_lv_frame;
    GtkTreeSelection *soundfont_file_lv_sel;
    GtkWidget *soundfont_file_bbox_vbox, *soundfont_file_bbox_addbt, *soundfont_file_bbox_rembt;
    GtkWidget *soundfont_file_bbox_mvupbt, *soundfont_file_bbox_mvdownbt;
    GtkWidget *soundfont_load_hsep, *soundfont_load_hbox, *soundfont_load_option[2];
    GtkWidget *synth_frame, *synth_hbox, *synth_leftcol_vbox, *synth_rightcol_vbox;
    GtkWidget *synth_samplerate_frame, *synth_samplerate_vbox, *synth_samplerate_option[4];
    GtkWidget *synth_samplerate_optionhbox, *synth_samplerate_optionentry, *synth_samplerate_optionlabel;
    GtkWidget *synth_gain_frame, *synth_gain_hbox, *synth_gain_value_hbox;
    GtkWidget *synth_gain_value_label, *synth_gain_value_spin, *synth_gain_defcheckbt;
    GtkWidget *synth_poly_frame, *synth_poly_hbox, *synth_poly_value_hbox;
    GtkWidget *synth_poly_value_label, *synth_poly_value_spin, *synth_poly_defcheckbt;
    GtkWidget *synth_reverb_frame, *synth_reverb_hbox, *synth_reverb_value_hbox;
    GtkWidget *synth_reverb_value_option[2], *synth_reverb_defcheckbt;
    GtkWidget *synth_chorus_frame, *synth_chorus_hbox, *synth_chorus_value_hbox;
    GtkWidget *synth_chorus_value_option[2], *synth_chorus_defcheckbt;
    GtkTooltips *tips;

    amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;

    tips = gtk_tooltips_new();
    g_object_set_data_full( G_OBJECT(fsyn_page_alignment) , "tt" , tips , g_object_unref );

    /* soundfont settings */
    soundfont_frame = gtk_frame_new( _("SoundFont settings") );
    soundfont_vbox = gtk_vbox_new( FALSE , 2 );
    gtk_container_set_border_width( GTK_CONTAINER(soundfont_vbox), 4 );
    gtk_container_add( GTK_CONTAINER(soundfont_frame) , soundfont_vbox );
    /* soundfont settings - soundfont files - listview */
    soundfont_file_store = gtk_list_store_new( LISTSFONT_N_COLUMNS, G_TYPE_STRING , G_TYPE_INT );
    if ( strlen(fsyncfg->fsyn_soundfont_file) > 0 )
    {
      /* fill soundfont list with fsyn_soundfont_file information */
      gchar **sffiles = g_strsplit( fsyncfg->fsyn_soundfont_file , ";" , 0 );
      GtkTreeIter iter;
      gint i = 0;
      while ( sffiles[i] != NULL )
      {
        gint filesize = -1;
        struct stat finfo;
        if ( g_stat( sffiles[i] , &finfo ) == 0 )
          filesize = finfo.st_size;
        gtk_list_store_prepend( GTK_LIST_STORE(soundfont_file_store) , &iter );
        gtk_list_store_set( GTK_LIST_STORE(soundfont_file_store) , &iter ,
                            LISTSFONT_FILENAME_COLUMN , sffiles[i] ,
                            LISTSFONT_FILESIZE_COLUMN , filesize , -1 );
        i++;
      }
      g_strfreev( sffiles );
    }
    soundfont_file_hbox = gtk_hbox_new( FALSE , 2 );
    soundfont_file_lv = gtk_tree_view_new_with_model( GTK_TREE_MODEL(soundfont_file_store) );
    gtk_tree_view_set_rules_hint( GTK_TREE_VIEW(soundfont_file_lv), TRUE );
    g_object_unref( soundfont_file_store );
    soundfont_file_lv_text_rndr = gtk_cell_renderer_text_new();
    soundfont_file_lv_fname_col = gtk_tree_view_column_new_with_attributes(
                                    _("Filename") , soundfont_file_lv_text_rndr , "text" ,
                                    LISTSFONT_FILENAME_COLUMN, NULL );
    gtk_tree_view_column_set_expand( GTK_TREE_VIEW_COLUMN(soundfont_file_lv_fname_col) , TRUE );
    soundfont_file_lv_fsize_col = gtk_tree_view_column_new_with_attributes(
                                    _("Size (bytes)") , soundfont_file_lv_text_rndr , "text" ,
                                    LISTSFONT_FILESIZE_COLUMN, NULL );
    gtk_tree_view_column_set_expand( GTK_TREE_VIEW_COLUMN(soundfont_file_lv_fsize_col) , FALSE );
    gtk_tree_view_append_column( GTK_TREE_VIEW(soundfont_file_lv), soundfont_file_lv_fname_col );
    gtk_tree_view_append_column( GTK_TREE_VIEW(soundfont_file_lv), soundfont_file_lv_fsize_col );
    soundfont_file_lv_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(soundfont_file_lv) );
    gtk_tree_selection_set_mode( GTK_TREE_SELECTION(soundfont_file_lv_sel) , GTK_SELECTION_SINGLE );
    soundfont_file_lv_sw = gtk_scrolled_window_new( NULL , NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(soundfont_file_lv_sw) ,
                                    GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );
    soundfont_file_lv_frame = gtk_frame_new( NULL );
    gtk_container_add( GTK_CONTAINER(soundfont_file_lv_sw) , soundfont_file_lv );
    gtk_container_add( GTK_CONTAINER(soundfont_file_lv_frame) , soundfont_file_lv_sw );
    /* soundfont settings - soundfont files - buttonbox */
    soundfont_file_bbox_vbox = gtk_vbox_new( TRUE , 0 );
    soundfont_file_bbox_addbt = gtk_button_new();
    gtk_button_set_image( GTK_BUTTON(soundfont_file_bbox_addbt) ,
                          gtk_image_new_from_stock( GTK_STOCK_ADD , GTK_ICON_SIZE_MENU ) );
    g_signal_connect_swapped( G_OBJECT(soundfont_file_bbox_addbt) , "clicked" ,
                              G_CALLBACK(i_configure_ev_sflist_add) , soundfont_file_lv );
    gtk_box_pack_start( GTK_BOX(soundfont_file_bbox_vbox) , soundfont_file_bbox_addbt , FALSE , FALSE , 0 );
    soundfont_file_bbox_rembt = gtk_button_new();
    gtk_button_set_image( GTK_BUTTON(soundfont_file_bbox_rembt) ,
                          gtk_image_new_from_stock( GTK_STOCK_REMOVE , GTK_ICON_SIZE_MENU ) );
    g_signal_connect_swapped( G_OBJECT(soundfont_file_bbox_rembt) , "clicked" ,
                              G_CALLBACK(i_configure_ev_sflist_rem) , soundfont_file_lv );
    gtk_box_pack_start( GTK_BOX(soundfont_file_bbox_vbox) , soundfont_file_bbox_rembt , FALSE , FALSE , 0 );
    soundfont_file_bbox_mvupbt = gtk_button_new();
    gtk_button_set_image( GTK_BUTTON(soundfont_file_bbox_mvupbt) ,
                          gtk_image_new_from_stock( GTK_STOCK_GO_UP , GTK_ICON_SIZE_MENU ) );
    g_object_set_data( G_OBJECT(soundfont_file_bbox_mvupbt) , "swapdire" , GUINT_TO_POINTER(0) );
    g_signal_connect( G_OBJECT(soundfont_file_bbox_mvupbt) , "clicked" ,
                      G_CALLBACK(i_configure_ev_sflist_swap) , soundfont_file_lv );
    gtk_box_pack_start( GTK_BOX(soundfont_file_bbox_vbox) , soundfont_file_bbox_mvupbt , FALSE , FALSE , 0 );
    soundfont_file_bbox_mvdownbt = gtk_button_new();
    gtk_button_set_image( GTK_BUTTON(soundfont_file_bbox_mvdownbt) ,
                          gtk_image_new_from_stock( GTK_STOCK_GO_DOWN , GTK_ICON_SIZE_MENU ) );
    g_object_set_data( G_OBJECT(soundfont_file_bbox_mvdownbt) , "swapdire" , GUINT_TO_POINTER(1) );
    g_signal_connect( G_OBJECT(soundfont_file_bbox_mvdownbt) , "clicked" ,
                      G_CALLBACK(i_configure_ev_sflist_swap) , soundfont_file_lv );
    gtk_box_pack_start( GTK_BOX(soundfont_file_bbox_vbox) , soundfont_file_bbox_mvdownbt , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(soundfont_file_hbox) , soundfont_file_lv_frame , TRUE , TRUE , 0 );
    gtk_box_pack_start( GTK_BOX(soundfont_file_hbox) , soundfont_file_bbox_vbox , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(soundfont_vbox) , soundfont_file_hbox , FALSE , FALSE , 0 );

    /* soundfont settings - load */
    soundfont_load_hsep = gtk_hseparator_new();
    soundfont_load_hbox = gtk_hbox_new( FALSE , 0 );
    soundfont_load_option[0] = gtk_radio_button_new_with_label( NULL ,
                                 _("Load SF on player start") );
    g_object_set_data( G_OBJECT(soundfont_load_option[0]) , "val" , GINT_TO_POINTER(0) );
    soundfont_load_option[1] = gtk_radio_button_new_with_label_from_widget(
                                 GTK_RADIO_BUTTON(soundfont_load_option[0]) ,
                                 _("Load SF on first midifile play") );
    g_object_set_data( G_OBJECT(soundfont_load_option[1]) , "val" , GINT_TO_POINTER(1) );
    if ( fsyncfg->fsyn_soundfont_load == 0 )
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(soundfont_load_option[0]) , TRUE );
    else
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(soundfont_load_option[1]) , TRUE );
    gtk_box_pack_start( GTK_BOX(soundfont_load_hbox) , soundfont_load_option[0] , TRUE , TRUE , 0 );
    gtk_box_pack_start( GTK_BOX(soundfont_load_hbox) , gtk_vseparator_new() , FALSE , FALSE , 4 );
    gtk_box_pack_start( GTK_BOX(soundfont_load_hbox) , soundfont_load_option[1] , TRUE , TRUE , 0 );
    gtk_box_pack_start( GTK_BOX(soundfont_vbox) , soundfont_load_hsep , FALSE , FALSE , 3 );
    gtk_box_pack_start( GTK_BOX(soundfont_vbox) , soundfont_load_hbox , FALSE , FALSE , 0 );

    gtk_box_pack_start( GTK_BOX(content_vbox) , soundfont_frame , FALSE , FALSE , 0 );

    /* synth settings */
    synth_frame = gtk_frame_new( _("Synthesizer settings") );
    synth_hbox = gtk_hbox_new( FALSE , 4 );
    gtk_container_set_border_width( GTK_CONTAINER(synth_hbox), 4 );
    gtk_container_add( GTK_CONTAINER(synth_frame) , synth_hbox );
    synth_leftcol_vbox = gtk_vbox_new( TRUE , 0 );
    synth_rightcol_vbox = gtk_vbox_new( FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_hbox) , synth_leftcol_vbox , TRUE , TRUE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_hbox) , synth_rightcol_vbox , FALSE , FALSE , 0 );
    /* synth settings - gain */
    synth_gain_frame = gtk_frame_new( _("gain") );
    gtk_frame_set_label_align( GTK_FRAME(synth_gain_frame) , 0.5 , 0.5 );
    gtk_box_pack_start( GTK_BOX(synth_leftcol_vbox) , synth_gain_frame , TRUE , TRUE , 0 );
    synth_gain_hbox = gtk_hbox_new( TRUE , 2 );
    gtk_container_set_border_width( GTK_CONTAINER(synth_gain_hbox), 2 );
    gtk_container_add( GTK_CONTAINER(synth_gain_frame) , synth_gain_hbox );
    synth_gain_defcheckbt = gtk_check_button_new_with_label( _("use default") );
    gtk_box_pack_start( GTK_BOX(synth_gain_hbox) , synth_gain_defcheckbt , FALSE , FALSE , 0 );
    synth_gain_value_hbox = gtk_hbox_new( FALSE , 4 );
    synth_gain_value_label = gtk_label_new( _("value:") );
    synth_gain_value_spin = gtk_spin_button_new_with_range( 0.0 , 10.0 , 0.1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(synth_gain_value_spin) , 0.2 );
    g_signal_connect( G_OBJECT(synth_gain_defcheckbt) , "toggled" ,
                      G_CALLBACK(i_configure_ev_toggle_default) , synth_gain_value_hbox );
    if ( fsyncfg->fsyn_synth_gain < 0 )
    {
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_gain_defcheckbt) , TRUE );
    }
    else
    {
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_gain_defcheckbt) , FALSE );
      gtk_spin_button_set_value( GTK_SPIN_BUTTON(synth_gain_value_spin) ,
                                 (gdouble)fsyncfg->fsyn_synth_gain / 10 );
    }
    gtk_box_pack_start( GTK_BOX(synth_gain_hbox) , synth_gain_value_hbox , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_gain_value_hbox) , synth_gain_value_label , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_gain_value_hbox) , synth_gain_value_spin , FALSE , FALSE , 0 );
    /* synth settings - poliphony */
    synth_poly_frame = gtk_frame_new( _("poliphony") );
    gtk_frame_set_label_align( GTK_FRAME(synth_poly_frame) , 0.5 , 0.5 );
    gtk_box_pack_start( GTK_BOX(synth_leftcol_vbox) , synth_poly_frame , TRUE , TRUE , 0 );
    synth_poly_hbox = gtk_hbox_new( TRUE , 2 );
    gtk_container_set_border_width( GTK_CONTAINER(synth_poly_hbox), 2 );
    gtk_container_add( GTK_CONTAINER(synth_poly_frame) , synth_poly_hbox );
    synth_poly_defcheckbt = gtk_check_button_new_with_label( _("use default") );
    gtk_box_pack_start( GTK_BOX(synth_poly_hbox) , synth_poly_defcheckbt , FALSE , FALSE , 0 );
    synth_poly_value_hbox = gtk_hbox_new( FALSE , 4 );
    synth_poly_value_label = gtk_label_new( _("value:") );
    synth_poly_value_spin = gtk_spin_button_new_with_range( 16 , 4096 , 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(synth_poly_value_spin) , 256 );
    g_signal_connect( G_OBJECT(synth_poly_defcheckbt) , "toggled" ,
                      G_CALLBACK(i_configure_ev_toggle_default) , synth_poly_value_hbox );
    if ( fsyncfg->fsyn_synth_poliphony < 0 )
    {
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_poly_defcheckbt) , TRUE );
    }
    else
    {
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_poly_defcheckbt) , FALSE );
      gtk_spin_button_set_value( GTK_SPIN_BUTTON(synth_poly_value_spin) ,
                                 (gdouble)fsyncfg->fsyn_synth_poliphony );
    }
    gtk_box_pack_start( GTK_BOX(synth_poly_hbox) , synth_poly_value_hbox , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_poly_value_hbox) , synth_poly_value_label , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_poly_value_hbox) , synth_poly_value_spin , FALSE , FALSE , 0 );
    /* synth settings - reverb */
    synth_reverb_frame = gtk_frame_new( _("reverb") );
    gtk_frame_set_label_align( GTK_FRAME(synth_reverb_frame) , 0.5 , 0.5 );
    gtk_box_pack_start( GTK_BOX(synth_leftcol_vbox) , synth_reverb_frame , TRUE , TRUE , 0 );
    synth_reverb_hbox = gtk_hbox_new( TRUE , 2 );
    gtk_container_set_border_width( GTK_CONTAINER(synth_reverb_hbox), 2 );
    gtk_container_add( GTK_CONTAINER(synth_reverb_frame) , synth_reverb_hbox );
    synth_reverb_defcheckbt = gtk_check_button_new_with_label( _("use default") );
    gtk_box_pack_start( GTK_BOX(synth_reverb_hbox) , synth_reverb_defcheckbt , FALSE , FALSE , 0 );
    synth_reverb_value_hbox = gtk_hbox_new( TRUE , 4 );
    synth_reverb_value_option[0] = gtk_radio_button_new_with_label( NULL , _("yes") );
    synth_reverb_value_option[1] = gtk_radio_button_new_with_label_from_widget(
                                     GTK_RADIO_BUTTON(synth_reverb_value_option[0]) , _("no") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_reverb_value_option[0]) , TRUE );
    g_signal_connect( G_OBJECT(synth_reverb_defcheckbt) , "toggled" ,
                      G_CALLBACK(i_configure_ev_toggle_default) , synth_reverb_value_hbox );
    if ( fsyncfg->fsyn_synth_reverb < 0 )
    {
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_reverb_defcheckbt) , TRUE );
    }
    else
    {
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_reverb_defcheckbt) , FALSE );
      if ( fsyncfg->fsyn_synth_reverb == 0 )
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_reverb_value_option[1]) , TRUE );
      else
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_reverb_value_option[0]) , TRUE );
    }
    gtk_box_pack_start( GTK_BOX(synth_reverb_hbox) , synth_reverb_value_hbox , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_reverb_value_hbox) , synth_reverb_value_option[0] , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_reverb_value_hbox) , synth_reverb_value_option[1] , FALSE , FALSE , 0 );
    /* synth settings - chorus */
    synth_chorus_frame = gtk_frame_new( _("chorus") );
    gtk_frame_set_label_align( GTK_FRAME(synth_chorus_frame) , 0.5 , 0.5 );
    gtk_box_pack_start( GTK_BOX(synth_leftcol_vbox) , synth_chorus_frame , TRUE , TRUE , 0 );
    synth_chorus_hbox = gtk_hbox_new( TRUE , 2 );
    gtk_container_set_border_width( GTK_CONTAINER(synth_chorus_hbox), 2 );
    gtk_container_add( GTK_CONTAINER(synth_chorus_frame) , synth_chorus_hbox );
    synth_chorus_defcheckbt = gtk_check_button_new_with_label( _("use default") );
    gtk_box_pack_start( GTK_BOX(synth_chorus_hbox) , synth_chorus_defcheckbt , FALSE , FALSE , 0 );
    synth_chorus_value_hbox = gtk_hbox_new( TRUE , 4 );
    synth_chorus_value_option[0] = gtk_radio_button_new_with_label( NULL , _("yes") );
    synth_chorus_value_option[1] = gtk_radio_button_new_with_label_from_widget(
                                     GTK_RADIO_BUTTON(synth_chorus_value_option[0]) , _("no") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_chorus_value_option[0]) , TRUE );
    g_signal_connect( G_OBJECT(synth_chorus_defcheckbt) , "toggled" ,
                      G_CALLBACK(i_configure_ev_toggle_default) , synth_chorus_value_hbox );
    if ( fsyncfg->fsyn_synth_chorus < 0 )
    {
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_chorus_defcheckbt) , TRUE );
    }
    else
    {
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_chorus_defcheckbt) , FALSE );
      if ( fsyncfg->fsyn_synth_chorus == 0 )
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_chorus_value_option[1]) , TRUE );
      else
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_chorus_value_option[0]) , TRUE );
    }
    gtk_box_pack_start( GTK_BOX(synth_chorus_hbox) , synth_chorus_value_hbox , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_chorus_value_hbox) , synth_chorus_value_option[0] , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_chorus_value_hbox) , synth_chorus_value_option[1] , FALSE , FALSE , 0 );
    /* synth settings - samplerate */
    synth_samplerate_frame = gtk_frame_new( _("sample rate") );
    gtk_frame_set_label_align( GTK_FRAME(synth_samplerate_frame) , 0.5 , 0.5 );
    synth_samplerate_vbox = gtk_vbox_new( TRUE , 0 );
    gtk_container_set_border_width( GTK_CONTAINER(synth_samplerate_vbox), 6 );
    gtk_container_add( GTK_CONTAINER(synth_samplerate_frame) , synth_samplerate_vbox );
    gtk_box_pack_start( GTK_BOX(synth_rightcol_vbox) , synth_samplerate_frame , FALSE , FALSE , 0 );
    synth_samplerate_option[0] = gtk_radio_button_new_with_label( NULL , _("22050 Hz ") );
    g_object_set_data( G_OBJECT(synth_samplerate_option[0]) , "val" , GINT_TO_POINTER(22050) );
    synth_samplerate_option[1] = gtk_radio_button_new_with_label_from_widget(
                                   GTK_RADIO_BUTTON(synth_samplerate_option[0]) , _("44100 Hz ") );
    g_object_set_data( G_OBJECT(synth_samplerate_option[1]) , "val" , GINT_TO_POINTER(44100) );
    synth_samplerate_option[2] = gtk_radio_button_new_with_label_from_widget(
                                   GTK_RADIO_BUTTON(synth_samplerate_option[0]) , _("96000 Hz ") );
    g_object_set_data( G_OBJECT(synth_samplerate_option[2]) , "val" , GINT_TO_POINTER(96000) );
    synth_samplerate_option[3] = gtk_radio_button_new_with_label_from_widget(
                                   GTK_RADIO_BUTTON(synth_samplerate_option[0]) , _("custom ") );
    synth_samplerate_optionhbox = gtk_hbox_new( FALSE , 4 );
    synth_samplerate_optionentry = gtk_entry_new();
    gtk_widget_set_sensitive( GTK_WIDGET(synth_samplerate_optionentry) , FALSE );
    gtk_entry_set_width_chars( GTK_ENTRY(synth_samplerate_optionentry) , 8 );
    gtk_entry_set_max_length( GTK_ENTRY(synth_samplerate_optionentry) , 5 );
    g_object_set_data( G_OBJECT(synth_samplerate_option[3]) , "customentry" , synth_samplerate_optionentry );
    g_signal_connect( G_OBJECT(synth_samplerate_option[3]) , "toggled" ,
                      G_CALLBACK(i_configure_ev_sysamplerate_togglecustom) , synth_samplerate_optionentry );
    synth_samplerate_optionlabel = gtk_label_new( _("Hz ") );
    gtk_box_pack_start( GTK_BOX(synth_samplerate_optionhbox) , synth_samplerate_optionentry , TRUE , TRUE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_samplerate_optionhbox) , synth_samplerate_optionlabel , FALSE , FALSE , 0 );
    switch ( fsyncfg->fsyn_synth_samplerate )
    {
      case 22050:
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_samplerate_option[0]) , TRUE );
        break;
      case 44100:
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_samplerate_option[1]) , TRUE );
        break;
      case 96000:
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_samplerate_option[2]) , TRUE );
        break;
      default:
        if (( fsyncfg->fsyn_synth_samplerate > 22050 ) && ( fsyncfg->fsyn_synth_samplerate < 96000 ))
        {
          gchar *samplerate_value = g_strdup_printf( "%i" , fsyncfg->fsyn_synth_samplerate );
          gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_samplerate_option[3]) , TRUE );
          gtk_entry_set_text( GTK_ENTRY(synth_samplerate_optionentry) , samplerate_value );
          g_free( samplerate_value );
        }
        else
          gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(synth_samplerate_option[1]) , TRUE );
    }
    gtk_box_pack_start( GTK_BOX(synth_samplerate_vbox) , synth_samplerate_option[0] , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_samplerate_vbox) , synth_samplerate_option[1] , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_samplerate_vbox) , synth_samplerate_option[2] , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_samplerate_vbox) , synth_samplerate_option[3] , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(synth_samplerate_vbox) , synth_samplerate_optionhbox , FALSE , FALSE , 0 );

    gtk_box_pack_start( GTK_BOX(content_vbox) , synth_frame , TRUE , TRUE , 0 );

    /* commit events  */
    g_signal_connect_swapped( G_OBJECT(commit_button) , "ap-commit" ,
                              G_CALLBACK(i_configure_ev_sflist_commit) , soundfont_file_lv );
    g_signal_connect_swapped( G_OBJECT(commit_button) , "ap-commit" ,
                              G_CALLBACK(i_configure_ev_sfload_commit) , soundfont_load_option[0] );
    g_signal_connect_swapped( G_OBJECT(commit_button) , "ap-commit" ,
                              G_CALLBACK(i_configure_ev_sygain_commit) , synth_gain_value_spin );
    g_signal_connect_swapped( G_OBJECT(commit_button) , "ap-commit" ,
                              G_CALLBACK(i_configure_ev_sypoly_commit) , synth_poly_value_spin );
    g_signal_connect_swapped( G_OBJECT(commit_button) , "ap-commit" ,
                              G_CALLBACK(i_configure_ev_syreverb_commit) , synth_reverb_value_option[0] );
    g_signal_connect_swapped( G_OBJECT(commit_button) , "ap-commit" ,
                              G_CALLBACK(i_configure_ev_sychorus_commit) , synth_chorus_value_option[0] );
    g_signal_connect_swapped( G_OBJECT(commit_button) , "ap-commit" ,
                              G_CALLBACK(i_configure_ev_sysamplerate_commit) , synth_samplerate_option[3] );

    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , soundfont_file_lv ,
                          _("* Select SoundFont files *\n"
                          "In order to play MIDI with FluidSynth, you need to specify at "
                          "least one valid SoundFont file here (use absolute paths). The "
                          "loading order is from the top (first) to the bottom (last).") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , soundfont_load_option[0] ,
                          _("* Load SoundFont on player start *\n"
                          "Depending on your system speed, SoundFont loading in FluidSynth will "
                          "require up to a few seconds. This is a one-time task (the soundfont "
                          "will stay loaded until it is changed or the backend is unloaded) that "
                          "can be done at player start, or before the first MIDI file is played "
                          "(the latter is a better choice if you don't use your player to listen "
                          "MIDI files only).") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , soundfont_load_option[1] ,
                          _("* Load SoundFont on first midifile play *\n"
                          "Depending on your system speed, SoundFont loading in FluidSynth will "
                          "require up to a few seconds. This is a one-time task (the soundfont "
                          "will stay loaded until it is changed or the backend is unloaded) that "
                          "can be done at player start, or before the first MIDI file is played "
                          "(the latter is a better choice if you don't use your player to listen "
                          "MIDI files only).") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , synth_gain_value_spin ,
                          _("* Synthesizer gain *\n"
                          "From FluidSynth docs: the gain is applied to the final or master output "
                          "of the synthesizer; it is set to a low value by default to avoid the "
                          "saturation of the output when random MIDI files are played.") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , synth_poly_value_spin ,
                          _("* Synthesizer polyphony *\n"
                          "From FluidSynth docs: the polyphony defines how many voices can be played "
                          "in parallel; the number of voices is not necessarily equivalent to the "
                          "number of notes played simultaneously; indeed, when a note is struck on a "
                          "specific MIDI channel, the preset on that channel may create several voices, "
                          "for example, one for the left audio channel and one for the right audio "
                          "channels; the number of voices activated depends on the number of instrument "
                          "zones that fall in the correspond to the velocity and key of the played "
                          "note.") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , synth_reverb_value_option[0] ,
                          _("* Synthesizer reverb *\n"
                          "From FluidSynth docs: when set to \"yes\" the reverb effects module is "
                          "activated; note that when the reverb module is active, the amount of "
                          "signal sent to the reverb module depends on the \"reverb send\" generator "
                          "defined in the SoundFont.") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , synth_reverb_value_option[1] ,
                          _("* Synthesizer reverb *\n"
                          "From FluidSynth docs: when set to \"yes\" the reverb effects module is "
                          "activated; note that when the reverb module is active, the amount of "
                          "signal sent to the reverb module depends on the \"reverb send\" generator "
                          "defined in the SoundFont.") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , synth_chorus_value_option[0] ,
                          _("* Synthesizer chorus *\n"
                          "From FluidSynth docs: when set to \"yes\" the chorus effects module is "
                          "activated; note that when the chorus module is active, the amount of "
                          "signal sent to the chorus module depends on the \"chorus send\" generator "
                          "defined in the SoundFont.") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , synth_chorus_value_option[1] ,
                          _("* Synthesizer chorus *\n"
                          "From FluidSynth docs: when set to \"yes\" the chorus effects module is "
                          "activated; note that when the chorus module is active, the amount of "
                          "signal sent to the chorus module depends on the \"chorus send\" generator "
                          "defined in the SoundFont.") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , synth_samplerate_option[0] ,
                          _("* Synthesizer samplerate *\n"
                          "The sample rate of the audio generated by the synthesizer. You can also specify "
                          "a custom value in the interval 22050Hz-96000Hz.") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , synth_samplerate_option[1] ,
                          _("* Synthesizer samplerate *\n"
                          "The sample rate of the audio generated by the synthesizer. You can also specify "
                          "a custom value in the interval 22050Hz-96000Hz.") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , synth_samplerate_option[2] ,
                          _("* Synthesizer samplerate *\n"
                          "The sample rate of the audio generated by the synthesizer. You can also specify "
                          "a custom value in the interval 22050Hz-96000Hz.") , "" );
    gtk_tooltips_set_tip( GTK_TOOLTIPS(tips) , synth_samplerate_option[3] ,
                          _("* Synthesizer samplerate *\n"
                          "The sample rate of the audio generated by the synthesizer. You can also specify "
                          "a custom value in the interval 22050Hz-96000Hz.") , "" );
  }
  else
  {
    /* display "not available" information */
    GtkWidget * info_label;
    info_label = gtk_label_new( _("FluidSynth Backend not loaded or not available") );
    gtk_box_pack_start( GTK_BOX(fsyn_page_vbox) , info_label , FALSE , FALSE , 2 );
  }

  gtk_box_pack_start( GTK_BOX(fsyn_page_vbox) , content_vbox , TRUE , TRUE , 2 );
  gtk_container_add( GTK_CONTAINER(fsyn_page_alignment) , fsyn_page_vbox );
}


void i_configure_gui_tablabel_fsyn( GtkWidget * fsyn_page_alignment ,
                                  gpointer backend_list_p ,
                                  gpointer commit_button )
{
  GtkWidget *pagelabel_vbox, *pagelabel_image, *pagelabel_label;
  GdkPixbuf *pagelabel_image_pix;
  pagelabel_vbox = gtk_vbox_new( FALSE , 1 );
  pagelabel_image_pix = gdk_pixbuf_new_from_xpm_data( (const gchar **)backend_fluidsynth_icon_xpm );
  pagelabel_image = gtk_image_new_from_pixbuf( pagelabel_image_pix ); g_object_unref( pagelabel_image_pix );
  pagelabel_label = gtk_label_new( "" );
  gtk_label_set_markup( GTK_LABEL(pagelabel_label) , _("<span size=\"smaller\">FluidSynth\nbackend</span>") );
  gtk_label_set_justify( GTK_LABEL(pagelabel_label) , GTK_JUSTIFY_CENTER );
  gtk_box_pack_start( GTK_BOX(pagelabel_vbox) , pagelabel_image , FALSE , FALSE , 1 );
  gtk_box_pack_start( GTK_BOX(pagelabel_vbox) , pagelabel_label , FALSE , FALSE , 1 );
  gtk_container_add( GTK_CONTAINER(fsyn_page_alignment) , pagelabel_vbox );
  gtk_widget_show_all( fsyn_page_alignment );
  return;
}


void i_configure_cfg_fsyn_alloc( void )
{
  amidiplug_cfg_fsyn_t * fsyncfg = g_malloc(sizeof(amidiplug_cfg_fsyn_t));
  amidiplug_cfg_backend->fsyn = fsyncfg;
}


void i_configure_cfg_fsyn_free( void )
{
  amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;
  g_free( fsyncfg->fsyn_soundfont_file );
}


void i_configure_cfg_fsyn_read( pcfg_t * cfgfile )
{
  amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;

  if (!cfgfile)
  {
    /* fluidsynth backend defaults */
    fsyncfg->fsyn_soundfont_file = g_strdup( "" );
    fsyncfg->fsyn_soundfont_load = 1;
    fsyncfg->fsyn_synth_samplerate = 44100;
    fsyncfg->fsyn_synth_gain = -1;
    fsyncfg->fsyn_synth_poliphony = -1;
    fsyncfg->fsyn_synth_reverb = -1;
    fsyncfg->fsyn_synth_chorus = -1;
  }
  else
  {
    i_pcfg_read_string( cfgfile , "fsyn" , "fsyn_soundfont_file" , &fsyncfg->fsyn_soundfont_file , "" );
    i_pcfg_read_integer( cfgfile , "fsyn" , "fsyn_soundfont_load" , &fsyncfg->fsyn_soundfont_load , 1 );
    i_pcfg_read_integer( cfgfile , "fsyn" , "fsyn_synth_samplerate" , &fsyncfg->fsyn_synth_samplerate , 44100 );
    i_pcfg_read_integer( cfgfile , "fsyn" , "fsyn_synth_gain" , &fsyncfg->fsyn_synth_gain , -1 );
    i_pcfg_read_integer( cfgfile , "fsyn" , "fsyn_synth_poliphony" , &fsyncfg->fsyn_synth_poliphony , -1 );
    i_pcfg_read_integer( cfgfile , "fsyn" , "fsyn_synth_reverb" , &fsyncfg->fsyn_synth_reverb , -1 );
    i_pcfg_read_integer( cfgfile , "fsyn" , "fsyn_synth_chorus" , &fsyncfg->fsyn_synth_chorus , -1 );
  }
}


void i_configure_cfg_fsyn_save( pcfg_t * cfgfile )
{
  amidiplug_cfg_fsyn_t * fsyncfg = amidiplug_cfg_backend->fsyn;

  i_pcfg_write_string( cfgfile , "fsyn" , "fsyn_soundfont_file" , fsyncfg->fsyn_soundfont_file );
  i_pcfg_write_integer( cfgfile , "fsyn" , "fsyn_soundfont_load" , fsyncfg->fsyn_soundfont_load );
  i_pcfg_write_integer( cfgfile , "fsyn" , "fsyn_synth_samplerate" , fsyncfg->fsyn_synth_samplerate );
  i_pcfg_write_integer( cfgfile , "fsyn" , "fsyn_synth_gain" , fsyncfg->fsyn_synth_gain );
  i_pcfg_write_integer( cfgfile , "fsyn" , "fsyn_synth_poliphony" , fsyncfg->fsyn_synth_poliphony );
  i_pcfg_write_integer( cfgfile , "fsyn" , "fsyn_synth_reverb" , fsyncfg->fsyn_synth_reverb );
  i_pcfg_write_integer( cfgfile , "fsyn" , "fsyn_synth_chorus" , fsyncfg->fsyn_synth_chorus );
}
