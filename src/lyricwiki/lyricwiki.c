/*
 * Copyright (c) 2010 William Pitcock <nenolod@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * TODO:
 *   - do the VFS operations in another thread
 *   - why don't we have a url encoding function?
 *     ... or do we and I'm just smoking crack?
 *   - no idea what this code does if lyricwiki is down - probably crashes!
 */

#include <stdio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <mowgli.h>

#include <audacious/drct.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <audacious/plugins.h>
#include <libaudcore/hook.h>
#include <libaudcore/vfs_async.h>

#include "urlencode.h"

/*
 * Suppress libxml warnings, because lyricwiki does not generate anything near
 * valid HTML.
 */
void
libxml_error_handler(void *ctx, const char *msg, ...)
{

}

/* Oh, this is going to be fun... */
gchar *
scrape_lyrics_from_lyricwiki_edit_page(const gchar *buf, gsize len)
{
	xmlDocPtr doc;
	gchar *ret = NULL;

	/*
	 * temporarily set our error-handling functor to our suppression function,
	 * but we have to set it back because other components of Audacious depend
	 * on libxml and we don't want to step on their code paths.
	 *
	 * unfortunately, libxml is anti-social and provides us with no way to get
	 * the previous error functor, so we just have to set it back to default after
	 * parsing and hope for the best.
	 */
	xmlSetGenericErrorFunc(NULL, libxml_error_handler);
	doc = htmlReadMemory(buf, (int) len, NULL, "utf-8", (HTML_PARSE_RECOVER | HTML_PARSE_NONET));
	xmlSetGenericErrorFunc(NULL, NULL);

	if (doc != NULL)
	{
		xmlXPathContextPtr xpath_ctx = NULL;
		xmlXPathObjectPtr xpath_obj = NULL;
		xmlNodePtr node = NULL;

		xpath_ctx = xmlXPathNewContext(doc);
		if (xpath_ctx == NULL)
			goto give_up;

		xpath_obj = xmlXPathEvalExpression((xmlChar *) "//*[@id=\"wpTextbox1\"]", xpath_ctx);
		if (xpath_obj == NULL)
			goto give_up;

		if (!xpath_obj->nodesetval->nodeMax)
			goto give_up;

		node = xpath_obj->nodesetval->nodeTab[0];
give_up:
		if (xpath_obj != NULL)
			xmlXPathFreeObject(xpath_obj);

		if (xpath_ctx != NULL)
			xmlXPathFreeContext(xpath_ctx);

		if (node != NULL)
		{
			xmlChar *lyric = xmlNodeGetContent(node);

			if (lyric != NULL)
			{
				GMatchInfo *match_info;
				GRegex *reg;

				reg = g_regex_new("<(lyrics?)>(.*)</\\1>", (G_REGEX_MULTILINE | G_REGEX_DOTALL), 0, NULL);
				g_regex_match(reg, (gchar *) lyric, G_REGEX_MATCH_NEWLINE_ANY, &match_info);

				ret = g_match_info_fetch(match_info, 2);
				if (!g_utf8_collate(ret, "\n\n<!-- PUT LYRICS HERE (and delete this entire line) -->\n\n"))
				{
					g_free(ret);
					ret = NULL;
				}
			}

			xmlFree(lyric);
		}
	}

	return ret;
}

gchar *
scrape_uri_from_lyricwiki_search_result(const gchar *buf, gsize len)
{
	xmlDocPtr doc;
	gchar *uri = NULL;

	/*
	 * temporarily set our error-handling functor to our suppression function,
	 * but we have to set it back because other components of Audacious depend
	 * on libxml and we don't want to step on their code paths.
	 *
	 * unfortunately, libxml is anti-social and provides us with no way to get
	 * the previous error functor, so we just have to set it back to default after
	 * parsing and hope for the best.
	 */
	xmlSetGenericErrorFunc(NULL, libxml_error_handler);
	doc = xmlParseMemory(buf, (int) len);
	xmlSetGenericErrorFunc(NULL, NULL);

	if (doc != NULL)
	{
		xmlNodePtr root, cur;

		root = xmlDocGetRootElement(doc);

		MOWGLI_ITER_FOREACH(cur, root->xmlChildrenNode)
		{
			if (xmlStrEqual(cur->name, (xmlChar *) "url"))
			{
				xmlChar *lyric;
				gchar *basename;

				lyric = xmlNodeGetContent(cur);
				basename = g_path_get_basename((gchar *) lyric);

				uri = g_strdup_printf("http://lyrics.wikia.com/index.php?action=edit&title=%s", basename);
				g_free(basename);
				xmlFree(lyric);
			}
		}
	}

	return uri;
}

void update_lyrics_window(const Tuple *tu, const gchar *lyrics);

gboolean
get_lyrics_step_3(gchar *buf, gint64 len, Tuple *tu)
{
	gchar *lyrics = NULL;

	if (buf != NULL)
	{
		lyrics = scrape_lyrics_from_lyricwiki_edit_page(buf, len);
		g_free(buf);
	}

	update_lyrics_window(tu, lyrics);
	mowgli_object_unref(tu);

	if (lyrics != NULL)
		g_free(lyrics);

	return buf == NULL ? FALSE : TRUE;
}

