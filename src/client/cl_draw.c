/*
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

//
// cl_draw.c - draw all 2D elements during active gameplay
//

void SCR_ExecuteLayoutString( char *s );
void SCR_DrawInventory( void );
void SCR_DrawNet( void );
void SCR_CheckDrawCenterString( void );

static cvar_t *scr_draw2d;

#define DSF_LEFT		1
#define DSF_RIGHT		2
#define DSF_BOTTOM		4
#define DSF_TOP			8
#define DSF_CENTERX		16
#define DSF_CENTERY		32
#define DSF_HIGHLIGHT	64
#define DSF_UNDERLINE	128
#define DSF_SELECTED	256

void SCR_ExecuteLayoutString( char *s );
void SCR_DrawInventory( void );
void SCR_DrawNet( void );
void SCR_CheckDrawCenterString( void );

static cvar_t *scr_draw2d;

/*
==============
SCR_DrawString
==============
*/
void SCR_DrawString( int xpos, int ypos, char *string, int flags ) {
	int x;
	int y;
	int len;

	len = strlen( string );

	if( flags & DSF_CENTERX ) {
		x = xpos - (len << 2);
	} else if( flags & DSF_RIGHT ) {
		x = xpos - (len << 3);
	} else {
		x = xpos;
	}

	if( flags & DSF_CENTERY ) {
		y = ypos - (1 << 2);
	} else if( flags & DSF_TOP ) {
		y = ypos - (1 << 3);
	} else {
		y = ypos;
	}

	if( flags & DSF_SELECTED ) {
		Draw_Fill( x - 1, y, (len << 3) + 2, 10, 16 );
	}

	if( flags & DSF_HIGHLIGHT ) {
		DrawAltString( x, y, string );
	} else {
		DrawString( x, y, string );
	}

	if( flags & DSF_UNDERLINE ) {
		Draw_Fill( x, y + 9, len << 3, 1, 0xDF );
	}
}

/*
===============================================================================

LAGOMETER
from q2pro
===============================================================================
*/

#define LAG_SAMPLES 64
#define LAG_MASK	(LAG_SAMPLES-1)

#define LAG_MAXPING		400

#define LAG_WIDTH	48.0f
#define LAG_HEIGHT	48.0f

typedef struct lagometer_s
{
	int ping[LAG_SAMPLES];
	int inSize[LAG_SAMPLES];
	int inTime[LAG_SAMPLES];
	int inPacketNum;

	int outSize[LAG_SAMPLES];
	int outTime[LAG_SAMPLES];
	int outPacketNum;
} lagometer_t;

static cvar_t *scr_drawlagometer;

static lagometer_t	scr_lagometer;

/*
==============
SCR_ClearLagometer
==============
*/
void SCR_ClearLagometer( void )
{
	memset( &scr_lagometer, 0, sizeof( scr_lagometer ) );
}

/*
==============
SCR_AddLagometerPacketInfo
==============
*/
void SCR_AddLagometerPacketInfo( void )
{
	int ping;
	int i;

	if( cls.netchan.dropped ) {
		ping = -1000;
	} else {
		i = cls.netchan.incoming_acknowledged & (CMD_BACKUP-1);
		ping = cls.realtime - cl.cmd_time[i];
		if( ping > 999 ) {
			ping = 999;
		}
		if( cl.surpressCount ) {
			ping = -ping;
		}
	}

	i = scr_lagometer.inPacketNum & LAG_MASK;
	scr_lagometer.inTime[i] = cls.realtime;
	scr_lagometer.ping[i] = ping;
	scr_lagometer.inSize[i] = net_message.cursize;

	scr_lagometer.inPacketNum++;

}

/*
==============
SCR_AddLagometerOutPacketInfo
==============
*/
void SCR_AddLagometerOutPacketInfo( int size )
{
	int i;

	i = scr_lagometer.outPacketNum & LAG_MASK;
	scr_lagometer.outTime[i] = cls.realtime;
	scr_lagometer.outSize[i] = size;

	scr_lagometer.outPacketNum++;
}

