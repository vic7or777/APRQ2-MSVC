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
// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// Quake refresh engine.

#include <assert.h>
#include <dlfcn.h> // ELF dl loader
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "../client/client.h"


// Console variables that we need to access from this module
cvar_t		*vid_gamma;
cvar_t		*vid_xpos;			// X coordinate of window position
cvar_t		*vid_ypos;			// Y coordinate of window position
cvar_t		*vid_fullscreen;

// Global variables used internally by this module
viddef_t	viddef;				// global video state; used by other modules

qboolean vid_restart = false;
qboolean vid_active = false;

#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )


//==========================================================================

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart_f (void)
{
	vid_restart = true;
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
	const char *description;
	int         width, height;
	int         mode;
} vidmode_t;

vidmode_t vid_modes[] =
{
	{ "Mode 0: 320x240",	 320,  240,	0  },
	{ "Mode 1: 400x300",	 400,  300,	1  },
	{ "Mode 2: 512x384",	 512,  384,	2  },
	{ "Mode 3: 640x480",	 640,  480,	3  },
	{ "Mode 4: 800x600",	 800,  600,	4  },
	{ "Mode 5: 960x720",	 960,  720,	5  },
	{ "Mode 6: 1024x768",	1024,  768,	6  },
	{ "Mode 7: 1152x864",	1152,  864,	7  },
	{ "Mode 8: 1280x960",	1280,  960,	8  },
	{ "Mode 9: 1600x1200",	1600, 1200,	9  },
	{ "Mode 10: 2048x1536",	2048, 1536,	10 },
	{ "Mode 11: 1024x480",	1024,  480,	11 },
	{ "Mode 12: 1280x768",	1280,  768,	12 },
	{ "Mode 13: 1280x1024",	1280, 1024,	13 }
};

qboolean VID_GetModeInfo( int *width, int *height, int mode )
{
	if ( mode < 0 || mode >= VID_NUM_MODES )
		return false;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return true;
}

/*
** VID_NewWindow
*/
void VID_NewWindow ( int width, int height)
{
	viddef.width  = width;
	viddef.height = height;
}


/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to 
update the rendering DLL and/or video mode to match.
============
*/
void VID_CheckChanges (void)
{

	if ( vid_restart )
	{
		S_StopAllSounds();

		/*
		** refresh has changed
		*/
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cls.disable_screen = true;

		VID_Shutdown();

		Com_Printf( "--------- [Loading Renderer] ---------\n" );

		Swap_Init ();

		if ( R_Init( 0, 0 ) == -1 )
		{
			R_Shutdown();
			vid_active = false;
			Com_Error (ERR_FATAL, "Couldn't initialize renderer!");
		}

		Com_Printf( "------------------------------------\n");

		vid_restart = false;
		vid_active = true;
		cls.disable_screen = false;
	}

}

/*
============
VID_Init
============
*/
void VID_Init (void)
{
	/* Create the video variables so we know how to start the graphics drivers */
	// if DISPLAY is defined, try X
	vid_xpos = Cvar_Get ("vid_xpos", "3", CVAR_ARCHIVE);
	vid_ypos = Cvar_Get ("vid_ypos", "22", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );

	/* Add some console commands that we want to handle */
	Cmd_AddCommand ("vid_restart", VID_Restart_f);

	/* Disable the 3Dfx splash screen */
	putenv("FX_GLIDE_NO_SPLASH=0");
		
	/* Start the graphics mode and load refresh DLL */
	vid_restart = true;
	vid_active = false;
	VID_CheckChanges();
}

/*
============
VID_Shutdown
============
*/
void VID_Shutdown (void)
{
	if ( vid_active )
	{
		R_Shutdown ();
		vid_active = false;
	}
}

