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

#include "ui_local.h"

/*
=======================================================================

DEMOS MENU

=======================================================================
*/
#define MAX_MENU_DEMOS	1024

#define DM_TITLE "-[ Demos Menu ]-"

#define FFILE_UP		1
#define FFILE_FOLDER	2
#define FFILE_DEMO		3
#define DEF_FOLDER		"demos"

typedef struct m_demos_s {
	menuframework_s		menu;
	menulist_s		list;

	char			*names[MAX_MENU_DEMOS];
	int				types[MAX_MENU_DEMOS];
} m_demos_t;

char	game_folder[128] = "";
char	d_folder[1024] = "";

static int			demo_count = 0;
static m_demos_t	m_demos;

static void Demos_MenuDraw( menuframework_s *self )
{

	DrawString((viddef.width - (strlen(DM_TITLE)*8))>>1, 10, DM_TITLE);
	DrawAltString(20, 20, va("Directory: demos%s/", d_folder));

	Menu_Draw( self );

	switch(m_demos.types[m_demos.list.curvalue]) {
	case FFILE_UP:
		DrawAltString(20, 30+m_demos.list.height, "Go one directory up");
		break;
	case FFILE_FOLDER:
		DrawAltString(20, 30+m_demos.list.height, va("Go to directory demos%s/%s", d_folder, m_demos.names[m_demos.list.curvalue]));
		break;
	case FFILE_DEMO:
		DrawAltString(20, 30+m_demos.list.height, va("Selected demo: %s", m_demos.names[m_demos.list.curvalue]));
		break;
	}
}

static void Demos_Free( void ) {
	int i;

	for( i=0 ; i<demo_count ; i++ ) {
		free( m_demos.names[i] );
		m_demos.types[i] = 0;
	}
	memset(m_demos.names, 0, sizeof(m_demos.names));
	demo_count = 0;
	m_demos.list.itemnames = NULL;
}

static void Demos_Scan( void) {
	int		numFiles;
	char	findname[1024];
	char	**list;
	int		i, skip = 0;

	Demos_Free();

	if(d_folder[0])
	{
		m_demos.names[demo_count] = strdup("..");
		m_demos.types[demo_count] = FFILE_UP;
		demo_count++;
		skip = 1;
	}

	sprintf(findname, "%s/demos%s/*", game_folder, d_folder);
	list = FS_ListFiles( findname, &numFiles, SFF_SUBDIR, SFF_HIDDEN | SFF_SYSTEM );
	if(list)
	{
		for( i=0 ; i<numFiles-1; i++ ) {
			if( demo_count < MAX_MENU_DEMOS ) {
				if (strrchr( list[i], '/' ))
					m_demos.names[demo_count] = strdup( strrchr( list[i], '/' ) + 1 );
				else
					m_demos.names[demo_count] = strdup( list[i] );
				m_demos.types[demo_count] = FFILE_FOLDER;
				demo_count++;
			}
			free( list[i] );
		}
		free( list );

		if(demo_count > skip) {
			qsort( m_demos.names + skip, demo_count - skip, sizeof( m_demos.names[0] ), SortStrcmp );
		}
		skip = demo_count;
	}

	sprintf(findname, "%s/demos%s/*.dm2", game_folder, d_folder);
	list = FS_ListFiles( findname, &numFiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM );
	if( !list ) {
		return;
	}

	for( i=0 ; i<numFiles-1; i++ ) {
		if( demo_count < MAX_MENU_DEMOS ) {
			if (strrchr( list[i], '/' ))
				m_demos.names[demo_count] = strdup( strrchr( list[i], '/' ) + 1 );
			else
				m_demos.names[demo_count] = strdup( list[i] );
			m_demos.types[demo_count] = FFILE_DEMO;
			demo_count++;
		}
		free( list[i] );
	}
	free( list );

	if(demo_count > skip) {
		qsort( m_demos.names + skip, demo_count - skip, sizeof( m_demos.names[0] ), SortStrcmp );
	}
}

static void Build_List(void)
{
	Demos_Scan();
	m_demos.list.curvalue = 0;
	m_demos.list.prestep = 0;
	m_demos.list.itemnames = m_demos.names;
	m_demos.list.count = demo_count;
}

static void Load_Demo (void *s)
{
	char *p;

	if(!m_demos.list.count)
		return;

	switch( m_demos.types[m_demos.list.curvalue] ) {
	case FFILE_UP:
		if ((p = strrchr(d_folder, '/')) != NULL)
			*p = 0;
		Build_List();
		break;
	case FFILE_FOLDER:
		Q_strncatz (d_folder, "/", sizeof(d_folder) - strlen(d_folder));
		Q_strncatz (d_folder, m_demos.names[m_demos.list.curvalue], sizeof(d_folder) - strlen(d_folder));
		Build_List();
		break;
	case FFILE_DEMO:
		if(d_folder[0])
			Cbuf_AddText( va( "demomap \"%s/%s\"\n", d_folder + 1, m_demos.names[m_demos.list.curvalue] ) );
		else
			Cbuf_AddText( va( "demomap \"%s\"\n", m_demos.names[m_demos.list.curvalue] ) );
		Demos_Free();
		M_ForceMenuOff();
		break;
	}

	return;
}

const char *Demos_MenuKey( menuframework_s *self, int key ) {
	switch( key ) {
	case K_ESCAPE:
		Demos_Free();
		M_PopMenu();
		return NULL;
	}
		
	return Default_MenuKey( self, key );
}

void Demos_MenuInit( void ) {
	memset( &m_demos.menu, 0, sizeof( m_demos.menu ) );

	if(!game_folder[0] || strcmp(FS_Gamedir(), game_folder)) {
		strcpy(game_folder, FS_Gamedir());
		d_folder[0] = 0;
	}

	m_demos.menu.x = 0;
	m_demos.menu.y = 0;

	m_demos.list.generic.type		= MTYPE_LIST;
	m_demos.list.generic.flags		= QMF_LEFT_JUSTIFY;
	m_demos.list.generic.name		= NULL;
	m_demos.list.generic.callback	= Load_Demo;
	m_demos.list.generic.x			= 20;
	m_demos.list.generic.y			= 30;
	m_demos.list.width				= viddef.width - 40;
	m_demos.list.height				= viddef.height - 60;

	Build_List();

	m_demos.menu.draw = Demos_MenuDraw;
	m_demos.menu.key = Demos_MenuKey;
	Menu_AddItem( &m_demos.menu, (void *)&m_demos.list );

	Menu_SetStatusBar( &m_demos.menu, NULL );

}



void M_Menu_Demos_f( void ) {
	Demos_MenuInit();
	M_PushMenu( &m_demos.menu );
}

