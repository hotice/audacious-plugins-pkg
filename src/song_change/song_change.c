/*
 * Audacious: A cross-platform multimedia player.
 * Copyright (c) 2005  Audacious Team
 */
#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#include <string.h>

#include <audacious/configdb.h>
#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>
#include <libaudcore/hook.h>
#include <libaudcore/audstrings.h>

#include "formatter.h"

static PluginPreferences preferences;

static void init(void);
static void cleanup(void);
static void songchange_playback_begin(gpointer unused, gpointer unused2);
static void songchange_playback_end(gpointer unused, gpointer unused2);
static void songchange_playlist_eof(gpointer unused, gpointer unused2);
//static void songchange_playback_ttc(gpointer, gpointer);

typedef struct
{
  gchar *title;
  gchar *filename;
}
songchange_playback_ttc_prevs_t;
static songchange_playback_ttc_prevs_t *ttc_prevs = NULL;

static char *cmd_line = NULL;
static char *cmd_line_after = NULL;
static char *cmd_line_end = NULL;
static char *cmd_line_ttc = NULL;

static GtkWidget *cmd_warn_label, *cmd_warn_img;

GeneralPlugin sc_gp =
{
	.description = "Song Change " PACKAGE_VERSION,
	.init = init,
	.cleanup = cleanup,
	.settings = &preferences,
};

GeneralPlugin *songchange_gplist[] = { &sc_gp, NULL };
SIMPLE_GENERAL_PLUGIN(songchange, songchange_gplist);

static void bury_child(int signal)
{
	waitpid(-1, NULL, WNOHANG);
}

static void execute_command(char *cmd)
{
	char *argv[4] = {"/bin/sh", "-c", NULL, NULL};
	int i;
	argv[2] = cmd;
	signal(SIGCHLD, bury_child);
	if (fork() == 0)
	{
		/* We don't want this process to hog the audio device etc */
		for (i = 3; i < 255; i++)
			close(i);
		execv("/bin/sh", argv);
	}
}

/* Format codes:
 *
 *   F - frequency (in hertz)
 *   c - number of channels
 *   f - filename (full path)
 *   l - length (in milliseconds)
 *   n - name
 *   r - rate (in bits per second)
 *   s - name (since everyone's used to it)
 *   t - playlist position (%02d)
 *   p - currently playing (1 or 0)
 */
/* do_command(): do @cmd after replacing the format codes
   @cmd: command to run
   @current_file: file name of current song
   @pos: playlist_pos */
static void
do_command(char *cmd, const char *current_file, int pos)
{
	int length, rate, freq, nch;
	char *str, *shstring = NULL, *temp, numbuf[32];
	gboolean playing;
	Formatter *formatter;

	if (cmd && strlen(cmd) > 0)
	{
		formatter = formatter_new();
		str = aud_drct_pl_get_title(pos);
		if (str)
		{
			temp = escape_shell_chars(str);
			formatter_associate(formatter, 's', temp);
			formatter_associate(formatter, 'n', temp);
			g_free(str);
			g_free(temp);
		}
		else
		{
			formatter_associate(formatter, 's', "");
			formatter_associate(formatter, 'n', "");
		}

		if (current_file)
		{
			temp = escape_shell_chars(current_file);
			formatter_associate(formatter, 'f', temp);
			g_free(temp);
		}
		else
			formatter_associate(formatter, 'f', "");
		g_snprintf(numbuf, sizeof(numbuf), "%02d", pos + 1);
		formatter_associate(formatter, 't', numbuf);
		length = aud_drct_pl_get_time(pos);
		if (length != -1)
		{
			g_snprintf(numbuf, sizeof(numbuf), "%d", length);
			formatter_associate(formatter, 'l', numbuf);
		}
		else
			formatter_associate(formatter, 'l', "0");
		aud_drct_get_info(&rate, &freq, &nch);
		g_snprintf(numbuf, sizeof(numbuf), "%d", rate);
		formatter_associate(formatter, 'r', numbuf);
		g_snprintf(numbuf, sizeof(numbuf), "%d", freq);
		formatter_associate(formatter, 'F', numbuf);
		g_snprintf(numbuf, sizeof(numbuf), "%d", nch);
		formatter_associate(formatter, 'c', numbuf);
		playing = aud_drct_get_playing();
		g_snprintf(numbuf, sizeof(numbuf), "%d", playing);
		formatter_associate(formatter, 'p', numbuf);
		shstring = formatter_format(formatter, cmd);
		formatter_destroy(formatter);

		if (shstring)
		{
			execute_command(shstring);
			/* FIXME: This can possibly be freed too early */
			g_free(shstring);
		}
	}
}

