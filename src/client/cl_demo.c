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

#include "client.h"

cvar_t	*cl_clan;
cvar_t	*cl_autorecord;

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
void CL_WriteDemoMessageFull( void )
{
	int		len, swlen;

	// the first eight bytes are just packet sequencing stuff
	len = net_message.cursize - 8;
	swlen = LittleLong( len );

	if(swlen > 0) // skip bad packets
	{
		fwrite( &swlen, 4, 1, cls.demofile);
		fwrite( net_message.data + 8, len, 1, cls.demofile);
	}
}

#ifdef R1Q2_PROTOCOL
void CL_WriteDemoMessage (byte *buff, int len, qboolean forceFlush)
{
	if (forceFlush)
	{
		if (!cls.demowaiting)
		{
			int	swlen;

			if (cl.demoBuff.overflowed)
			{
				Com_DPrintf ("Dropped a demo frame, maximum message size exceeded: %d > %d\n", cl.demoBuff.cursize, cl.demoBuff.maxsize);

				//we write a message regardless to keep in sync time-wise.
				SZ_Clear (&cl.demoBuff);
				MSG_WriteByte (&cl.demoBuff, svc_nop);
			}

			swlen = LittleLong(cl.demoBuff.cursize);
			fwrite (&swlen, 4, 1, cls.demofile);
			fwrite (cl.demoFrame, cl.demoBuff.cursize, 1, cls.demofile);
		}
		SZ_Clear (&cl.demoBuff);
	}

	if (len)
		SZ_Write (&cl.demoBuff, buff, len);
}
#endif

/*
====================
CL_CloseDemoFile
====================
*/
void CL_CloseDemoFile( void )
{
	int len;

	if (!cls.demofile)
		return;

	// finish up
	len = -1;
	fwrite (&len, 4, 1, cls.demofile);

	fclose( cls.demofile );
	 
	cls.demofile = NULL;

#ifdef R1Q2_PROTOCOL
	// inform server we are done with extra data
	if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
	{
		MSG_WriteByte  (&cls.netchan.message, clc_setting);
		MSG_WriteShort (&cls.netchan.message, CLSET_RECORDING);
		MSG_WriteShort (&cls.netchan.message, 0);
	}
#endif
}


/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f( void )
{
	if( !cls.demorecording )
	{
		Com_Printf( "Not recording a demo.\n" );
		return;
	}

	CL_CloseDemoFile();
	cls.demorecording = false;
	Com_Printf( "Stopped demo.\n" );
}

void CL_StopAutoRecord (void)
{
	if(cl_autorecord->integer && cls.demorecording)
		CL_Stop_f();
}

