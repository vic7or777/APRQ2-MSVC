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

/*
NeVo - Winamp Integration, version 2
Should work for all versions (1.x, 2.x)?
AFAIK Winamp3 ignores Windows Messages...
*/

cvar_t	*cl_winampverbose;

#define WINAMP_TITLE_LENGTH	128		// Length of title string
#define WINAMP_VERSION_HEX	0x2081	// Version of winamp this code works with
#define WINAMP_VERSION_STR	"2.81"	// String representation of above version

typedef struct qwinamp_s
{
	HWND	 hWnd;
	qboolean isOK;
	char	 title[WINAMP_TITLE_LENGTH];
	int		 trackminutes;
	int		 trackseconds;
	int		 version;
} qwinamp_t;

qwinamp_t mywinamp;

void S_WinAmp_GetWinAmp (void)
{
	mywinamp.hWnd = FindWindow( "Winamp v1.x", NULL);
	if (mywinamp.hWnd)
	{
		mywinamp.version = SendMessage(mywinamp.hWnd, WM_USER, NULL, 0);
		mywinamp.isOK = true;

		if (mywinamp.version < WINAMP_VERSION_HEX)
			Com_Printf ("Nullsoft Winamp found...\n   old version.\n Some features may not work.\n");
		else
			Com_Printf ("Nullsoft Winamp found...\n");

		Com_Printf ("Winamp Integration Enabled\n");
	}
	else
	{
		// Winamp not running, or we couldn't find it
		mywinamp.hWnd = NULL;
		mywinamp.isOK = false;
		mywinamp.version = NULL;
		Com_Printf ("Nullsoft Winamp not found...\n");
		Com_Printf ("Winamp Integration Disabled\n");
	}
}

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
	if (vol > 255)
		vol = 255;
	else if (vol < 0)
		vol = 0;

	PostMessage(mywinamp.hWnd, WM_USER, vol, 122);
	if (cl_winampverbose->value)
	{
		Com_Printf ("Winamp volume set to %i\n", percent);
	}
}

// Fuh
char *S_WinAmp_SongTitle (void)
{ 
   static char title[WINAMP_TITLE_LENGTH]; 
   char *s; 

   GetWindowText(mywinamp.hWnd, title, sizeof(title)); 
   
   //cut out the crap from the title 
   if ((s = strrchr(title, '-')) && s > title) 
      *(s - 1) = 0; 
   for (s = title + strlen(title) - 1; s > title; s--) { 
      if (*s == ' ') 
         *s = 0; 
      else 
         break; 
   } 
   return title; 
}

void S_WinAmp_SongLength (void)
{
	int rawtime;

	if (!mywinamp.isOK)
		return;

	rawtime = SendMessage(mywinamp.hWnd, WM_USER, 1, 105);
	mywinamp.trackminutes = rawtime / 60;
	mywinamp.trackseconds = rawtime % 60;
}

void S_WinAmp_ToggleShuffle (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	PostMessage(mywinamp.hWnd, WM_COMMAND, 40023, 0);
	if (cl_winampverbose->value)
	{
		Com_Printf ("Winamp Shuffle Toggled\n");
	}
}

void S_WinAmp_ToggleRepeat (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	PostMessage(mywinamp.hWnd, WM_COMMAND, 40022, 0);
	if (cl_winampverbose->value)
	{
		Com_Printf ("Winamp Repeat Toggled\n");
	}
}

void S_WinAmp_VolumeUp (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	PostMessage(mywinamp.hWnd, WM_COMMAND, 40058, 0);
	if (cl_winampverbose->value)
		Com_Printf ("Winamp Volume Up\n");
}

void S_WinAmp_VolumeDown (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	PostMessage(mywinamp.hWnd, WM_COMMAND, 40059, 0);
	if (cl_winampverbose->value)
		Com_Printf ("Winamp Volume Down\n");
}

void S_WinAmp_Play (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	PostMessage(mywinamp.hWnd, WM_COMMAND, 40045, 0);
	if (cl_winampverbose->value)
		Com_Printf ("Winamp Play\n");
}

void S_WinAmp_Stop (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	// Fade out and stop
	PostMessage(mywinamp.hWnd, WM_COMMAND, 40147, 0);
	if (cl_winampverbose->value)
		Com_Printf ("Winamp Stop\n");
}

void S_WinAmp_Pause (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	PostMessage(mywinamp.hWnd, WM_COMMAND, 40046, 0);
	if (cl_winampverbose->value)
		Com_Printf ("Winamp Pause\n");
}

void S_WinAmp_NextTrack (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	PostMessage(mywinamp.hWnd, WM_COMMAND, 40048, 0);
	if (cl_winampverbose->value)
		Com_Printf ("Winamp Next Track\n");
}