static void read_config(void)
{
	mcs_handle_t *db;

	db = aud_cfg_db_open();
	if ( !aud_cfg_db_get_string(db, "song_change", "cmd_line", &cmd_line) )
		cmd_line = g_strdup("");
	if ( !aud_cfg_db_get_string(db, "song_change", "cmd_line_after", &cmd_line_after) )
		cmd_line_after = g_strdup("");
	if ( !aud_cfg_db_get_string(db, "song_change", "cmd_line_end", &cmd_line_end) )
		cmd_line_end = g_strdup("");
	if ( !aud_cfg_db_get_string(db, "song_change", "cmd_line_ttc", &cmd_line_ttc) )
		cmd_line_ttc = g_strdup("");
	aud_cfg_db_close(db);
}

static void cleanup(void)
{
	hook_dissociate("playback begin", songchange_playback_begin);
	hook_dissociate("playback end", songchange_playback_end);
	hook_dissociate("playlist end reached", songchange_playlist_eof);
//      hook_dissociate( "playlist set info" , songchange_playback_ttc);

	if ( ttc_prevs != NULL )
	{
		if ( ttc_prevs->title != NULL ) g_free( ttc_prevs->title );
		if ( ttc_prevs->filename != NULL ) g_free( ttc_prevs->filename );
		g_free( ttc_prevs );
		ttc_prevs = NULL;
	}

	g_free(cmd_line);
	g_free(cmd_line_after);
	g_free(cmd_line_end);
	g_free(cmd_line_ttc);
	cmd_line = NULL;
	cmd_line_after = NULL;
	cmd_line_end = NULL;
	cmd_line_ttc = NULL;
	signal(SIGCHLD, SIG_DFL);
}

static void save_and_close(gchar * cmd, gchar * cmd_after, gchar * cmd_end, gchar * cmd_ttc)
{
	mcs_handle_t *db;

	db = aud_cfg_db_open();
	aud_cfg_db_set_string(db, "song_change", "cmd_line", cmd);
	aud_cfg_db_set_string(db, "song_change", "cmd_line_after", cmd_after);
	aud_cfg_db_set_string(db, "song_change", "cmd_line_end", cmd_end);
	aud_cfg_db_set_string(db, "song_change", "cmd_line_ttc", cmd_ttc);
	aud_cfg_db_close(db);

	if (cmd_line != NULL)
		g_free(cmd_line);

	cmd_line = g_strdup(cmd);

	if (cmd_line_after != NULL)
		g_free(cmd_line_after);

	cmd_line_after = g_strdup(cmd_after);

	if (cmd_line_end != NULL)
		g_free(cmd_line_end);

	cmd_line_end = g_strdup(cmd_end);

	if (cmd_line_ttc != NULL)
		g_free(cmd_line_ttc);

	cmd_line_ttc = g_strdup(cmd_ttc);
}

static int check_command(char *command)
{
	const char *dangerous = "fns";
	char *c;
	int qu = 0;

	for (c = command; *c != '\0'; c++)
	{
		if (*c == '"' && (c == command || *(c - 1) != '\\'))
			qu = !qu;
		else if (*c == '%' && !qu && strchr(dangerous, *(c + 1)))
			return -1;
	}
	return 0;
}

static void init(void)
{
	read_config();

	hook_associate("playback begin", songchange_playback_begin, NULL);
	hook_associate("playback end", songchange_playback_end, NULL);
	hook_associate("playlist end reached", songchange_playlist_eof, NULL);

	ttc_prevs = g_malloc0(sizeof(songchange_playback_ttc_prevs_t));
	ttc_prevs->title = NULL;
	ttc_prevs->filename = NULL;
//	hook_associate( "playlist set info" , songchange_playback_ttc , ttc_prevs );
}

static void
songchange_playback_begin(gpointer unused, gpointer unused2)
{
	int pos;
	char *current_file;

	pos = aud_drct_pl_get_pos();
	current_file = aud_drct_pl_get_file(pos);

	do_command(cmd_line, current_file, pos);

	g_free(current_file);
}

