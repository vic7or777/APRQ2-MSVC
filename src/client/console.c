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
// console.c

#include "client.h"

#define		MAX_FIELD_TEXT	256
#define		HISTORY_LINES	32
#define		HISTORY_MASK	(HISTORY_LINES-1)

console_t	con;

cvar_t		*con_notifytime;

cvar_t		*con_notifylines;	//Notifylines
cvar_t		*con_notifyfade;	//Notifyfade
cvar_t		*con_alpha;			//transparent console
cvar_t		*con_scrlines;

typedef struct
{
	char	text[HISTORY_LINES][MAX_FIELD_TEXT];
	int		cursorPos;
	int		editLine;
	int		historyLine;
} inputField_t;

static inputField_t con_inputLines;
static inputField_t chat_inputLines;

qboolean	key_insert	= true;
qboolean	chat_team;

void DrawString (int x, int y, const char *s)
{
	while (*s)
	{
		Draw_Char (x, y, *s, COLOR_WHITE, 1);
		x+=8;
		s++;
	}
}

void DrawAltString (int x, int y, const char *s)
{
	while (*s)
	{
		Draw_Char (x, y, *s ^ 0x80, COLOR_WHITE, 1);
		x+=8;
		s++;
	}
}

void DrawString2 (int x, int y, const char *s, float alpha)
{
	int flags = COLOR_WHITE;
	qboolean colors = true;


	if(Q_IsColorString( s ) && !strncmp(s+2, S_DISABLE_COLOR, 3))
	{
		if(cl_textcolors->integer)
			flags = ColorIndex(s[1]);

		colors = false;
		s += 5;
	}

	while (*s)
	{

		if ( Q_IsColorString( s ) && colors )
		{
			if(cl_textcolors->integer)
				flags = ColorIndex(s[1]);

			s += 2;
			continue;
		}

		Draw_Char (x, y, *s, flags, alpha);
		x+=8;
		s++;
	}
}

void DrawColorString (int x, int y, const char *s, int color, float alpha)
{
	while (*s)
	{
		Draw_Char (x, y, *s, ColorIndex(clamp(color, 0, 7)), alpha);
		x+=8;
		s++;
	}
}

void Draw_StringLen (int x, int y, char *str, int len, float alpha)
{
	char saved_byte;

	if (len < 0)
		DrawString2 (x, y, str, alpha);

	saved_byte = str[len];
	str[len] = 0;
	DrawString2 (x, y, str, alpha);
	str[len] = saved_byte;
}

void Key_ClearTyping (void)
{
	memset(con_inputLines.text[con_inputLines.editLine], 0, sizeof(con_inputLines.text[con_inputLines.editLine])); // clear any typing
	con_inputLines.cursorPos = 0;
	con_inputLines.historyLine = con_inputLines.editLine;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	SCR_EndLoadingPlaque ();	// get rid of loading plaque

	//Changed Usable console during demoplay, -Maniac
	/*
	if (cl.attractloop)
	{
		Cbuf_AddText ("killserver\n");
		return;
	}
	*/

	if (cls.state == ca_disconnected)
	{	// start the demo loop again
		//Cbuf_AddText ("d1\n");
		return;
	}

	Key_ClearTyping ();
	Con_ClearNotify ();

	if (cls.key_dest == key_console)
	{
		M_ForceMenuOff ();
		Cvar_Set ("paused", "0");
	}
	else
	{
		M_ForceMenuOff ();
		cls.key_dest = key_console;	

		if (Cvar_VariableValue ("maxclients") == 1 && Com_ServerState () && !cl.attractloop)
			Cvar_Set ("paused", "1");
	}
}

