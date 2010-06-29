/*      xmms - jack output plugin
 *    Copyright 2002 Chris Morgan<cmorgan@alum.wpi.edu>
 *
 *      audacious port (2005-2006) by Giacomo Lozito from develia.org
 *
 *    This code maps xmms calls into the jack translation library
 */

#include <audacious/plugin.h>
#include <dlfcn.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include <audacious/i18n.h>
#include <stdio.h>
#include "config.h"
#include "bio2jack.h" /* includes for the bio2jack library */
#include "jack.h"
#include <string.h>



/* set to 1 for verbose output */
#define VERBOSE_OUTPUT          0

jackconfig jack_cfg;

#define OUTFILE stderr

#define TRACE(...)                                      \
    if(jack_cfg.isTraceEnabled) {                       \
        fprintf(OUTFILE, "%s:", __FUNCTION__),          \
        fprintf(OUTFILE, __VA_ARGS__),                    \
        fflush(OUTFILE);                                \
    }

#define ERR(...)                                        \
    if(jack_cfg.isTraceEnabled) {                       \
        fprintf(OUTFILE, "ERR: %s:", __FUNCTION__),     \
        fprintf(OUTFILE, __VA_ARGS__),                    \
        fflush(OUTFILE);                                \
    }


static int driver = 0; /* handle to the jack output device */

typedef struct format_info {
  AFormat format;
  long    frequency;
  int     channels;
  long    bps;
} format_info_t;

static format_info_t input;
static format_info_t effect;
static format_info_t output;

static gboolean output_opened; /* true if we have a connection to jack */


/* Giacomo's note: removed the destructor from the original xmms-jack, cause
   destructors + thread join + NPTL currently leads to problems; solved this
   by adding a cleanup function callback for output plugins in Audacious, this
   is used to close the JACK connection and to perform a correct shutdown */
static void jack_cleanup(void)
{
  int errval;
  TRACE("cleanup\n");

  if((errval = JACK_Close(driver)))
    ERR("error closing device, errval of %d\n", errval);

  return;
}


/* Set the volume */
static void jack_set_volume(int l, int r)
{
  if(output.channels == 1)
  {
    TRACE("l(%d)\n", l);
  } else if(output.channels > 1)
  {
    TRACE("l(%d), r(%d)\n", l, r);
  }

  if(output.channels > 0) {
      JACK_SetVolumeForChannel(driver, 0, l);
      jack_cfg.volume_left = l;
  }
  if(output.channels > 1) {
      JACK_SetVolumeForChannel(driver, 1, r);
      jack_cfg.volume_right = r;
  }
}


/* Get the current volume setting */
static void jack_get_volume(int *l, int *r)
{
  unsigned int _l, _r;

  if(output.channels > 0)
  {
      JACK_GetVolumeForChannel(driver, 0, &_l);
      (*l) = _l;
  }
  if(output.channels > 1)
  {
      JACK_GetVolumeForChannel(driver, 1, &_r);
      (*r) = _r;
  }

#if VERBOSE_OUTPUT
  if(output.channels == 1)
      TRACE("l(%d)\n", *l);
  else if(output.channels > 1)
      TRACE("l(%d), r(%d)\n", *l, *r);
#endif
}


/* Return the number of milliseconds of audio data that has been */
/* written out to the device */
static gint jack_get_written_time(void)
{
  long return_val;
  return_val = JACK_GetPosition(driver, MILLISECONDS, WRITTEN);

  TRACE("returning %ld milliseconds\n", return_val);
  return return_val;
}


/* Return the current number of milliseconds of audio data that has */
/* been played out of the audio device, not including the buffer */
static gint jack_get_output_time(void)
{
  gint return_val;

  /* don't try to get any values if the device is still closed */
  if(JACK_GetState(driver) == CLOSED)
    return_val = 0;
  else
    return_val = JACK_GetPosition(driver, MILLISECONDS, PLAYED); /* get played position in terms of milliseconds */

  TRACE("returning %d milliseconds\n", return_val);
  return return_val;
}


/* returns TRUE if we are currently playing */
/* NOTE: this was confusing at first BUT, if the device is open and there */
/* is no more audio to be played, then the device is NOT PLAYING */
static gint jack_playing(void)
{
  gint return_val;

  /* If we are playing see if we ACTUALLY have something to play */
  if(JACK_GetState(driver) == PLAYING)
  {
    /* If we have zero bytes stored, we are done playing */
    if(JACK_GetBytesStored(driver) == 0)
      return_val = FALSE;
    else
      return_val = TRUE;
  }
  else
    return_val = FALSE;

  TRACE("returning %d\n", return_val);
  return return_val;
}


