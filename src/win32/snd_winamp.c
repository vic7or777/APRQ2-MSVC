/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2003 Bleeding Eye Studios

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

"Nullsoft", "Winamp", and "Winamp3" are trademarks of Nullsoft, Inc.
*/
#include <float.h>

#include "../client/client.h"
#include "../client/sound.h"
#include "winquake.h"

#define WINAMP_TITLE_LENGTH	128		// Length of title string
#define WINAMP_VERSION_HEX	0x2081	// Version of winamp this code works with
#define WINAMP_VERSION_STR	"2.81"	// String representation of above version

typedef struct qwinamp_s
{
	HWND	 hWnd;
	qboolean isOK;
	char	 title[WINAMP_TITLE_LENGTH];
	int		 track;
	int		 version;
} qwinamp_t;

qwinamp_t mywinamp;

cvar_t	*cl_winampmessages;
cvar_t	*cl_winamp_dir;

void S_WinAmp_GetWinAmp (void)
{
	mywinamp.hWnd = FindWindow( "Winamp v1.x", NULL);
	if (mywinamp.hWnd)
	{
		mywinamp.version = SendMessage(mywinamp.hWnd, WM_USER, 0, 0);
		mywinamp.isOK = true;

		if (mywinamp.version < WINAMP_VERSION_HEX)
			Com_Printf ("Nullsoft Winamp found...  old version. Some features may not work.\n");

		Com_Printf ("Winamp Integration Enabled\n");
	}
	else
	{
		// Winamp not running, or we couldn't find it
		mywinamp.hWnd = NULL;
		mywinamp.isOK = false;
		mywinamp.version = 0;
		Com_Printf ("Winamp Integration Disabled\n");
	}
}

/*
===================
S_WinAmp_SetVolume

Updates Winamp's volume to give value
===================
*/
void S_WinAmp_SetVolume (void)
{
	int vol, percent;
	char *args;

	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled");
		return;
	}
	
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: winampvolume <value>\n" );
		return;
    }

	args = Cmd_Args();
	percent = atof(args);

	// Command argument is percent volume, which needs to be converted to absolute volume (0-255)
	vol = (percent * 0.01) * 255;

	// Clamp volume
	clamp(vol, 0, 255);

	SendMessage(mywinamp.hWnd, WM_USER, vol, 122);
	if (cl_winampmessages->value)
		Com_Printf ("Winamp volume set to %i%%\n", percent);
}

/*
===================
S_WinAmp_ToggleShuffle

Toggles suffle mode
===================
*/
void S_WinAmp_ToggleShuffle (void)
{
	int ret;

	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40023, 0);

	ret = SendMessage(mywinamp.hWnd, WM_USER, 0, 250);
	if (ret == 1)
		Com_Printf ("Winamp Shuffle is ON\n");
	else
		Com_Printf ("Winamp Shuffle OFF\n");
}

/*
===================
S_WinAmp_ToggleRepeat

Toggles repeat mode
===================
*/
void S_WinAmp_ToggleRepeat (void)
{
	int ret;

	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40022, 0);

	ret = SendMessage(mywinamp.hWnd, WM_USER, 0, 251);
	if (ret == 1)
		Com_Printf ("Winamp Repeat is ON\n");
	else
		Com_Printf ("Winamp Repeat is OFF\n");
}

/*
===================
S_WinAmp_VolumeUp

Increase winamp volume by 1%
===================
*/
void S_WinAmp_VolumeUp (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40058, 0);
	if (cl_winampmessages->value)
		Com_Printf ("Winamp Volume Increased by 1%%\n");
}

/*
===================
S_WinAmp_VolumeDown

Decreased winamp volume by 1%
===================
*/
void S_WinAmp_VolumeDown (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40059, 0);
	if (cl_winampmessages->value)
		Com_Printf ("Winamp Volume Decreased by 1%%\n");
}

/*
===================
S_WinAmp_Play
===================
*/
void S_WinAmp_Play (void)
{
	int ret, track;
	char *args;

	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	if (Cmd_Argc() == 2)
	{
		args = Cmd_Args();
		track = atof(args);
        
		ret = SendMessage(mywinamp.hWnd, WM_USER, 1, 124);
		
		if(track > ret)
		{
			Com_Printf("Winamp: playlist got only %i tracks\n", ret);
            return;
		}

		SendMessage(mywinamp.hWnd, WM_USER, track-1, 121);	//Sets position in playlist
		SendMessage(mywinamp.hWnd, WM_COMMAND, 40045, 0);	//Play it

		if (cl_winampmessages->value)
			Com_Printf ("Winamp Play track %i\n", track);

		return;
    }
	

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40045, 0);
	if (cl_winampmessages->value)
		Com_Printf ("Winamp Play\n");
}