/*
==============
SCR_DrawLagometer
==============
*/
static void SCR_DrawLagometer( void )
{
	int x, y;
	int i, j, v;
	int count;
	int ping;
	float size;
	char string[8];
	int color;
	int startTime, endTime;

	if( scr_drawlagometer->integer < 1 )
		return;

	x = viddef.width - LAG_WIDTH - 1;
	y = viddef.height - 96 - LAG_HEIGHT - 1;

	Draw_Fill( x, y, LAG_WIDTH, LAG_HEIGHT, 0x04 );

	ping = 0;
	count = 0;
	for( i=1 ; i<LAG_WIDTH+1; i++ )
	{
		j = scr_lagometer.inPacketNum - i - 1;
		if( j < 0 )
			break;

		j &= LAG_MASK;

		v = scr_lagometer.ping[j];
		if( v == -1000 )
		{
			Draw_Fill( x + LAG_WIDTH - i, y, 1, LAG_HEIGHT, 0xF2 );
		}
		else
		{
			color = 0xd0;
			if( v < 0 )
			{
				v = -v;
				color = 0xDC;
			}
			if( i < LAG_SAMPLES/8 )
			{
				ping += v;
				count++;
			}

			v *= LAG_HEIGHT / LAG_MAXPING;
			if( v > LAG_HEIGHT )
				v = LAG_HEIGHT;

			Draw_Fill( x + LAG_WIDTH - i, y + LAG_HEIGHT - v, 1, v, color );
		

		}
	}

	//border...
	Draw_Fill (x-1,				y,				LAG_WIDTH+2,		1,					0);
	Draw_Fill (x-1,				y+LAG_HEIGHT,	LAG_WIDTH+2,		1,					0);

	Draw_Fill (x-1	,			y,				1,					LAG_HEIGHT,			0);
	Draw_Fill (x+LAG_WIDTH,		y,				1,					LAG_HEIGHT,			0);
//
// draw ping
//
	if( scr_drawlagometer->integer < 2 )
		return;

	i = count ? ping / count : 0;
	Com_sprintf( string, sizeof( string ), "%i", i );
	SCR_DrawString( x + LAG_WIDTH/2, y + 22, string, DSF_CENTERX );


//
// draw download speed
//
	if( scr_drawlagometer->integer < 3 )
		return;

	i = scr_lagometer.inPacketNum - LAG_SAMPLES/8 + 1;
	if( i < 0 ) {
		i = 0;
	}

	startTime = scr_lagometer.inTime[i & LAG_MASK];
	endTime = scr_lagometer.inTime[(scr_lagometer.inPacketNum - 1) & LAG_MASK];

	size = 0.0f;
	if( startTime != endTime ) {
		for( ; i<scr_lagometer.inPacketNum ; i++ ) {
			size += scr_lagometer.inSize[i & LAG_MASK];
		}

		size /= endTime - startTime;
		size *= 1000.0f / 1024.0f;
	}
	Com_sprintf( string, sizeof( string ), "%1.2f", size );
	SCR_DrawString( x + LAG_WIDTH/2, y + 12, string, DSF_CENTERX );

//
// draw upload speed
//
	i = scr_lagometer.outPacketNum - LAG_SAMPLES/8 + 1;
	if( i < 0 ) {
		i = 0;
	}

	startTime = scr_lagometer.outTime[i & LAG_MASK];
	endTime = scr_lagometer.outTime[(scr_lagometer.outPacketNum - 1) & LAG_MASK];

	size = 0.0f;
	if( startTime != endTime ) {
		for( ; i<scr_lagometer.outPacketNum ; i++ ) {
			size += scr_lagometer.outSize[i & LAG_MASK];
		}

		size /= endTime - startTime;
		size *= 1000.0f / 1024.0f;
	}
	Com_sprintf( string, sizeof( string ), "%1.2f", size );
	SCR_DrawString( x + LAG_WIDTH/2, y + 2, string, DSF_CENTERX );
}

/*
===============================================================================

CHAT HUD

===============================================================================
*/

#define MAX_CHAT_LENGTH		128
#define MAX_CHAT_LINES		8

char chathudtext[MAX_CHAT_LINES][MAX_CHAT_LENGTH];

cvar_t *cl_chathud;
cvar_t *cl_chathudlines;
cvar_t *cl_chathudx;
cvar_t *cl_chathudy;

/*
==============
SCR_ClearChatHUD_f
==============
*/
void SCR_ClearChatHUD_f( void )
{
	memset(chathudtext, 0, sizeof(chathudtext));
}

