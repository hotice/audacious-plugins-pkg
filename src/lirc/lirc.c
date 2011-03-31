/* Lirc plugin

   Copyright (C) 2005 Audacious development team

   Copyright (c) 1998-1999 Carl van Schaik (carl@leg.uct.ac.za)

   Copyright (C) 2000 Christoph Bartelmus (xmms@bartelmus.de)

   some code was stolen from:
   IRman plugin for xmms by Charles Sielski (stray@teklabs.net)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <ctype.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <lirc/lirc_client.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>

#include "lirc.h"

#include "common.h"

const char *plugin_name="LIRC Plugin";

GeneralPlugin lirc_plugin = {
    .description = "LIRC Plugin",
    .init = init,
    .about = about,
    .configure = configure,
    .cleanup = cleanup
};

GeneralPlugin *lirc_gplist[] = { &lirc_plugin, NULL };
DECLARE_PLUGIN(lirc, NULL, NULL, NULL, NULL, NULL, lirc_gplist, NULL, NULL);

int lirc_fd=-1;
struct lirc_config *config=NULL;
gint tracknr=0;
gint mute=0;        /* mute flag */
gint mute_vol=0;    /* holds volume before mute */

gint input_tag;

char track_no[64];
int track_no_pos;

gint tid;

void init_lirc(void)
{
	int flags;

	if((lirc_fd=lirc_init("audacious",1))==-1)
	{
		fprintf(stderr,_("%s: could not init LIRC support\n"),
			plugin_name);
		return;
	}
	if(lirc_readconfig(NULL,&config,NULL)==-1)
	{
		lirc_deinit();
		fprintf(stderr,
			_("%s: could not read LIRC config file\n"
			"%s: please read the documentation of LIRC\n"
			"%s: how to create a proper config file\n"),
			plugin_name,plugin_name,plugin_name);
		return;
	}
	input_tag=gdk_input_add(lirc_fd,GDK_INPUT_READ,
				lirc_input_callback,NULL);
	fcntl(lirc_fd,F_SETOWN,getpid());
	flags=fcntl(lirc_fd,F_GETFL,0);
	if(flags!=-1)
	{
		fcntl(lirc_fd,F_SETFL,flags|O_NONBLOCK);
	}
	fflush(stdout);
}

void init(void)
{
	load_cfg();
	init_lirc();
	track_no_pos=0;
	tid=0;
}

gboolean reconnect_lirc(gpointer data)
{
	fprintf(stderr,_("%s: trying to reconnect...\n"),plugin_name);
	init();
	return (lirc_fd==-1);
}

gboolean jump_to(gpointer data)
{
	aud_drct_pl_set_pos(atoi(track_no)-1);
	track_no_pos=0;
	tid=0;
	return FALSE;
}