void CL_StartRecording(char *name)
{
	byte	buf_data[MAX_MSGLEN];
	sizebuf_t	buf;
	int		i, len;
	entity_state_t	*ent;
	entity_state_t	nullstate;
	char *string;

	FS_CreatePath( name );
	cls.demofile = fopen (name, "wb");
	if( !cls.demofile )
	{
		Com_Printf( "ERROR: Couldn't open demo file %s.\n", name );
		return;
	}

	Com_Printf( "Recording demo to %s.\n", name );

	cls.demorecording = true;

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = true;

#ifdef R1Q2_PROTOCOL
	// inform server we need to receive more data
	if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
	{
		MSG_WriteByte  (&cls.netchan.message, clc_setting);
		MSG_WriteShort (&cls.netchan.message, CLSET_RECORDING);
		MSG_WriteShort (&cls.netchan.message, 1);
	}
#endif

	//
	// write out messages to hold the startup information
	//
	SZ_Init( &buf, buf_data, sizeof( buf_data ) );

	// send the serverdata
	MSG_WriteByte( &buf, svc_serverdata );
	MSG_WriteLong( &buf, ORIGINAL_PROTOCOL_VERSION );
	MSG_WriteLong( &buf, 0x10000 + cl.servercount );
	MSG_WriteByte( &buf, 1 );	// demos are always attract loops
	MSG_WriteString( &buf, cl.gamedir );
	MSG_WriteShort( &buf, cl.playernum );
	MSG_WriteString( &buf, cl.configstrings[CS_NAME] );

	// configstrings
	for( i=0 ; i<MAX_CONFIGSTRINGS ; i++ )
	{
		string = cl.configstrings[i];
		if( !string[0] )
			continue;
		
		if( buf.cursize + strlen( string ) + 32 > buf.maxsize )
		{	// write it out
			len = LittleLong (buf.cursize);
			fwrite (&len, 4, 1, cls.demofile);
			fwrite (buf.data, buf.cursize, 1, cls.demofile);
			buf.cursize = 0;
		}

		MSG_WriteByte( &buf, svc_configstring );
		MSG_WriteShort( &buf, i );
		MSG_WriteString( &buf, string );

	}

	// baselines
	memset (&nullstate, 0, sizeof(nullstate));
	for( i=1; i<MAX_EDICTS ; i++ )
	{
		ent = &cl_entities[i].baseline;
		if( !ent->modelindex )
			continue;

		if( buf.cursize + 64 > buf.maxsize )
		{	// write it out
			len = LittleLong (buf.cursize);
			fwrite (&len, 4, 1, cls.demofile);
			fwrite (buf.data, buf.cursize, 1, cls.demofile);
			buf.cursize = 0;
		}

		MSG_WriteByte( &buf, svc_spawnbaseline );
		MSG_WriteDeltaEntity(&nullstate, ent, &buf, true, false);
	}

	MSG_WriteByte( &buf, svc_stufftext );
	MSG_WriteString( &buf, "precache\n" );

	// write it to the demo file
	len = LittleLong (buf.cursize);
	fwrite (&len, 4, 1, cls.demofile);
	fwrite (buf.data, buf.cursize, 1, cls.demofile);

	// the rest of the demo file will be individual frames
}

/*
====================
CL_Record_f

record <demoname>

Begins recording a demo from the current position
====================
*/
void CL_Record_f( void )
{
	int		i, c;
    char 	name[MAX_OSPATH], timebuf[32];
    time_t	clock;

	c = Cmd_Argc();
	if( c != 1 && c != 2) {
		Com_Printf( "Usage: %s [demoname]\n", Cmd_Argv(0) );
		return;
	}

	if( cls.demorecording )
	{
		Com_Printf( "Already recording.\n" );
		return;
	}

	if( cls.state != ca_active )
	{
		Com_Printf( "You must be in a level to record.\n" );
		return;
	}

	if (cl.attractloop)
	{
		Com_Printf ("Unable to record from a demo stream due to insufficient deltas.\n");
		return;
	}

	//
	// open the demo file
	//
    if (c == 1)
    {
		time( &clock );
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d", localtime(&clock));

        Com_sprintf (name, sizeof(name), "%s/demos/%s_%s_%s.dm2", FS_Gamedir(), timebuf, cl_clan->string, cls.mapname);
        for (i=2; i<100; i++)
        {
			cls.demofile = fopen (name, "rb");
			if (!cls.demofile)
				break;
			fclose (cls.demofile);
			Com_sprintf (name, sizeof(name), "%s/demos/%s_%s_%s_%i%i.dm2", FS_Gamedir(), timebuf, cl_clan->string, cls.mapname, (int)(i/10)%10, i%10);
		}
		if (i == 100)
		{
			Com_Printf ("ERROR: Too many demos with same name.\n");
			return;
		}
    }
	else
	{
		Com_sprintf( name, sizeof( name ), "%s/demos/%s.dm2", FS_Gamedir(), Cmd_Argv( 1 ) );
	}

	CL_StartRecording(name);
}

