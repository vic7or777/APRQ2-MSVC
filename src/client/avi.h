
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
// AVI.H
//
// An AVI Video exporter for DM2 files
// By Robert 'Heffo' Heffernan
//
#ifdef AVI_EXPORT
typedef struct
{
	char				filename[MAX_OSPATH];

	long				frame;
	DWORD				framerate;

	BITMAPINFOHEADER	bitmapheader;
	PAVIFILE			AVIFile;
	PAVISTREAM			Stream;
	PAVISTREAM			StreamCompressed;
} avi_Data_t;

extern cvar_t *avi_fps;
extern avi_Data_t *avidm2;

avi_Data_t *AVI_InitExporter (char *filename, LPBITMAPINFOHEADER bitmapheader, DWORD framerate);
void AVI_ReleaseExporter(avi_Data_t *avi);

#endif