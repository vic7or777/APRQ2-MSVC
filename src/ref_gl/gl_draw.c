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

// draw.c

#include "gl_local.h"

image_t		*draw_chars;

//extern	qboolean	scrap_dirty;
//void Scrap_Upload (void);

// vertex arrays
float	tex_array[MAX_ARRAY][2];
float	vert_array[MAX_ARRAY][3];
float	col_array[MAX_ARRAY][4];

extern cvar_t *gl_fontshadow;

/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	// load console characters (don't bilerp characters)
	draw_chars = GL_FindImage ("pics/conchars.pcx", it_pic);
	GL_Bind( draw_chars->texnum );
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

const vec3_t color_table[8] = {
	{0, 0, 0},	// Black
	{1, 0, 0},	// Red
	{0, 1, 0},	// Green
	{1, 1, 0},	// Yellow
	{0, 0, 1},	// Blue
	{0, 1, 1},	// Cyan
	{1, 0, 1},	// Magenta
	{1, 1, 1}	// White
};

/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/

void Draw_Char (int x, int y, int num, int color, float alpha)
{
	int				row, col;
	float			frow, fcol, size;
	int				fsize;
	vec4_t			colors = {1, 1, 1, 1};

	if (color != COLOR_WHITE && num > 127)
		num &= 127;

	num &= 255;
	
	if ( (num&127) == 32 )
		return;		// space

	if (y <= -8)
		return;			// totally off screen

	if(num < 128)
		VectorCopy(color_table[(color&7)], colors);

	colors[3] = alpha;

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;
	fsize = 8;


	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	GL_TexEnv (GL_MODULATE);

	GL_Bind (draw_chars->texnum);

	if(gl_fontshadow->integer)
	{
		qglColor4f (0, 0, 0, alpha);
		qglBegin (GL_QUADS);

		qglTexCoord2f (fcol, frow);
		qglVertex2f (x+1, y+1);
		qglTexCoord2f (fcol + size, frow);
		qglVertex2f (x+fsize+1, y+1);
		qglTexCoord2f (fcol + size, frow + size);
		qglVertex2f (x+fsize+1, y+fsize+1);
		qglTexCoord2f (fcol, frow + size);
		qglVertex2f (x+1, y+fsize+1);

		qglEnd ();
	}

	qglColor4fv (colors);

	qglBegin (GL_QUADS);

	qglTexCoord2f (fcol, frow);
	qglVertex2f (x, y);
	qglTexCoord2f (fcol + size, frow);
	qglVertex2f (x+fsize, y);
	qglTexCoord2f (fcol + size, frow + size);
	qglVertex2f (x+fsize, y+fsize);
	qglTexCoord2f (fcol, frow + size);
	qglVertex2f (x, y+fsize);
	qglEnd ();

	qglColor4f (1,1,1,1);

	GL_TexEnv(GL_REPLACE);
	qglEnable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
}

/*
=============
Draw_FindPic
=============
*/
image_t	*Draw_FindPic (const char *name)
{
	image_t *gl;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
		if(!strncmp("../", name, 3)) //gentoo doesnt seems to handle .. path?
			Com_sprintf (fullname, sizeof(fullname), "%s.pcx", name+3);
		else
			Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);

		gl = GL_FindImage (fullname, it_pic);
	}
	else
		gl = GL_FindImage (name+1, it_pic);

	return gl;
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize (int *w, int *h, const char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		*w = *h = -1;
		return;
	}
	*w = gl->width;
	*h = gl->height;
}

