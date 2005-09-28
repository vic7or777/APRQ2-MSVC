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

#include <jpeglib.h>
#include <png.h>

#define WAL_SCALE 1
#define PCX_SCALE 2
#define IMAGES_HASH_SIZE	64

image_t		gltextures[MAX_GLTEXTURES];
static image_t	*images_hash[IMAGES_HASH_SIZE];
int			numgltextures = 0;
//int			base_textureid;		// gltextures[i] = base_textureid+i

static byte			 intensitytable[256];
static unsigned char gammatable[256];

cvar_t		*intensity;
extern cvar_t *gl_sgis_mipmap;

unsigned	d_8to24table[256];
float		d_8to24tablef[256][3];

static int	gl_solid_format = 3;
static int	gl_alpha_format = 4;

int		gl_tex_solid_format = 3;
int		gl_tex_alpha_format = 4;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;
extern cvar_t *gl_gammapics;

void GL_SetTexturePalette( unsigned palette[256] )
{
	int i;
	unsigned char temptable[768];

	if ( qglColorTableEXT && gl_ext_palettedtexture->integer )
	{
		for ( i = 0; i < 256; i++ )
		{
			temptable[i*3+0] = ( palette[i] >> 0 ) & 0xff;
			temptable[i*3+1] = ( palette[i] >> 8 ) & 0xff;
			temptable[i*3+2] = ( palette[i] >> 16 ) & 0xff;
		}

		qglColorTableEXT( GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE, temptable );
	}
}

void GL_EnableMultitexture( qboolean enable )
{
	if ( !qglSelectTextureSGIS && !qglActiveTextureARB )
		return;

	if ( enable )
	{
		GL_SelectTexture( QGL_TEXTURE1 );
		qglEnable( GL_TEXTURE_2D );
		GL_TexEnv( GL_REPLACE );
	}
	else
	{
		GL_SelectTexture( QGL_TEXTURE1 );
		qglDisable( GL_TEXTURE_2D );
		GL_TexEnv( GL_REPLACE );
	}
	GL_SelectTexture( QGL_TEXTURE0 );
	GL_TexEnv( GL_REPLACE );
}

void GL_SelectTexture( GLenum texture )
{
	int tmu = 1;

	if ( !qglSelectTextureSGIS && !qglActiveTextureARB )
		return;

	if ( texture == QGL_TEXTURE0 )
		tmu = 0;

	if ( tmu == gl_state.currenttmu )
		return;

	gl_state.currenttmu = tmu;

	if ( qglSelectTextureSGIS )
	{
		qglSelectTextureSGIS( texture );
	}
	else if ( qglActiveTextureARB )
	{
		qglActiveTextureARB( texture );
		qglClientActiveTextureARB( texture );
	}
}

void GL_TexEnv( GLenum mode )
{
	static int lastmodes[2] = { -1, -1 };

	if ( mode != lastmodes[gl_state.currenttmu] )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
		lastmodes[gl_state.currenttmu] = mode;
	}
}

void GL_Bind (int texnum)
{
	extern	image_t	*draw_chars;

	if (gl_nobind->integer && draw_chars)		// performance evaluation option
		texnum = draw_chars->texnum;
	if ( gl_state.currenttextures[gl_state.currenttmu] == texnum)
		return;
	gl_state.currenttextures[gl_state.currenttmu] = texnum;
	qglBindTexture (GL_TEXTURE_2D, texnum);
}

void GL_MBind( GLenum target, int texnum )
{
	GL_SelectTexture( target );
	if ( target == QGL_TEXTURE0 )
	{
		if ( gl_state.currenttextures[0] == texnum )
			return;
	}
	else
	{
		if ( gl_state.currenttextures[1] == texnum )
			return;
	}
	GL_Bind( texnum );
}

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

static const glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

#define NUM_GL_MODES (sizeof(modes) / sizeof (glmode_t))

typedef struct
{
	char *name;
	int mode;
} gltmode_t;

