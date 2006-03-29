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

/*
==============
SCR_DrawString
==============
*/
void SCR_DrawString( int x, int y, const char *string, int flags )
{
	int len;

	len = strlen( string );

	if( flags & DSF_CENTERX )
		x -= (len << 2);
	else if( flags & DSF_RIGHT )
		x -= (len << 3);

	if( flags & DSF_CENTERY )
		y -= (1 << 2);
	else if( flags & DSF_TOP )
		y -= (1 << 3);

	if( flags & DSF_SELECTED )
		Draw_Fill( x - 1, y, (len << 3) + 2, 10, 16 );

	if( flags & DSF_HIGHLIGHT )
		DrawAltString( x, y, string );
	else
		DrawString( x, y, string );

	if( flags & DSF_UNDERLINE )
		Draw_Fill( x, y + 9, len << 3, 1, 0xDF );
}

static void DrawBorder (int x, int y, int w, int h, int c, int s)
{
	Draw_Fill( x,		y,		w, s, c );
	Draw_Fill( x,		y+h-s,	w, s, c );
	Draw_Fill( x,		y,		s, h, c );
	Draw_Fill( x+w-s,	y,		s, h, c );
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
		if( ping > 999 )
			ping = 999;
		if( cl.surpressCount )
			ping = -ping;
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

	DrawBorder(x-1, y, LAG_WIDTH+2, LAG_HEIGHT+1, 0, 1);

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
	if( i < 0 )
		i = 0;

	startTime = scr_lagometer.inTime[i & LAG_MASK];
	endTime = scr_lagometer.inTime[(scr_lagometer.inPacketNum - 1) & LAG_MASK];

	size = 0.0f;
	if( startTime != endTime )
	{
		for( ; i<scr_lagometer.inPacketNum ; i++ )
			size += scr_lagometer.inSize[i & LAG_MASK];

		size /= endTime - startTime;
		size *= 1000.0f / 1024.0f;
	}
	Com_sprintf( string, sizeof( string ), "%1.2f", size );
	SCR_DrawString( x + LAG_WIDTH/2, y + 12, string, DSF_CENTERX );

//
// draw upload speed
//
	i = scr_lagometer.outPacketNum - LAG_SAMPLES/8 + 1;
	if( i < 0 )
		i = 0;

	startTime = scr_lagometer.outTime[i & LAG_MASK];
	endTime = scr_lagometer.outTime[(scr_lagometer.outPacketNum - 1) & LAG_MASK];

	size = 0.0f;
	if( startTime != endTime )
	{
		for( ; i<scr_lagometer.outPacketNum ; i++ )
			size += scr_lagometer.outSize[i & LAG_MASK];

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
#define CHAT_MASK			(MAX_CHAT_LINES-1)

typedef struct chatMessage_s {
	char	text[MAX_CHAT_LENGTH];
	int		time;
	int		color;
} chatMessage_t;

static chatMessage_t	chatMsgs[MAX_CHAT_LINES];
static int	chatMsgsNum = 0;

cvar_t *cl_chathud;
cvar_t *cl_chathudlines;
cvar_t *cl_chathudtime;
cvar_t *cl_chathudx;
cvar_t *cl_chathudy;

static void OnChange_Chathudlines (cvar_t *self, const char *oldValue)
{
	if (self->integer > MAX_CHAT_LINES)
		Cvar_SetValue (self->name, MAX_CHAT_LINES);
	else if (self->integer < 1)
		Cvar_SetValue (self->name, 1);
}

/*
==============
SCR_ClearChatHUD_f
==============
*/
void SCR_ClearChatHUD_f( void )
{
	memset(chatMsgs, 0, sizeof(chatMsgs));
	chatMsgsNum = 0;
}

/*
==============
SCR_AddToChatHUD
==============
*/
void SCR_AddToChatHUD( const char *string, int color, qboolean mm2 )
{
	char *p;
	chatMessage_t *msg;

	if(cl_chathud->integer > 2 && !mm2)
		return;
	
	msg = &chatMsgs[chatMsgsNum++ & CHAT_MASK];
	msg->time = cls.realtime;
	msg->color = color;
	Q_strncpyz(msg->text, string, MAX_CHAT_LENGTH);
	p = strchr(msg->text, '\n' );
	if( p )
		*p = 0;
}

/*
==============
SCR_DrawChatHUD
==============
*/
static void SCR_DrawChatHUD( void )
{
    int i, y = viddef.height-22, x = 5;
	float time = cl_chathudtime->value * 1000, alpha = 1;
	qboolean altColor;
	chatMessage_t *msg;

	altColor = cl_chathud->integer & 2;

	if(cl_chathudx->integer || cl_chathudy->integer)
	{
		x = cl_chathudx->integer;
		y = cl_chathudy->integer;
	}

    for (i = chatMsgsNum-1; i >= 0 && i > chatMsgsNum-1 - cl_chathudlines->integer; i--)
	{
		msg = &chatMsgs[i & CHAT_MASK];
		if( time ) {
			if(cls.realtime - msg->time > time ) {
				msg->text[0] = 0;
				msg->time = 0;
				break;
			}
			alpha = (msg->time+time-cls.realtime) * (cl_chathudtime->value < 4 ? 0.001f : 0.0005f);
		}
		y -= 8;
		Draw_String(x, y, msg->text, msg->color, alpha, msg->color != 7 ? 0 : altColor);
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
	char timebuf[32];
	time_t clock;

	time( &clock );
	strftime( timebuf, sizeof(timebuf), cl_clockformat->string, localtime(&clock));

	if(cl_clockx->integer || cl_clocky->integer)
		DrawColorString(cl_clockx->integer, cl_clocky->integer, timebuf, cl_clock->integer, 1);
	else
		DrawColorString(5, viddef.height-10, timebuf, cl_clock->integer, 1);
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

	index++;

	if (index < FPS_FRAMES)
		return;

	if ((index % FPS_FRAMES) == 0)
	{
		Com_sprintf(fps, 32, "%ifps",
			(int) (1000 / ((float) (Sys_Milliseconds() - prevTime) / FPS_FRAMES)));
		prevTime = Sys_Milliseconds();
	}

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

	if(cl_maptime->integer > 10) {
		time = (cl.time - cls.roundtime) / 1000;
		color = cl_maptime->integer - 10;
	} else {
		time = cl.time / 1000;
		color = cl_maptime->integer;
	}

	hour = time/3600;
	mins = (time%3600) /60;
	secs = time%60;
	
	if (hour > 0)
		Com_sprintf(temp, sizeof(temp), "%i:%02i:%02i", hour, mins, secs);
	else
		Com_sprintf(temp, sizeof(temp), "%i:%02i", mins, secs);

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
extern	char		crosshair_pic[8];
extern	int			crosshair_width, crosshair_height;

cvar_t	*crosshair;
cvar_t	*ch_alpha;
cvar_t	*ch_pulse;
cvar_t	*ch_scale;
cvar_t	*ch_red;
cvar_t	*ch_green;
cvar_t	*ch_blue;
cvar_t	*ch_health;
cvar_t	*ch_x;
cvar_t	*ch_y;

static void OnChange_Crosshair(cvar_t *self, const char *oldValue)
{
	SCR_TouchPics ();
}

/*
=================
SCR_DrawCrosshair
=================
*/
static void SCR_DrawCrosshair (void)
{
	float	alpha;

	if (!crosshair_pic[0])
		return;

	if (ch_pulse->value)
		alpha = (0.75f*ch_alpha->value) + (0.25f*ch_alpha->value)*(float)sin(anglemod((cl.time*0.005)*ch_pulse->value));
	else
		alpha = ch_alpha->value;

	if(ch_health->integer)
	{
		float red, green, blue;
		int health = (int)cl.frame.playerstate.stats[STAT_HEALTH];
	
		if(health <= 0)
		{
			red = 0;
			green = 0;
			blue = 0;
		}
		else
		{
			red = 1.0f;
			if ( health >= 100 )
				blue = 1.0;
			else if ( health < 66 )
				blue = 0;
			else
				blue = ( health - 66 ) / 33.0f;

			if ( health > 60 )
				green = 1.0f;
			else if ( health < 30 )
				green = 0;
			else
				green = ( health - 30 ) / 30.0f;
		}
		Draw_ScaledPic (scr_vrect.x + ch_x->integer + ((scr_vrect.width - crosshair_width)>>1)
			, scr_vrect.y + ch_y->integer + ((scr_vrect.height - crosshair_height)>>1), ch_scale->value, crosshair_pic, red, green, blue, alpha);
	}
	else
	{
		Draw_ScaledPic (scr_vrect.x + ch_x->integer + ((scr_vrect.width - crosshair_width)>>1)
		, scr_vrect.y + ch_y->integer + ((scr_vrect.height - crosshair_height)>>1), ch_scale->value, crosshair_pic, ch_red->value, ch_green->value, ch_blue->value, alpha);
	}
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

	if (crosshair->integer)
		SCR_DrawCrosshair ();

	// draw status bar
	SCR_ExecuteLayoutString( cl.configstrings[CS_STATUSBAR] );

	// draw layout
	if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 1)
		SCR_ExecuteLayoutString( cl.layout );
	// draw inventory
	if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 2)
		CL_DrawInventory ();

	SCR_DrawNet ();
	SCR_CheckDrawCenterString ();

	if(scr_drawlagometer->integer)
		SCR_DrawLagometer();

	if(cl_fps->integer)
		SCR_DrawFPS ();

	if(cl_maptime->integer)
		SCR_ShowTIME ();

	if(cl_chathud->integer)
		SCR_DrawChatHUD ();

	if(cl_clock->integer)
		SCR_DrawClock ();
}


/*
================
SCR_InitDraw
================
*/
void SCR_InitDraw( void )
{
	scr_draw2d = Cvar_Get("scr_draw2d", "1", 0);

	crosshair = Cvar_Get ("crosshair", "0", CVAR_ARCHIVE);
	crosshair->OnChange = OnChange_Crosshair;

	ch_alpha  = Cvar_Get("ch_alpha", "1", CVAR_ARCHIVE);
	ch_pulse  = Cvar_Get("ch_pulse", "0", CVAR_ARCHIVE);
	ch_scale  = Cvar_Get("ch_scale", "1", CVAR_ARCHIVE);
	ch_red    = Cvar_Get("ch_red",   "1", CVAR_ARCHIVE);
	ch_green  = Cvar_Get("ch_green", "1", CVAR_ARCHIVE);
	ch_blue   = Cvar_Get("ch_blue",  "1", CVAR_ARCHIVE);
	ch_health = Cvar_Get("ch_health", "0", 0);
	ch_x	  = Cvar_Get("ch_x", "0", 0);
	ch_y	  = Cvar_Get("ch_y", "0", 0);

	cl_clock =  Cvar_Get("cl_clock",  "0", CVAR_ARCHIVE);
	cl_clockx = Cvar_Get("cl_clockx", "0", 0);
	cl_clocky = Cvar_Get("cl_clocky", "0", 0);
	cl_clockformat = Cvar_Get("cl_clockformat", "%H:%M:%S", 0);

	cl_maptime =  Cvar_Get("cl_maptime",  "0", CVAR_ARCHIVE);
	cl_maptimex = Cvar_Get("cl_maptimex", "0", 0);
	cl_maptimey = Cvar_Get("cl_maptimey", "0", 0);

	cl_fps =  Cvar_Get("cl_fps",  "0", CVAR_ARCHIVE);
	cl_fpsx = Cvar_Get("cl_fpsx", "0", 0);
	cl_fpsy = Cvar_Get("cl_fpsy", "0", 0);

	cl_chathud =  Cvar_Get("cl_chathud",  "0", CVAR_ARCHIVE);
	cl_chathudx = Cvar_Get("cl_chathudx", "0", 0);
	cl_chathudy = Cvar_Get("cl_chathudy", "0", 0);
	cl_chathudlines = Cvar_Get("cl_chathudlines", "4", CVAR_ARCHIVE);
	cl_chathudtime =  Cvar_Get("cl_chathudtime", "0", 0);
	cl_chathudlines->OnChange = OnChange_Chathudlines;
	OnChange_Chathudlines(cl_chathudlines, cl_chathudlines->resetString);

	scr_drawlagometer = Cvar_Get("scr_drawlagometer", "0", 0);

}