/*
=============
Draw_ScaledPic
=============
*/
void Draw_ScaledPic (int x, int y, float scale, const char *pic, float red, float green, float blue, float alpha)
{
	image_t *gl;
	int yoff, xoff;
	int enabled = 0;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		Com_DPrintf ( "Can't find pic: %s\n", pic);
		return;
	}


	if (alpha < 1 || (gl->bits == 32 && gl->has_alpha))
	{
		enabled = 1;
		qglEnable(GL_BLEND);
		qglDisable(GL_ALPHA_TEST);
		GL_TexEnv(GL_MODULATE);
	}
	else if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
	{
		enabled = 2;
		qglDisable(GL_ALPHA_TEST);
	}

	qglColor4f(red, green, blue, alpha);

	GL_Bind (gl->texnum);

	qglBegin (GL_QUADS);
	xoff = gl->width * scale - gl->width;
	yoff = gl->height * scale - gl->height;
	x -= xoff/2;
	y -= yoff/2;
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x+gl->width+xoff, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x+gl->width+xoff, y+gl->height+yoff);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y+gl->height+yoff);

	qglEnd ();

	if (enabled == 1)
	{
		GL_TexEnv(GL_REPLACE);
		qglEnable(GL_ALPHA_TEST);
		qglDisable(GL_BLEND);

	}
	else if (enabled == 2)
		qglEnable(GL_ALPHA_TEST);

	qglColor4f(1,1,1,1);
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int w, int h, const char *pic, float alpha)
{
	image_t *gl;
	int enabled = 0;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		Com_DPrintf ( "Can't find pic: %s\n", pic);
		return;
	}

	if (alpha < 1 || (gl->bits == 32 && gl->has_alpha))
	{
		enabled = 1;
		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
		GL_TexEnv(GL_MODULATE);
		qglColor4f(1,1,1,alpha);
	}
	else if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
	{
		enabled = 2;
		qglDisable(GL_ALPHA_TEST);
	}

	GL_Bind (gl->texnum);

	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x+w, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x+w, y+h);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y+h);
	qglEnd ();

	if (enabled == 1)
	{
		GL_TexEnv (GL_REPLACE);
		qglEnable(GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
		qglColor4f(1,1,1,1);
	}
	else if (enabled == 2)
		qglEnable(GL_ALPHA_TEST);

}
// End

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, const char *pic, float alpha)
{
	image_t *gl;
	int enabled = 0;


	gl = Draw_FindPic (pic);
	if (!gl)
	{
		Com_DPrintf ( "Can't find pic: %s\n", pic);
		return;
	}

	if (alpha < 1 || (gl->bits == 32 && gl->has_alpha))
	{
		enabled = 1;
		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
		GL_TexEnv(GL_MODULATE);
		qglColor4f(1,1,1,alpha);
	}
	else if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
	{
		enabled = 2;
		qglDisable(GL_ALPHA_TEST);
	}

	GL_Bind (gl->texnum);

	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2f (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2f (x+gl->width, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2f (x+gl->width, y+gl->height);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2f (x, y+gl->height);
	qglEnd ();
	
	if (enabled == 1)
	{
		GL_TexEnv (GL_REPLACE);
		qglEnable(GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
		qglColor4f(1,1,1,1);
	}
	else if (enabled == 2)
		qglEnable(GL_ALPHA_TEST);

}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, const char *pic)
{
	image_t	*image;

	image = Draw_FindPic (pic);
	if (!image)
	{
		Com_DPrintf ( "Can't find pic: %s\n", pic);
		return;
	}

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )  && !image->has_alpha)
		qglDisable(GL_ALPHA_TEST);

	GL_Bind (image->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (x * 0.015625, y * 0.015625);
	qglVertex2f (x, y);
	qglTexCoord2f ( (x+w) * 0.015625, y * 0.015625);
	qglVertex2f (x+w, y);
	qglTexCoord2f ( (x+w) * 0.015625, (y+h) * 0.015625);
	qglVertex2f (x+w, y+h);
	qglTexCoord2f ( x * 0.015625, (y+h) * 0.015625 );
	qglVertex2f (x, y+h);
	qglEnd ();

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )  && !image->has_alpha)
		qglEnable(GL_ALPHA_TEST);
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	union
	{
		unsigned	c;
		byte		v[4];
	} color;

	if ( (unsigned)c > 255)
		Com_Error (ERR_FATAL, "Draw_Fill: bad color");

	qglDisable (GL_TEXTURE_2D);

	color.c = d_8to24table[c];
	qglColor3f (color.v[0]/255.0, color.v[1]/255.0, color.v[2]/255.0);

	qglBegin (GL_QUADS);

	qglVertex2f (x,y);
	qglVertex2f (x+w, y);
	qglVertex2f (x+w, y+h);
	qglVertex2f (x, y+h);

	qglEnd ();
	qglColor3f (1,1,1);
	qglEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	qglEnable(GL_BLEND);
	qglDisable (GL_TEXTURE_2D);
	qglColor4f (0, 0, 0, 0.75);
	qglBegin (GL_QUADS);

	qglVertex2f (0,0);
	qglVertex2f (vid.width, 0);
	qglVertex2f (vid.width, vid.height);
	qglVertex2f (0, vid.height);

	qglEnd ();
	qglColor4f (1,1,1,1);
	qglEnable (GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
extern unsigned	r_rawpalette[256];

void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	unsigned	image32[256*256];
	unsigned char image8[256*256];
	int			i, j, trows;
	byte		*source;
	int			frac, fracstep;
	float		hscale;
	int			row;
	float		t;

	GL_Bind (0);

	if (rows<=256)
	{
		hscale = 1;
		trows = rows;
	}
	else
	{
		hscale = rows /256.0;
		trows = 256;
	}
	t = rows*hscale / 256.0;
	fracstep = cols*0x10000 /256.0;

	if ( !qglColorTableEXT )
	{
		unsigned *dest = image32;

		memset ( image32, 0, sizeof(unsigned)*256*256 );

		for (i=0 ; i<trows ; i++, dest+=256)
		{
			row = (int)(i*hscale);
			if (row > rows)
				break;
			source = data + cols*row;

			frac = fracstep >> 1;
			for (j=0 ; j<256 ; j+=4)
			{
				dest[j] = r_rawpalette[source[frac>>16]];
				frac += fracstep;
				dest[j+1] = r_rawpalette[source[frac>>16]];
				frac += fracstep;
				dest[j+2] = r_rawpalette[source[frac>>16]];
				frac += fracstep;
				dest[j+3] = r_rawpalette[source[frac>>16]];
				frac += fracstep;
			}
		}

		qglTexImage2D (GL_TEXTURE_2D, 0, gl_tex_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image32);
	}
	else
	{
		unsigned char *dest = image8;

		memset ( image8, 0, sizeof(unsigned char)*256*256 );

		for (i=0 ; i<trows ; i++, dest+=256)
		{
			row = (int)(i*hscale);
			if (row > rows)
				break;
			source = data + cols*row;
			frac = fracstep >> 1;
			for (j=0 ; j<256 ; j+=4)
			{
				dest[j] = source[frac>>16];
				frac += fracstep;
				dest[j+1] = source[frac>>16];
				frac += fracstep;
				dest[j+2] = source[frac>>16];
				frac += fracstep;
				dest[j+3] = source[frac>>16];
				frac += fracstep;
			}
		}

		qglTexImage2D( GL_TEXTURE_2D, 
			           0, 
					   GL_COLOR_INDEX8_EXT, 
					   256, 256, 
					   0, 
					   GL_COLOR_INDEX, 
					   GL_UNSIGNED_BYTE, 
					   image8 );
	}
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) 
		qglDisable(GL_ALPHA_TEST);

	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2f (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2f (x+w, y);
	qglTexCoord2f (1, t);
	qglVertex2f (x+w, y+h);
	qglTexCoord2f (0, t);
	qglVertex2f (x, y+h);
	qglEnd ();

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) 
		qglEnable(GL_ALPHA_TEST);
}