static const gltmode_t gl_alpha_modes[] = {
	{"default", 4},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

#define NUM_GL_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof (gltmode_t))

static const gltmode_t gl_solid_modes[] = {
	{"default", 3},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
#ifdef GL_RGB2_EXT
	{"GL_RGB2", GL_RGB2_EXT},
#endif
};

#define NUM_GL_SOLID_MODES (sizeof(gl_solid_modes) / sizeof (gltmode_t))

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode( const char *string )
{
	int		i;
	image_t	*glt;

	for (i=0 ; i< NUM_GL_MODES ; i++)
	{
		if ( !Q_stricmp( modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_MODES)
	{
		Com_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i = 0, glt=gltextures; i < numgltextures; i++, glt++)
	{
		if (glt->type != it_pic && glt->type != it_sky )
		{
			GL_Bind (glt->texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
GL_TextureAlphaMode
===============
*/
void GL_TextureAlphaMode( const char *string )
{
	int		i;

	for (i=0; i < NUM_GL_ALPHA_MODES; i++)
	{
		if ( !Q_stricmp( gl_alpha_modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_ALPHA_MODES)
	{
		Com_Printf ("bad alpha texture mode name\n");
		return;
	}

	gl_tex_alpha_format = gl_alpha_modes[i].mode;
}

/*
===============
GL_TextureSolidMode
===============
*/
void GL_TextureSolidMode( const char *string )
{
	int		i;

	for (i=0 ; i< NUM_GL_SOLID_MODES ; i++)
	{
		if ( !Q_stricmp( gl_solid_modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_SOLID_MODES)
	{
		Com_Printf ("bad solid texture mode name\n");
		return;
	}

	gl_tex_solid_format = gl_solid_modes[i].mode;
}

/*
===============
GL_ImageList_f
===============
*/

static int ImageSort( const image_t *a, const image_t *b )
{
	return strcmp (a->name, b->name);
}

void	GL_ImageList_f (void)
{
	int		i, count;
	image_t	*image;
	image_t *sortedList;
	int		texels;
	const char *palstrings[2] =
	{
		"RGB",
		"PAL"
	};

	for (i=0, count = 0, image=gltextures; i<numgltextures ; i++, image++)
	{
		if(image->texnum <= 0)
			continue;

		count++;
	}

	sortedList = Z_TagMalloc(count * sizeof(image_t), TAGMALLOC_RENDER_IMAGE);

	count = 0;
	for (i=0, image=gltextures; i<numgltextures ; i++, image++)
	{
		if(image->texnum <= 0)
			continue;

		sortedList[count++] = *image;
	}

	qsort (sortedList, count, sizeof(sortedList[0]), (int (*)(const void *, const void *))ImageSort);

	Com_Printf ("------------------\n");
	texels = 0;

	for (i=0; i<count; i++)
	{
		image = &sortedList[i];

		texels += image->upload_width*image->upload_height;
		switch (image->type)
		{
		case it_skin:
			Com_Printf ("M");
			break;
		case it_sprite:
			Com_Printf ("S");
			break;
		case it_wall:
			Com_Printf ("W");
			break;
		case it_pic:
			Com_Printf ("P");
			break;
		default:
			Com_Printf (" ");
			break;
		}

		Com_Printf (" %3i %3i %s: %s%s\n",
			image->upload_width, image->upload_height, palstrings[image->paletted], image->name, image->extension);
	}
	Com_Printf ("%i images\n", count);
	Com_Printf ("Total texel count (not counting mipmaps): %i\n", texels);

	Z_Free(sortedList);
}


/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up inefficient hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		1
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

static int		scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
static byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT];
//qboolean	scrap_dirty;

// returns a texture number and the position inside it
static int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum = 0; texnum < MAX_SCRAPS; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH-w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	return -1;
//	Sys_Error ("Scrap_AllocBlock: full");
}
/*
int	scrap_uploads = 0;

void Scrap_Upload (void)
{
	scrap_uploads++;
	GL_Bind(TEXNUM_SCRAPS);
	GL_Upload8 (scrap_texels[0], BLOCK_WIDTH, BLOCK_HEIGHT, false, false, it_pic);
//	scrap_dirty = false;
}*/

/*
=================================================================

PCX LOADING

=================================================================
*/


/*
==============
LoadPCX
==============
*/
static void LoadPCXPal(const char *filename, byte **palette)
{
	byte	*raw;
	pcx_t	*pcx;
	int		len;
	int		xmax, ymax;

	*palette = NULL;

	// load the file
	len = FS_LoadFile (filename, (void **)&raw);
	if (!raw)
		return;

	// parse the PCX file
	pcx = (pcx_t *)raw;
	raw = &pcx->data;

  	xmax = LittleShort(pcx->xmax);
    ymax = LittleShort(pcx->ymax);

	if ((pcx->manufacturer != 0x0a) || (pcx->version != 5) ||
		(pcx->encoding != 1) || (pcx->bits_per_pixel != 8) ||
		(xmax >= 640) || (ymax >= 480))
	{
		Com_Printf ("LoadPCXPal: Bad pcx file: %s (%i x %i) (%i x %i)\n", filename, xmax+1, ymax+1, pcx->xmax, pcx->ymax);
		FS_FreeFile ((void *)pcx);
		return;
	}

	*palette = Z_TagMalloc(768, TAGMALLOC_RENDER_IMAGE);
	memcpy (*palette, (byte *)pcx + len - 768, 768);

	FS_FreeFile ((void *)pcx);
}

static void LoadPCX (const char *filename, byte **pic, int *width, int *height)
{
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte	*out, *pix;
	int		xmax, ymax;

	*pic = NULL;

	// load the file
	len = FS_LoadFile (filename, (void **)&raw);
	if (!raw)
		return;

	// parse the PCX file
	pcx = (pcx_t *)raw;
	raw = &pcx->data;

  	xmax = LittleShort(pcx->xmax);
    ymax = LittleShort(pcx->ymax);

	if ((pcx->manufacturer != 0x0a) || (pcx->version != 5) ||
		(pcx->encoding != 1) || (pcx->bits_per_pixel != 8) ||
		(xmax >= 640) || (ymax >= 480))
	{
		Com_Printf ("LoadPCX: Bad pcx file: %s (%i x %i) (%i x %i)\n", filename, xmax+1, ymax+1, pcx->xmax, pcx->ymax);
		FS_FreeFile ((void *)pcx);
		return;
	}

	out = Z_TagMalloc ( (ymax+1) * (xmax+1), TAGMALLOC_RENDER_IMAGE);
	*pic = out;
	pix = out;

	*width = xmax+1;
	*height = ymax+1;

	for ( y = 0; y <= ymax; y++, pix += xmax+1 )
	{
		for ( x = 0; x <= xmax; )
		{
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if ( raw - (byte *)pcx > len)
	{
		Com_DPrintf ( "LoadPCX: file %s was malformed", filename);
		Z_Free(*pic);
		*pic = NULL;
	}

	FS_FreeFile ((void *)pcx);
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

// Definitions for image types
#define TGA_Null		0	// no image data
#define TGA_Map			1	// Uncompressed, color-mapped images
#define TGA_RGB			2	// Uncompressed, RGB images
#define TGA_Mono		3	// Uncompressed, black and white images
#define TGA_RLEMap		9	// Runlength encoded color-mapped images
#define TGA_RLERGB		10	// Runlength encoded RGB images
#define TGA_RLEMono		11	// Compressed, black and white images
#define TGA_CompMap		32	// Compressed color-mapped data, using Huffman, Delta, and runlength encoding
#define TGA_CompMap4	33	// Compressed color-mapped data, using Huffman, Delta, and runlength encoding. 4-pass quadtree-type process

// Definitions for interleave flag
#define TGA_IL_None		0	// non-interleaved
#define TGA_IL_Two		1	// two-way (even/odd) interleaving
#define TGA_IL_Four		2	// four way interleaving
#define TGA_IL_Reserved	3	// reserved

// Definitions for origin flag
#define TGA_O_UPPER		0	// Origin in lower left-hand corner
#define TGA_O_LOWER		1	// Origin in upper left-hand corner

#define MAXCOLORS 16384

/*
=============
LoadTGA
=============
*/
static void LoadTGA( const char *filename, byte **pic, int *width, int *height )
{
	int			w, h, x, y, i, temp1, temp2;
	int			realrow, truerow, baserow, size, interleave, origin;
	int			pixel_size, map_idx, mapped, rlencoded, RLE_count, RLE_flag;
	TargaHeader	header;
	byte		tmp[2], r, g, b, a, j, k, l;
	byte		*dst, *ColorMap, *data, *pdata;

	// load file
	FS_LoadFile( filename, (void **)&data );

	if( !data )
		return;

	pdata = data;

	header.id_length = *pdata++;
	header.colormap_type = *pdata++;
	header.image_type = *pdata++;
	
	tmp[0] = pdata[0];
	tmp[1] = pdata[1];
	header.colormap_index = LittleShort( *((short *)tmp) );
	pdata+=2;
	tmp[0] = pdata[0];
	tmp[1] = pdata[1];
	header.colormap_length = LittleShort( *((short *)tmp) );
	pdata+=2;
	header.colormap_size = *pdata++;
	header.x_origin = LittleShort( *((short *)pdata) );
	pdata+=2;
	header.y_origin = LittleShort( *((short *)pdata) );
	pdata+=2;
	header.width = LittleShort( *((short *)pdata) );
	pdata+=2;
	header.height = LittleShort( *((short *)pdata) );
	pdata+=2;
	header.pixel_size = *pdata++;
	header.attributes = *pdata++;

	if( header.id_length )
		pdata += header.id_length;

	// validate TGA type
	switch( header.image_type ) {
		case TGA_Map:
		case TGA_RGB:
		case TGA_Mono:
		case TGA_RLEMap:
		case TGA_RLERGB:
		case TGA_RLEMono:
			break;
		default:
			Com_Printf ("LoadTGA: Only type 1, 2, 3, 9, 10 and 11 Targas are supported. Image:(%s).\n", filename );
			FS_FreeFile(data);
			return;
	}

	// validate color depth
	switch( header.pixel_size ) {
		case 8:
		case 15:
		case 16:
		case 24:
		case 32:
			break;
		default:
			Com_Printf ("LoadTGA: Only 8, 15, 16, 24 and 32 bit Targas (with colormaps) supported. Image:(%s).\n", filename );
			FS_FreeFile(data);
			return;
	}

	r = g = b = a = l = 0;

	// if required, read the color map information
	ColorMap = NULL;
	mapped = ( header.image_type == TGA_Map || header.image_type == TGA_RLEMap || header.image_type == TGA_CompMap || header.image_type == TGA_CompMap4 ) && header.colormap_type == 1;
	if( mapped ) {
		// validate colormap size
		switch( header.colormap_size ) {
			case 8:
			case 16:
			case 32:
			case 24:
				break;
			default:
				Com_Printf ("LoadTGA: Only 8, 16, 24 and 32 bit colormaps supported. Image:(%s).\n", filename );
				FS_FreeFile( data );
				return;
		}

		temp1 = header.colormap_index;
		temp2 = header.colormap_length;
		if( (temp1 + temp2 + 1) >= MAXCOLORS ) {
			FS_FreeFile( data );
			return;
		}
		ColorMap = (byte *)Z_TagMalloc( MAXCOLORS * 4, TAGMALLOC_RENDER_IMAGE);
		map_idx = 0;
		for( i = temp1; i < temp1 + temp2; ++i, map_idx += 4 ) {
			// read appropriate number of bytes, break into rgb & put in map
			switch( header.colormap_size ) {
				case 8:
					r = g = b = *pdata++;
					a = 255;
					break;
				case 15:
					j = *pdata++;
					k = *pdata++;
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = 255;
					break;
				case 16:
					j = *pdata++;
					k = *pdata++;
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = (k & 0x80) ? 255 : 0;
					break;
				case 24:
					b = *pdata++;
					g = *pdata++;
					r = *pdata++;
					a = 255;
					l = 0;
					break;
				case 32:
					b = *pdata++;
					g = *pdata++;
					r = *pdata++;
					a = *pdata++;
					l = 0;
					break;
			}
			ColorMap[map_idx + 0] = r;
			ColorMap[map_idx + 1] = g;
			ColorMap[map_idx + 2] = b;
			ColorMap[map_idx + 3] = a;
		}
	}

	// check run-length encoding
	rlencoded = header.image_type == TGA_RLEMap || header.image_type == TGA_RLERGB || header.image_type == TGA_RLEMono;
	RLE_count = 0;
	RLE_flag = 0;

	w = header.width;
	h = header.height;

	*width = w;
	*height = h;

	size = w * h * 4;
	*pic = (byte *)Z_TagMalloc( size, TAGMALLOC_RENDER_IMAGE);
	memset( *pic, 0, size );

	// read the Targa file body and convert to portable format
	pixel_size = header.pixel_size;
	origin = (header.attributes & 0x20) >> 5;
	interleave = (header.attributes & 0xC0) >> 6;
	truerow = 0;
	baserow = 0;
	for( y = 0; y < h; y++ ) {
		realrow = truerow;
		if( origin == TGA_O_UPPER )
			realrow = h - realrow - 1;

		dst = *pic + realrow * w * 4;

		for( x = 0; x < w; x++ ) {
			// check if run length encoded
			if( rlencoded ) {
				if( !RLE_count ) {
					// have to restart run
					i = *pdata++;
					RLE_flag = (i & 0x80);
					if( !RLE_flag ) {
						// stream of unencoded pixels
						RLE_count = i + 1;
					} else {
						// single pixel replicated
						RLE_count = i - 127;
					}
					// decrement count & get pixel
					--RLE_count;
				} else {
					// have already read count & (at least) first pixel
					--RLE_count;
					if( RLE_flag )
						// replicated pixels
						goto PixEncode;
				}
			}

			// read appropriate number of bytes, break into RGB
			switch( pixel_size ) {
				case 8:
					r = g = b = l = *pdata++;
					a = 255;
					break;
				case 15:
					j = *pdata++;
					k = *pdata++;
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = 255;
					break;
				case 16:
					j = *pdata++;
					k = *pdata++;
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = 255;
					break;
				case 24:
					b = *pdata++;
					g = *pdata++;
					r = *pdata++;
					a = 255;
					l = 0;
					break;
				case 32:
					b = *pdata++;
					g = *pdata++;
					r = *pdata++;
					a = *pdata++;
					l = 0;
					break;
				default:
					Com_Printf ("LoadTGA: Illegal pixel_size '%d' in file '%s'\n", filename );
					FS_FreeFile( data );
					return;
			}

PixEncode:
			if ( mapped )
			{
				map_idx = l * 4;
				*dst++ = ColorMap[map_idx + 0];
				*dst++ = ColorMap[map_idx + 1];
				*dst++ = ColorMap[map_idx + 2];
				*dst++ = ColorMap[map_idx + 3];
			}
			else
			{
				*dst++ = r;
				*dst++ = g;
				*dst++ = b;
				*dst++ = a;
			}
		}

		if (interleave == TGA_IL_Four)
			truerow += 4;
		else if (interleave == TGA_IL_Two)
			truerow += 2;
		else
			truerow++;

		if (truerow >= h)
			truerow = ++baserow;
	}

	if (mapped)
		Z_Free( ColorMap );
	
	FS_FreeFile ((void *)data );
}

qboolean WriteTGA( const char *name, byte *buffer, int width, int height )
{
	FILE		*f;
	int			i, c, temp;

	if( !(f = fopen( name, "wb" ) ) ) {
		Com_Printf( "WriteTGA: Couldn't create a file: %s\n", name ); 
		return false;
	}

	buffer[2] = 2;		// uncompressed type
	buffer[12] = width&255;
	buffer[13] = width>>8;
	buffer[14] = height&255;
	buffer[15] = height>>8;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr
	c = 18+width*height*3;
	for( i = 18; i < c; i += 3 ) {
		temp = buffer[i];
		buffer[i] = buffer[i+2];
		buffer[i+2] = temp;
	}
	fwrite( buffer, 1, c, f );
	fclose( f );

	return true;
} 

/*
=================================================================

JPEG LOADING

=================================================================
*/

static void jpg_null( j_decompress_ptr cinfo )
{
}

static boolean jpg_fill_input_buffer( j_decompress_ptr cinfo )
{
    Com_DPrintf("Premature end of JPEG data\n");
    return 1;
}

static void jpg_skip_input_data( j_decompress_ptr cinfo, long num_bytes )
{
    cinfo->src->next_input_byte += (size_t) num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

static void jpeg_mem_src(j_decompress_ptr cinfo, byte *mem, int len)
{
    cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));
    cinfo->src->init_source = jpg_null;
    cinfo->src->fill_input_buffer = jpg_fill_input_buffer;
    cinfo->src->skip_input_data = jpg_skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart;
    cinfo->src->term_source = jpg_null;
    cinfo->src->bytes_in_buffer = len;
    cinfo->src->next_input_byte = mem;
}

/*
==============
LoadJPG
==============
*/
static void LoadJPG (const char *filename, byte **pic, int *width, int *height)
{
	struct jpeg_decompress_struct	cinfo;
	struct jpeg_error_mgr			jerr;
	byte							*rawdata, *rgbadata, *scanline, *p, *q;
	int								rawsize, i;

	*pic = NULL;

	// Load JPEG file into memory
	rawsize = FS_LoadFile(filename, (void **)&rawdata);

	if(!rawdata)
		return;	

	if ( rawdata[6] != 'J' || rawdata[7] != 'F' || rawdata[8] != 'I' || rawdata[9] != 'F')
	{ 
		Com_Printf ("LoadJPG: Invalid JPEG header: %s\n", filename); 
		FS_FreeFile(rawdata); 
		return; 
	} 

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, rawdata, rawsize);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	if(cinfo.output_components != 3 && cinfo.output_components != 4)
	{
		Com_Printf ( "LoadJPG: Invalid JPEG colour components (%s)\n", filename);
		jpeg_destroy_decompress(&cinfo);
		FS_FreeFile(rawdata);
		return;
	}

	// Allocate Memory for decompressed image
	rgbadata = Z_TagMalloc(cinfo.output_width * cinfo.output_height * 4, TAGMALLOC_RENDER_IMAGE);

	// Pass sizes to output
	*width = cinfo.output_width;
	*height = cinfo.output_height;

	// Allocate Scanline buffer
	scanline = Z_TagMalloc (cinfo.output_width * 3, TAGMALLOC_RENDER_IMAGE);
	if (!scanline)
	{
		Com_Printf ("LoadJPG: Insufficient memory for JPEG scanline buffer\n");
		Z_Free (rgbadata);
		jpeg_destroy_decompress (&cinfo);
		FS_FreeFile (rawdata);
		return;
	}

	// Read Scanlines, and expand from RGB to RGBA
	q = rgbadata;
	while (cinfo.output_scanline < cinfo.output_height)
	{
		p = scanline;
		jpeg_read_scanlines(&cinfo, &scanline, 1);

		for (i = 0; i < cinfo.output_width; i++)
		{
			q[0] = p[0];
			q[1] = p[1];
			q[2] = p[2];
			q[3] = 255;
			p += 3;
			q += 4;
		}
	}

	Z_Free(scanline);
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	*pic = rgbadata;
	FS_FreeFile (rawdata);
}

qboolean WriteJPG( const char *name, byte *buffer, int width, int height, int quality )
{
	struct jpeg_compress_struct		cinfo;
	struct jpeg_error_mgr			jerr;
	FILE							*f;
	JSAMPROW						s[1];
	int								offset, w3;

	if( !(f = fopen( name, "wb" )) ) {
		Com_Printf( "WriteJPG: Couldn't create a file: %s\n", name ); 
		return false;
	}

	// initialize the JPEG compression object
	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_compress( &cinfo );
	jpeg_stdio_dest( &cinfo, f );

	// setup JPEG parameters
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;

	jpeg_set_defaults( &cinfo );

	if( (quality > 100) || (quality <= 0) )
		quality = 85;

	jpeg_set_quality( &cinfo, quality, TRUE );

	// start compression
	jpeg_start_compress( &cinfo, true );

	// feed scanline data
	w3 = cinfo.image_width * 3;
	offset = w3 * cinfo.image_height - w3;
	while( cinfo.next_scanline < cinfo.image_height ) {
		s[0] = &buffer[offset - cinfo.next_scanline * w3];
		jpeg_write_scanlines( &cinfo, s, 1 );
	}

	// finish compression
	jpeg_finish_compress( &cinfo );
	jpeg_destroy_compress( &cinfo );

	fclose ( f );

	return true;
}


/*
=============
LoadPNG
by R1CH
=============
*/

typedef struct {
    byte *Buffer;
    size_t Pos;
} TPngFileBuffer;

static void PngReadFunc(png_struct *Png, png_bytep buf, png_size_t size)
{
    TPngFileBuffer *PngFileBuffer=(TPngFileBuffer*)png_get_io_ptr(Png);
    memcpy(buf,PngFileBuffer->Buffer+PngFileBuffer->Pos,size);
    PngFileBuffer->Pos+=size;
}

static void LoadPNG (const char *filename, byte **pic, int *width, int *height)
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
		Com_Printf ("LoadPNG: couldnt create read struct\n");
		return;
	}

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
	{
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		FS_FreeFile (PngFileBuffer.Buffer);
		Com_Printf ("LoadPNG: couldnt create info struct\n", filename);
		return;
    }
    
	end_info = png_create_info_struct(png_ptr);
    if (!end_info)
	{
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		FS_FreeFile (PngFileBuffer.Buffer);
		Com_Printf ("LoadPNG: couldnt create end info struct\n");
		return;
    }

	png_set_read_fn (png_ptr,(png_voidp)&PngFileBuffer,(png_rw_ptr)PngReadFunc);

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	row_pointers = png_get_rows(png_ptr, info_ptr);

	rowptr = 0;

	*pic = Z_TagMalloc (info_ptr->width * info_ptr->height * sizeof(int), TAGMALLOC_RENDER_IMAGE);

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

	*width = info_ptr->width;
	*height = info_ptr->height;

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	FS_FreeFile (PngFileBuffer.Buffer);
}

/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

static void R_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0, filledcolor = -1, i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

//=======================================================

/*
================
GL_ResampleTexture
================
*/
static int			resampleWidth = 0;
static unsigned		*resampleBuffer = NULL;

static void GL_ResampleTexture (const unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	const unsigned	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	*p1, *p2;
	//unsigned *resampleBuffer;
	byte		*pix1, *pix2, *pix3, *pix4;

	if( outwidth > resampleWidth ) {
		if( resampleBuffer )
			Z_Free( resampleBuffer );
		resampleWidth = outwidth;
		resampleBuffer = Z_TagMalloc( outwidth * sizeof( unsigned ) * 2, TAGMALLOC_RENDER_IMGRESAMPLE);
	}

	p1 = resampleBuffer;
	p2 = resampleBuffer + outwidth;

	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for (i = 0; i < outwidth; i++)
	{
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for (i = 0; i < outwidth; i++)
	{
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth)
	{
		inrow = in + inwidth*(int)((i+0.25)*inheight / outheight);
		inrow2 = in + inwidth*(int)((i+0.75)*inheight / outheight);
		frac = fracstep >> 1;
		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(out+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(out+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(out+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((byte *)(out+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}

	//free(resampleBuffer);
}

/*
================
GL_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
static void GL_LightScaleTexture (unsigned *in, int inwidth, int inheight, qboolean only_gamma )
{
	if ( only_gamma )
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth * inheight;
		for (i = 0; i < c; i++, p += 4)
		{
			p[0] = gammatable[p[0]];
			p[1] = gammatable[p[1]];
			p[2] = gammatable[p[2]];
		}
	}
	else
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth * inheight;
		for (i = 0; i < c; i++, p += 4)
		{
			p[0] = gammatable[intensitytable[p[0]]];
			p[1] = gammatable[intensitytable[p[1]]];
			p[2] = gammatable[intensitytable[p[2]]];
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
static void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out = in;

	width <<=2;
	height >>= 1;

	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
===============
GL_Upload32

Returns has_alpha
===============
*/

static void GL_BuildPalettedTexture( byte *paletted_texture, const byte *scaled, int scaled_width, int scaled_height )
{
	int i;

	for ( i = 0; i < scaled_width * scaled_height; i++ )
	{
		unsigned int r, g, b, c;

		r = ( scaled[0] >> 3 ) & 31;
		g = ( scaled[1] >> 2 ) & 63;
		b = ( scaled[2] >> 3 ) & 31;

		c = r | ( g << 5 ) | ( b << 11 );

		paletted_texture[i] = gl_state.d_16to8table[c];

		scaled += 4;
	}
}

static int	upload_width, upload_height;
static qboolean uploaded_paletted;

static qboolean GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap, qboolean is_pic)
{
	int			samples;
	unsigned	*scaled = NULL;
	unsigned char paletted_texture[256*256];
	int			scaled_width, scaled_height;
	int			i, c;
	byte		*scan;
	int			comp;

	uploaded_paletted = false;

	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;

	if (mipmap)
	{
		if (gl_round_down->integer)
		{
			if (scaled_width > width)
				scaled_width >>= 1;
			if (scaled_height > height)
				scaled_height >>= 1;
		}

		// let people sample down the world textures for speed
		if (gl_picmip->integer > 0)
		{
			scaled_width >>= gl_picmip->integer;
			scaled_height >>= gl_picmip->integer;
		}
	}

	// don't ever bother with maxtexsize textures
	if ( qglColorTableEXT && gl_ext_palettedtexture->integer ) //palettedtexture is limited to 256
	{
		while ( scaled_width > 256 || scaled_height > 256 ) {
			scaled_width >>= 1;
			scaled_height >>= 1;
		}
	}
	else
	{
		while ( scaled_width > gl_state.maxtexsize || scaled_height > gl_state.maxtexsize ) {
			scaled_width >>= 1;
			scaled_height >>= 1;
		}
	}

	if (scaled_width < 1)
		scaled_width = 1;

	if (scaled_height < 1)
		scaled_height = 1;

	upload_width = scaled_width;
	upload_height = scaled_height;
	// scan the texture for any non-255 alpha
	c = width*height;
	scan = ((byte *)data) + 3;
	samples = gl_solid_format;
	for (i=0 ; i<c ; i++, scan += 4)
	{
		if ( *scan != 255 )
		{
			samples = gl_alpha_format;
			break;
		}
	}

	if (samples == gl_solid_format)
		comp = (gl_state.texture_compression) ? GL_COMPRESSED_RGB_ARB : gl_tex_solid_format;
	else
		comp = (gl_state.texture_compression) ? GL_COMPRESSED_RGBA_ARB : gl_tex_alpha_format;


	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			if ( qglColorTableEXT && gl_ext_palettedtexture->integer && samples == gl_solid_format )
			{
				uploaded_paletted = true;
				GL_BuildPalettedTexture( paletted_texture, ( unsigned char * ) data, scaled_width, scaled_height );
				qglTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, paletted_texture );
			}
			else
			{
				qglTexImage2D (GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			}
			goto done;
		}
		scaled = data;
	}
	else
	{
		scaled = Z_TagMalloc(scaled_width * scaled_height * 4, TAGMALLOC_RENDER_IMAGE);
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}

	if(!is_pic || gl_gammapics->integer)
		GL_LightScaleTexture (scaled, scaled_width, scaled_height, !mipmap );

	if ( qglColorTableEXT && gl_ext_palettedtexture->integer && samples == gl_solid_format )
	{
		uploaded_paletted = true;
		GL_BuildPalettedTexture( paletted_texture, ( unsigned char * ) scaled, scaled_width, scaled_height );
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, paletted_texture );
	}
	else
	{
		if(mipmap && gl_sgis_mipmap->integer && gl_state.sgis_mipmap)
		{
			//qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			qglTexParameteri (GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
		}

		qglTexImage2D( GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled );
	}

	if (mipmap && !gl_state.sgis_mipmap && !gl_sgis_mipmap->integer)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap ((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;

			if ( uploaded_paletted )
			{
				GL_BuildPalettedTexture( paletted_texture, ( unsigned char * ) scaled, scaled_width, scaled_height );
				qglTexImage2D( GL_TEXTURE_2D, miplevel, GL_COLOR_INDEX8_EXT, scaled_width, scaled_height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, paletted_texture );
			}
			else
			{
				qglTexImage2D (GL_TEXTURE_2D, miplevel, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
			}
		}
	}
	if (scaled && scaled != data)
		Z_Free(scaled);

done: ;

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (mipmap) ? gl_filter_min : gl_filter_max);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	return (samples == gl_alpha_format);
}

/*
===============
GL_Upload8

Returns has_alpha
===============
*/

static qboolean GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean is_sky, qboolean is_pic )
{
	unsigned	trans[512*256];
	int			i, s, p;

	s = width*height;

	if (s > sizeof(trans) / 4)
		Com_Error (ERR_DROP, "GL_Upload8: too large");

	if ( qglColorTableEXT && gl_ext_palettedtexture->integer && is_sky )
	{
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, width, height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, data );
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

		return false;
	}
	else
	{
		for (i = 0; i < s; i++)
		{
			p = data[i];
			trans[i] = d_8to24table[p];

			if (p == 255)
			{
				// transparent, so scan around for another color
				// to avoid alpha fringes
				// FIXME: do a full flood fill so mips work...
				if (i > width && data[i-width] != 255)
					p = data[i-width];
				else if (i < s-width && data[i+width] != 255)
					p = data[i+width];
				else if (i > 0 && data[i-1] != 255)
					p = data[i-1];
				else if (i < s-1 && data[i+1] != 255)
					p = data[i+1];
				else
					p = 0;
				// copy rgb components
				((byte *)&trans[i])[0] = ((byte *)&d_8to24table[p])[0];
				((byte *)&trans[i])[1] = ((byte *)&d_8to24table[p])[1];
				((byte *)&trans[i])[2] = ((byte *)&d_8to24table[p])[2];
			}
		}

		return GL_Upload32 (trans, width, height, mipmap, is_pic);
	}

}


/*
================
GL_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t *GL_LoadPic (const char *name, byte *pic, int width, int height, imagetype_t type, int bits, int scale)
{
	image_t		*image;
	int			i;
	char		s[MAX_QPATH];


	// find a free image_t
	for (i=0, image=gltextures ; i<numgltextures ; i++,image++)
	{
		if (!image->texnum)
			break;
	}
	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
			Com_Error (ERR_DROP, "MAX_GLTEXTURES");
		numgltextures++;
	}
	image = &gltextures[i];
	Q_strncpyz( image->name, name, sizeof(image->name) );
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;

	image->bits = bits;
	if (scale == WAL_SCALE)
	{
		miptex_t *mt;

		strcpy(s, name);
		Q_strncatz(s, ".wal", sizeof(s));
		FS_LoadFile (s, (void **)&mt);
		if (mt)
		{
			image->width = LittleLong(mt->width);
			image->height = LittleLong(mt->height);
			FS_FreeFile ((void *)mt);
		}
	}
	else if (scale == PCX_SCALE)
	{
		byte	*raw;
		pcx_t	*pcx;

		strcpy(s, name);
		Q_strncatz(s, ".pcx", sizeof(s));
		FS_LoadFile (s, (void **)&raw);
		if (raw)
		{
			pcx = (pcx_t *)raw;
			image->width = LittleShort(pcx->xmax) + 1;
			image->height = LittleShort(pcx->ymax) + 1;
			FS_FreeFile ((void *)pcx);
		}
	}

	if (type == it_skin && bits == 8)
		R_FloodFillSkin(pic, width, height);

	// load little pics into the scrap
	if (image->type == it_pic && bits == 8 && image->width < 64 && image->height < 64)
	{
		int		x, y;
		int		i, j, k = 0;
		int		texnum;

		texnum = Scrap_AllocBlock (image->width, image->height, &x, &y);
		if (texnum == -1)
			goto nonscrap;

		// copy the texels into the scrap block
		for (i = 0; i < image->height; i++)
			for (j = 0; j < image->width; j++, k++)
				scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = pic[k];

		image->texnum = TEXNUM_SCRAPS + texnum;
		image->scrap = true;
		image->has_alpha = true;
		image->sl = (x+0.01)/(float)BLOCK_WIDTH;
		image->sh = (x+image->width-0.01)/(float)BLOCK_WIDTH;
		image->tl = (y+0.01)/(float)BLOCK_WIDTH;
		image->th = (y+image->height-0.01)/(float)BLOCK_WIDTH;

		GL_Bind(image->texnum);
		GL_Upload8 (scrap_texels[0], BLOCK_WIDTH, BLOCK_HEIGHT, false, false, true);

		return image;
	}

nonscrap:
	image->scrap = false;
	image->texnum = TEXNUM_IMAGES + (image - gltextures);
	GL_Bind(image->texnum);
	if (bits == 8)
		image->has_alpha = GL_Upload8 (pic, width, height, (image->type != it_pic && image->type != it_sky), image->type == it_sky, image->type == it_pic );
	else
		image->has_alpha = GL_Upload32 ((unsigned *)pic, width, height, (image->type != it_pic && image->type != it_sky), image->type == it_pic );

	image->upload_width = upload_width;		// after power of 2 and scales
	image->upload_height = upload_height;
	image->paletted = uploaded_paletted;
	image->sl = 0;
	image->sh = 1;
	image->tl = 0;
	image->th = 1;

	return image;
}


/*
================
GL_LoadWal
================
*/
static image_t *GL_LoadWal (const char *name, const char *uploadName)
{
	miptex_t	*mt;
	int			width, height, ofs;
	image_t		*image;

	FS_LoadFile (name, (void **)&mt);
	if (!mt)
	{
		Com_Printf ("GL_FindImage: can't load %s\n", name);
		return NULL;
	}

	width = LittleLong (mt->width);
	height = LittleLong (mt->height);
	ofs = LittleLong (mt->offsets[0]);

	image = GL_LoadPic (uploadName, (byte *)mt + ofs, width, height, it_wall, 8, 0);

	FS_FreeFile ((void *)mt);

	return image;
}

static unsigned int IMG_HashKey (const char *name)
{
	int i;
	unsigned int hash = 0;

	for( i = 0; name[i]; i++ )
		hash += tolower(name[i]) * (i+119);

	return hash & (IMAGES_HASH_SIZE-1);
}

/*
===============
GL_FindImage

Finds or loads the given image
===============
*/
static void GL_LoadImage32 (char *pathname, int len, byte **pic, int *width, int *height)
{
	pathname[len+1]='p'; pathname[len+2]='n'; pathname[len+3]='g';
	LoadPNG(pathname, pic, width, height);
	if (*pic)
		return;

	pathname[len+1]='t'; pathname[len+2]='g'; pathname[len+3]='a';
	LoadTGA(pathname, pic, width, height);
	if (*pic)
		return;

	pathname[len+1]='j'; pathname[len+2]='p'; pathname[len+3]='g';
	LoadJPG(pathname, pic, width, height);
}

image_t	*GL_FindImage (const char *name, imagetype_t type)
{
	image_t	*image;
	int		i, len = 0, lastDot = -1;
	byte	*pic = NULL;
	int		width, height;
	char	uploadName[MAX_QPATH], pathname[MAX_QPATH];
	unsigned int hash;


	if (!name)
		return NULL;

	for( i = ( name[0] == '/' || name[0] == '\\' ); name[i] && (len < sizeof(pathname)-5); i++ ) {
		if( name[i] == '.' ) {
			lastDot = len;
			pathname[len++] = name[i];
		}
		else if( name[i] == '\\' ) 
			pathname[len++] = '/';
		else
			pathname[len++] = name[i];
	}

	if (len < 5)
		return NULL;
	else if( lastDot != -1 )
		len = lastDot;

	pathname[len] = 0;

	hash = IMG_HashKey(pathname);
	// look for it
	for (image = images_hash[hash]; image; image = image->hashNext)
	{
		if (!strcmp(pathname, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	// load the pic from disk
	strcpy (uploadName, pathname);
	pathname[len] = '.';
	pathname[len+4] = 0;

	if (!strcmp(pathname+len, ".pcx"))
	{
		if(gl_replacepcx->integer)
		{
			GL_LoadImage32(pathname, len, &pic, &width, &height);
			if(!pic)
			{
				pathname[len+1]='p'; pathname[len+2]='c'; pathname[len+3]='x';
				LoadPCX(pathname, &pic, &width, &height);
				if (!pic)
					return NULL;

				image = GL_LoadPic(uploadName, pic, width, height, type, 8, 0);
			}
			else
				image = GL_LoadPic(uploadName, pic, width, height, type, 32, PCX_SCALE);
		}
		else
		{
			LoadPCX(pathname, &pic, &width, &height);
			if (!pic)
				return NULL;

			image = GL_LoadPic(uploadName, pic, width, height, type, 8, 0);
		}
	}
	else if (!strcmp(pathname+len, ".wal"))
	{
		if(gl_replacewal->integer)
		{
			GL_LoadImage32(pathname, len, &pic, &width, &height);
			if(!pic)
			{
				pathname[len+1]='w'; pathname[len+2]='a'; pathname[len+3]='l';
				image = GL_LoadWal(pathname, uploadName);
				if(!image)
					return r_notexture;
			}
			else
				image = GL_LoadPic(uploadName, pic, width, height, type, 32, WAL_SCALE);
		}
		else
		{
			image = GL_LoadWal(pathname, uploadName);
			if(!image)
				return r_notexture;
		}
	}
	else //try png, tga and jpg
	{
		GL_LoadImage32(pathname, len, &pic, &width, &height);
		if(!pic)
			return NULL;

		image = GL_LoadPic(uploadName, pic, width, height, type, 32, 0);
	}

	if (pic)
		Z_Free(pic);

	strcpy (image->extension, pathname+len);
	image->hashNext = images_hash[hash];
	images_hash[hash] = image;

	return image;
}

/*
===============
R_RegisterSkin
===============
*/
struct image_s *R_RegisterSkin (const char *name)
{
	return GL_FindImage (name, it_skin);
}


/*
================
GL_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void GL_FreeUnusedImages (void)
{
	int		i;
	image_t	*image, *entry, **back;
	unsigned int hash;

	// never free r_notexture or particle texture
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;
	r_caustictexture->registration_sequence = registration_sequence;
	r_bholetexture->registration_sequence = registration_sequence;
	r_shelltexture->registration_sequence = registration_sequence;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue;		// used this sequence
		if (!image->registration_sequence)
			continue;		// free image_t slot
		if (image->type == it_pic)
			continue;		// don't free pics

		// delete it from hash table
		hash = IMG_HashKey (image->name);
		for( back=&images_hash[hash], entry=images_hash[hash]; entry; back=&entry->hashNext, entry=entry->hashNext ) {
			if( entry == image ) {
				*back = entry->hashNext;
				break;
			}
		}

		// free it
		qglDeleteTextures (1, (GLuint *)&image->texnum);
		memset (image, 0, sizeof(*image));
	}
}


/*
===============
Draw_GetPalette
===============
*/
int Draw_GetPalette (void)
{
	int			i, r, g, b;	
	unsigned	v;
	byte		*pal;
	byte	default_pal[] = 
	{
	#include "../qcommon/def_pal.dat"
	};

	// get the palette

	LoadPCXPal("pics/colormap.pcx", &pal);
	if (!pal)
		pal = default_pal;

	for (i = 0; i < 256; i++)
	{
		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];
		
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		d_8to24table[i] = LittleLong(v);

		d_8to24tablef[i][0] = r*ONEDIV255;
		d_8to24tablef[i][1] = g*ONEDIV255;
		d_8to24tablef[i][2] = b*ONEDIV255;
	}

	d_8to24table[255] &= LittleLong(0xffffff);	// 255 is transparent

	if (pal != default_pal)
		Z_Free (pal);

	return 0;
}


/*
===============
GL_InitImages
===============
*/
void	GL_InitImages (void)
{
	int		i, j;
	float	g = vid_gamma->value;

	registration_sequence = 1;

	// init intensity conversions
	intensity = Cvar_Get ("intensity", "2", 0);

	if ( intensity->value < 1 )
		Cvar_Set( "intensity", "1" );

	gl_state.inverse_intensity = 1 / intensity->value;

	Draw_GetPalette ();

	if ( qglColorTableEXT )
	{
		FS_LoadFile( "pics/16to8.dat", (void **)&gl_state.d_16to8table );
		if ( !gl_state.d_16to8table )
			Com_Error( ERR_FATAL, "Couldn't load pics/16to8.dat" );
	}

	if ( gl_config.renderer & ( GL_RENDERER_VOODOO | GL_RENDERER_VOODOO2 ) )
	{
		g = 1.0F;
	}

	for ( i = 0; i < 256; i++ )
	{
		if ( g == 1 )
		{
			gammatable[i] = i;
		}
		else
		{
			float inf;

			inf = 255 * (float)pow ( (i+0.5)*ONEDIV255_5, g ) + 0.5f;
			if (inf < 0)
				inf = 0;
			if (inf > 255)
				inf = 255;
			gammatable[i] = inf;
		}

		j = i*intensity->value;
		if (j > 255)
			j = 255;
		intensitytable[i] = j;
	}
}

/*
===============
GL_ShutdownImages
===============
*/
void	GL_ShutdownImages (void)
{
	int		i;
	image_t	*image;

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		if (!image->registration_sequence)
			continue;		// free image_t slot
		// free it
		qglDeleteTextures (1, (GLuint *)&image->texnum);
		memset (image, 0, sizeof(*image));
	}
	numgltextures = 0;
	memset(images_hash, 0, sizeof(images_hash) );
	memset(scrap_allocated, 0, sizeof(scrap_allocated));

	if(resampleWidth) {
		Z_Free (resampleBuffer);
		resampleWidth = 0;
		resampleBuffer = NULL;
	}
}

