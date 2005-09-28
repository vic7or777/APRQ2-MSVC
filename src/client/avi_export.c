
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
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.
#include "client.h"
#ifdef AVI_EXPORT
#define WIN32_LEAN_AND_MEAN
#define VC_LEANMEAN
#include <windows.h>
#include <vfw.h>
#include "snd_loc.h"

static void (CALLBACK *qAVIFileInit)(void);
static HRESULT (CALLBACK *qAVIFileOpen)(PAVIFILE *, LPCTSTR, UINT, LPCLSID);
static HRESULT (CALLBACK *qAVIFileCreateStream)(PAVIFILE, PAVISTREAM *, AVISTREAMINFO *);
static HRESULT (CALLBACK *qAVIMakeCompressedStream)(PAVISTREAM *, PAVISTREAM, AVICOMPRESSOPTIONS *, CLSID *);
static HRESULT (CALLBACK *qAVIStreamSetFormat)(PAVISTREAM, LONG, LPVOID, LONG);
static HRESULT (CALLBACK *qAVIStreamWrite)(PAVISTREAM, LONG, LONG, LPVOID, LONG, DWORD, LONG *, LONG *);
static BOOL	 (CALLBACK *qAVISaveOptions)(HWND, UINT, int, PAVISTREAM *, LPAVICOMPRESSOPTIONS *);
static LONG	 (CALLBACK *qAVISaveOptionsFree)(int, LPAVICOMPRESSOPTIONS *);
static ULONG (CALLBACK *qAVIStreamRelease)(PAVISTREAM);
static ULONG (CALLBACK *qAVIFileRelease)(PAVIFILE);
static void (CALLBACK *qAVIFileExit)(void);

static MMRESULT (ACMAPI *qacmDriverOpen)(LPHACMDRIVER, HACMDRIVERID, DWORD);
static MMRESULT (ACMAPI *qacmDriverDetails)(HACMDRIVERID, LPACMDRIVERDETAILS, DWORD);
static MMRESULT (ACMAPI *qacmDriverEnum)(ACMDRIVERENUMCB, DWORD, DWORD);
static MMRESULT (ACMAPI *qacmFormatTagDetails)(HACMDRIVER, LPACMFORMATTAGDETAILS, DWORD);
static MMRESULT (ACMAPI *qacmStreamOpen)(LPHACMSTREAM, HACMDRIVER, LPWAVEFORMATEX, LPWAVEFORMATEX, LPWAVEFILTER, DWORD, DWORD, DWORD);
static MMRESULT (ACMAPI *qacmStreamSize)(HACMSTREAM, DWORD, LPDWORD, DWORD);
static MMRESULT (ACMAPI *qacmStreamPrepareHeader)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
static MMRESULT (ACMAPI *qacmStreamUnprepareHeader)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
static MMRESULT (ACMAPI *qacmStreamConvert)(HACMSTREAM, LPACMSTREAMHEADER, DWORD);
static MMRESULT (ACMAPI *qacmStreamClose)(HACMSTREAM, DWORD);
static MMRESULT (ACMAPI *qacmDriverClose)(HACMDRIVER, DWORD);

static HINSTANCE handle_avi = NULL, handle_acm = NULL;

typedef struct
{
	float			m_fps;
	int				m_video_frame_size;
	int				m_video_frame_counter;
	int				m_audio_frame_counter;
	unsigned long	m_codec_fourcc;

	PAVIFILE		m_file;
	PAVISTREAM		m_uncompressed_video_stream;
	PAVISTREAM		m_compressed_video_stream;
	PAVISTREAM		m_audio_stream;
	WAVEFORMATEX	m_wave_format;
} avi_Data_t;

static avi_Data_t avi;

static qboolean movie_acm_loaded = false, movie_avi_loaded = false;
static HACMDRIVER		had;
static HACMSTREAM		hstr;
static ACMSTREAMHEADER	strhdr;
static MPEGLAYER3WAVEFORMAT mp3_format;
static qboolean		mp3_driver = false;
static qboolean		m_audio_is_mp3 = false;
static int			avi_recording = 0; // 0 no, 1 yes, 2 with sounds