static void
songchange_playback_end(gpointer unused, gpointer unused2)
{
	int pos;
	char *current_file;

	pos = aud_drct_pl_get_pos();
	current_file = aud_drct_pl_get_file(pos);

	do_command(cmd_line_after, current_file, pos);

	g_free(current_file);
}

#if 0
static void
songchange_playback_ttc(gpointer plentry_gp, gpointer prevs_gp)
{
  if ( ( aud_ip_state->playing ) && ( strcmp(cmd_line_ttc,"") ) )
  {
    songchange_playback_ttc_prevs_t *prevs = prevs_gp;
    PlaylistEntry *pl_entry = plentry_gp;

    /* same filename but title changed, useful to detect http stream song changes */

    if ( ( prevs->title != NULL ) && ( prevs->filename != NULL ) )
    {
      if ( ( pl_entry->filename != NULL ) && ( !strcmp(pl_entry->filename,prevs->filename) ) )
      {
        if ( ( pl_entry->title != NULL ) && ( strcmp(pl_entry->title,prevs->title) ) )
        {
          int pos = aud_drct_pl_get_pos();
          char *current_file = aud_drct_pl_get_file(pos);
          do_command(cmd_line_ttc, current_file, pos);
          g_free(current_file);
          g_free(prevs->title);
          prevs->title = g_strdup(pl_entry->title);
        }
      }
      else
      {
        g_free(prevs->filename);
        prevs->filename = g_strdup(pl_entry->filename);
        /* if filename changes, reset title as well */
        if ( prevs->title != NULL )
          g_free(prevs->title);
        prevs->title = g_strdup(pl_entry->title);
      }
    }
    else
    {
      if ( prevs->title != NULL )
        g_free(prevs->title);
      prevs->title = g_strdup(pl_entry->title);
      if ( prevs->filename != NULL )
        g_free(prevs->filename);
      prevs->filename = g_strdup(pl_entry->filename);
    }
  }
}
#endif

static void
songchange_playlist_eof(gpointer unused, gpointer unused2)
{
	gint pos;
	gchar *current_file;

	pos = aud_drct_pl_get_pos();
	current_file = aud_drct_pl_get_file(pos);

	do_command(cmd_line_end, current_file, pos);

	g_free(current_file);
}

typedef struct {
    gchar * cmd;
    gchar * cmd_after;
    gchar * cmd_end;
    gchar * cmd_ttc;
} SongChangeConfig;

static SongChangeConfig config = {NULL};

static void configure_ok_cb()
{
	char *cmd, *cmd_after, *cmd_end, *cmd_ttc;
g_message("AAAA");
	cmd = g_strdup(config.cmd);
	cmd_after = g_strdup(config.cmd_after);
	cmd_end = g_strdup(config.cmd_end);
	cmd_ttc = g_strdup(config.cmd_ttc);

	if (check_command(cmd) < 0 || check_command(cmd_after) < 0 ||
	    check_command(cmd_end) < 0 || check_command(cmd_ttc) < 0)
	{
		gtk_widget_show(cmd_warn_img);
		gtk_widget_show(cmd_warn_label);
	}
	else
	{
		gtk_widget_hide(cmd_warn_img);
		gtk_widget_hide(cmd_warn_label);
		save_and_close(cmd, cmd_after, cmd_end, cmd_ttc);
	}

	g_free(cmd);
	g_free(cmd_after);
	g_free(cmd_end);
	g_free(cmd_ttc);
}

