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

"Nullsoft", "Winamp", and "Winamp3" are trademarks of Nullsoft, Inc.
*/
#include <float.h>

#include "../client/client.h"
#include "winquake.h"

#include "winamp.h"

typedef struct qwinamp_s
{
	HWND	 hWnd;
	qboolean isOK;
	char	 title[WA_TITLE_LENGHT];
	int		 track;
} qwinamp_t;

qwinamp_t mywinamp;

cvar_t	*cl_winampmessages;
cvar_t	*cl_winamp_dir;

void WA_GetWinAmp (void)
{

	mywinamp.hWnd = FindWindow( "Winamp v1.x", NULL);
	if (mywinamp.hWnd)
	{
		mywinamp.isOK = true;
		Com_Printf ("Winamp Integration Enabled\n");
	}
	else
	{
		// Winamp not running, or we couldn't find it
		mywinamp.hWnd = NULL;
		mywinamp.isOK = false;
		Com_Printf ("Winamp Integration Disabled\n");
	}
}

qboolean WA_Status(void)
{
	if (!mywinamp.isOK)
	{
		mywinamp.hWnd = FindWindow( "Winamp v1.x", NULL);
		if (mywinamp.hWnd)
		{
			mywinamp.isOK = true;
			return true;
		}
		mywinamp.hWnd = NULL;
		Com_Printf ("Winamp Integration Disabled\n");
		return false;
	}
	return true;
}
/*
===================
WA_SetVolume

Updates Winamp's volume to give value
===================
*/
void WA_SetVolume (void)
{
	int vol, percent;

	if (!WA_Status())
		return;
	
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: winampvolume <value>\n" );
		return;
    }

	percent = atof(Cmd_Args());

	vol = (percent * 0.01) * 255;
	clamp(vol, 0, 255);

	SendMessage(mywinamp.hWnd, WM_USER, vol, 122);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp volume set to %i%%\n", percent);
}

/*
===================
WA_ToggleShuffle
Toggles suffle mode
===================
*/
void WA_ToggleShuffle (void)
{
	int ret;

	if (!WA_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40023, 0);

	ret = SendMessage(mywinamp.hWnd, WM_USER, 0, 250);
	if (ret == 1)
		Com_Printf ("Winamp Shuffle is ON\n");
	else
		Com_Printf ("Winamp Shuffle OFF\n");
}

/*
===================
WA_ToggleRepeat
Toggles repeat mode
===================
*/
void WA_ToggleRepeat (void)
{
	int ret;

	if (!WA_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40022, 0);

	ret = SendMessage(mywinamp.hWnd, WM_USER, 0, 251);
	if (ret == 1)
		Com_Printf ("Winamp Repeat is ON\n");
	else
		Com_Printf ("Winamp Repeat is OFF\n");
}