void S_WinAmp_PreviousTrack (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	PostMessage(mywinamp.hWnd, WM_COMMAND, 40044, 0);
	if (cl_winampverbose->value)
		Com_Printf ("Winamp Previous Track\n");
}


void S_WinAmp_Restart (void)
{
	S_WinAmp_Shutdown();
	S_WinAmp_Init();
}

void S_WinAmp_ConsoleTitle (void)
{
	if (!mywinamp.isOK)
	{
		Com_Printf ("Winamp Integration Disabled\n");
		return;
	}

	if (!mywinamp.title)
		return;

	Com_Printf ("Winamp Current Title: %s [%i:%2i]\n", mywinamp.title, mywinamp.trackminutes, mywinamp.trackseconds);
}

/*void S_WinAmp_Command (void)
{
	char *command;

	if (Cmd_Argc() != 2 && Cmd_Argc() != 3)
	{
            Com_Printf("Usage: Winamp command\n" );
            return;
    }

	command = Cmd_Argv(1);
	if (Q_strcasecmp(command, "next") == 0) { S_WinAmp_NextTrack(); }
	else if (Q_strcasecmp(command, "previous") == 0) { S_WinAmp_PreviousTrack; }
	else if (Q_strcasecmp(command, "play") == 0) { S_WinAmp_Play(); }
	else if (Q_strcasecmp(command, "pause") == 0) { S_WinAmp_Pause(); }
	else if (Q_strcasecmp(command, "stop") == 0) { S_WinAmp_Stop(); }
	else if (Q_strcasecmp(command, "volup") == 0) { S_WinAmp_VolumeUp(); }
	else if (Q_strcasecmp(command, "voldown") == 0) { S_WinAmp_VolumeDown(); }
	else if (Q_strcasecmp(command, "shuffle") == 0) { S_WinAmp_ToggleShuffle(); }
	else if (Q_strcasecmp(command, "repeat") == 0) { S_WinAmp_ToggleRepeat(); }
	else if (Q_strcasecmp(command, "title") == 0) { S_WinAmp_ConsoleTitle(); }
	else if (Q_strcasecmp(command, "restart")== 0) { S_WinAmp_Restart(); }
	else if (Q_strcasecmp(command, "help") == 0)
	{
            Com_Printf("Usage: Winamp <command> [param]\n" );
			Com_Printf("===============\n" );
			Com_Printf("    help: displays this help screen\n" );
			Com_Printf("    exit: exits WinAMP if running\n" );
			Com_Printf("    play: plays the current or otherwise specified playlist entry\n" );
			Com_Printf("    stop: stops the playback\n" );
			Com_Printf("   pause: pauses the playback of the currently playing song\n" );
			Com_Printf("  resume: resumes the playback of the paused song\n" );
			Com_Printf("    next: plays the next playlist entry\n" );
			Com_Printf("previous: plays the previous playlist entry\n" );
			Com_Printf(" shuffle: toggles shuffle on/off\n" );
			Com_Printf("  repeat: toggles repeat on/off\n" );
			Com_Printf("   title: returns the currently loaded track's title\n" );
			Com_Printf("  volume: <value> updates WinAMP's volume to the value of given value\n" );
			Com_Printf("   volup: increases WinAMP's volume by 1%\n" );
			Com_Printf(" voldown: decreases WinAMP's volume by 1%\n" );
	}
	else if (Q_strcasecmp(command, "volume") == 0)
	{
				if (Cmd_Argc() != 3)
				{
			        Com_Printf("Usage: Winamp volume <value>\n" );
					return;
				}
				S_WinAmp_SetVolume(Cmd_Argv(2));
	}
	else
	{		
		Com_Printf("Usage: Winamp <command>. More info type \"winamp help\".\n" );
		return;
	}
}*/

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
	//Cmd_RemoveCommand ( "winamp" );
}

void S_WinAmp_Init (void)
{
	cl_winampverbose = Cvar_Get("cl_winampverbose", "1", CVAR_ARCHIVE);

	// Get WinAmp
	S_WinAmp_GetWinAmp();

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
	Cmd_AddCommand ( "winamptitle", S_WinAmp_ConsoleTitle );
	//Cmd_AddCommand ( "winamp", S_WinAmp_Command );
}

void S_WinAmp_Frame (void)
{
	char *tempstr;

	if (!mywinamp.isOK)
		return;

	tempstr = S_WinAmp_SongTitle();

	if (!strcmp(tempstr, mywinamp.title))
		return;
	else
		strcpy(mywinamp.title, tempstr);

	S_WinAmp_SongLength();
	if (cl_winampverbose->value)
		S_WinAmp_ConsoleTitle();
}