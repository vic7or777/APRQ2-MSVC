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

extern	qboolean	scrap_dirty;
void Scrap_Upload (void);

/* 
 * Changed, tgahud, -Maniac
 * Copied from Q2ICE project (http://q2ice.iceware.net)
 */

extern cvar_t		*gl_hudformat;
extern cvar_t		*gl_conformat;
extern cvar_t		*gl_hudtrans;
//ignorecantfindpic
extern cvar_t *ignorecantfindpic;
//end

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



/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Char (int x, int y, int num)
{
	int				row, col;
	float			frow, fcol, size;

	num &= 255;
	
	if ( (num&127) == 32 )
		return;		// space

	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	GL_Bind (draw_chars->texnum);

	qglBegin (GL_QUADS);
	qglTexCoord2f (fcol, frow);
	qglVertex2f (x, y);
	qglTexCoord2f (fcol + size, frow);
	qglVertex2f (x+8, y);
	qglTexCoord2f (fcol + size, frow + size);
	qglVertex2f (x+8, y+8);
	qglTexCoord2f (fcol, frow + size);
	qglVertex2f (x, y+8);
	qglEnd ();
}

/*
 * Added Draw_Char_Sized -Maniac
 * from Q2ACE (http://www.savageservers.com/q2ace/)
 */
void Draw_Char_Sized (int x, int y, int num, int xsize, int ysize)
{
        int                             row, col;
        float                   frow, fcol, size;

        num &= 255;

        if ( (num&127) == 32 )
                return;         // space

        if (y <= -ysize)
                return;                 // totally off screen

        row = num>>4;
        col = num&15; 

        frow = row*0.0625;
        fcol = col*0.0625;
        size = 0.0625;

        GL_Bind (draw_chars->texnum);

        qglBegin (GL_QUADS);

        qglTexCoord2f (fcol, frow);
        qglVertex2f (x, y);
        qglTexCoord2f (fcol + size, frow);
        qglVertex2f (x+xsize, y);
        qglTexCoord2f (fcol + size, frow + size);
        qglVertex2f (x+xsize, y+ysize);
        qglTexCoord2f (fcol, frow + size);
        qglVertex2f (x, y+ysize);
        qglEnd ();
}

// End

/*
=============
Draw_FindPic
=============
*/
image_t	*Draw_FindPic (char *name)
{
	image_t *gl;
	char	fullname[MAX_QPATH], format[4];

	if (name[0] != '/' && name[0] != '\\')
	{
		if(!strcmp( name, "conback" ))
		{
			if(gl_conformat->value)
			{
				switch((int)gl_conformat->value)
				{
					case 1:	strcpy(format, "tga"); break;
					case 2: strcpy(format, "png"); break;
					case 3: strcpy(format, "jpg"); break;
					default: strcpy(format, "pcx"); break;
				}
			    Com_sprintf (fullname, sizeof(fullname), "pics/%s.%s", name, format);
				gl = GL_FindImage (fullname, it_pic);

				if (!gl)
				{
						Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
						gl = GL_FindImage (fullname, it_pic);
				}
			}
			else
			{
				Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
				gl = GL_FindImage (fullname, it_pic);
            }
		}
		else
		{
		//Changed, added different hud formats, -Maniac
		    if (gl_hudformat->value)
			{
				switch((int)gl_hudformat->value)
				{
					case 1:	strcpy(format, "tga"); break;
					case 2: strcpy(format, "png"); break;
					case 3: strcpy(format, "jpg"); break;
					default: strcpy(format, "pcx"); break;
				}
				Com_sprintf (fullname, sizeof(fullname), "pics/%s.%s", name, format);
				gl = GL_FindImage (fullname, it_pic);

				if (!gl)
				{
					Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
					gl = GL_FindImage (fullname, it_pic);
				}

			}
			else
			{
							Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
							gl = GL_FindImage (fullname, it_pic);
            }
		}

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
void Draw_GetPicSize (int *w, int *h, char *pic)
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
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int w, int h, char *pic, float alpha)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		//Added ignore cantfindpic -Maniac
		if(!ignorecantfindpic->value)
		{
			ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		}
		//End
		return;
	}

	if (scrap_dirty)
		Scrap_Upload ();

	if (gl->paletted) {
		if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
			GLSTATE_DISABLE_ALPHATEST
	}
	else
	{
		GLSTATE_DISABLE_ALPHATEST
		GLSTATE_ENABLE_BLEND
	}

		//Psychospaz's transparent console support -Maniac
	if (gl->has_alpha || alpha < 1)
	{
		GLSTATE_DISABLE_ALPHATEST
		GL_Bind(gl->texnum);
		GL_TexEnv(GL_MODULATE);
		qglColor4f(1,1,1,alpha);	
		GLSTATE_ENABLE_BLEND
		qglDepthMask(false);
	}
	else //end
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

	//Psychospaz's transparent console support -Maniac
	if (gl->has_alpha || alpha < 1)
	{
		qglDepthMask (true);
		GL_TexEnv(GL_REPLACE);
		GLSTATE_DISABLE_BLEND
		qglColor4f(1,1,1,1);
		GLSTATE_ENABLE_ALPHATEST
	}
	//end Knightmare

	if (gl->paletted) {
			if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
                   GLSTATE_ENABLE_ALPHATEST
	}
	else
	{
		GLSTATE_ENABLE_ALPHATEST
		GLSTATE_DISABLE_BLEND
	}

}
// End
/*
Added alpha_pic -Maniac
=====================
alpha_pic
=====================
*//*
qboolean alpha_pic(char *s)
{
	int i = 0, joku;
	char *alphapics[10] = {"anum_",
							"num_",
							"minius",
							"a_",
							"i_",
							"p_",
							"w_",
							"loading",
							"quit"};
	
	while ( alphapics[i] != NULL )
	{
		joku = strlen(alphapics[i]);
		if ( !strncmp(s, alphapics[i], joku) )
			return true;
		i++;
	}

	return false;
}*/

