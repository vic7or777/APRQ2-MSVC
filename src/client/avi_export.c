
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

//
// AVI_EXPORT.C
//
// An AVI Video exporter for DM2 files
// By Robert 'Heffo' Heffernan
//

#include "client.h"
#include <windows.h>
#include <memory.h>
#include <vfw.h>
#include "avi.h"

extern cvar_t	*avi_gamma_r;
extern cvar_t	*avi_gamma_g;
extern cvar_t	*avi_gamma_b;
extern cvar_t	*avi_monochrome;
extern cvar_t	*avi_border_size;
extern cvar_t	*avi_border_r;
extern cvar_t	*avi_border_g;
extern cvar_t	*avi_border_b;

extern GLAVI_ReadFrameData_t GLAVI_ReadFrameData;
extern viddef_t	viddef;
avi_Data_t *avidm2;

avi_Data_t *AVI_InitExporter (char *filename, LPBITMAPINFOHEADER bitmapheader, DWORD framerate)
{
	AVISTREAMINFO		streaminfo;
	AVICOMPRESSOPTIONS	options;
	AVICOMPRESSOPTIONS FAR * aoptions[1] = {&options};
	HRESULT				hr;
	DWORD				vfwver;
	avi_Data_t			*newavi;

	if(!filename || !bitmapheader || !framerate)
		return NULL;

	vfwver = HIWORD(VideoForWindowsVersion());
	if(vfwver < 0x010a)
		return NULL;

	newavi = (avi_Data_t *)malloc(sizeof(avi_Data_t));
	if(!newavi)
		return NULL;

	strcpy(newavi->filename, filename);
	memcpy(&newavi->bitmapheader, bitmapheader, sizeof(BITMAPINFOHEADER));

	newavi->framerate			= framerate;
	newavi->frame				= 0;
	newavi->AVIFile				= NULL;
	newavi->Stream				= NULL;
	newavi->StreamCompressed	= NULL;

	AVIFileInit();
	hr = AVIFileOpen(&newavi->AVIFile, newavi->filename, OF_WRITE|OF_CREATE, NULL);
	if(hr != AVIERR_OK)
	{
		free(newavi);
		return NULL;
	}

	memset(&streaminfo, 0, sizeof(streaminfo));
	streaminfo.fccType                = streamtypeVIDEO;
	streaminfo.fccHandler             = 0;
	streaminfo.dwScale                = 1;
	streaminfo.dwRate                 = framerate;
	streaminfo.dwSuggestedBufferSize  = bitmapheader->biSizeImage;
	SetRect(&streaminfo.rcFrame, 0, 0, (int)bitmapheader->biWidth, (int)bitmapheader->biHeight);

	hr = AVIFileCreateStream(newavi->AVIFile, &newavi->Stream, &streaminfo);
	if(hr != AVIERR_OK)
	{
		free(newavi);
		return NULL;
	}

	memset(&options, 0, sizeof(options));
	if (!AVISaveOptions(NULL, 0, 1, &newavi->Stream, (LPAVICOMPRESSOPTIONS FAR *) &aoptions))
	{
		AVISaveOptionsFree(1,(LPAVICOMPRESSOPTIONS FAR *) &aoptions);
		free(newavi);
		return NULL;
	}

	hr = AVIMakeCompressedStream(&newavi->StreamCompressed, newavi->Stream, &options, NULL);
	if(hr != AVIERR_OK)
	{
		free(newavi);
		return NULL;
	}

	hr = AVISaveOptionsFree(1,(LPAVICOMPRESSOPTIONS FAR *) &aoptions);
	if(hr != AVIERR_OK)
	{
		free(newavi);
		return NULL;
	}

	hr = AVIStreamSetFormat(newavi->StreamCompressed, 0, &newavi->bitmapheader, newavi->bitmapheader.biSize + newavi->bitmapheader.biClrUsed * sizeof(RGBQUAD));
	if(hr != AVIERR_OK)
	{
		free(newavi);
		return NULL;
	}

	return newavi;
}

void AVI_ReleaseExporter(avi_Data_t *avi)
{
	if(!avi)
		return;

	if (avi->Stream)
	{
		AVIStreamRelease(avi->Stream);
		avi->Stream=NULL;
	}

	if (avi->StreamCompressed)
	{
		AVIStreamRelease(avi->StreamCompressed);
		avi->StreamCompressed=NULL;
	}

	if (avi->AVIFile)
	{
		AVIFileRelease(avi->AVIFile);
		avi->AVIFile=NULL;
	}

	// Close engine
	AVIFileExit();
}

int AVI_WriteFrame (avi_Data_t *avi, byte *framedata)
{
	HRESULT hr;

	hr = AVIStreamWrite(avi->StreamCompressed, avi->frame, 1, framedata, avi->bitmapheader.biSizeImage, AVIIF_KEYFRAME,	NULL, NULL);
	if(hr != AVIERR_OK)
		return 0;

	avi->frame++;

	return 1;
}

BITMAPINFOHEADER bmih;
byte avi_gamma[256][3];

