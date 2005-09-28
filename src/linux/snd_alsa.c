/*
	snd_alsa.c

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id: snd_alsa.c,v 1.5 2005/01/02 03:29:11 bburns Exp $
*/

#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include "../client/client.h"
#include "../client/snd_loc.h"

int SNDDMA_GetDMAPos_ALSA (void);

// Define all dynamic ALSA functions...
#define ALSA_FUNC(ret, func, params) \
static ret (*alsa_##func) params;
#include "snd_alsa_funcs.h"
#undef ALSA_FUNC

// Catch the sizeof functions...
#define snd_pcm_hw_params_sizeof alsa_snd_pcm_hw_params_sizeof
#define snd_pcm_sw_params_sizeof alsa_snd_pcm_sw_params_sizeof

//static qboolean  snd_inited = false; //checked in snd_linux

static snd_pcm_t *playback_handle;
static snd_pcm_uframes_t buffer_size;

extern int paintedtime, soundtime;

extern cvar_t *sndbits;
extern cvar_t *sndspeed;
extern cvar_t *sndchannels;
extern cvar_t *snddevice;

qboolean SNDDMA_Init_ALSA (void)
{
	int err;
	//int buffersize;
	int rate, rrate = 0;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;
	snd_pcm_uframes_t   frag_size;
	static void         *alsa_handle;
 
	if(! (alsa_handle = dlopen("libasound.so.2", RTLD_GLOBAL | RTLD_NOW)) )
	{
		Com_Printf("Could open 'libasound.so.2'\n");
        return false;
	}

#define ALSA_FUNC(ret, func, params) \
    if (!(alsa_##func = dlsym (alsa_handle, #func))) \
    { \
        Com_Printf("Couldn't load ALSA function %s\n", #func); \
        dlclose (alsa_handle); \
        alsa_handle = NULL; \
        return false; \
    }
#include "snd_alsa_funcs.h"
#undef ALSA_FUNC

	err = alsa_snd_pcm_open(&playback_handle, snddevice->string, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if (err < 0) {
		Com_Printf("ALSA snd error, cannot open device %s (%s)\n", snddevice->string, alsa_snd_strerror(err));
		if(strcmp(snddevice->string, "default"))
			Com_Printf("Try to set snddevice to \"default\"\n");

		return false;
	}

    // Allocate memory for configuration of ALSA...
    snd_pcm_hw_params_alloca (&hw_params);
    snd_pcm_sw_params_alloca (&sw_params);

	err = alsa_snd_pcm_hw_params_any (playback_handle, hw_params);
	if (err < 0) {
		Com_Printf("ALSA snd error, cannot init hw params (%s)\n", alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
	}

	err = alsa_snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
	if (err < 0) {
		Com_Printf("ALSA snd error, cannot set access (%s)\n", alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
	}

	dma.samplebits = sndbits->integer;
	if (dma.samplebits != 8)
	{
		err = alsa_snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16);
		if (err < 0) {
			Com_Printf("ALSA snd error, 16 bit sound not supported, trying 8\n");
			dma.samplebits = 8;
		}
		else
			dma.samplebits = 16;
	}

	if (dma.samplebits == 8) {
		err = alsa_snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_U8);
		if (err < 0) {
			Com_Printf("ALSA snd error, cannot set sample format (%s)\n", alsa_snd_strerror(err));
			alsa_snd_pcm_close(playback_handle);
			return false;
		}
	}

	if(sndspeed->integer)
		rate = sndspeed->integer;
	else
		rate = 44100;

	rrate = rate;

	err = alsa_snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rrate, 0);
	if (err < 0) {
		Com_Printf("ALSA snd: rate %d Hz is not available for playback: (%s)\n", rate, alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
	}
	if (rrate != rate)
		Com_Printf("ALSA snd: rate %d Hz is not available, using %dHz\n", rate, rrate);

	dma.speed = rrate;
	frag_size = 8 * dma.samplebits * rrate / 11025;

	dma.channels = sndchannels->integer;
	if (dma.channels < 1 || dma.channels > 2)
		dma.channels = 2;

	err = alsa_snd_pcm_hw_params_set_channels(playback_handle,hw_params,dma.channels);
	if (err < 0) {
		Com_Printf("ALSA snd error couldn't set channels %d (%s).\n", dma.channels, alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
	}

    err = alsa_snd_pcm_hw_params_set_period_size_near(playback_handle,hw_params, &frag_size, 0);
    if(err < 0)
    {
        Com_Printf("ALSA: unable to set period size near %i. %s\n", (int)frag_size, alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
    }

	err = alsa_snd_pcm_hw_params(playback_handle, hw_params);
	if (err < 0) {
		Com_Printf("ALSA snd error couldn't set params (%s).\n",alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
	}

	dma.samplepos = 0;
///////////////////////////////////////////////
    err = alsa_snd_pcm_sw_params_current (playback_handle, sw_params);
    if(err < 0)
    {
        Com_Printf("ALSA: unable to determine current sw params. %s\n", alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
    }
    err = alsa_snd_pcm_sw_params_set_start_threshold (playback_handle, sw_params, ~0U);
    if(err < 0)
    {
        Com_Printf("ALSA: unable to set playback threshold. %s\n", alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
    }
    err = alsa_snd_pcm_sw_params_set_stop_threshold (playback_handle, sw_params, ~0U);
    if(err < 0)
    {
        Com_Printf("ALSA: unable to set playback stop threshold. %s\n", alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
    }
    err = alsa_snd_pcm_sw_params (playback_handle, sw_params);
    if(err < 0)
    {
        Com_Printf("ALSA: unable to install sw params. %s\n", alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
    }

    // don't mix less than this in mono samples:
    err = alsa_snd_pcm_hw_params_get_period_size (hw_params, (snd_pcm_uframes_t *)&dma.submission_chunk, 0);
    if(err < 0)
    {
        Com_Printf("ALSA: unable to get period size. %s\n", alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
    }

    err = alsa_snd_pcm_hw_params_get_buffer_size (hw_params, &buffer_size);
    if(err < 0)
    {
        Com_Printf("ALSA: unable to get buffer size. %s\n", alsa_snd_strerror(err));
		alsa_snd_pcm_close(playback_handle);
        return false;
    }
///////////////////////////////

	dma.samples = buffer_size * dma.channels;

	SNDDMA_GetDMAPos_ALSA();

    Com_Printf("%5d stereo\n", dma.channels - 1);
    Com_Printf("%5d samples\n", dma.samples);
    Com_Printf("%5d samplepos\n", dma.samplepos);
    Com_Printf("%5d samplebits\n", dma.samplebits);
    Com_Printf("%5d submission_chunk\n", dma.submission_chunk);
    Com_Printf("%5d speed\n", dma.speed);
//    Com_Printf("0x%lx dma buffer\n", (long)dma.buffer);

	return true;
}

int SNDDMA_GetDMAPos_ALSA (void)
{
    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t offset;
    snd_pcm_uframes_t nframes = dma.samples/dma.channels;

    alsa_snd_pcm_avail_update (playback_handle);
    alsa_snd_pcm_mmap_begin (playback_handle, &areas, &offset, &nframes);
    offset *= dma.channels;
    nframes *= dma.channels;
    dma.samplepos = offset;
    dma.buffer = areas->addr;
	return dma.samplepos;
}

void SNDDMA_Shutdown_ALSA (void)
{
	alsa_snd_pcm_close(playback_handle);
	dma.buffer = NULL;
}

/*
  SNDDMA_Submit
Send sound to device if buffer isn't really the dma buffer
*/
void SNDDMA_Submit_ALSA (void)
{
    int state;
    int count = paintedtime - soundtime;
    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t nframes;
    snd_pcm_uframes_t offset;


    nframes = count / dma.channels;

    alsa_snd_pcm_avail_update (playback_handle);
    alsa_snd_pcm_mmap_begin (playback_handle, &areas, &offset, &nframes);

    state = alsa_snd_pcm_state (playback_handle);

    switch (state) 
    {
        case SND_PCM_STATE_PREPARED:
            alsa_snd_pcm_mmap_commit (playback_handle, offset, nframes);
            alsa_snd_pcm_start (playback_handle);
            break;
        case SND_PCM_STATE_RUNNING:
            alsa_snd_pcm_mmap_commit (playback_handle, offset, nframes);
            break;
        default:
            break;
    }
}


void SNDDMA_BeginPainting_ALSA (void)
{    
}

