#include "ayemu.h"
#include "vtx.h"

#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

void vtx_file_info(const gchar *filename)
{
  static GtkWidget *box;
  ayemu_vtx_t vtx;

  if (!ayemu_vtx_open(&vtx, filename))
    {
      fprintf(stderr, "Can't open file %s\n", filename);
      return;
    }
  else
    {
      gchar head[1024];
      gchar body[8192];

      sprintf(head, "Details about %s", filename);

	  ayemu_vtx_sprintname(&vtx, body, sizeof(body),
	      "Title: %t\n"
	      "Author: %a\n"
	      "From : %f\n"
	      "Tracker : %T\n"
	      "Comment : %C\n"
	      "Chip type: %c\n"
	      "Stereo: %s\n"
	      "Loop: %l\n"
	      "Chip freq: %F\n"
	      "Player Freq:%P\n"
	      "Year: %y");

      audgui_simple_message (& box, GTK_MESSAGE_INFO, head, body);
    }
}