/*
===================
S_WinAmp_Play
===================
*/
void S_WinAmp_Stop (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40047, 0);
	if (cl_winampmessages->value)
		Com_Printf ("Winamp Stop\n");
}

/*
===================
S_WinAmp_Play
===================
*/
void S_WinAmp_Pause (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40046, 0);
	if (cl_winampmessages->value)
		Com_Printf ("Winamp Pause\n");
}

/*
===================
S_WinAmp_NextTrack
===================
*/
void S_WinAmp_NextTrack (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40048, 0);
	if (cl_winampmessages->value)
		Com_Printf ("Winamp Next Track\n");
}

/*
===================
S_WinAmp_PreviousTrack
===================
*/
void S_WinAmp_PreviousTrack (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40044, 0);
	if (cl_winampmessages->value)
		Com_Printf ("Winamp Previous Track\n");
}

/*
===================
S_WinAmp_SongTitle

Returns current song title
===================
*/
char *S_WinAmp_SongTitle (void)
{ 
   static char title[WINAMP_TITLE_LENGTH]; 
   char			*s; 

   GetWindowText(mywinamp.hWnd, title, sizeof(title)); 
   
   //cut out the crap from the title 
   if ((s = strrchr(title, '-')) && s > title) 
      *(s - 1) = 0;

   for (s = title + strlen(title) - 1; s > title; s--)
   { 
      if (*s == ' ') 
         *s = 0; 
      else 
         break; 
   }

   return title; 
}

/*
===================
S_WinAmp_Title
===================
*/
void S_WinAmp_Title (void)
{
	char *songtitle;
	int	 rawtime;

	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	rawtime = 	rawtime = SendMessage(mywinamp.hWnd, WM_USER, 1, 105);
	if(rawtime == -1)
		return;

	songtitle = S_WinAmp_SongTitle();
	if (!songtitle)
		return;

	Com_Printf ("%sWinamp Title: %s%s %s[%i:%02i]\n", S_COLOR_CYAN, S_COLOR_YELLOW, songtitle, S_COLOR_CYAN, rawtime / 60, rawtime % 60);
}

/*
===================
S_WinAmp_SongInfo
===================
*/
void S_WinAmp_SongInfo (void)
{
	char *songtitle;
	int rawtime, elapsed, remaining, samplerate, bitrate;

	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	rawtime = SendMessage(mywinamp.hWnd, WM_USER, 1, 105);
	if(rawtime == -1)
		return;

	songtitle = S_WinAmp_SongTitle();
	if (!songtitle)
		return;

	elapsed = SendMessage(mywinamp.hWnd, WM_USER, 0, 105) / 1000;
	remaining = rawtime - elapsed;

	samplerate = SendMessage(mywinamp.hWnd, WM_USER, 0, 126);
	bitrate = SendMessage(mywinamp.hWnd, WM_USER, 1, 126);

	Com_Printf ("%sWinAmp current song info:\n", S_COLOR_CYAN);
	Com_Printf ("%sName: %s%s%s Length: %s%i:%02i\n", S_COLOR_CYAN, S_COLOR_YELLOW, songtitle, S_COLOR_CYAN, S_COLOR_YELLOW, rawtime / 60, rawtime % 60);
	Com_Printf ("%sElapsed: %s%i:%02i%s Remaining: %s%i:%02i%s Bitrate: %s%ikbps%s Samplerate: %s%ikHz\n",
				S_COLOR_CYAN, S_COLOR_YELLOW, elapsed / 60, elapsed % 60,
				S_COLOR_CYAN, S_COLOR_YELLOW, remaining / 60, remaining % 60,
				S_COLOR_CYAN, S_COLOR_YELLOW, bitrate, S_COLOR_CYAN, S_COLOR_YELLOW, samplerate);
}