//----------------------------------------------------
//		AUTO RECORD
//----------------------------------------------------
void CL_StartAutoRecord(void)
{
	char	timebuf[32], name[MAX_OSPATH];
	time_t	clock;

	if (cls.state != ca_active)
		return;

	if (!cl_autorecord->integer || cls.demorecording || cl.attractloop)
		return;

	time( &clock );
	strftime( timebuf, sizeof(timebuf), "%Y-%m-%d_%H-%M-%S", localtime(&clock));

	if(strlen(cl_clan->string) > 2)
		Com_sprintf(name, sizeof(name), "%s/demos/%s_%s_%s.dm2", FS_Gamedir(), timebuf, cl_clan->string, cls.mapname);
	else
		Com_sprintf(name, sizeof(name), "%s/demos/%s_%s.dm2", FS_Gamedir(), timebuf, cls.mapname);	

	CL_StartRecording(name);
}

static void OnChange_AutoRecord(cvar_t *self, const char *oldValue)
{
	CL_StartAutoRecord();
}

/*
==============
CL_Demo_List_f
==============
*/
void CL_Demo_List_f ( void )
{

	char	findname[1024];
	char	**dirnames;
	int		ndirs;
	char	*tmp;

	Com_sprintf(findname, sizeof(findname), "%s/demos/*%s*.dm2", FS_Gamedir(), Cmd_Argv( 1 )) ;

	tmp = findname;

	while( *tmp != 0 )
	{
		if ( *tmp == '\\' ) 
			*tmp = '/';
		tmp++;
	}
	Com_Printf( "Directory of %s\n", findname );
	Com_Printf( "----\n" );

	if( ( dirnames = FS_ListFiles( findname, &ndirs, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) ) != 0 )
	{
		int i;

		for( i = 0; i < ndirs-1; i++ )
		{
			if( strrchr( dirnames[i], '/' ) )
				Com_Printf( "%i %s\n", i, strrchr( dirnames[i], '/' ) + 1 );
			else
				Com_Printf( "%i %s\n", i, dirnames[i] );

			Z_Free( dirnames[i] );
		}
		Z_Free( dirnames );
	}

	Com_Printf( "\n" );
}

/*
==============
CL_Demo_Play_f
==============
*/
void CL_Demo_Play_f ( void )
{

	char	findname[1024];
	char	**dirnames;
	int		ndirs;
	int		find;
	char	buf[1024] ;
	int		found;
	char	*tmp;

	found = 0;

	if(Cmd_Argc() == 1)
	{
		Com_Printf("Usage: %s <id> [search card]\n", Cmd_Argv(0)) ;
		return;
	}

	find = atoi(Cmd_Argv (1));

	Com_sprintf (findname, sizeof(findname), "%s/demos/*%s*.dm2", FS_Gamedir(), Cmd_Argv( 2 ));

	tmp = findname;

	while( *tmp != 0 )
	{
		if ( *tmp == '\\' ) 
			*tmp = '/';
		tmp++;
	}

	Com_Printf( "Directory of %s\n", findname );
	Com_Printf( "----\n" );

	if( ( dirnames = FS_ListFiles( findname, &ndirs, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) ) != 0 )
	{
		int i;

		for( i = 0; i < ndirs-1; i++ )
		{
			if (i == find)
			{
				if( strrchr( dirnames[i], '/' ) )
				{
					Com_Printf( "%i %s\n", i, strrchr( dirnames[i], '/' ) + 1 );
					found = 1;
					sprintf(buf, "demomap \"%s\"\n", strrchr( dirnames[i], '/' ) + 1);
				}	
				else
					Com_Printf( "%i %s\n", i, dirnames[i] );
			}

			Z_Free( dirnames[i] );
		}
		Z_Free( dirnames );
	}

	Com_Printf( "\n" );

	if (found)		
		Cbuf_AddText(buf);
}

/*
====================
CL_InitDemos
====================
*/
void CL_InitDemos( void )
{
	cl_clan = Cvar_Get ("cl_clan", "", 0);
	Cmd_AddCommand( "record", CL_Record_f );
	Cmd_AddCommand( "stop", CL_Stop_f );

	Cmd_AddCommand ("demolist", CL_Demo_List_f );
	Cmd_AddCommand ("demoplay", CL_Demo_Play_f );

	cl_autorecord = Cvar_Get("cl_autorecord", "0", 0);
	cl_autorecord->OnChange = OnChange_AutoRecord;
}