void AVI_InitGamma (void)
{
	int i;
	float v;

	for(i=0;i<255;i++)
	{
		// RED
		v = 255 * pow ( (i+0.5)*0.0039138943248532289628180039138943 ,avi_gamma_r->value ) + 0.5;
		if (v > 255) v=255;
		if (v < 0) v=0;

		avi_gamma[i][0] = (byte)v;

		// GREEN
		v = 255 * pow ( (i+0.5)*0.0039138943248532289628180039138943 ,avi_gamma_g->value ) + 0.5;
		if (v > 255) v=255;
		if (v < 0) v=0;

		avi_gamma[i][1] = (byte)v;

		// BLUE
		v = 255 * pow ( (i+0.5)*0.0039138943248532289628180039138943 ,avi_gamma_b->value ) + 0.5;
		if (v > 255) v=255;
		if (v < 0) v=0;

		avi_gamma[i][2] = (byte)v;
	}
}

void AVI_ProcessFrame (void)
{
	int i, j;
	byte *buf, *gam;

	if (cls.state != ca_active || !cl.refresh_prepped || !avi_fps || !avi_fps->value)
		return;

	buf = (byte *)malloc(viddef.width*viddef.height*3);
	GLAVI_ReadFrameData(buf);

	// Convert To Monochrome (if Needed)
	if(avi_monochrome->value == 1)
	{
		gam = buf;
		for(i=0;i<(viddef.width * viddef.height);i++)
		{
			gam[0] = gam[1];
			gam[2] = gam[1];

			gam += 3;
		}
	}

	// Do Gamma (if Needed)
	if(avi_gamma_r->value != 1 || avi_gamma_g->value != 1 || avi_gamma_b->value != 1)
	{
		gam = buf;
		for(i=0;i<(viddef.width * viddef.height);i++)
		{
			gam[0] = avi_gamma[gam[0]][2]; gam++;
			gam[0] = avi_gamma[gam[0]][1]; gam++;
			gam[0] = avi_gamma[gam[0]][0]; gam++;
		}
	}

	// Do Border (if Needed)
	if(avi_border_size->value >= 1)
	{
		gam = buf;
		for(i=0;i<(viddef.width * avi_border_size->value);i++)
		{
			gam[0] = (byte)avi_border_b->value; gam++;
			gam[0] = (byte)avi_border_g->value; gam++;
			gam[0] = (byte)avi_border_r->value; gam++;
		}

		gam = buf + (int)(viddef.width * (viddef.height - avi_border_size->value) * 3);
		for(i=0;i<(viddef.width * avi_border_size->value);i++)
		{
			gam[0] = (byte)avi_border_b->value; gam++;
			gam[0] = (byte)avi_border_g->value; gam++;
			gam[0] = (byte)avi_border_r->value; gam++;
		}

		for(i=0;i<viddef.height;i++)
		{
			gam = buf + (int)((viddef.width * i) * 3);

			for(j=0;j<avi_border_size->value;j++)
			{
				gam[0] = (byte)avi_border_b->value; gam++;
				gam[0] = (byte)avi_border_g->value; gam++;
				gam[0] = (byte)avi_border_r->value; gam++;
			}

			gam = buf + (int)(((viddef.width * i) * 3) + ((viddef.width - avi_border_size->value) * 3));

			for(j=0;j<avi_border_size->value;j++)
			{
				gam[0] = (byte)avi_border_b->value; gam++;
				gam[0] = (byte)avi_border_g->value; gam++;
				gam[0] = (byte)avi_border_r->value; gam++;
			}
		}
	}

	AVI_WriteFrame(avidm2, buf);
	free(buf);
}

void SV_Map (qboolean attractloop, char *levelstring, qboolean loadgame);

void AVI_Export_f (void)
{
	char		name[MAX_OSPATH];
	FILE		*f;

	// Is the current renderer AVI Export enabled?
	if(GLAVI_ReadFrameData == NULL)
	{
		Com_Printf("Current renderer is not AVI enabled, AVI export is disabled\n");
		return;
	}

	// Check we have the correct number of arguments
	if(Cmd_Argc() != 4)
	{
		Com_Printf("Usage: AVIEXPORT <Framerate> <Demo Name> <AVI Name>\nFor Example 'aviexport 25 demo1.dm2 demo1.avi' \n");
		return;
	}

	// Check to see if the demo actually exists
	Com_sprintf (name, sizeof(name), "demos/%s", Cmd_Argv(2));
	FS_FOpenFile (name, &f);
	if (!f)
	{
		Com_Printf("ERROR: Specified demo doesn't exist\n");
		return;
	}
	FS_FCloseFile(f);

	// Setup AVI Structures
	memset(&bmih, 0, sizeof(bmih));
	bmih.biSize=sizeof(BITMAPINFOHEADER);
	bmih.biWidth=viddef.width;
	bmih.biHeight=viddef.height;
	bmih.biPlanes=1;
	bmih.biBitCount=24;
	bmih.biSizeImage=((bmih.biWidth*bmih.biBitCount+31)/32 * 4)*bmih.biHeight; 
	bmih.biCompression=BI_RGB;		//BI_RGB means BRG in reality

	// Initialise AVI Exporter
	Cvar_Set("avi_fps", Cmd_Argv(1));
	avidm2 = AVI_InitExporter(Cmd_Argv(3), &bmih, (DWORD)avi_fps->value);
	if(!avidm2)
	{
		Com_Printf("Error initialising AVI Exporter\n");

		AVI_ReleaseExporter(avidm2);
		Cvar_Set("avi_fps", "0");

		return;
	}

	// Initialise AVI Gamma Correction & Start Demo
	AVI_InitGamma();
	SV_Map (true, Cmd_Argv(2), false );
}
