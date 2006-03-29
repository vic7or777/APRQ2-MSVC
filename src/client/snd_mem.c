/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// snd_mem.c: sound caching

#include "client.h"
#include "snd_loc.h"

/*
================
ResampleSfx
================
*/
static void ResampleSfx (sfxcache_t *sc, int inwidth, byte *data)
{
	int i, outcount, fracstep;
	int	sample, samplefrac, srcsample;
	float	stepscale;

	stepscale = (float)sc->speed / dma.speed; // this is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = dma.speed;
	if (s_loadas8bit->integer)
		sc->width = 1;

// resample / decimate to the current source rate
	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
		for (i=0 ; i<outcount ; i++)
			((signed char *)sc->data)[i]
			= (int)( (unsigned char)(data[i]) - 128);
	}
	else
	{
// general case
		samplefrac = 0;
		fracstep = stepscale*256;
		for (i=0 ; i<outcount ; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort ( ((int16 *)data)[srcsample] );
			else
				sample = (int)( (unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((int16 *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}


//=============================================================================

/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound (sfx_t *s)
{
    char	namebuffer[MAX_QPATH];
	byte	*data;
	wavinfo_t	info;
	int		len;
	sfxcache_t	*sc;
	int		size;
	char	*name;

	if (s->name[0] == '*')
		return NULL;

// see if still in memory
	sc = s->cache;
	if (sc)
		return sc;

// load it in
	if (s->truename)
		name = s->truename;
	else
		name = s->name;

	if (name[0] == '#')
		Q_strncpyz (namebuffer, &name[1], sizeof(namebuffer));
	else
		Com_sprintf (namebuffer, sizeof(namebuffer), "sound/%s", name);

	size = FS_LoadFile (namebuffer, (void **)&data);

	if (!data)
	{
		Com_DPrintf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	info = GetWavinfo (s->name, data, size);
#ifdef USE_OPENAL
	if(alSound) {
			len = 0;
	} else {
#endif
		if (info.channels != 1)
		{
			Com_Printf ("%s is a stereo sample\n", s->name);
			FS_FreeFile (data);
			return NULL;
		}

		// calculate resampled length
		len = (int)((double)info.samples * ((double)dma.speed / (double)info.rate));
		len = len * info.width * info.channels;
#ifdef USE_OPENAL
	}
#endif

	sc = s->cache = Z_TagMalloc (len + sizeof(sfxcache_t), TAGMALLOC_CLIENT_SOUNDCACHE);
	if (!sc)
	{
		FS_FreeFile (data);
		return NULL;
	}

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->channels = info.channels;

#ifdef USE_OPENAL
	sc->alBufferNum = 0;
	sc->alFormat = 0;
	if(alSound)
		ALSnd_CreateBuffer (sc, info.width, info.channels, data + info.dataofs, info.samples * info.width * info.channels, info.rate);
	else
#endif
		ResampleSfx (sc, sc->width, data + info.dataofs);

	FS_FreeFile (data);

	return sc;
}



/*
===============================================================================

WAV loading

===============================================================================
*/

static byte		*data_p;
static byte 	*iff_end;
static byte 	*last_chunk;
static byte 	*iff_data;
static int 		iff_chunk_len;

static int16 GetLittleShort(void)
{
	int16 val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	data_p += 2;
	return val;
}

static int32 GetLittleLong(void)
{
	int32 val = 0;
	val = *data_p;
	val += (*(data_p+1)<<8);
	val += (*(data_p+2)<<16);
	val += (*(data_p+3)<<24);
	data_p += 4;
	return val;
}

static void FindNextChunk(const char *name)
{
	while (1)
	{
		data_p = last_chunk;

		data_p += 4;

		if (data_p >= iff_end)
		{	// didn't find the chunk
			data_p = NULL;
			return;
		}

		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
		{
			data_p = NULL;
			return;
		}

		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if (!strncmp((char *)data_p, name, 4))
			return;
	}
}

static void FindChunk(const char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}

/*
============
GetWavinfo
============
*/
wavinfo_t GetWavinfo (const char *name, byte *wav, int wavlength)
{
	wavinfo_t	info = {0};
	int     i, format, samples;

	if (!wav)
		return info;
		
	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !strncmp((char *)data_p+8, "WAVE", 4)))
	{
		Com_Printf("GetWavinfo: Missing RIFF/WAVE chunks (%s)\n", name);
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk("fmt ");
	if (!data_p)
	{
		Com_Printf("GetWavinfo: Missing fmt chunk (%s)\n", name);
		return info;
	}

	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		Com_Printf("GetWavinfo: Microsoft PCM format only (%s)\n", name);
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4+2;
	info.width = GetLittleShort() / 8;

// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p)
		{
			if (!strncmp ((char *)data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				info.samples = info.loopstart + i;
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Com_Printf("GetWavinfo: Missing data chunk (%s)\n", name);
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width / info.channels;

	if (info.samples)
	{
		if (samples < info.samples)
			Com_Error (ERR_DROP, "Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;
	
	return info;
}