void lirc_input_callback(gpointer data,gint source,
			 GdkInputCondition condition)
{
	char *code;
	char *c;
	gint playlist_time,playlist_pos,output_time,v;
	int ret;
	char *ptr;
	gint balance;
#if 0
	gboolean show_pl;
#endif
	int n;
	gchar *utf8_title_markup;

	while((ret=lirc_nextcode(&code))==0 && code!=NULL)
	{
		while((ret=lirc_code2char(config,code,&c))==0 && c!=NULL)
		{
			if(strcasecmp("PLAY",c)==0)
			{
				aud_drct_play();
			}
			else if(strcasecmp("STOP",c)==0)
			{
				aud_drct_stop();
			}
			else if(strcasecmp("PAUSE",c)==0)
			{
				aud_drct_pause();
			}
			else if(strcasecmp("PLAYPAUSE",c) == 0)
			{
				if(aud_drct_get_playing())
					aud_drct_pause();
				else
					aud_drct_play();
			}
			else if(strncasecmp("NEXT",c,4)==0)
			{
                                ptr=c+4;
                                while(isspace(*ptr)) ptr++;
				n=atoi(ptr);

				if(n<=0) n=1;
				for(;n>0;n--)
				{
					aud_drct_pl_next();
				}
			}
			else if(strncasecmp("PREV",c,4)==0)
			{
                                ptr=c+4;
                                while(isspace(*ptr)) ptr++;
				n=atoi(ptr);

				if(n<=0) n=1;
				for(;n>0;n--)
				{
					aud_drct_pl_prev();
				}
			}
			else if(strcasecmp("SHUFFLE",c)==0)
			{
				aud_drct_pl_shuffle_toggle();
			}
			else if(strcasecmp("REPEAT",c)==0)
			{
				aud_drct_pl_repeat_toggle();
			}
			else if(strncasecmp("FWD",c,3)==0)
			{
                                ptr=c+3;
                                while(isspace(*ptr)) ptr++;
				n=atoi(ptr)*1000;

				if(n<=0) n=5000;
				output_time=aud_drct_get_time();
				playlist_pos=aud_drct_pl_get_pos();
				playlist_time=aud_drct_pl_get_time(playlist_pos);
				if(playlist_time-output_time<n)
					output_time=playlist_time-n;
				aud_drct_seek(output_time+n);
			}
			else if(strncasecmp("BWD",c,3)==0)
			{
                                ptr=c+3;
                                while(isspace(*ptr)) ptr++;
				n=atoi(ptr)*1000;

				if(n<=0) n=5000;
				output_time=aud_drct_get_time();
				if(output_time<n)
					output_time=n;
				aud_drct_seek(output_time-n);
			}
			else if(strncasecmp("VOL_UP",c,6)==0)
			{
                                ptr=c+6;
                                while(isspace(*ptr)) ptr++;
				n=atoi(ptr);
                                if(n<=0) n=5;

				aud_drct_get_volume_main(&v);
				if(v > (100-n)) v=100-n;
				aud_drct_set_volume_main(v+n);
			}
			else if(strncasecmp("VOL_DOWN",c,8)==0)
			{
                                ptr=c+8;
                                while (isspace(*ptr)) ptr++;
				n=atoi(ptr);
                                if(n<=0) n=5;

				aud_drct_get_volume_main(&v);
				if(v<n) v=n;
				aud_drct_set_volume_main(v-n);
			}
			else if(strcasecmp("QUIT",c)==0)
			{
				aud_drct_quit();
			}
			else if(strcasecmp("MUTE",c)==0)
 			{
				if(mute==0)
 				{
 					mute=1;
 					/* store the master volume so
                                           we can restore it on unmute. */
					aud_drct_get_volume_main(&mute_vol);
 					aud_drct_set_volume_main(0);
 				}
 				else
 				{
 					mute=0;
 					aud_drct_set_volume_main(mute_vol);
 				}
			}
			else if(strncasecmp("BAL_LEFT",c,8)==0)
			{
                                ptr=c+8;
                                while(isspace(*ptr)) ptr++;
				n=atoi(ptr);
				if(n<=0) n=5;

				aud_drct_get_volume_balance(&balance);
				balance-=n;
				if(balance<-100) balance=-100;
				aud_drct_set_volume_balance(balance);
			}
			else if(strncasecmp("BAL_RIGHT",c,9)==0)
			{
                                ptr=c+9;
                                while(isspace(*ptr)) ptr++;
				n=atoi(ptr);
				if(n<=0) n=5;

				aud_drct_get_volume_balance(&balance);
				balance+=n;
				if(balance>100) balance=100;
				aud_drct_set_volume_balance(balance);
			}
			else if(strcasecmp("BAL_CENTER",c)==0)
			{
				balance=0;
				aud_drct_set_volume_balance(balance);
			}
			else if(strcasecmp("LIST",c)==0)
			{
#if 0
				show_pl=aud_drct_pl_win_is_visible();
				show_pl=(show_pl) ? 0:1;
				aud_drct_pl_win_toggle(show_pl);
#endif
 			}
			else if(strcasecmp("PLAYLIST_CLEAR",c)==0)
			{
				aud_drct_stop();
				aud_drct_pl_clear();
			}
			else if(strncasecmp("PLAYLIST_ADD ",c,13)==0)
			{
				GList list;
				list.prev=list.next=NULL;
				list.data=c+13;
				aud_drct_pl_add_list (& list, -1);
			}
			else if((strlen(c)==1) && ((*c>='0') || (*c<='9')))
			{
				if (track_no_pos<63)
				{
					if (tid) g_source_remove(tid);
					track_no[track_no_pos++]=*c;
					track_no[track_no_pos]=0;
					tid=g_timeout_add(1500, jump_to, NULL);
					utf8_title_markup = g_markup_printf_escaped(
					    "<span font_desc='%s'>%s</span>", aosd_font, track_no);
					hook_call("aosd toggle", utf8_title_markup);
				}
			}
			else
			{
				fprintf(stderr,_("%s: unknown command \"%s\"\n"),
					plugin_name,c);
			}
		}
		free(code);
		if(ret==-1) break;
	}
	if(ret==-1)
	{
		/* something went badly wrong */
		fprintf(stderr,_("%s: disconnected from LIRC\n"),plugin_name);
		cleanup();
		if(b_enable_reconnect)
		{
			fprintf(stderr,_("%s: will try reconnect every %d seconds...\n"),plugin_name,reconnect_timeout);
			g_timeout_add(1000*reconnect_timeout, reconnect_lirc, NULL);
		}
	}
}

void cleanup()
{
	if(config)
	{
		if(input_tag)
			gtk_input_remove(input_tag);

		config=NULL;
	}
	if(lirc_fd!=-1)
	{
		lirc_deinit();
		lirc_fd=-1;
	}
	g_free(aosd_font);
}
