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

#define WA_MAX_TITLES 1024
#define WA_TITLE_LENGHT 128

typedef struct
{
	int num[WA_MAX_TITLES];
	char *name[WA_MAX_TITLES];

} wa_tracks_t;

typedef struct
{
	int track;
	int total;
	char name[WA_TITLE_LENGHT];

} wa_track_t;

void WA_GetPlayListInfo (int *current, int *length);
qboolean WA_GetTrackTime(int *elapsed, int *total);
qboolean WA_PlayTrack (int num);
char *WA_SongTitle (void);
long WA_GetPlaylist (char **buf);
int WA_ParsePlaylist_EXTM3U(char *playlist_buf, unsigned int length, wa_tracks_t *song, char *filter);