static PreferencesWidget elements[] = {
    {WIDGET_LABEL, N_("Command to run when Audacious starts a new song."), NULL, NULL, NULL, FALSE, {.label = {.single_line = TRUE}}},
    {WIDGET_ENTRY, N_("Command:"), &config.cmd, configure_ok_cb, NULL, FALSE, {.entry = {FALSE}}, VALUE_STRING},
    {WIDGET_SEPARATOR, NULL, NULL, NULL, NULL, FALSE, {.separator = {TRUE}}},

    {WIDGET_LABEL, N_("Command to run toward the end of a song."), NULL, NULL, NULL, FALSE, {.label = {.single_line = TRUE}}},
    {WIDGET_ENTRY, N_("Command:"), &config.cmd_after, configure_ok_cb, NULL, FALSE, {.entry = {FALSE}}, VALUE_STRING},
    {WIDGET_SEPARATOR, NULL, NULL, NULL, NULL, FALSE, {.separator = {TRUE}}},

    {WIDGET_LABEL, N_("Command to run when Audacious reaches the end of the playlist."), NULL, NULL, NULL, FALSE, {.label = {.single_line = TRUE}}},
    {WIDGET_ENTRY, N_("Command:"), &config.cmd_end, configure_ok_cb, NULL, FALSE, {.entry = {FALSE}}, VALUE_STRING},
    {WIDGET_SEPARATOR, NULL, NULL, NULL, NULL, FALSE, {.separator = {TRUE}}},

    {WIDGET_LABEL, N_("Command to run when title changes for a song (i.e. network streams titles)."), NULL, NULL, NULL, FALSE, {.label = {.single_line = TRUE}}},
    {WIDGET_ENTRY, N_("Command:"), &config.cmd_ttc, configure_ok_cb, NULL, FALSE, {.entry = {FALSE}}, VALUE_STRING},
    {WIDGET_SEPARATOR, NULL, NULL, NULL, NULL, FALSE, {.separator = {TRUE}}},

    {WIDGET_LABEL, N_("You can use the following format strings which\n"
                      "will be substituted before calling the command\n"
                      "(not all are useful for the end-of-playlist command).\n\n"
                      "%F: Frequency (in hertz)\n"
                      "%c: Number of channels\n"
                      "%f: filename (full path)\n"
                      "%l: length (in milliseconds)\n"
                      "%n or %s: Song name\n"
                      "%r: Rate (in bits per second)\n"
                      "%t: Playlist position (%02d)\n"
                      "%p: Currently playing (1 or 0)"), NULL, NULL, NULL, FALSE},
};

/* static GtkWidget * custom_warning (void) */
static void * custom_warning (void)
{
    GtkWidget *bbox_hbox;
    gchar * temp;

    bbox_hbox = gtk_hbox_new(FALSE, 6);

    cmd_warn_img = gtk_image_new_from_stock("gtk-dialog-warning", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(bbox_hbox), cmd_warn_img, FALSE, FALSE, 0);

    temp = g_strdup_printf(_("<span size='small'>Parameters passed to the shell should be encapsulated in quotes. Doing otherwise is a security risk.</span>"));
    cmd_warn_label = gtk_label_new(temp);
    gtk_label_set_markup(GTK_LABEL(cmd_warn_label), temp);
    gtk_label_set_line_wrap(GTK_LABEL(cmd_warn_label), TRUE);
    gtk_box_pack_start(GTK_BOX(bbox_hbox), cmd_warn_label, FALSE, FALSE, 0);
    g_free(temp);

    return bbox_hbox;
}

static PreferencesWidget settings[] = {
    {WIDGET_BOX, N_("Commands"), NULL, NULL, NULL, FALSE,
        {.box = {elements, G_N_ELEMENTS(elements),
                 FALSE, TRUE}}},
    {WIDGET_CUSTOM, NULL, NULL, NULL, NULL, FALSE, {.populate = custom_warning}},
/*    {WIDGET_LABEL, N_("<span size='small'>Parameters passed to the shell should be encapsulated in quotes. Doing otherwise is a security risk.</span>"),
        NULL, NULL, NULL, FALSE, {.label = {"gtk-dialog-warning"}}},*/
};

static void
configure_init(void)
{
    read_config();

    config.cmd = g_strdup(cmd_line);
    config.cmd_after = g_strdup(cmd_line_after);
    config.cmd_end = g_strdup(cmd_line_end);
    config.cmd_ttc = g_strdup(cmd_line_ttc);
}

static void
configure_cleanup(void)
{
    g_free(config.cmd);
    config.cmd = NULL;

    g_free(config.cmd_after);
    config.cmd_after = NULL;

    g_free(config.cmd_end);
    config.cmd_end = NULL;

    g_free(config.cmd_ttc);
    config.cmd_ttc = NULL;
}

static PluginPreferences preferences = {
    .title = N_("Song Change"),
    .imgurl = DATA_DIR "/images/plugins.png",
    .prefs = settings,
    .n_prefs = G_N_ELEMENTS(settings),
    .init = configure_init,
    .cleanup = configure_cleanup,
};