void jack_set_port_connection_mode()
{
  /* setup the port connection mode that determines how bio2jack will connect ports */
  enum JACK_PORT_CONNECTION_MODE mode;

  if(strcmp(jack_cfg.port_connection_mode, "CONNECT_ALL") == 0)
      mode = CONNECT_ALL;
  else if(strcmp(jack_cfg.port_connection_mode, "CONNECT_OUTPUT") == 0)
      mode = CONNECT_OUTPUT;
  else if(strcmp(jack_cfg.port_connection_mode, "CONNECT_NONE") == 0)
      mode = CONNECT_NONE;
  else
  {
      TRACE("Defaulting to CONNECT_ALL");
      mode = CONNECT_ALL;
  }
  JACK_SetPortConnectionMode(mode);
}

/* Initialize necessary things */
static OutputPluginInitStatus jack_init(void)
{
  /* read the isTraceEnabled setting from the config file */
  mcs_handle_t *cfgfile;

  cfgfile = aud_cfg_db_open();
  if (!cfgfile)
  {
      jack_cfg.isTraceEnabled = FALSE;
      jack_cfg.port_connection_mode = "CONNECT_ALL"; /* default to connect all */
      jack_cfg.volume_left = 25; /* set default volume to 25 % */
      jack_cfg.volume_right = 25;
  } else
  {
      aud_cfg_db_get_bool(cfgfile, "jack", "isTraceEnabled", &jack_cfg.isTraceEnabled);
      if(!aud_cfg_db_get_string(cfgfile, "jack", "port_connection_mode", &jack_cfg.port_connection_mode))
          jack_cfg.port_connection_mode = "CONNECT_ALL";
      if(!aud_cfg_db_get_int(cfgfile, "jack", "volume_left", &jack_cfg.volume_left))
          jack_cfg.volume_left = 25;
      if(!aud_cfg_db_get_int(cfgfile, "jack", "volume_right", &jack_cfg.volume_right))
          jack_cfg.volume_right = 25;
  }

  aud_cfg_db_close(cfgfile);


  TRACE("initializing\n");
  JACK_Init(); /* initialize the driver */

  /* set the bio2jack name so users will see xmms-jack in their */
  /* jack client list */
  JACK_SetClientName("audacious-jack");

  /* set the port connection mode */
  jack_set_port_connection_mode();

  output_opened = FALSE;
  
  /* Always return OK, as we don't know about physical devices here */
  return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
}


/* Return the amount of data that can be written to the device */
static gint audacious_jack_free(void)
{
  unsigned long return_val = JACK_GetBytesFreeSpace(driver);
  unsigned long tmp;

  /* adjust for frequency differences, otherwise xmms could send us */
  /* as much data as we have free, then we go to convert this to */
  /* the output frequency and won't have enough space, so adjust */
  /* by the ratio of the two */
  if(effect.frequency != output.frequency)
  {
    tmp = return_val;
    return_val = (return_val * effect.frequency) / output.frequency;
    TRACE("adjusting from %ld to %ld free bytes to compensate for frequency differences\n", tmp, return_val);
  }

  if(return_val > G_MAXINT)
  {
      TRACE("Warning: return_val > G_MAXINT\n");
      return_val = G_MAXINT;
  }

  TRACE("free space of %ld bytes\n", return_val);

  return return_val;
}


/* Close the device */
static void jack_close(void)
{
  mcs_handle_t *cfgfile;

  cfgfile = aud_cfg_db_open();
  aud_cfg_db_set_int(cfgfile, "jack", "volume_left", jack_cfg.volume_left); /* stores the volume setting */
  aud_cfg_db_set_int(cfgfile, "jack", "volume_right", jack_cfg.volume_right);
  aud_cfg_db_close(cfgfile);

  TRACE("\n");

  JACK_Reset(driver); /* flush buffers, reset position and set state to STOPPED */
  TRACE("resetting driver, not closing now, destructor will close for us\n");
}


