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

#include "gl_local.h"
#ifdef _WIN32
#include "../lib/png.h"
#else
#include <libpng/png.h>
#endif

typedef struct {
    byte *Buffer;
    int Pos;
} TPngFileBuffer;

void PngReadFunc(png_struct *Png, png_bytep buf, png_size_t size)
{
    TPngFileBuffer *PngFileBuffer=(TPngFileBuffer*)png_get_io_ptr(Png);
    memcpy(buf,PngFileBuffer->Buffer+PngFileBuffer->Pos,size);
    PngFileBuffer->Pos+=size;
}

/*
=============
LoadPNG
by R1CH
=============
*/

void LoadPNG (const char *filename, byte **pic, int *width, int *height)
{
	int				i, rowptr;
	png_structp		png_ptr;
	png_infop		info_ptr;
	png_infop		end_info;
	unsigned char	**row_pointers;
	TPngFileBuffer	PngFileBuffer = {NULL,0};

	*pic = NULL;

	FS_LoadFile (filename, (void **)&PngFileBuffer.Buffer);

    if (!PngFileBuffer.Buffer)
		return;

	if ((png_check_sig(PngFileBuffer.Buffer, 8)) == 0)
	{
		FS_FreeFile (PngFileBuffer.Buffer); 
		Com_Printf ("LoadPNG: Not a PNG file: %s\n", filename);
		return;
    }

	PngFileBuffer.Pos=0;

    png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL,  NULL, NULL);

    if (!png_ptr)
	{
		FS_FreeFile (PngFileBuffer.Buffer);
		Com_Printf ("LoadPNG: Bad PNG file: %s\n", filename);
		return;
	}

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
	{
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		FS_FreeFile (PngFileBuffer.Buffer);
		Com_Printf ("LoadPNG: Bad PNG file: %s\n", filename);
		return;
    }
    
	end_info = png_create_info_struct(png_ptr);
    if (!end_info)
	{
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		FS_FreeFile (PngFileBuffer.Buffer);
		Com_Printf ("LoadPNG: Bad PNG file: %s\n", filename);
		return;
    }

	png_set_read_fn (png_ptr,(png_voidp)&PngFileBuffer,(png_rw_ptr)PngReadFunc);

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	row_pointers = png_get_rows(png_ptr, info_ptr);

	rowptr = 0;

	*pic = malloc (info_ptr->width * info_ptr->height * sizeof(int));

	if (info_ptr->channels == 4)
	{
		for (i = 0; i < info_ptr->height; i++)
		{
			memcpy (*pic + rowptr, row_pointers[i], info_ptr->rowbytes);
			rowptr += info_ptr->rowbytes;
		}
	}
	else
	{
		int j, x;
		memset (*pic, 255, info_ptr->width * info_ptr->height * sizeof(int));
		x = 0;
		for (i = 0; i < info_ptr->height; i++)
		{
			for (j = 0; j < info_ptr->rowbytes; j+=info_ptr->channels)
			{
				memcpy (*pic + x, row_pointers[i] + j, info_ptr->channels);
				x+= sizeof(int);
			}
			rowptr += info_ptr->rowbytes;
		}
	}

	if (width)
		*width = info_ptr->width;
	if (height)
		*height = info_ptr->height;

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	FS_FreeFile (PngFileBuffer.Buffer);
}