static cvar_t	*avi_codec;
static cvar_t	*avi_codecmenu;
static cvar_t	*avi_sound;
static cvar_t	*avi_mp3_kbps;

static byte *aviFrameBuf = NULL;

void GLAVI_ReadFrameData (byte *buffer);
extern viddef_t	viddef;

extern short *snd_out;
extern int snd_linear_count, soundtime;

// Variables for buffering audio
static short capture_audio_samples[44100];	// big enough buffer for 1fps at 44100Hz
static int captured_audio_samples = 0;

extern cvar_t *fixedtime;

char *FourccToString (unsigned long fourcc)
{
	static char s[8];
    
	if (!fourcc)
		return "";

    Com_sprintf(s, sizeof(s), "%c%c%c%c", (char)(fourcc), (char)(fourcc >> 8),
        (char)(fourcc >> 16), (char)(fourcc >> 24));
	return s;
}

static void Capture_InitAVI (void)
{
	handle_avi = LoadLibrary("avifil32.dll");
	if (!handle_avi)
	{
		Com_Printf ("Avi capturing module 'avifil32.dll' not found\n");
		Com_Printf ("Aviexporting disabled\n");
		handle_avi = NULL;
		return;
	}

#define AVI_GETFUNC(func) \
	if ( !(qAVI##func = (void *)GetProcAddress(handle_avi, "AVI" #func)) ) \
	{ \
        Com_Printf("Couldn't load AVI function %s\n", #func); \
		Com_Printf ("Avi capturing module not initialized\n"); \
		FreeLibrary (handle_avi); \
		handle_avi = NULL; \
        return; \
    }

	AVI_GETFUNC(FileInit);
	AVI_GETFUNC(FileOpen);
	AVI_GETFUNC(FileCreateStream);
	AVI_GETFUNC(MakeCompressedStream);
	AVI_GETFUNC(StreamSetFormat);
	AVI_GETFUNC(StreamWrite);
	AVI_GETFUNC(StreamRelease);
	AVI_GETFUNC(FileRelease);
	AVI_GETFUNC(FileExit);
	AVI_GETFUNC(SaveOptions);
	AVI_GETFUNC(SaveOptionsFree);

	Com_Printf ("Avi capturing module initialized\n");
	movie_avi_loaded = true;
}

static void Capture_InitACM (void)
{
	if(!movie_avi_loaded) //No need to load if aviexport is disabled
		return;

	handle_acm = LoadLibrary("msacm32.dll");
	if (!handle_acm)
	{
		Com_Printf ("ACM module 'msacm32.dll' not found\n");
		Com_Printf ("AVI mp3 compression disabled\n");
		handle_acm = NULL;
		return;
	}

#define ACM_GETFUNC(func) \
	if ( !(qacm##func = (void *)GetProcAddress(handle_acm, "acm" #func)) ) \
	{ \
        Com_Printf("Couldn't load acm function %s\n", #func); \
		Com_Printf ("ACM module not initialized\n"); \
		FreeLibrary (handle_acm); \
		handle_acm = NULL; \
        return; \
    }

	ACM_GETFUNC(DriverOpen);
	ACM_GETFUNC(DriverEnum);
	ACM_GETFUNC(StreamOpen);
	ACM_GETFUNC(StreamSize);
	ACM_GETFUNC(StreamPrepareHeader);
	ACM_GETFUNC(StreamUnprepareHeader);
	ACM_GETFUNC(StreamConvert);
	ACM_GETFUNC(StreamClose);
	ACM_GETFUNC(DriverClose);
	qacmDriverDetails = (void *)GetProcAddress (handle_acm, "acmDriverDetailsA");
	qacmFormatTagDetails = (void *)GetProcAddress (handle_acm, "acmFormatTagDetailsA");

	if (!qacmDriverDetails || !qacmFormatTagDetails)
	{
		Com_Printf ("ACM module not initialized\n");
		FreeLibrary (handle_acm);
		handle_acm = NULL;
		return;
	}

	Com_Printf ("ACM module initialized\n");
	movie_acm_loaded = true;
}

BOOL CALLBACK acmDriverEnumCallback (HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport)
{
	if (fdwSupport & ACMDRIVERDETAILS_SUPPORTF_CODEC)
	{
		int	i;
		ACMDRIVERDETAILS details;

		details.cbStruct = sizeof(details);
		qacmDriverDetails (hadid, &details, 0);
		qacmDriverOpen (&had, hadid, 0);
	
		for (i = 0 ; i < details.cFormatTags ; i++)
		{
			ACMFORMATTAGDETAILS	fmtDetails;

			memset (&fmtDetails, 0, sizeof(fmtDetails));
			fmtDetails.cbStruct = sizeof(fmtDetails);
			fmtDetails.dwFormatTagIndex = i;
			qacmFormatTagDetails (had, &fmtDetails, ACM_FORMATTAGDETAILSF_INDEX);
			if (fmtDetails.dwFormatTag == WAVE_FORMAT_MPEGLAYER3)
			{
				Com_DPrintf ("MP3-capable ACM codec found: %s\n", details.szLongName);
				mp3_driver = true;

				return false;
			}
		}
		qacmDriverClose (had, 0);
	}

	return true;
}

PAVISTREAM Capture_VideoStream (void)
{
	return avi.m_codec_fourcc ? avi.m_compressed_video_stream : avi.m_uncompressed_video_stream;
}

static  qboolean AVI_InitExporter (const char *filename, float fps)
{
	BITMAPINFOHEADER	bitmap_info_header;
	AVISTREAMINFO		stream_header;
	HRESULT				hr;

	if(!filename)
		return false;

	avi.m_fps = avi.m_video_frame_counter = avi.m_audio_frame_counter = 0;
	avi.m_file = NULL;
	avi.m_codec_fourcc = 0;
	avi.m_compressed_video_stream = avi.m_uncompressed_video_stream = avi.m_audio_stream = NULL;

	qAVIFileInit();
	hr = qAVIFileOpen(&avi.m_file, filename, OF_WRITE|OF_CREATE, NULL);
	if(FAILED(hr))
	{	
		Com_Printf ("ERROR: Couldn't open AVI file\n");
		return false;
	}

	avi.m_fps = fps;
	avi.m_video_frame_size = viddef.width * viddef.height * 3;

	memset(&bitmap_info_header, 0, sizeof(bitmap_info_header));
	bitmap_info_header.biSize			= sizeof(BITMAPINFOHEADER);
	bitmap_info_header.biWidth			= viddef.width;
	bitmap_info_header.biHeight			= viddef.height;
	bitmap_info_header.biPlanes			= 1;
	bitmap_info_header.biBitCount		= 24;
	bitmap_info_header.biCompression	= BI_RGB;
	bitmap_info_header.biSizeImage		= avi.m_video_frame_size; 


	memset(&stream_header, 0, sizeof(stream_header));
	stream_header.fccType                = streamtypeVIDEO;
	stream_header.fccHandler             = 0;
	stream_header.dwScale                = 100;
	stream_header.dwRate                 = (unsigned long)(0.5f + avi.m_fps * 100.0f);
	stream_header.dwSuggestedBufferSize  = bitmap_info_header.biSizeImage;
	SetRect(&stream_header.rcFrame, 0, 0, (int)bitmap_info_header.biWidth, (int)bitmap_info_header.biHeight);

	hr = qAVIFileCreateStream(avi.m_file, &avi.m_uncompressed_video_stream, &stream_header);
	if(FAILED(hr))
	{
		Com_Printf ("ERROR: Couldn't create video stream\n");
		return false;
	}

	if(avi_codecmenu->integer) //Windows menu to select codec
	{
		AVICOMPRESSOPTIONS	opts;
		AVICOMPRESSOPTIONS FAR * aoptions;

		aoptions = &opts;
		memset (&opts, 0, sizeof(opts));
		if (!qAVISaveOptions(NULL, 0, 1, &avi.m_uncompressed_video_stream, (LPAVICOMPRESSOPTIONS FAR *) &aoptions))
		{
			Com_Printf ("ERROR: Couldn't get options\n");
			qAVISaveOptionsFree(1,(LPAVICOMPRESSOPTIONS FAR *) &aoptions);
			return false;
		}

		if(opts.fccHandler) //Compression selected
		{
			hr = qAVIMakeCompressedStream(&avi.m_compressed_video_stream, avi.m_uncompressed_video_stream, &opts, NULL);
			if(FAILED(hr))
			{
				Com_Printf ("ERROR: Couldn't make compressed video stream using %s codec\n", FourccToString(opts.fccHandler));
				qAVISaveOptionsFree(1,(LPAVICOMPRESSOPTIONS FAR *) &aoptions);
				return false;
			}
			avi.m_codec_fourcc = opts.fccHandler;
		}

		hr = qAVISaveOptionsFree(1,(LPAVICOMPRESSOPTIONS FAR *) &aoptions);
		if(FAILED(hr))
		{
			Com_Printf ("ERROR: Couldn't free options\n");
			return false;
		}
	}
	else if (strlen(avi_codec->string) >= 4) // codec fourcc supplied
	{
		AVICOMPRESSOPTIONS	opts;

		memset (&opts, 0, sizeof(opts));
		opts.fccType = stream_header.fccType;
		opts.fccHandler = mmioFOURCC (avi_codec->string[0], avi_codec->string[1], avi_codec->string[2], avi_codec->string[3]);

		// Make the stream according to compression
		hr = qAVIMakeCompressedStream (&avi.m_compressed_video_stream, avi.m_uncompressed_video_stream, &opts, NULL);
		if (FAILED(hr))
		{
			Com_Printf ("ERROR: Couldn't make compressed video stream using %s codec\n", FourccToString(opts.fccHandler));
			return false;
		}
		avi.m_codec_fourcc = opts.fccHandler;
	}

	hr = qAVIStreamSetFormat(Capture_VideoStream(), 0, &bitmap_info_header, bitmap_info_header.biSize + bitmap_info_header.biClrUsed * sizeof(RGBQUAD));
	if(FAILED(hr))
	{
		Com_Printf ("ERROR: Couldn't set video stream format\n");
		return false;
	}

	if(!avi_sound->integer) //No sounds
		return true;

	// initialize audio data
	memset (&avi.m_wave_format, 0, sizeof(avi.m_wave_format));
	avi.m_wave_format.wFormatTag		= WAVE_FORMAT_PCM;
	avi.m_wave_format.nChannels			= 2; // always stereo in Quake sound engine
	avi.m_wave_format.nSamplesPerSec	= dma.speed;
	avi.m_wave_format.wBitsPerSample	= 16;	// always 16bit in Quake sound engine
	avi.m_wave_format.nBlockAlign		= avi.m_wave_format.wBitsPerSample/8 * avi.m_wave_format.nChannels;
	avi.m_wave_format.nAvgBytesPerSec	= avi.m_wave_format.nSamplesPerSec * avi.m_wave_format.nBlockAlign;

	memset (&stream_header, 0, sizeof(stream_header));
	stream_header.fccType		= streamtypeAUDIO;
	stream_header.dwScale		= avi.m_wave_format.nBlockAlign;
	stream_header.dwRate		= stream_header.dwScale * (unsigned long)avi.m_wave_format.nSamplesPerSec;
	stream_header.dwSampleSize	= avi.m_wave_format.nBlockAlign;

	hr = qAVIFileCreateStream (avi.m_file, &avi.m_audio_stream, &stream_header);
	if (FAILED(hr))
	{
		Com_Printf ("ERROR: Couldn't create audio stream\n");
		return false;
	}

	m_audio_is_mp3 = false;
	if (avi_sound->integer == 2 && !movie_acm_loaded)
	{
		Com_Printf("MP3 decoder disabled, setting uncompressed format\n");
	}
	else if (avi_sound->integer == 2 && movie_acm_loaded)
	{
		MMRESULT	mmr;

		// try to find an MP3 codec
		had = NULL;
		mp3_driver = false;
		qacmDriverEnum (acmDriverEnumCallback, 0, 0);
		if (!mp3_driver)
		{
			Com_Printf ("ERROR: Couldn't find any MP3 decoder\n");
			return false;
		}

		memset (&mp3_format, 0, sizeof(mp3_format));
		mp3_format.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
		mp3_format.wfx.nChannels = 2;
		mp3_format.wfx.nSamplesPerSec = dma.speed;
		mp3_format.wfx.wBitsPerSample = 0;
		mp3_format.wfx.nBlockAlign = 1;
		mp3_format.wfx.nAvgBytesPerSec = avi_mp3_kbps->integer * 125;
		mp3_format.wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
		mp3_format.wID = MPEGLAYER3_ID_MPEG;
		mp3_format.fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF;
		mp3_format.nBlockSize = mp3_format.wfx.nAvgBytesPerSec / avi.m_fps;
		mp3_format.nFramesPerBlock = 1;
		mp3_format.nCodecDelay = 1393;

		hstr = NULL;
		if ((mmr = qacmStreamOpen(&hstr, had, &avi.m_wave_format, &mp3_format.wfx, NULL, 0, 0, 0)))
		{
			switch (mmr)
			{
			case MMSYSERR_INVALPARAM:
				Com_Printf ("ERROR: Invalid parameters passed to acmStreamOpen\n");
				return false;

			case ACMERR_NOTPOSSIBLE:
				Com_Printf ("ERROR: No ACM filter found capable of decoding MP3\n");
				return false;

			default:
				Com_Printf ("ERROR: Couldn't open ACM decoding stream\n");
				return false;
			}
		}

		hr = qAVIStreamSetFormat (avi.m_audio_stream, 0, &mp3_format, sizeof(MPEGLAYER3WAVEFORMAT));
		if (FAILED(hr))
		{
			Com_Printf ("ERROR: Couldn't set mp3 audio stream format\n");
			return false;
		}

		m_audio_is_mp3 = true;
	}
	else
	{
		hr = qAVIStreamSetFormat (avi.m_audio_stream, 0, &avi.m_wave_format, sizeof(WAVEFORMATEX));
		if (FAILED(hr))
		{
			Com_Printf ("ERROR: Couldn't set audio stream format\n");
			return false;
		}
	}

	return true;
}

static void AVI_ReleaseExporter(void)
{
	if (avi.m_uncompressed_video_stream)
		qAVIStreamRelease (avi.m_uncompressed_video_stream);
	if (avi.m_compressed_video_stream)
		qAVIStreamRelease (avi.m_compressed_video_stream);
	if (avi.m_audio_stream)
		qAVIStreamRelease (avi.m_audio_stream);
	if (m_audio_is_mp3)
	{
		qacmStreamClose (hstr, 0);
		qacmDriverClose (had, 0);
	}
	if (avi.m_file)
		qAVIFileRelease (avi.m_file);

	if(aviFrameBuf) {
		Z_Free(aviFrameBuf);
		aviFrameBuf = NULL;
	}

	avi_recording = 0;
	captured_audio_samples = 0;

	// Close engine
	qAVIFileExit();
}

void AVI_StopExport(void)
{
	if(avi_recording)
	{
		AVI_ReleaseExporter();
		Cvar_SetValue("fixedtime", 0.0);
		avi_recording = 0;
	}
}

void AVI_ProcessFrame (void)
{

	if (!avi_recording || cls.state != ca_active || !cl.refresh_prepped)
		return;

	if(avi.m_video_frame_size != viddef.width * viddef.height * 3)
	{
		//resolution changed?
		AVI_StopExport ();
		Com_Printf("AVI_ProcessFrame: Wrong framesize, changed resolution?\n");
		Com_Printf("AVI export stopped\n");
		return;
	}

	GLAVI_ReadFrameData(aviFrameBuf);

	qAVIStreamWrite(Capture_VideoStream(), avi.m_video_frame_counter++, 1, aviFrameBuf, avi.m_video_frame_size, AVIIF_KEYFRAME, NULL, NULL);
}

static void AVI_WriteAudio (int samples, byte *sample_buffer)
{
	HRESULT		hr;
	unsigned long	sample_bufsize;

	if (!avi.m_audio_stream)
	{
		Com_Printf ("ERROR: Audio stream is NULL\n");
		return;
	}

	sample_bufsize = samples * avi.m_wave_format.nBlockAlign;
	if (m_audio_is_mp3)
	{
		MMRESULT	mmr;
		byte		*mp3_buffer;
		unsigned long	mp3_bufsize;

		if ((mmr = qacmStreamSize(hstr, sample_bufsize, &mp3_bufsize, ACM_STREAMSIZEF_SOURCE)))
		{
			Com_Printf ("ERROR: Couldn't get mp3bufsize\n");
			return;
		}
		if (!mp3_bufsize)
		{
			Com_Printf ("ERROR: mp3bufsize is zero\n");
			return;
		}
		mp3_buffer = malloc (mp3_bufsize);

		memset (&strhdr, 0, sizeof(strhdr));
		strhdr.cbStruct = sizeof(strhdr);
		strhdr.pbSrc = sample_buffer;
		strhdr.cbSrcLength = sample_bufsize;
		strhdr.pbDst = mp3_buffer;
		strhdr.cbDstLength = mp3_bufsize;

		if ((mmr = qacmStreamPrepareHeader(hstr, &strhdr, 0)))
		{
			Com_Printf ("ERROR: Couldn't prepare header\n");
			free (mp3_buffer);
			return;
		}

		if ((mmr = qacmStreamConvert(hstr, &strhdr, ACM_STREAMCONVERTF_BLOCKALIGN)))
		{
			Com_Printf ("ERROR: Couldn't convert audio stream\n");
			goto clean;
		}

		hr = qAVIStreamWrite (avi.m_audio_stream, avi.m_audio_frame_counter++, 1, mp3_buffer, strhdr.cbDstLengthUsed, AVIIF_KEYFRAME, NULL, NULL);

clean:
		if ((mmr = qacmStreamUnprepareHeader(hstr, &strhdr, 0)))
		{
			Com_Printf ("ERROR: Couldn't unprepare header\n");
			free (mp3_buffer);
			return;
		}

		free (mp3_buffer);
	}
	else
	{
		hr = qAVIStreamWrite (avi.m_audio_stream, avi.m_audio_frame_counter++, 1, sample_buffer, samples * avi.m_wave_format.nBlockAlign, AVIIF_KEYFRAME, NULL, NULL);
	}
	if (FAILED(hr))
	{
		Com_Printf ("ERROR: Couldn't write to AVI file\n");
		return;
	}
}

void SV_Map (qboolean attractloop, char *levelstring, qboolean loadgame);

void AVI_StartExporting(const char *name, float fps)
{
	char aviName[MAX_OSPATH];

	Q_strncpyz(aviName, name, sizeof(aviName));
	COM_DefaultExtension(aviName, sizeof(aviName), ".avi");
	if( !AVI_InitExporter(aviName, fps) )
	{
		AVI_ReleaseExporter();
		return;
	}

	aviFrameBuf = Z_TagMalloc(avi.m_video_frame_size, TAGMALLOC_AVIEXPORT);

	if(avi_sound->integer)
		avi_recording = 2;
	else
		avi_recording = 1;

	if(avi.m_codec_fourcc)
		Com_Printf("Exporting %s using %s codec ", aviName, FourccToString(avi.m_codec_fourcc));
	else
		Com_Printf("Exporting uncompressed %s ", aviName);

	if(m_audio_is_mp3)
		Com_Printf("with mp3 compressed sound\n"); 
	else
		Com_Printf("%s\n", (avi_recording == 2) ? "with uncompressed sound" : "without sound");

    Cvar_SetValue("fixedtime", (int)(0.5f + 1000.0f / avi.m_fps));
    if (avi_recording == 2 && Cvar_VariableValue("s_mixahead") < 1.0f / avi.m_fps) {
        Com_Printf("Warning: you may want to increase s_mixahead to %f!\n", 1.0f / avi.m_fps);
    }
    if (Cvar_VariableValue("cl_maxfps") < avi.m_fps) {
        Com_Printf("Warning: you may need to increase cl_maxfps!\n");
    }
}

void AVI_Export_f (void)
{
	char		demoName[MAX_QPATH];
	FILE		*f;


	if(!movie_avi_loaded)
	{
		Com_Printf("Aviexport unavailable\n");
		return;
	}

	// Check we have the correct number of arguments
	if(Cmd_Argc() != 4)
	{
		Com_Printf("Usage: AVIEXPORT <Framerate> <Demo Name> <AVI Name>\n"
				   "For Example 'aviexport 25 demo1.dm2 demo1.avi'\n");
		return;
	}

	Q_strncpyz(demoName, Cmd_Argv(2), sizeof(demoName));
	COM_DefaultExtension(demoName, sizeof(demoName), ".dm2");
	// Check to see if the demo actually exists
	FS_FOpenFile (va("demos/%s", demoName), &f);
	if (!f)
	{
		Com_Printf("Aviexport: Couldnt find demo 'demos/%s'\n", demoName);
		return;
	}
	FS_FCloseFile(f);

	AVI_StartExporting( Cmd_Argv(3), atof(Cmd_Argv(1)) );
	
	if(!avi_recording)
		return;

	SV_Map (true, demoName, false );
}

void AVI_Record_f(void)
{
	char	aviName[MAX_OSPATH];
	FILE	*f;
	int		i;


	if(!movie_avi_loaded)
	{
		Com_Printf("Aviexport unavailable\n");
		return;
	}

	// Check we have the correct number of arguments
	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: AVIRECORD <Framerate>\n"
				   "For Example 'avirecord 16'\n");
		return;
	}

	if(cls.state < ca_connected || !cl.attractloop)
	{
		Com_Printf("This command is only available on demo playback\n");
		return;
	}

	// Find a file name to save it to
	for (i = 0; i < 100; i++)
	{
		Com_sprintf (aviName, sizeof(aviName), "demo%02i.avi", i);
		f = fopen (aviName, "rb");
		if (!f)
			break;	// file doesn't exist
		fclose (f);
	}
	if (i == 100)
	{
		Com_Printf ("AVI_Record_f: Couldn't create a file\n"); 
		return;
	}

	AVI_StartExporting( aviName, atof(Cmd_Argv(1)) );
}

void AVI_Stop_f (void) {

	if (!avi_recording) {
		Com_Printf("Not exporting video\n");
		return;
	}

	AVI_StopExport ();
	Com_Printf("Aviexporting stopped\n");
}

void CL_InitAVIExport( void )
{
	avi_codec = Cvar_Get("avi_codec", "0", 0);
	avi_codecmenu = Cvar_Get("avi_codecmenu", "0", CVAR_ARCHIVE);
	avi_sound = Cvar_Get("avi_sound", "1", CVAR_ARCHIVE);
	avi_mp3_kbps = Cvar_Get("avi_mp3_kbps", "128", 0);
	Cmd_AddCommand ("aviexport", AVI_Export_f);
	Cmd_AddCommand ("avirecord", AVI_Record_f);
	Cmd_AddCommand ("avistop", AVI_Stop_f);

	Capture_InitAVI();
	Capture_InitACM();
}

static float VideoFps(void)
{
    if (fixedtime->value)
		return 1000.0f / fixedtime->value;

	return 0.0f;
}

void Movie_TransferStereo16(void)
{
	if (!avi_recording || cls.state != ca_active || !cl.refresh_prepped)
		return;

	if(avi_recording == 1) 
		return;

	// Copy last audio chunk written into our temporary buffer
	memcpy (capture_audio_samples + (captured_audio_samples << 1), snd_out, snd_linear_count * dma.channels);
	captured_audio_samples += (snd_linear_count >> 1);

	if (captured_audio_samples >= (int)(0.5f + (float)dma.speed / VideoFps())) {
		// We have enough audio samples to match one frame of video
		AVI_WriteAudio (captured_audio_samples, (byte *)capture_audio_samples);
		captured_audio_samples = 0;
	}
}

qboolean Movie_GetSoundtime(void)
{
	if (!avi_recording || cls.state != ca_active || !cl.refresh_prepped)
		return false;

	if(avi_recording == 1) 
		false;

	soundtime += (int)(0.5f + (float)dma.speed / VideoFps());
	return true;
}
#endif