/*
================
Con_ToggleChat_f
================
*/
void Con_ToggleChat_f (void)
{
	Key_ClearTyping ();

	if (cls.key_dest == key_console)
	{
		if (cls.state == ca_active)
		{
			M_ForceMenuOff ();
			cls.key_dest = key_game;
		}
	}
	else
		cls.key_dest = key_console;
	
	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	memset (con.text, ' ', CON_TEXTSIZE);
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f (void)
{
	int		l, x;
	char	*line;
	FILE	*f;
	char	buffer[1024];
	char	name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: condump <filename>\n");
		return;
	}

	Com_sprintf (name, sizeof(name), "%s/%s.txt", FS_Gamedir(), Cmd_Argv(1));

	Com_Printf ("Dumped console text to %s.\n", name);
	FS_CreatePath (name);
	f = fopen (name, "w");
	if (!f)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1 ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x=0 ; x<con.linewidth ; x++)
			if (line[x] != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	// write the remaining lines
	buffer[con.linewidth] = 0;
	for ( ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		strncpy (buffer, line, con.linewidth);
		for (x=con.linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x=0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		fprintf (f, "%s\n", buffer);
	}

	fclose (f);
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	memset (con.times, 0, sizeof (con.times));
}

						
/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	chat_team = false;
	cls.key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	chat_team = true;
	cls.key_dest = key_message;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];


	width = (viddef.width >> 3) - 2;

	if (width == con.linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		memset (con.text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;
	
		if (con.linewidth < numchars)
			numchars = con.linewidth;

		memcpy (tbuf, con.text, CON_TEXTSIZE);
		memset (con.text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
	con.linewidth = -1;

	Con_CheckResize ();
	
	Com_Printf ("Console initialized.\n");

//
// register our commands
//
	con_notifytime = Cvar_Get ("con_notifytime", "3", 0);

	con_notifylines = Cvar_Get("con_notifylines","4", CVAR_ARCHIVE); //Notifylines -Maniac
	con_notifyfade = Cvar_Get("con_notifyfade","0", CVAR_ARCHIVE); //Notify fade
	con_scrlines = Cvar_Get("con_scrlines", "2", CVAR_ARCHIVE);

	//transparent console -Maniac
	con_alpha = Cvar_Get ("con_alpha", "0.6", CVAR_ARCHIVE);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	con.initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	memset (&con.text[(con.current%con.totallines)*con.linewidth]
	, ' ', con.linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Con_Print (const char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;

	if (!con.initialized)
		return;

	if (txt[0] == 1 || txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;


	while ( (c = *txt) )
	{
	// count word length
		for (l=0 ; l< con.linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con.linewidth && (con.x + l > con.linewidth) )
			con.x = 0;

		txt++;

		if (cr)
		{
			con.current--;
			cr = false;
		}

		
		if (!con.x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con.current >= 0)
				con.times[con.current % MAX_CON_TIMES] = cls.realtime;
		}

		switch (c)
		{
		case '\n':
			con.x = 0;
			break;

		case '\r':
			con.x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = c | mask | con.ormask;
			con.x++;
			if (con.x >= con.linewidth)
				con.x = 0;
			break;
		}
		
	}
}


/*
==============
Con_CenteredPrint
==============
*/
void Con_CenteredPrint (const char *text)
{
	int		l;
	char	buffer[1024];

	l = strlen(text);
	l = (con.linewidth-l)*0.5;
	if (l < 0)
		l = 0;
	memset (buffer, ' ', l);
	strcpy (buffer+l, text);
	strcat (buffer, "\n");
	Con_Print (buffer);
}

/*
==============================================================================

DRAWING

==============================================================================
*/
void Draw_Input( const char *text, int x, int y, int curPos )
{
	int cursorPos;
	int i, len = strlen(text);

	cursorPos = curPos;

	// prestep if horizontally scrolling
	if (cursorPos >= con.linewidth - x/8 + 2)
	{
		cursorPos = con.linewidth - 1 - x/8 + 2;
		text += curPos - cursorPos;
	}

	for( i=0 ; i<con.linewidth && text[i]; i++ )
		Draw_Char( x + (i<<3), y, text[i], COLOR_WHITE, 1);
	
	// add the cursor frame
	if ((int)(cls.realtime>>8)&1)
	{
		if (curPos == len)
			Draw_Char ( x+cursorPos*8, y, 11, COLOR_WHITE, 1);
		else
			Draw_Char ( x+cursorPos*8, y+4, key_insert ? '_' : 11, COLOR_WHITE, 1);
	}
}
/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	int		x = 8, y = con.vislines - 22;

	if (cls.key_dest == key_menu)
		return;
	if (cls.key_dest != key_console && cls.state == ca_active)
		return;		// don't draw anything (always draw if not active)

	// draw command prompt
	Draw_Char( x, y, ']', COLOR_WHITE, 1);
	x += 8;

	// draw it
	Draw_Input(con_inputLines.text[con_inputLines.editLine], x, y, con_inputLines.cursorPos);
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		v;
	char	*text;
	int		i;
	int		time;
	int		skip;
	float	alpha = 1;
	int lines;

	v = 0;
	
	lines = con_notifylines->integer;
	clamp(lines, 0, MAX_CON_TIMES);

	if (lines)
	{
		for (i= con.current - lines + 1 ; i<=con.current ; i++)
		{
			if (i < 0)
				continue;
			time = con.times[i % MAX_CON_TIMES];
			if (time == 0)
				continue;
			time = cls.realtime - time;
			if (time > con_notifytime->value*1000)
				continue;
			text = con.text + (i % con.totallines)*con.linewidth;
			
			if (con_notifyfade->value)
				alpha = 0.1 + 0.9*(con_notifytime->value-(time*0.0015)+(con_notifytime->value/2)) / con_notifytime->value;

			Draw_StringLen (8, v, text, con.linewidth, alpha);

			v += 8;
		}
	}


	if (cls.key_dest == key_message)
	{
		if (chat_team)
		{
			DrawString (8, v, "say_team:");
			skip = 11;
		}
		else
		{
			DrawString (8, v, "say:");
			skip = 6;
		}

		Draw_Input(chat_inputLines.text[chat_inputLines.editLine], skip*8, v, chat_inputLines.cursorPos);
		v += 8;
	}
	
	if (v)
	{
		SCR_AddDirtyPoint (0,0);
		SCR_AddDirtyPoint (viddef.width-1, v);
	}
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawConsole (float frac, qboolean ingame)
{
	int				i, j, x, y, n;
	int				rows;
	char			*text;
	int				row;
	int				lines;
	char			version[64];
	char			dlbar[1024];
	float			alpha = 1;

	lines = viddef.height * frac;
	if (lines <= 0)
		return;

	if (lines > viddef.height)
		lines = viddef.height;

	if (ingame)
		alpha = con_alpha->value;

	// draw the background
	Draw_StretchPic (0, lines-viddef.height, viddef.width, viddef.height, "conback", alpha);
	SCR_AddDirtyPoint (0,0);
	SCR_AddDirtyPoint (viddef.width-1,lines-1);

	Com_sprintf (version, sizeof(version), "%s v%s", APR_APPNAME, APR_VERSION);

	for (x = 0; x < strlen(version); x++)
		Draw_Char (viddef.width-(strlen(version)*8+4)+x*8, lines-12, version[x] + 128, COLOR_WHITE, 1 );


// draw the text
	con.vislines = lines;
	
	rows = (lines-22)>>3;		// rows of text to draw

	y = lines - 30;

// draw from the bottom up
	if (con.display != con.current)
	{
	// draw arrows to show the buffer is backscrolled
		for (x=0 ; x<con.linewidth ; x+=4)
			Draw_Char ( (x+1)<<3, y, '^', COLOR_WHITE, 1);
	
		y -= 8;
		rows--;
	}
	
	row = con.display;
	for (i=0 ; i<rows ; i++, y-=8, row--)
	{
		if (row < 0)
			break;
		if (con.current - row >= con.totallines)
			break;		// past scrollback wrap point
			
		text = con.text + (row % con.totallines)*con.linewidth;

		Draw_StringLen (8, y, text, con.linewidth, 1);
	}

//ZOID
	// draw the download bar
	// figure out width
	if (cls.download) {
		if ((text = strrchr(cls.downloadname, '/')) != NULL)
			text++;
		else
			text = cls.downloadname;

		x = con.linewidth - ((con.linewidth * 7) / 40);
		y = x - strlen(text) - 8;
		i = con.linewidth/3;
		if (strlen(text) > i) {
			y = x - i - 11;
			strncpy(dlbar, text, i);
			dlbar[i] = 0;
			strcat(dlbar, "...");
		} else
			strcpy(dlbar, text);
		strcat(dlbar, ": ");
		i = strlen(dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;
			
		for (j = 0; j < y; j++)
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		sprintf(dlbar + strlen(dlbar), " %02d%%", cls.downloadpercent);

		// draw it
		y = con.vislines-12;
		for (i = 0; i < strlen(dlbar); i++)
			Draw_Char ( (i+1)<<3, y, dlbar[i], COLOR_WHITE, 1);
	}
//ZOID

// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

void IF_Init( inputField_t *field )
{
	memset( field->text[field->editLine], 0, sizeof(field->text[field->editLine]) );
	field->cursorPos = 0;
}

qboolean Cmd_IsComplete (const char *cmd);
void CompleteCommand (void)
{
	char	*cmd, *text;

	text = con_inputLines.text[con_inputLines.editLine];
	if (*text == '\\' || *text == '/')
		text++;

	if( *text == '\0' )
		return;

	cmd = Cmd_CompleteCommand (text);
	if (cmd)
	{
		IF_Init(&con_inputLines);
		con_inputLines.text[con_inputLines.editLine][0] = '/';
		strcpy (con_inputLines.text[con_inputLines.editLine]+1, cmd);
		con_inputLines.cursorPos = strlen(con_inputLines.text[con_inputLines.editLine]);
		if (Cmd_IsComplete(cmd)) {
			con_inputLines.text[con_inputLines.editLine][con_inputLines.cursorPos] = ' ';
			con_inputLines.cursorPos++;
		}
		return;
	}
}

void IF_CharEvent( inputField_t *field, int key )
{
	if( key < 32 || key > 127 )
		return;	// non printable

	if( field->cursorPos >= MAX_FIELD_TEXT - 1 )
		return;

	if( key_insert )
		memmove( field->text[field->editLine] + field->cursorPos + 1, field->text[field->editLine] + field->cursorPos, sizeof(field->text[field->editLine]) - field->cursorPos - 1 );


	field->text[field->editLine][field->cursorPos] = key;
	field->cursorPos++;
}


void IF_KeyEvent( inputField_t *field, int key )
{
	if ( ( toupper( key ) == 'V' && Key_IsDown(K_CTRL) ) ||
		 ( (key == K_INS || key == K_KP_INS) && Key_IsDown(K_SHIFT) ) )
	{
		char *cbd;
		
		if ( (cbd = Sys_GetClipboardData()) != 0 )
		{
			int i;

			//strtok( cbd, "\n\r\b" );

			for( i=0; cbd[i]; i++ )
			{
				if(cbd[i] == '\n')
					cbd[i] = ' ';

				IF_CharEvent (field, cbd[i]);
			}
			free( cbd );
		}

		return;
	}

	if ( key == K_UPARROW || key == K_KP_UPARROW || (key == 'p' && Key_IsDown(K_CTRL)) )
	{
		int tmp = field->historyLine;
		do
		{
			field->historyLine = (field->historyLine - 1) & HISTORY_MASK;
		} while (field->historyLine != field->editLine && !field->text[field->historyLine][0]);

		if (field->historyLine == field->editLine)
		{
			field->historyLine = tmp;
			return;
		}

		IF_Init(field);

		strcpy(field->text[field->editLine], field->text[field->historyLine]);
		field->cursorPos = strlen(field->text[field->editLine]);
		return;
	}

	if ( key == K_DOWNARROW || key == K_KP_DOWNARROW || (key == 'n' && Key_IsDown(K_CTRL)) )
	{
		if (field->historyLine == field->editLine)
			return;
		do
		{
			field->historyLine = (field->historyLine + 1) & HISTORY_MASK;
		} while (field->historyLine != field->editLine && !field->text[field->historyLine][0]);

		IF_Init(field);

		if (field->historyLine != field->editLine)
		{
			strcpy(field->text[field->editLine], field->text[field->historyLine]);
			field->cursorPos = strlen(field->text[field->editLine]);
		}
		return;
	}

	if (key == K_HOME || key == K_KP_HOME )
	{
		field->cursorPos = 0;
		return;
	}

	if (key == K_END || key == K_KP_END )
	{
		field->cursorPos = strlen(field->text[field->editLine]);
		return;
	}

	if ( key == K_DEL )
	{
		if (field->text[field->editLine][field->cursorPos])
			memmove( field->text[field->editLine] + field->cursorPos, field->text[field->editLine] + field->cursorPos + 1, sizeof( field->text[field->editLine] ) - field->cursorPos );

		return;
	}

	if ( key == K_BACKSPACE )
	{
		if (field->cursorPos > 0)
		{
			memmove(field->text[field->editLine] + field->cursorPos - 1, field->text[field->editLine] + field->cursorPos, sizeof( field->text[field->editLine] ) - field->cursorPos );
			field->cursorPos--;
		}
		return;
	}

	if ( key == K_LEFTARROW || key == K_KP_LEFTARROW || (key == 'h' && Key_IsDown(K_CTRL)) )
	{
		if(field->cursorPos > 0)
			field->cursorPos--;

		return;
	}

	if ( key == K_RIGHTARROW )
	{
		if (field->text[field->editLine][field->cursorPos])
			field->cursorPos++;

		return;
	}

	if ( key == K_INS )
	{ // toggle insert mode
		key_insert = !key_insert;
		return;
	}

	IF_CharEvent ( field, key);
}

/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
void Key_Console (int key)
{
	switch ( key )
	{
	case K_KP_SLASH:
		key = '/';
		break;
	case K_KP_MINUS:
		key = '-';
		break;
	case K_KP_PLUS:
		key = '+';
		break;
	case K_KP_HOME:
		key = '7';
		break;
	case K_KP_UPARROW:
		key = '8';
		break;
	case K_KP_PGUP:
		key = '9';
		break;
	case K_KP_LEFTARROW:
		key = '4';
		break;
	case K_KP_5:
		key = '5';
		break;
	case K_KP_RIGHTARROW:
		key = '6';
		break;
	case K_KP_END:
		key = '1';
		break;
	case K_KP_DOWNARROW:
		key = '2';
		break;
	case K_KP_PGDN:
		key = '3';
		break;
	case K_KP_INS:
		key = '0';
		break;
	case K_KP_DEL:
		key = '.';
		break;
	}

	if ( key == 'l' && Key_IsDown(K_CTRL) ) 
	{
		Con_Clear_f();
		return;
	}

	if ( key == K_ENTER || key == K_KP_ENTER )
	{	// backslash text are commands, else chat
		if (con_inputLines.text[con_inputLines.editLine][0] == '\\' || con_inputLines.text[con_inputLines.editLine][0] == '/')
			Cbuf_AddText (con_inputLines.text[con_inputLines.editLine]+1);	// skip the >
		else
			Cbuf_AddText (con_inputLines.text[con_inputLines.editLine]);	// valid command

		Cbuf_AddText ("\n");

		Com_Printf ("]%s\n", con_inputLines.text[con_inputLines.editLine]);

		con_inputLines.editLine = (con_inputLines.editLine + 1) & HISTORY_MASK;
		con_inputLines.historyLine = con_inputLines.editLine;
		IF_Init(&con_inputLines);

		if (cls.state == ca_disconnected)
			SCR_UpdateScreen ();	// force an update, because the command
									// may take some time
		return;
	}

	if (key == K_TAB)
	{	// command completion
		CompleteCommand();
		return;
	}

	if (key == K_PGUP || key == K_KP_PGUP || key == K_MWHEELUP)
	{
		if (con_scrlines->integer < 1)
			Cvar_SetValue ("con_scrlines", 1);

		con.display -= con_scrlines->integer;
		return;
	}

	if (key == K_PGDN || key == K_KP_PGDN || key == K_MWHEELDOWN)
	{
		if (con_scrlines->integer < 1)
			Cvar_SetValue ("con_scrlines", 1);

		con.display += con_scrlines->integer;
		if (con.display > con.current)
			con.display = con.current;
		return;
	}

	if (key == K_HOME || key == K_KP_HOME )
	{
		if (Key_IsDown(K_CTRL) || !con_inputLines.text[con_inputLines.editLine][0])
		{
			con.display = con.current - con.totallines + 10;
			return;
		}
	}

	if (key == K_END || key == K_KP_END )
	{
		if (Key_IsDown(K_CTRL) || !con_inputLines.text[con_inputLines.editLine][0])
		{
			con.display = con.current;
			return;
		}
	}

	IF_KeyEvent (&con_inputLines, key);

}

/*
====================
Key_Message

Interactive line editing
====================
*/

void Key_Message (int key)
{
	switch ( key )
	{
	case K_KP_SLASH:
		key = '/';
		break;
	case K_KP_MINUS:
		key = '-';
		break;
	case K_KP_PLUS:
		key = '+';
		break;
	case K_KP_HOME:
		key = '7';
		break;
	case K_KP_UPARROW:
		key = '8';
		break;
	case K_KP_PGUP:
		key = '9';
		break;
	case K_KP_LEFTARROW:
		key = '4';
		break;
	case K_KP_5:
		key = '5';
		break;
	case K_KP_RIGHTARROW:
		key = '6';
		break;
	case K_KP_END:
		key = '1';
		break;
	case K_KP_DOWNARROW:
		key = '2';
		break;
	case K_KP_PGDN:
		key = '3';
		break;
	case K_KP_INS:
		key = '0';
		break;
	case K_KP_DEL:
		key = '.';
		break;
	}

	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		if (chat_team)
			Cbuf_AddText ("say_team \"");
		else
			Cbuf_AddText ("say \"");

		Cbuf_AddText(chat_inputLines.text[chat_inputLines.editLine]);
		Cbuf_AddText("\"\n");

		chat_inputLines.editLine = (chat_inputLines.editLine + 1) & HISTORY_MASK;
		chat_inputLines.historyLine = chat_inputLines.editLine;
		IF_Init(&chat_inputLines);
		cls.key_dest = key_game;
		return;
	}

	if (key == K_ESCAPE)
	{
		chat_inputLines.historyLine = chat_inputLines.editLine;
		IF_Init(&chat_inputLines);
		cls.key_dest = key_game;
		return;
	}

	IF_KeyEvent (&chat_inputLines, key);
}