qboolean alpha_pic(char *s)
{
	int i = 0, joku;
	char *alphapics[12] = {"conchars",
							"scope",
							"m_",
							"ch1",
							"ch2",
							"ch3",
							"ch4",
							"ch5",
							"ch6",
							"ch7",
							"ch8"};
	
	while ( alphapics[i] != NULL )
	{
		joku = strlen(alphapics[i]);
		if ( !strncmp(s, alphapics[i], joku) )
			return false;
		i++;
	}

	return true;
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		//Added ignore cantfindpic -Maniac
		if(!ignorecantfindpic->value)
		{
			ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		}
		//End
		return;
	}
	if (scrap_dirty)
		Scrap_Upload ();

	//Changed -Maniac
	if (gl->paletted) {
			if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
                  GLSTATE_DISABLE_ALPHATEST
	}
	else
	{
			GLSTATE_DISABLE_ALPHATEST             // Idle: disable the alpha before mipping
		    GLSTATE_ENABLE_BLEND                  // Idle: enable blending for targa files
	}


	if(gl_hudtrans->value < 1 && (alpha_pic(pic)))
	{
		GLSTATE_DISABLE_ALPHATEST
		GL_Bind(gl->texnum);
		GL_TexEnv(GL_MODULATE);
		qglColor4f(1,1,1,gl_hudtrans->value);	
		GLSTATE_ENABLE_BLEND
		qglDepthMask(false);
	}
	else
	{
	GL_Bind (gl->texnum);
	}

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

	if(gl_hudtrans->value < 1 && (alpha_pic(pic)))
	{
		qglDepthMask (true);
		GL_TexEnv(GL_REPLACE);
		GLSTATE_DISABLE_BLEND
		qglColor4f(1,1,1,1);
		GLSTATE_ENABLE_ALPHATEST
	}


	if (gl->paletted) {
			if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )  && !gl->has_alpha)
                  GLSTATE_ENABLE_ALPHATEST
	}
	else
	{
		    GLSTATE_ENABLE_ALPHATEST
			GLSTATE_DISABLE_BLEND
	}
}
// End

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	image_t	*image;

	image = Draw_FindPic (pic);
	if (!image)
	{
		//Added ignore cantfindpic -Maniac
		if(!ignorecantfindpic->value)
		{
			ri.Con_Printf (PRINT_ALL, "Can't find pic: %s\n", pic);
		}
		//End
		return;
	}

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )  && !image->has_alpha)
		GLSTATE_DISABLE_ALPHATEST

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
		GLSTATE_ENABLE_ALPHATEST
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
		ri.Sys_Error (ERR_FATAL, "Draw_Fill: bad color");

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
	GLSTATE_ENABLE_BLEND
	qglDisable (GL_TEXTURE_2D);
	qglColor4f (0, 0, 0, 0.8);
	qglBegin (GL_QUADS);

	qglVertex2f (0,0);
	qglVertex2f (vid.width, 0);
	qglVertex2f (vid.width, vid.height);
	qglVertex2f (0, vid.height);

	qglEnd ();
	qglColor4f (1,1,1,1);
	qglEnable (GL_TEXTURE_2D);
	GLSTATE_DISABLE_BLEND
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
		hscale = rows * 0.00390625;
		trows = 256;
	}
	t = rows*hscale * 0.00390625;

	if ( !qglColorTableEXT )
	{
		unsigned *dest;

		for (i=0 ; i<trows ; i++)
		{
			row = (int)(i*hscale);
			if (row > rows)
				break;
			source = data + cols*row;
			dest = &image32[i*256];
			fracstep = cols*0x10000 *0.00390625;
			frac = fracstep >> 1;
			for (j=0 ; j<256 ; j++)
			{
				dest[j] = r_rawpalette[source[frac>>16]];
				frac += fracstep;
			}
		}

		qglTexImage2D (GL_TEXTURE_2D, 0, gl_tex_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image32);
	}
	else
	{
		unsigned char *dest;

		for (i=0 ; i<trows ; i++)
		{
			row = (int)(i*hscale);
			if (row > rows)
				break;
			source = data + cols*row;
			dest = &image8[i*256];
			fracstep = cols*0x10000 *0.00390625;
			frac = fracstep >> 1;
			for (j=0 ; j<256 ; j++)
			{
				dest[j] = source[frac>>16];
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
		GLSTATE_DISABLE_ALPHATEST

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
		GLSTATE_ENABLE_ALPHATEST
}