/*
===================
WA_VolumeUp
Increase winamp volume by 1%
===================
*/
void WA_VolumeUp (void)
{
	if (!WA_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40058, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Volume Increased by 1%%\n");
}

void WA_VolumeDown (void)
{
	if (!WA_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40059, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Volume Decreased by 1%%\n");
}
/*
===================
WA_GetPlayListInfo
===================
*/
void WA_GetPlayListInfo (int *current, int *length)
{
	if (!mywinamp.isOK)
		return;

	if (length)
		*length = SendMessage (mywinamp.hWnd, WM_USER, 0, 124);

	if (current)
		*current = SendMessage (mywinamp.hWnd, WM_USER, 0, 125);
}

qboolean WA_GetTrackTime(int *elapsed, int *total) {
	int ret1, ret2;

	if (!mywinamp.isOK)
		return false;
	if (elapsed && (ret1 = SendMessage(mywinamp.hWnd, WM_USER, 0, 105)) == -1)
		return false;
	if (total && (ret2 = SendMessage(mywinamp.hWnd, WM_USER, 1, 105)) == -1)
		return false;

	if(elapsed)
		*elapsed = ret1 / 1000;

	if(total)
		*total = ret2;

	return true;
}
/*
===================
WA_PlayTrack
Start playing given track
===================
*/
qboolean WA_PlayTrack (int num)
{
	int lenght, ret;

	WA_GetPlayListInfo(NULL, &lenght);
		
	if(num > lenght)
	{
		Com_Printf("Winamp: playlist got only %i tracks\n", lenght);
        return false;
	}

	ret = SendMessage(mywinamp.hWnd, WM_USER, 1, 104); //status of playback. ret 1 is playing. ret 3 is paused.
	if(ret == 3) //in paused case it just resume it so we need to stop it
		SendMessage(mywinamp.hWnd, WM_COMMAND, 40047, 0);

	SendMessage(mywinamp.hWnd, WM_USER, num-1, 121);	//Sets position in playlist
	SendMessage(mywinamp.hWnd, WM_COMMAND, 40045, 0);	//Play it

	return true;
}
/*
===================
WA_Play
===================
*/
void WA_Play (void)
{
	int track;

	if (!WA_Status())
		return;

	if (Cmd_Argc() == 2)
	{
		track = atoi(Cmd_Args());
        
		if(!WA_PlayTrack (track))
			return;

		if (cl_winampmessages->integer)
			Com_Printf ("Winamp Play track %i\n", track);

		return;
    }
	

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40045, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Play\n");
}

/*
===================
WA_Play
===================
*/
void WA_Stop (void)
{
	if (!WA_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40047, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Stop\n");
}

/*
===================
WA_Play
===================
*/
void WA_Pause (void)
{
	if (!WA_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40046, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Pause\n");
}

/*
===================
WA_NextTrack
===================
*/
void WA_NextTrack (void)
{
	if (!WA_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40048, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Next Track\n");
}

/*
===================
WA_PreviousTrack
===================
*/
void WA_PreviousTrack (void)
{
	if (!WA_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40044, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Previous Track\n");
}

/*
===================
WA_SongTitle

Returns current song title
===================
*/
char *WA_SongTitle (void)
{ 
   static char title[WA_TITLE_LENGHT]; 
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
WA_Title
===================
*/
void WA_Title (void)
{
	char *songtitle;
	int	 total;

	if (!WA_Status())
		return;

	if(!WA_GetTrackTime(NULL, &total))
		return;

	songtitle = WA_SongTitle();
	if (!songtitle)
		return;

	Com_Printf ("%sWinamp Title: %s%s %s[%i:%02i]\n", S_COLOR_CYAN, S_COLOR_YELLOW, songtitle, S_COLOR_CYAN, total / 60, total % 60);
}

/*
===================
WA_SongInfo
===================
*/
void WA_SongInfo (void)
{
	char *songtitle;
	int total, elapsed, remaining, samplerate, bitrate;

	if (!WA_Status())
		return;

	if(!WA_GetTrackTime(&elapsed, &total))
		return;

	songtitle = WA_SongTitle();
	if (!songtitle)
		return;

	remaining = total - elapsed;

	samplerate = SendMessage(mywinamp.hWnd, WM_USER, 0, 126);
	bitrate = SendMessage(mywinamp.hWnd, WM_USER, 1, 126);

	Com_Printf ("WinAmp current song info:\n");
	Com_Printf ("Name: %s Length: %i:%02i\n", S_COLOR_CYAN, S_COLOR_YELLOW, songtitle, S_COLOR_CYAN, S_COLOR_YELLOW, total / 60, total % 60);
	Com_Printf ("Elapsed: %i:%02i Remaining: %i:%02i Bitrate: %ikbps Samplerate: %ikHz\n",
				elapsed / 60, elapsed % 60, remaining / 60, remaining % 60, bitrate, samplerate);
}

extern int FS_filelength (FILE *f);
/*
===================
WA_GetPlaylist

===================
*/
long WA_GetPlaylist (char **buf)
{
	FILE			*file;
	char			path[512];
	int				pathlength;
	long			filelength;

	if (!WA_Status())
		return -1;

	SendMessage (mywinamp.hWnd, WM_USER, 0, 120);
	Q_strncpyz (path, cl_winamp_dir->string, sizeof (path));
	pathlength = strlen(path);

	if (pathlength && (path[pathlength - 1] == '\\' || path[pathlength - 1] == '/'))
		path[pathlength - 1] = 0;

	strcat(path, "/winamp.m3u");
	file = fopen (path, "rb");
	if (!file) {
		Com_Printf("Cant find winamp in \"%s\", use cl_winamp_dir to change dir\n", path);
		return -1;
	}
	filelength = FS_filelength (file);

	*buf = malloc (filelength);
	if (filelength != fread (*buf, 1,  filelength, file))
	{
		free (*buf);
		fclose (file);
		return -1;
	}

	fclose (file);

	return filelength;
}

int WA_ParsePlaylist_EXTM3U(char *playlist_buf, unsigned int length, wa_tracks_t *song, char *filter)
{
	int skip = 0, playlist_size = 0;
	char *s, *t, *buf, *line;
	char track[WA_TITLE_LENGHT];
	int tracknum = 0;

	buf = playlist_buf;
	while (playlist_size < WA_MAX_TITLES) {
		for (s = line = buf; s - playlist_buf < length && *s && *s != '\n' && *s != '\r'; s++)
			;
		if (s - playlist_buf >= length)
			break;
		*s = 0;
		buf = s + 2;
		if (skip || !strncmp(line, "#EXTM3U", 7)) {
			skip = 0;
			continue;
		}
		if (!strncmp(line, "#EXTINF:", 8)) {
			if (!(s = strstr(line, ",")) || ++s - playlist_buf >= length) 
				break;
			
			skip = 1;
			goto print;
		}
	
		for (s = line + strlen(line); s > line && *s != '\\' && *s != '/'; s--)
			;
		if (s != line)
			s++;
	
		if ((t = strrchr(s, '.')) && t - playlist_buf < length)
			*t = 0;		
	
		for (t = s + strlen(s) - 1; t > s && *t == ' '; t--)
			*t = 0;

print:
		tracknum++;
		Q_strncpyz (track, s, sizeof (track));
		Q_strlwr(track);
		if(!strstr(track, filter))
			continue;

		song->num[playlist_size] = tracknum;
		if (strlen(s) > WA_TITLE_LENGHT)
			s[WA_TITLE_LENGHT] = 0;
		song->name[playlist_size++] = strdup(s);
	}
	return playlist_size;
}



/*
===================
WA_PrintPlaylist

===================
*/
void WA_PrintPlaylist (void)
{
	char *playlist_buf;
	unsigned int length;
	int i, playlist_size, current;
	char *filter;
	wa_tracks_t song;

	if (!WA_Status())
		return;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: %s <song name>\n", Cmd_Argv(0));
		return;
    }

	if ((length = WA_GetPlaylist(&playlist_buf)) == -1)
		return;

	filter = Cmd_Args();
	Q_strlwr(filter);

	WA_GetPlayListInfo (&current, NULL);
	playlist_size = WA_ParsePlaylist_EXTM3U(playlist_buf, length, &song, filter);
	
	for (i = 0; i < playlist_size; i++) {
		Com_Printf("%s%3d. %s\n", song.num[i] == current+1 ? "\x02" : "", song.num[i], song.name[i]);
		free(&song.num[i]);
	}
	free (playlist_buf);
}

/*
===================
WA_Restart
===================
*/
void WA_Restart (void)
{
	WA_GetWinAmp();
}

/*
===================
WA_Init
===================
*/
void WA_Init (void)
{
	cl_winampmessages = Cvar_Get("cl_winampmessages", "1", CVAR_ARCHIVE);
	cl_winamp_dir = Cvar_Get("cl_winamp_dir", "C:/Program Files/Winamp", CVAR_ARCHIVE);

	Cmd_AddCommand ( "winampnext", WA_NextTrack );
	Cmd_AddCommand ( "winamppause", WA_Pause );
	Cmd_AddCommand ( "winampplay", WA_Play );
	Cmd_AddCommand ( "winampprev", WA_PreviousTrack );
	Cmd_AddCommand ( "winampstop", WA_Stop );
	Cmd_AddCommand ( "winampvolup", WA_VolumeUp );
	Cmd_AddCommand ( "winampvoldown", WA_VolumeDown );
	Cmd_AddCommand ( "winamprestart", WA_Restart );
	Cmd_AddCommand ( "winampshuffle", WA_ToggleShuffle );
	Cmd_AddCommand ( "winamprepeat", WA_ToggleRepeat );
	Cmd_AddCommand ( "winampvolume", WA_SetVolume );
	Cmd_AddCommand ( "winamptitle", WA_Title );
	Cmd_AddCommand ( "winampsonginfo", WA_SongInfo );
	Cmd_AddCommand ( "winampsearch", WA_PrintPlaylist );


	// Get WinAmp
	WA_GetWinAmp();
}

/*
===================
WA_Shutdown
===================
*/
void WA_Shutdown (void)
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
WA_Frame
===================
*/
void WA_Frame (void)
{
	char *songtitle;
	int total, track;

	if (!mywinamp.isOK || !cl_winampmessages->integer)
		return;

	WA_GetPlayListInfo (&track, NULL);
	if(track == mywinamp.track)
		return;

	if(!WA_GetTrackTime(NULL, &total))
		return;

	songtitle = WA_SongTitle();
	if (!songtitle)
		return;

	mywinamp.track = track;

	Com_Printf ("%sWinamp Title: %s%s %s[%i:%02i]\n", S_COLOR_CYAN, S_COLOR_YELLOW, songtitle, S_COLOR_CYAN, total / 60, total % 60);
}