/*
===================
S_WinAmp_Playlist
===================
*/
void S_WinAmp_Playlist(void)
{
	FILE *f;
	char *filter, track[128], path[1024];
	int tracknum = 0, skip;

	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: %s <song name>\n", Cmd_Argv(0));
		return;
    }

	filter = Cmd_Args();

	SendMessage(mywinamp.hWnd, WM_USER, 0, 120); //Make a Winamp write winamp.m3u
	strcpy(path, cl_winamp_dir->string);

	f = fopen(va("%s/winamp.m3u", path), "r");
	if (!f)
	{
		Com_Printf("Cant find winamp in \"%s\", use cl_winamp_dir to change dir\n", path);
		return;
	}

	while (!feof(f)) {
		char line[128], *nl;

		// read a line
		fgets(line, sizeof(line), f);

		if(strncmp(line, "#EXTINF:", 8))
			continue;

		tracknum++;

		strncpy(track, line+8, sizeof(track) - 1);
		strlwr(track);
		strlwr(filter);

		if(!strstr(track, filter))
			continue;

		// overwrite new line characters with null
		nl = strchr(line, '\n');
		if (nl)
			*nl = '\0';

		skip = 0;
		nl = line+8;
		while( *(nl) != ',' && *(nl) != 0 )
		{
			nl++;
			skip++;
		}
		skip++;

		Com_Printf ("%4i. %s\n", tracknum, line+8+skip);
	}

	fclose(f);
}

/*
===================
S_WinAmp_Restart
===================
*/
void S_WinAmp_Restart (void)
{
	S_WinAmp_Shutdown();
	S_WinAmp_Init();
}

/*
===================
S_WinAmp_Init
===================
*/
void S_WinAmp_Init (void)
{
	cl_winampmessages = Cvar_Get("cl_winampmessages", "1", CVAR_ARCHIVE);
	cl_winamp_dir = Cvar_Get("cl_winamp_dir", "C:/Program Files/Winamp", CVAR_ARCHIVE);

	Cmd_AddCommand ( "winampnext", S_WinAmp_NextTrack );
	Cmd_AddCommand ( "winamppause", S_WinAmp_Pause );
	Cmd_AddCommand ( "winampplay", S_WinAmp_Play );
	Cmd_AddCommand ( "winampprev", S_WinAmp_PreviousTrack );
	Cmd_AddCommand ( "winampstop", S_WinAmp_Stop );
	Cmd_AddCommand ( "winampvolup", S_WinAmp_VolumeUp );
	Cmd_AddCommand ( "winampvoldown", S_WinAmp_VolumeDown );
	Cmd_AddCommand ( "winamprestart", S_WinAmp_Restart );
	Cmd_AddCommand ( "winampshuffle", S_WinAmp_ToggleShuffle );
	Cmd_AddCommand ( "winamprepeat", S_WinAmp_ToggleRepeat );
	Cmd_AddCommand ( "winampvolume", S_WinAmp_SetVolume );
	Cmd_AddCommand ( "winamptitle", S_WinAmp_Title );
	Cmd_AddCommand ( "winampsonginfo", S_WinAmp_SongInfo );
	Cmd_AddCommand ( "winampsearch", S_WinAmp_Playlist );

	// Get WinAmp
	S_WinAmp_GetWinAmp();
}

/*
===================
S_WinAmp_Shutdown
===================
*/
void S_WinAmp_Shutdown (void)
{
	Cmd_RemoveCommand ( "winampnext" );
	Cmd_RemoveCommand ( "winamppause" );
	Cmd_RemoveCommand ( "winampplay" );
	Cmd_RemoveCommand ( "winampprev" );
	Cmd_RemoveCommand ( "winampstop" );
	Cmd_RemoveCommand ( "winampvolup" );
	Cmd_RemoveCommand ( "winampvoldown" );
	Cmd_RemoveCommand ( "winamprestart" );
	Cmd_RemoveCommand ( "winampshuffle" );
	Cmd_RemoveCommand ( "winamprepeat" );
	Cmd_RemoveCommand ( "winampvolume" );
	Cmd_RemoveCommand ( "winamptitle" );
	Cmd_RemoveCommand ( "winampsonginfo" );
	Cmd_RemoveCommand ( "winampsearch" );
}

/*
===================
S_WinAmp_Frame
===================
*/
void S_WinAmp_Frame (void)
{
	char *songtitle;
	int rawtime, track;

	if (!mywinamp.isOK || !cl_winampmessages->value)
		return;

	track = SendMessage(mywinamp.hWnd, WM_USER, 1, 125);
	if(track == mywinamp.track)
		return;

	rawtime = SendMessage(mywinamp.hWnd, WM_USER, 1, 105);
	if(rawtime == -1)
		return;

	songtitle = S_WinAmp_SongTitle();
	if (!songtitle)
		return;

	mywinamp.track = track;

	Com_Printf ("%sWinamp Title: %s%s %s[%i:%02i]\n", S_COLOR_CYAN, S_COLOR_YELLOW, songtitle, S_COLOR_CYAN, rawtime / 60, rawtime % 60);
}