/* Open the device up */
static gint jack_open(AFormat fmt, gint sample_rate, gint num_channels)
{
  int bits_per_sample;
  int floating_point = FALSE;
  int retval;
  unsigned long rate;

  TRACE("fmt == %d, sample_rate == %d, num_channels == %d\n",
    fmt, sample_rate, num_channels);

  if((fmt == FMT_U8) || (fmt == FMT_S8))
  {
    bits_per_sample = 8;
  } else if(fmt == FMT_S16_NE)
  {
    bits_per_sample = 16;
  } else if (fmt == FMT_S24_NE)
  {
    /* interpreted by bio2jack as 24 bit values packed to 32 bit samples */
    bits_per_sample = 24;
  } else if (fmt == FMT_S32_NE)
  {
    bits_per_sample = 32;
  } else if (fmt == FMT_FLOAT)
  {
    bits_per_sample = 32;
    floating_point = TRUE;
  } else {
    TRACE("sample format not supported\n");
    return 0;
  }

  /* record some useful information */
  input.format    = fmt;
  input.frequency = sample_rate;
  input.bps       = bits_per_sample * sample_rate * num_channels;
  input.channels  = num_channels;

  /* setup the effect as matching the input format */
  effect.format    = input.format;
  effect.frequency = input.frequency;
  effect.channels  = input.channels;
  effect.bps       = input.bps;

  /* if we are already opened then don't open again */
  if(output_opened)
  {
    /* if something has changed we should close and re-open the connect to jack */
    if((output.channels != input.channels) ||
       (output.frequency != input.frequency) ||
       (output.format != input.format))
    {
      TRACE("output.channels is %d, jack_open called with %d channels\n", output.channels, input.channels);
      TRACE("output.frequency is %ld, jack_open called with %ld\n", output.frequency, input.frequency);
      TRACE("output.format is %d, jack_open called with %d\n", output.format, input.format);
      jack_close();
      JACK_Close(driver);
    } else
    {
        TRACE("output_opened is TRUE and no options changed, not reopening\n");
        return 1;
    }
  }

  /* try to open the jack device with the requested rate at first */
  output.frequency = input.frequency;
  output.bps       = input.bps;
  output.channels  = input.channels;
  output.format    = input.format;

  rate = output.frequency;
  retval = JACK_Open(&driver, bits_per_sample, floating_point, &rate, output.channels);
  output.frequency = rate; /* avoid compile warning as output.frequency differs in type
                              from what JACK_Open() wants for the type of the rate parameter */
  if((retval == ERR_RATE_MISMATCH))
  {
    TRACE("set the resampling rate properly");
    return 0;
  } else if(retval != ERR_SUCCESS)
  {
    TRACE("failed to open jack with JACK_Open(), error %d\n", retval);
    return 0;
  }

  jack_set_volume(jack_cfg.volume_left, jack_cfg.volume_right); /* sets the volume to stored value */
  output_opened = TRUE;

  return 1;
}


/* write some audio out to the device */
static void jack_write(gpointer ptr, gint length)
{
  long written;

  TRACE("starting length of %d\n", length);

  /* loop until we have written all the data out to the jack device */
  /* this is due to xmms' audio driver api */
  char *buf = (char*)ptr;
  while(length > 0)
  {
    TRACE("writing %d bytes\n", length);
    written = JACK_Write(driver, (unsigned char*)buf, length);
    length-=written;
    buf+=written;
  }
  TRACE("finished\n");
}


/* Flush any output currently buffered */
/* and set the number of bytes written based on ms_offset_time, */
/* the number of milliseconds of offset passed in */
/* This is done so the driver itself keeps track of */
/* current playing position of the mp3 */
static void jack_flush(gint ms_offset_time)
{
  TRACE("setting values for ms_offset_time of %d\n", ms_offset_time);

  JACK_Reset(driver); /* flush buffers and set state to STOPPED */

  /* update the internal driver values to correspond to the input time given */
  JACK_SetPosition(driver, MILLISECONDS, ms_offset_time);

  JACK_SetState(driver, PLAYING);
}


/* Pause the jack device */
static void jack_pause(short p)
{
  TRACE("p == %d\n", p);

  /* pause the device if p is non-zero, unpause the device if p is zero and */
  /* we are currently paused */
  if(p)
    JACK_SetState(driver, PAUSED);
  else if(JACK_GetState(driver) == PAUSED)
    JACK_SetState(driver, PLAYING);
}


static void jack_about(void)
{
    static GtkWidget *aboutbox = NULL;

    if (aboutbox == NULL)
    {
        gchar *description = g_strdup_printf(
            _("XMMS jack Driver 0.17\n\n"
              "xmms-jack.sf.net\nChris Morgan<cmorgan@alum.wpi.edu>\n\n"
              "Audacious port by\nGiacomo Lozito from develia.org"));

        audgui_simple_message (& aboutbox, GTK_MESSAGE_INFO,
         _("About JACK Output Plugin 0.17"), description);
        
        g_free(description);
    }
}

static void jack_tell_audio(AFormat * fmt, gint * srate, gint * nch)
{
    (*fmt) = input.format;
    (*srate) = input.frequency;
    (*nch) = input.channels;
}

OutputPlugin jack_op =
{
    .description = "JACK Output Plugin 0.17",
    .init = jack_init,
    .cleanup = jack_cleanup,
    .about = jack_about,
    .configure = jack_configure,
    .get_volume = jack_get_volume,
    .set_volume = jack_set_volume,
    .open_audio = jack_open,
    .write_audio = jack_write,
    .close_audio = jack_close,
    .flush = jack_flush,
    .pause = jack_pause,
    .buffer_free = audacious_jack_free,
    .buffer_playing = jack_playing,
    .output_time = jack_get_output_time,
    .written_time = jack_get_written_time,
    .tell_audio = jack_tell_audio
};

OutputPlugin *jack_oplist[] = { &jack_op, NULL };

SIMPLE_OUTPUT_PLUGIN(jack, jack_oplist);