/*
==============
SCR_AddToChatHUD
==============
*/
void SCR_AddToChatHUD( const char *string, qboolean mm2 )
{
	int i;

	if(cl_chathud->integer > 2 && !mm2)
		return;

	for(i = 0; i < MAX_CHAT_LINES - 1; i++)
		memcpy(chathudtext[i], chathudtext[i+1], MAX_CHAT_LENGTH);

	memset(chathudtext[MAX_CHAT_LINES -1], 0, MAX_CHAT_LENGTH);
	strncpy(chathudtext[MAX_CHAT_LINES -1], string, MAX_CHAT_LENGTH-1);
	// Overwrite some odd character that shouldn't be there
	chathudtext[MAX_CHAT_LINES -1][strlen(chathudtext[MAX_CHAT_LINES -1]) - 1] = '\0';
}

/*
==============
SCR_DrawChatHUD
==============
*/
void SCR_DrawChatHUD( void )
{
    int i, y, x;

	if(!cl_chathud->integer)
		return;

	if (cl_chathudlines->integer > MAX_CHAT_LINES)
		Cvar_SetValue ("cl_chathudlines", MAX_CHAT_LINES);
	else if (cl_chathudlines->integer < 1)
		Cvar_SetValue ("cl_chathudlines", 1);


	if(cl_chathudx->integer || cl_chathudy->integer)
	{
		x = cl_chathudx->integer;
		y = cl_chathudy->integer;
	}
	else
	{
		x = 5;
		y = viddef.height-22-8*cl_chathudlines->integer;
	}

    for (i = MAX_CHAT_LINES - cl_chathudlines->integer; i < MAX_CHAT_LINES; i++)
	{
		if(chathudtext[i][0])
		{
            if (cl_chathud->integer &= 2)
				DrawAltString(x, y, chathudtext[i]);
			else
				DrawString(x, y, chathudtext[i]);

			y += 8;
		}
    }
}

/*
===============================================================================

HUD CLOCK

===============================================================================
*/

cvar_t *cl_clock;
cvar_t *cl_clockx;
cvar_t *cl_clocky;
cvar_t *cl_clockformat;

/*
==============
SCR_DrawClock
==============
*/
static void SCR_DrawClock( void )
{
	struct tm *ntime;
	char stime[32];
	time_t l_time;

	if(!cl_clock->integer)
		return;

	time( &l_time );
	ntime = localtime( &l_time ); 
	strftime( stime, sizeof(stime), cl_clockformat->string, ntime );

	if(cl_clockx->integer || cl_clocky->integer)
		DrawColorString(cl_clockx->integer, cl_clocky->integer, stime, cl_clock->integer, 1);
	else
		DrawColorString(5, viddef.height-10, stime, cl_clock->integer, 1);
}

/*
===============================================================================

FPS COUNTER

===============================================================================
*/

#define	FPS_FRAMES	64

cvar_t *cl_fps;
cvar_t *cl_fpsx;
cvar_t *cl_fpsy;

/*
==============
SCR_DrawFPS
==============
*/
static void SCR_DrawFPS( void )
{
	static int prevTime = 0, index = 0;
	static char fps[32];

	if(!cl_fps->integer)
		return;

	index++;

	if ((index % FPS_FRAMES) == 0)
	{
		Com_sprintf(fps, 32, "%dfps",
			(int) (1000 / ((float) (Sys_Milliseconds() - prevTime) / FPS_FRAMES)));
		prevTime = Sys_Milliseconds();
	}

	if (index <= FPS_FRAMES)
		return;

	if(cl_fpsx->integer || cl_fpsy->integer)
		DrawColorString (cl_fpsx->integer, cl_fpsy->integer, fps, cl_fps->integer, 1);
	else
		DrawColorString (5, viddef.height-20, fps, cl_fps->integer, 1);

}

/*
===============================================================================

MAP TIME

===============================================================================
*/

cvar_t *cl_maptime;
cvar_t *cl_maptimex;
cvar_t *cl_maptimey;

/*
================
SCR_ShowTIME
================
*/
static void SCR_ShowTIME(void)
{
	char	temp[32];
	int		time, hour, mins, secs;
	int		color;

	if(!cl_maptime->integer)
		return;

	if(cl_maptime->integer >= 11)
		time = (cl.time - cls.roundtime) / 1000;
	else
		time = cl.time / 1000;

	hour = time/3600;
	mins = (time%3600) /60;
	secs = time%60;
	
	if (hour > 0)
		Com_sprintf(temp, sizeof(temp), "%i:%02i:%02i", hour, mins, secs);
	else
		Com_sprintf(temp, sizeof(temp), "%i:%02i", mins, secs);

	if (cl_maptime->integer > 10)
		color = cl_maptime->integer - 10;
	else
		color = cl_maptime->integer;

	if(cl_maptimex->integer || cl_maptimey->integer)
		DrawColorString (cl_maptimex->integer, cl_maptimey->integer, temp, color, 1);
	else
		DrawColorString (77, viddef.height-10, temp, color, 1);
}