gboolean
get_lyrics_step_2(gchar *buf, gint64 len, Tuple *tu)
{
	gchar *uri;

	uri = scrape_uri_from_lyricwiki_search_result(buf, len);

	vfs_async_file_get_contents(uri, (VFSConsumer) get_lyrics_step_3, tu);

	g_free(buf);

	return TRUE;
}

void
get_lyrics_step_1(const Tuple *tu)
{
	gchar *uri;
	gchar *artist, *title;
	Tuple *tuple = tuple_copy(tu);

	artist = lyricwiki_url_encode(tuple_get_string(tu, FIELD_ARTIST, NULL));
	title = lyricwiki_url_encode(tuple_get_string(tu, FIELD_TITLE, NULL));

	uri = g_strdup_printf("http://lyrics.wikia.com/api.php?action=lyrics&artist=%s&song=%s&fmt=xml", artist, title);

	g_free(artist);
	g_free(title);

	vfs_async_file_get_contents(uri, (VFSConsumer) get_lyrics_step_2, tuple);

	g_free(uri);
}

GtkWidget *window, *textview;
GtkTextBuffer *textbuffer;

static gboolean window_delete();

GtkWidget *
build_widget(void)
{
	GtkWidget *scrollview, *vbox;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("LyricWiki"));
	gtk_window_set_default_size(GTK_WINDOW(window), 300, 500);

	textview = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 12);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textview), 12);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD);
	textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	scrollview = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollview), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	vbox = gtk_vbox_new(FALSE, 10);

	gtk_container_add(GTK_CONTAINER(scrollview), textview);

	gtk_box_pack_start(GTK_BOX(vbox), scrollview, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtk_widget_show(textview);
	gtk_widget_show(scrollview);
	gtk_widget_show(vbox);
	gtk_widget_show(window);

	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(textbuffer), "weight_bold", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(textbuffer), "size_x_large", "scale", PANGO_SCALE_X_LARGE, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(textbuffer), "style_italic", "style", PANGO_STYLE_ITALIC, NULL);

	g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(window_delete), NULL);

	return window;
}

void
clear_lyrics_window(void)
{
	GtkTextIter iter1, iter2;

	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(textbuffer), &iter1);
	gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(textbuffer), &iter2);

	gtk_text_buffer_delete(GTK_TEXT_BUFFER(textbuffer), &iter1, &iter2);
}

void
update_lyrics_window(const Tuple *tu, const gchar *lyrics)
{
	GtkTextIter iter;
	const gchar *artist, *title;
	const gchar *real_lyrics;
	gchar *f_name, *f_ext = NULL;

	if (textbuffer == NULL)
		return;

	clear_lyrics_window();

	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(textbuffer), &iter);

	title = tuple_get_string(tu, FIELD_TITLE, NULL);
	artist = tuple_get_string(tu, FIELD_ARTIST, NULL);

	if (title == NULL)
	{
		f_name = (gchar *) tuple_get_string(tu, FIELD_FILE_NAME, NULL);
		f_ext = (gchar *) tuple_get_string(tu, FIELD_FILE_EXT, NULL);

		title = g_strdup(f_name);

		f_name = g_strrstr(title, f_ext);
		if (f_name != NULL && f_name != title)
		{
			f_name--;
			f_name[0] = '\0';
		}
	}
	gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(textbuffer), &iter,
			title, strlen(title), "weight_bold", "size_x_large", NULL);
	if (f_ext != NULL)
		g_free((gpointer) title);

	gtk_text_buffer_insert(GTK_TEXT_BUFFER(textbuffer), &iter, "\n", 1);

	if (artist != NULL)
	{
		gtk_text_buffer_insert_with_tags_by_name(GTK_TEXT_BUFFER(textbuffer),
				&iter, artist, strlen(artist), "style_italic", NULL);

		gtk_text_buffer_insert(GTK_TEXT_BUFFER(textbuffer), &iter, "\n", 1);
	}

	real_lyrics = lyrics != NULL ? lyrics : _("\nNo lyrics were found.");

	gtk_text_buffer_insert(GTK_TEXT_BUFFER(textbuffer), &iter, real_lyrics, strlen(real_lyrics));
}

void
lyricwiki_playback_began(void)
{
	gint playlist, pos;
	const Tuple *tu;

	if (!aud_drct_get_playing())
		return;

	playlist = aud_playlist_get_active();
	pos = aud_playlist_get_position(playlist);
	tu = aud_playlist_entry_get_tuple (playlist, pos, FALSE);

	get_lyrics_step_1(tu);
}

static void
init(void)
{
	hook_associate("playback begin", (HookFunction) lyricwiki_playback_began, NULL);

	build_widget();

	lyricwiki_playback_began();
}

static void
cleanup(void)
{
	hook_dissociate("playback begin", (HookFunction) lyricwiki_playback_began);

	gtk_widget_destroy(window);
	window = NULL;
	textbuffer = NULL;
}

GeneralPlugin lyricwiki =
{
	.description = "LyricWiki",
	.init = init,
	.cleanup = cleanup,
};

GeneralPlugin *lyricwiki_gplist[] = { &lyricwiki, NULL };
SIMPLE_GENERAL_PLUGIN(lyricwiki, lyricwiki_gplist);

static gboolean window_delete(void)
{
	aud_general_plugin_enable (aud_plugin_by_header (& lyricwiki), FALSE);
	return TRUE;
}
