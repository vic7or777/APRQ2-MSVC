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
#ifdef _WIN32
#include "ui_local.h"
#include "../win32/winamp.h"
/*
=======================================================================

WINAMP MENU

=======================================================================
*/
#define WAM_TITLE "-[ Winamp Menu ]-"

typedef struct m_tracks_s {
	menuframework_s	menu;
	menulist_s		list;
	menufield_s		filter;

	wa_tracks_t		track;
} m_tracks_t;

static m_tracks_t	m_tracks;
static int			track_count = 0;
static char			*m_filter = "";
static wa_track_t	current;

static void WA_MenuDraw( menuframework_s *self ) {

	char *songtitle;
	int track, total;
	char song_print[WA_TITLE_LENGHT];

	DrawString((viddef.width - (strlen(WAM_TITLE)*8))>>1, 10, WAM_TITLE);

	Menu_Draw( self );

	WA_GetPlayListInfo (&track, NULL);
	if(track != current.track)
	{
		if(WA_GetTrackTime(NULL, &total))
		{
			songtitle = WA_SongTitle();
			if (songtitle)
			{
				Q_strncpyz(current.name, songtitle, sizeof(current.name));
				current.track = track;
				current.total = total;
			}
		}
	}
	Com_sprintf (song_print, sizeof(song_print), "Current: %s [%i:%02i]\n", current.name, current.total / 60, current.total % 60);
	DrawAltString (20, 20, song_print);
}

static void Tracks_Free( void ) {
	int i;

	for( i=0 ; i<track_count; i++ ) {
		free( m_tracks.track.name[i] );
	}
	memset(&m_tracks.track, 0, sizeof(m_tracks.track));
	track_count = 0;
	m_tracks.list.itemnames = NULL;
}

static void Tracks_Scan( void)
{
	long length;	
	char *playlist_buf = NULL;

	Tracks_Free();

	if ((length = WA_GetPlaylist(&playlist_buf)) == -1)
		return;

	track_count = WA_ParsePlaylist_EXTM3U(playlist_buf, length, &m_tracks.track, m_tracks.filter.buffer);
	free(playlist_buf);
}


static void Build_Tracklist (void)
{
	Tracks_Scan();
	m_tracks.list.curvalue = 0;
	m_tracks.list.prestep = 0;
	m_tracks.list.itemnames = m_tracks.track.name;
	m_tracks.list.count = track_count;
}
static void Tracks_Filter( void *s ) {
	Build_Tracklist();
}

void Select_Track ( void *s) {
	if(!m_tracks.list.count)
		return;

	WA_PlayTrack(m_tracks.track.num[m_tracks.list.curvalue]);
}
const char *WA_MenuKey( menuframework_s *self, int key ) {
	switch( key ) {
	case K_ESCAPE:
		Tracks_Free();
		M_PopMenu();
		return NULL;
	}

	return Default_MenuKey( self, key );
}

void WA_MenuInit( void ) {
	memset( &m_tracks.menu, 0, sizeof( m_tracks.menu ) );

	m_tracks.menu.x = 0;
	m_tracks.menu.y = 0;

	m_tracks.list.generic.type		= MTYPE_LIST;
	m_tracks.list.generic.flags		= QMF_LEFT_JUSTIFY;
	m_tracks.list.generic.name		= NULL;
	m_tracks.list.generic.callback  = Select_Track;
	m_tracks.list.generic.x			= 20;
	m_tracks.list.generic.y			= 30;
	m_tracks.list.width				= viddef.width - 40;
	m_tracks.list.height			= viddef.height - 110;

	m_tracks.filter.generic.type	= MTYPE_FIELD;
	m_tracks.filter.generic.name	= "Search";
	m_tracks.filter.generic.callback = Tracks_Filter;
	m_tracks.filter.generic.x		= (viddef.width/2)-100;
	m_tracks.filter.generic.y		= 30 + m_tracks.list.height + 5;
	m_tracks.filter.length			= 30;
	m_tracks.filter.visible_length	= 15;
	memset(m_tracks.filter.buffer, 0, sizeof(m_tracks.filter.buffer));
	m_tracks.filter.cursor			= 0;

	Build_Tracklist();

	m_tracks.menu.draw = WA_MenuDraw;
	m_tracks.menu.key = WA_MenuKey;
	Menu_AddItem( &m_tracks.menu, (void *)&m_tracks.list );
	Menu_AddItem( &m_tracks.menu, (void *)&m_tracks.filter );

	Menu_SetStatusBar( &m_tracks.menu, NULL );
	m_tracks.menu.cursor = 1; //cursor default to search box
	Menu_AdjustCursor( &m_tracks.menu, 1);
}



void M_Menu_WA_f( void ) {
	WA_MenuInit();
	M_PushMenu( &m_tracks.menu );
}
#endif