/*
===============================================================================

CROSSHAIR

===============================================================================
*/

cvar_t	*crosshair;
cvar_t	*ch_alpha;
cvar_t	*ch_pulse;
cvar_t	*ch_scale;
cvar_t	*ch_red;
cvar_t	*ch_green;
cvar_t	*ch_blue;

/*
=================
SCR_DrawCrosshair
=================
*/
static void SCR_DrawCrosshair (void)
{
	float	alpha = ch_alpha->value;

	if (!crosshair->integer)
		return;

	if (crosshair->modified)
	{
		crosshair->modified = false;
		SCR_TouchPics ();
	}

	if (!crosshair_pic[0])
		return;

	if (ch_pulse->value)
		alpha = (0.75*ch_alpha->value) + (0.25*ch_alpha->value)*sin(anglemod((cl.time*0.005)*ch_pulse->value));

	Draw_ScaledPic (scr_vrect.x + ((scr_vrect.width - crosshair_width)>>1)
	, scr_vrect.y + ((scr_vrect.height - crosshair_height)>>1), ch_scale->value, crosshair_pic, ch_red->value, ch_green->value, ch_blue->value, alpha);

}

/*
================
SCR_Draw2D
================
*/
void SCR_Draw2D( void )
{
	if(!scr_draw2d->integer)
		return;

	SCR_DrawCrosshair ();

	// draw status bar
	SCR_ExecuteLayoutString( cl.configstrings[CS_STATUSBAR] );

	// draw layout
	if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 1)
		SCR_ExecuteLayoutString( cl.layout );
	// draw inventory
	if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 2)
		CL_DrawInventory ();

	SCR_DrawLagometer();
	SCR_DrawNet ();
	SCR_CheckDrawCenterString ();
	SCR_DrawFPS ();
	SCR_ShowTIME ();
	SCR_DrawChatHUD ();
	SCR_DrawClock ();
}

/*
================
SCR_InitDraw
================
*/
void SCR_InitDraw( void )
{
	scr_draw2d = Cvar_Get( "scr_draw2d", "1", 0 );

	crosshair = Cvar_Get ("crosshair", "0", CVAR_ARCHIVE);
	ch_alpha = Cvar_Get ("ch_alpha", "1", CVAR_ARCHIVE);
	ch_pulse = Cvar_Get ("ch_pulse", "0", CVAR_ARCHIVE);
	ch_scale = Cvar_Get ("ch_scale", "1", CVAR_ARCHIVE);
	ch_red   = Cvar_Get ("ch_red",   "1", CVAR_ARCHIVE);
	ch_green = Cvar_Get ("ch_green", "1", CVAR_ARCHIVE);
	ch_blue  = Cvar_Get ("ch_blue",  "1", CVAR_ARCHIVE);

	cl_clock = Cvar_Get ("cl_clock", "0", CVAR_ARCHIVE);
	cl_clockx = Cvar_Get ("cl_clockx", "0", 0);
	cl_clocky = Cvar_Get ("cl_clocky", "0", 0);
	cl_clockformat = Cvar_Get ("cl_clockformat", "%H:%M:%S", 0);

	cl_maptime = Cvar_Get ("cl_maptime", "0", CVAR_ARCHIVE);
	cl_maptimex = Cvar_Get ("cl_maptimex", "0", 0);
	cl_maptimey = Cvar_Get ("cl_maptimey", "0", 0);

	cl_fps = Cvar_Get ("cl_fps", "0", CVAR_ARCHIVE);
	cl_fpsx = Cvar_Get ("cl_fpsx", "0", 0);
	cl_fpsy = Cvar_Get ("cl_fpsy", "0", 0);

	cl_chathud = Cvar_Get ("cl_chathud", "0", CVAR_ARCHIVE);
	cl_chathudx = Cvar_Get ("cl_chathudx", "0", 0);
	cl_chathudy = Cvar_Get ("cl_chathudy", "0", 0);
	cl_chathudlines = Cvar_Get("cl_chathudlines", "4", CVAR_ARCHIVE);

	scr_drawlagometer = Cvar_Get( "scr_drawlagometer", "0", 0 );
}
