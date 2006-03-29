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
// r_misc.c

#include "gl_local.h"

/*
==================
R_InitParticleTexture
==================
*/
static const byte	dottexture[8][8] =
{
	{0,0,0,0,0,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0}
};

void R_InitParticleTexture (void)
{
	int		x,y;
	byte	data[8][8][4];
	byte	data1[16][16][4];
	int		dx2, dy, d;

	//
	// particle texture
	//

	for (x = 0; x < 16; x++) {
		dx2 = x - 8;
		dx2 *= dx2;
		for (y = 0; y < 16; y++) {
			dy = y - 8;
			d = 255 - 4 * (dx2 + (dy * dy));
			if (d <= 0) {
				d = 0;
				data1[y][x][0] = 0;
				data1[y][x][1] = 0;
				data1[y][x][2] = 0;
			} else {
				data1[y][x][0] = 255;
				data1[y][x][1] = 255;
				data1[y][x][2] = 255;
			}

			data1[y][x][3] = (byte) d;
		}
	}

	//r_particletexture = GL_FindImage("pics/particle.tga",it_sprite);
	//if(!r_particletexture)
		r_particletexture = GL_LoadPic ("***particle***", (byte *)data1, 16, 16, 0, 32, 0);

	r_shelltexture = GL_FindImage("pics/shell.tga", it_pic);
	if(!r_shelltexture)
		r_shelltexture = r_particletexture;

	//
	// also use this for bad textures, but without alpha
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = dottexture[x&3][y&3]*255;
			data[y][x][1] = 0; // dottexture[x&3][y&3]*255;
			data[y][x][2] = 0; //dottexture[x&3][y&3]*255;
			data[y][x][3] = 255;
		}
	}
	r_notexture = GL_LoadPic ("***r_notexture***", (byte *)data, 8, 8, it_wall, 32, 0);

	r_caustictexture = GL_FindImage("pics/caustic.png", it_wall);
	if(!r_caustictexture)
		r_caustictexture = r_notexture;

	r_bholetexture = GL_FindImage("pics/bullethole.png", it_sprite);
	if(!r_bholetexture)
		r_bholetexture = r_notexture;

}

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 
qboolean WriteJPG( const char *name, byte *buffer, int width, int height, int quality );
qboolean WriteTGA( const char *name, byte *buffer, int width, int height );
/* 
================== 
GL_ScreenShot_f
================== 
*/  
void GL_ScreenShot_f (void) 
{
	FILE	*f;
	byte	*buffer;
	int		i;
	char	picname[80], checkname[MAX_OSPATH];
	char	date[32], map[32] = "\0";
	time_t	clock;
	qboolean jpg = false;

	if(CL_Mapname()[0])
		Com_sprintf(map, sizeof(map), "_%s", CL_Mapname());

	if(!Q_stricmp( Cmd_Argv( 0 ), "screenshotjpg" ))
		jpg = true;


    // Create the scrnshots directory if it doesn't exist
    Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot", FS_Gamedir());
    Sys_Mkdir (checkname);

	if(Cmd_Argc() == 1)
	{
		time( &clock );
		strftime( date, sizeof(date), "%Y-%m-%d_%H-%M", localtime(&clock));

		// Find a file name to save it to
		for (i=0 ; i<100 ; i++)
		{
			if(jpg)
				Com_sprintf (picname, sizeof(picname), "%s%s-%02i.jpg", date, map, i);
			else
				Com_sprintf (picname, sizeof(picname), "%s%s-%02i.tga", date, map, i);
			Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot/%s", FS_Gamedir(), picname);
			f = fopen (checkname, "rb");
			if (!f)
				break;	// file doesn't exist
			fclose (f);
		}
		if (i == 100)
		{
			Com_Printf ("GL_ScreenShot_f: Couldn't create a file\n"); 
			return;
		}
	}
	else
	{
			if(jpg)
				Com_sprintf (picname, sizeof(picname), "%s.jpg", Cmd_Argv( 1 ));
			else
				Com_sprintf (picname, sizeof(picname), "%s.tga", Cmd_Argv( 1 ));

			Com_sprintf( checkname, sizeof(checkname), "%s/scrnshot/%s", FS_Gamedir(), picname );
	}

	if( jpg ) {
		buffer = Z_TagMalloc(vid.width * vid.height * 3, TAGMALLOC_RENDER_SCRSHOT);
		qglReadPixels( 0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

		if( WriteJPG(checkname, buffer, vid.width, vid.height, gl_screenshot_quality->integer) )
			Com_Printf( "Wrote %s\n", picname );
	} else {
		buffer = Z_TagMalloc(vid.width * vid.height * 3 + 18, TAGMALLOC_RENDER_SCRSHOT);
		memset (buffer, 0, 18);
		qglReadPixels( 0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18 ); 

		if( WriteTGA(checkname, buffer, vid.width, vid.height) )
			Com_Printf( "Wrote %s\n", picname );
	}

	Z_Free( buffer );
} 

/* 
================== 
GLAVI_ReadFrameData - Grabs a frame for exporting to AVI EXPORT
By Robert 'Heffo' Heffernan
================== 
*/
#ifdef AVI_EXPORT
void GLAVI_ReadFrameData (byte *buffer)
{
	if(!buffer)
		return;

	qglReadPixels(0, 0, vid.width, vid.height, GL_BGR_EXT, GL_UNSIGNED_BYTE, buffer);
}
#endif
/*
** GL_Strings_f
*/
void GL_Strings_f( void )
{
	Com_Printf ("GL_VENDOR: %s\n", gl_config.vendor_string );
	Com_Printf ("GL_RENDERER: %s\n", gl_config.renderer_string );
	Com_Printf ("GL_VERSION: %s\n", gl_config.version_string );
	Com_Printf ("GL_EXTENSIONS: %s\n", gl_config.extensions_string );
}

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState( void )
{
	qglClearColor (1, 0.2f, 0, 1);
	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.666f);

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglDisable (GL_BLEND);

	qglDisable(GL_FOG);

	qglColor4fv(colorWhite);

	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	qglShadeModel(GL_SMOOTH);
	qglDepthMask(GL_TRUE);

	GL_TextureMode( gl_texturemode->string );
	GL_TextureAlphaMode( gl_texturealphamode->string );
	GL_TextureSolidMode( gl_texturesolidmode->string );

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_TexEnv( GL_REPLACE );

	if ( gl_ext_pointparameters->integer && qglPointParameterfEXT )
	{
		float attenuations[3];

		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;

		qglEnable( GL_POINT_SMOOTH );
		qglPointParameterfEXT( GL_POINT_SIZE_MIN_EXT, gl_particle_min_size->value );
		qglPointParameterfEXT( GL_POINT_SIZE_MAX_EXT, gl_particle_max_size->value );
		qglPointParameterfvEXT( GL_DISTANCE_ATTENUATION_EXT, attenuations );
	}

	if ( qglColorTableEXT && gl_ext_palettedtexture->integer )
	{
		qglEnable( GL_SHARED_TEXTURE_PALETTE_EXT );

		GL_SetTexturePalette( d_8to24table );
	}

	GL_UpdateSwapInterval();
}

void GL_UpdateSwapInterval( void )
{
	if ( gl_swapinterval->modified )
	{
		gl_swapinterval->modified = false;

#ifdef _WIN32
		if ( !gl_state.stereo_enabled ) 
		{
			if ( qwglSwapIntervalEXT )
				qwglSwapIntervalEXT( gl_swapinterval->integer );
		}
#endif
	}
}
