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
// cmd.c -- Quake script command processing module

#include "qcommon.h"

void Cmd_ForwardToServer (void);

#define	MAX_ALIAS_NAME	32
#define	ALIAS_LOOP_COUNT	16

typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	struct cmdalias_s	*hashNext;

	char	name[MAX_ALIAS_NAME];
	char	*value;
} cmdalias_t;

#define ALIAS_HASH_SIZE	64
static cmdalias_t	*cmd_aliasHash[ALIAS_HASH_SIZE];

cmdalias_t	*cmd_alias;

qboolean	cmd_wait;

int		alias_count;		// for detecting runaway loops


//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f (void)
{
	cmd_wait = true;
}


/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

sizebuf_t	cmd_text;

byte		cmd_text_buf[32768];
byte		defer_text_buf[32768];

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	SZ_Init (&cmd_text, cmd_text_buf, sizeof(cmd_text_buf));
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (char *text)
{
	int		l;
	
	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Com_Printf ("Cbuf_AddText: overflow\n");
		return;
	}
	SZ_Write (&cmd_text, text, l);
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (char *text)
{
	char	*temp;
	int		templen;

	// copy off any commands still remaining in the exec buffer
	templen = cmd_text.cursize;
	if (templen)
	{
		temp = Z_Malloc (templen);
		memcpy (temp, cmd_text.data, templen);
		SZ_Clear (&cmd_text);
	}
	else
		temp = NULL;	// shut up compiler
		
	// add the entire text of the file
	Cbuf_AddText (text);
	
	if( text[strlen( text ) - 1] != '\n' )
		Cbuf_AddText( "\n" );

	// add the copied off data
	if (templen)
	{
		SZ_Write (&cmd_text, temp, templen);
		Z_Free (temp);
	}
}


/*
============
Cbuf_CopyToDefer
============
*/
void Cbuf_CopyToDefer (void)
{
	if (!cmd_text.cursize)
		return;

	memcpy (defer_text_buf, cmd_text_buf, cmd_text.cursize);
	defer_text_buf[cmd_text.cursize] = 0;
	cmd_text.cursize = 0;
}

/*
============
Cbuf_InsertFromDefer
============
*/
void Cbuf_InsertFromDefer (void)
{
	Cbuf_InsertText ((char *)defer_text_buf);
	defer_text_buf[0] = 0;
}


/*
============
Cbuf_ExecuteText
============
*/
void Cbuf_ExecuteText (int exec_when, char *text)
{
	switch (exec_when)
	{
	case EXEC_NOW:
		Cmd_ExecuteString (text);
		break;
	case EXEC_INSERT:
		Cbuf_InsertText (text);
		break;
	case EXEC_APPEND:
		Cbuf_AddText (text);
		break;
	default:
		Com_Error (ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int		i;
	char	*text;
	char	line[1024];
	int		quotes;

	alias_count = 0;		// don't allow infinite alias loops

	while (cmd_text.cursize)
	{
		// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i = 0; i < cmd_text.cursize; i++)
		{
			if (text[i] == '"')
				quotes++;
			if ( !(quotes&1) &&  text[i] == ';')
				break;	// don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}
			
		if (i >= sizeof(line)-1) {
			Com_Printf ("Cbuf_Execute: overflow of %d truncated\n", i);
			memcpy (line, text, sizeof(line)-1);
		} else {				
			memcpy (line, text, i);
		}
		line[i] = 0;
		
		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec, alias) can insert data at the
		// beginning of the text buffer

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			if (cmd_text.cursize)
				memmove (text, text+i, cmd_text.cursize);
		}

		// execute the command line
		Cmd_ExecuteString (line);
		
		if (cmd_wait)
		{
			// skip out while text still remains in buffer, leaving it for next frame
			cmd_wait = false;
			break;
		}
	}
}


/*
===============
Cbuf_AddEarlyCommands

Adds command line parameters as script statements
Commands lead with a +, and continue until another +

Set commands are added early, so they are guaranteed to be set before
the client and server initialize for the first time.

Other commands are added late, after all initialization is complete.
===============
*/
void Cbuf_AddEarlyCommands (qboolean clear)
{
	int		i;
	char	*s;

	for (i = 0; i < COM_Argc(); i++)
	{
		s = COM_Argv(i);
		if (strcmp (s, "+set"))
			continue;
		Cbuf_AddText (va("set %s %s\n", COM_Argv(i+1), COM_Argv(i+2)));
		if (clear)
		{
			COM_ClearArgv(i);
			COM_ClearArgv(i+1);
			COM_ClearArgv(i+2);
		}
		i+=2;
	}
}

/*
=================
Cbuf_AddLateCommands

Adds command line parameters as script statements
Commands lead with a + and continue until another + or -
quake +vid_ref gl +map amlev1

Returns true if any late commands were added, which
will keep the demoloop from immediately starting
=================
*/
qboolean Cbuf_AddLateCommands (void)
{
	int		i, j, s, argc;
	char	*text, *build, c;	
	qboolean	ret;

	// build the combined string to parse from
	s = 0;
	argc = COM_Argc();
	for (i = 1; i < argc; i++)
	{
		s += strlen (COM_Argv(i)) + 1;
	}
	if (!s)
		return false;
		
	text = Z_Malloc (s+1);
	text[0] = 0;
	for (i = 1; i < argc; i++)
	{
		strcat (text,COM_Argv(i));
		if (i != argc-1)
			strcat (text, " ");
	}
	
	// pull out the commands
	build = Z_Malloc (s+1);
	build[0] = 0;
	
	for (i = 0; i < s-1; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j=i ; (text[j] != '+') && (text[j] != '-') && (text[j] != 0) ; j++)
				;

			c = text[j];
			text[j] = 0;
			
			strcat (build, text+i);
			strcat (build, "\n");
			text[j] = c;
			i = j-1;
		}
	}

	ret = (build[0] != 0);
	if (ret)
		Cbuf_AddText (build);
	
	Z_Free (text);
	Z_Free (build);

	return ret;
}


/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/


/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f (void)
{
	char	*f, *f2;
	int		len;

	if (Cmd_Argc () != 2)
	{
		Com_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	len = FS_LoadFile (Cmd_Argv(1), (void **)&f);
	if (!f || !len)
	{
		Com_Printf ("couldn't exec %s\n",Cmd_Argv(1));
		return;
	}
	Com_Printf ("execing %s\n",Cmd_Argv(1));
	
	// the file doesn't have a trailing 0, so we need to copy it off
	f2 = Z_Malloc(len+2);
	memcpy (f2, f, len);
	f2[len] = '\n';
	f2[len+1] = '\0';

	Cbuf_InsertText (f2);

	Z_Free (f2);
	FS_FreeFile (f);
}

/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	int		i;
	
	for (i = 1; i < Cmd_Argc(); i++)
		Com_Printf ("%s ",Cmd_Argv(i));
	Com_Printf ("\n");
}

static int aliassort( const void *_a, const void *_b )
{
	const cmdalias_t	*a = (const cmdalias_t *)_a;
	const cmdalias_t	*b = (const cmdalias_t *)_b;

	return strcmp (a->name, b->name);
}

void Cmd_Aliaslist_f (void)
{
	cmdalias_t	*a;
    int		i, j, c;
    char    *filter = "*";
	int		len, count = 0;
	cmdalias_t *sortedList;

	c = Cmd_Argc();

    if (c == 2)
		filter = Cmd_Argv(1);

	for (a = cmd_alias, i = 0; a ; a = a->next, i++);

	len = i * sizeof(cmdalias_t);
	sortedList = Z_Malloc (len);
	
	for (a = cmd_alias, i = 0; a ; a = a->next, i++)
	{
		sortedList[i] = *a;
	}

	qsort (sortedList, i, sizeof(sortedList[0]), (int (*)(const void *, const void *))aliassort);

	//for (a = cmd_alias ; a ; a=a->next)
	for (j = 0; j < i; j++)
	{
		a = &sortedList[j];
		if (c == 2 && !Com_WildCmp(filter, a->name, 1) && !strstr(a->name, filter))
			continue;


		Com_Printf ("%s : \"%s\"\n", a->name, a->value);
		count++;
	}

	if (c == 2)
		Com_Printf("%i alias found (%i total alias)\n", count, i);
	else
		Com_Printf("%i alias\n", i);

	Z_Free (sortedList);
}

/*
===============
Cmd_AliasFind
===============
*/
static cmdalias_t *Cmd_AliasFind( const char *name )
{
	unsigned int hash;
	cmdalias_t *alias;

	hash = Com_HashKey (name, ALIAS_HASH_SIZE);
	for( alias=cmd_aliasHash[hash]; alias ; alias=alias->hashNext ) {
		if( !Q_stricmp( name, alias->name ) ) {
			return alias;
		}
	}

	return NULL;
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
void Cmd_Alias_f (void)
{
	cmdalias_t	*a;
	char		cmd[1024], *s;
	int			i, c;
	unsigned int hash;

	if (Cmd_Argc() == 1)
	{
		Com_Printf ("usage: alias <name> <command>\n");
		return;
	}

	s = Cmd_Argv(1);

	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Com_Printf ("Alias name is too long\n");
		return;
	}

	if (Cmd_Argc() == 2)
	{
		a = Cmd_AliasFind( s );
		if( a )
			Com_Printf( "\"%s\" = \"%s\"\n", a->name, a->value );
		else
			Com_Printf( "\"%s\" is undefined\n", s );

		return;		
	}

	// if the alias already exists, reuse it
	a = Cmd_AliasFind (s);
	if( a )
	{
		Z_Free (a->value);
	}
	else
	{
		a = Z_Malloc (sizeof(cmdalias_t));
		strcpy (a->name, s);
		a->next = cmd_alias;
		cmd_alias = a;

		hash = Com_HashKey( s, ALIAS_HASH_SIZE );
		a->hashNext = cmd_aliasHash[hash];
		cmd_aliasHash[hash] = a;
	}

// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	c = Cmd_Argc();
	for (i=2 ; i< c ; i++)
	{
		strcat (cmd, Cmd_Argv(i));
		if (i != (c - 1))
			strcat (cmd, " ");
	}
	a->value = CopyString (cmd);
}

void Cmd_UnAlias_f (void)
{
	char		*s;
	unsigned int hash;
	cmdalias_t	*a, **back;

	if (Cmd_Argc() == 1)
	{
		Com_Printf ("usage: unalias <name>\n");
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Com_Printf ("Alias name is too long\n");
		return;
	}

	hash = Com_HashKey( s, ALIAS_HASH_SIZE );
	back = &cmd_aliasHash[hash];
	while( 1 ) {
		a = *back;
		if( !a ) {
			Com_Printf ("Cmd_Unalias_f: %s not added\n", s);
			return;
		}
		if( !Q_stricmp( s, a->name ) ){
			*back = a->hashNext;
			break;
		}
		back = &a->hashNext;
	}

	back = &cmd_alias;
	while (1)
	{
		a = *back;
		if (!a)
		{
			Com_Printf ("Cmd_Unalias_f: %s not added\n", s);
			return;
		}
		if (!Q_stricmp (s, a->name))
		{
			*back = a->next;
			Z_Free (a->value);
			Z_Free (a);
			Com_Printf ("Alias \"%s\" removed\n", s);
			return;
		}
		back = &a->next;
	}

}
/*
===================
WriteAliases
===================
*/
void Cmd_WriteAliases (FILE *f)
{
	cmdalias_t	*a;

	for (a = cmd_alias; a; a = a->next)
		fprintf (f, "alias %s \"%s\"\n", a->name, a->value);
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	struct cmd_function_s	*hashNext;

	char					*name;
	xcommand_t				function;
} cmd_function_t;


static	int			cmd_argc;
static	char		*cmd_argv[MAX_STRING_TOKENS];
static	char		*cmd_null_string = "";
static	char		cmd_args[MAX_STRING_CHARS];

static	cmd_function_t	*cmd_functions;		// possible commands to execute

#define CMD_HASH_SIZE	64
static cmd_function_t	*cmd_hash[CMD_HASH_SIZE];

/*
============
Cmd_Argc
============
*/
int Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char *Cmd_Argv (int arg)
{
	if ( (unsigned)arg >= cmd_argc )
		return cmd_null_string;
	return cmd_argv[arg];	
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char *Cmd_Args (void)
{
	return cmd_args;
}


/*
======================
Cmd_MacroExpandString
======================
*/
char *Cmd_MacroExpandString (char *text)
{
	int		i, j, count, len;
	qboolean	inquote;
	char	*scan;
	static	char	expanded[MAX_STRING_CHARS];
	char	temporary[MAX_STRING_CHARS];
	char	*token, *start;

	inquote = false;
	scan = text;

	len = strlen (scan);
	if (len >= MAX_STRING_CHARS)
	{
		Com_Printf ("Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
		return NULL;
	}

	count = 0;

	for (i=0 ; i<len ; i++)
	{
		if (scan[i] == '"')
			inquote ^= 1;
		if (inquote)
			continue;	// don't expand inside quotes
		if (scan[i] != '$')
			continue;
		// scan out the complete macro
		start = scan+i+1;
//		token = COM_Parse (&start);
		if (!*start)
			break;

		while( *start == 32 ) {
			start++;
		}

		// allow $var1$$var2 scripting
		token = temporary;
		while( *start > 32 ) {
			*token++ = *start++;
			if( *start == '$' ) {
				start++;
				break;
			}
		}
		*token = 0;

		if( token == temporary )
			continue;

		token = Cvar_VariableString ( temporary );

		j = strlen(token);
		len += j;
		if (len >= MAX_STRING_CHARS)
		{
			Com_Printf ("Expanded line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
			return NULL;
		}

		strncpy (temporary, scan, i);
		strcpy (temporary+i, token);
		strcpy (temporary+i+j, start);

		strcpy (expanded, temporary);
		scan = expanded;
		i--;

		if (++count == 100)
		{
			Com_Printf ("Macro expansion loop, discarded.\n");
			return NULL;
		}
	}

	if (inquote)
	{
		Com_Printf ("Line has unmatched quote, discarded.\n");
		return NULL;
	}

	return scan;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
$Cvars will be expanded unless they are in a quoted token
============
*/
void Cmd_TokenizeString (char *text, qboolean macroExpand)
{
	int		i;
	char	*com_token;

// clear the args from the last string
	for (i=0 ; i<cmd_argc ; i++)
		Z_Free (cmd_argv[i]);
		
	cmd_argc = 0;
	cmd_args[0] = 0;
	
	// macro expand the text
	if (macroExpand && strstr(text, "$"))
		text = Cmd_MacroExpandString (text);
	if (!text)
		return;

	while (1)
	{
// skip whitespace up to a /n
		while (*text && *text <= ' ' && *text != '\n')
		{
			text++;
		}
		
		if (*text == '\n')
		{	// a newline seperates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		// set cmd_args to everything after the first arg
		if (cmd_argc == 1)
		{
			int		l;

			Q_strncpyz (cmd_args, text, sizeof(cmd_args));

			// strip off any trailing whitespace
			l = strlen(cmd_args) - 1;
			for ( ; l >= 0 ; l--)
				if (cmd_args[l] <= ' ')
					cmd_args[l] = 0;
				else
					break;
		}
			
		com_token = COM_Parse (&text);
		if (!text)
			return;

		if (cmd_argc < MAX_STRING_TOKENS)
		{
			cmd_argv[cmd_argc++] = CopyString( com_token );
		}
	}
	
}

/*
============
Cmd_Find
============
*/
static cmd_function_t *Cmd_Find( const char *name ) {
	cmd_function_t *cmd;
	unsigned int hash;

	hash = Com_HashKey( name, CMD_HASH_SIZE );
	for( cmd=cmd_hash[hash]; cmd; cmd=cmd->hashNext ) {
		if( !Q_stricmp( cmd->name, name ) ) {
			return cmd;
		}
	}

	return NULL;
}

/*
============
Cmd_AddCommand
============
*/
cvar_t *Cvar_FindVar (const char *var_name);

void Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
	cmd_function_t	*cmd;
	unsigned int hash;
	
// fail if the command is a variable name
	if (Cvar_FindVar( cmd_name ))
	{
		Com_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		//return; //Cmd's are priority 1
	}
	
// fail if the command already exists
	if (Cmd_Find( cmd_name ))
	{
		Com_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
		return;
	}

	cmd = Z_Malloc (sizeof(cmd_function_t) + strlen(cmd_name) + 1);
	cmd->name = (char *)((byte *)cmd + sizeof(cmd_function_t));
	strcpy (cmd->name, cmd_name);
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;

	hash = Com_HashKey( cmd_name, CMD_HASH_SIZE );
	cmd->hashNext = cmd_hash[hash];
	cmd_hash[hash] = cmd;
}

/*
============
Cmd_RemoveCommand
============
*/
void Cmd_RemoveCommand (const char *cmd_name)
{
	cmd_function_t	*cmd, **back;
	unsigned int hash;

	hash = Com_HashKey( cmd_name, CMD_HASH_SIZE );
	back = &cmd_hash[hash];
	while( 1 ) {
		cmd = *back;
		if( !cmd ) {
			Com_Printf ("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}
		if( !Q_stricmp( cmd_name, cmd->name ) ){
			*back = cmd->hashNext;
			break;
		}
		back = &cmd->hashNext;
	}

	back = &cmd_functions;
	while (1)
	{
		cmd = *back;
		if (!cmd)
		{
			Com_Printf ("Cmd_RemoveCommand: %s not added\n", cmd_name);
			return;
		}
		if (!Q_stricmp (cmd_name, cmd->name))
		{
			*back = cmd->next;
			Z_Free (cmd);
			return;
		}
		back = &cmd->next;
	}
}


/*
============
Cmd_CompleteCommand
============
*/
char *Cmd_CompleteCommand (const char *partial)
{
	cmd_function_t	*cmd;
	int				len,i,o,p;
	cmdalias_t		*a;
	cvar_t			*cvar;
	char			*pmatch[1024];
	qboolean		diff = false;
	static char		retval[256];

	
	len = strlen(partial);
	
	if (!len)
		return NULL;
		
	// check for exact match
	cmd = Cmd_Find(partial);
	if(cmd)
		return cmd->name;
	a = Cmd_AliasFind(partial);
	if(a)
		return a->name;
	cvar = Cvar_FindVar(partial);
	if(cvar)
		return cvar->name;

	memset(pmatch, 0, 1024);
	i=0;

	// check for partial match
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
		if (!Q_strnicmp (partial,cmd->name, len)) {
			pmatch[i]=cmd->name;
			i++;
		}
	for (a=cmd_alias ; a ; a=a->next)
		if (!Q_strnicmp (partial, a->name, len)) {
			pmatch[i]=a->name;
			i++;
		}
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!Q_strnicmp (partial,cvar->name, len)) {
			pmatch[i]=cvar->name;
			i++;
		}

	if (i)
	{
		if (i == 1)
			return pmatch[0];

		Com_Printf("]/%s\n",partial);
		for (o=0; o<i; o++)
			Com_Printf("   %s\n",pmatch[o]);

		memset(retval, 0, sizeof(retval));
		p=0;

		while (!diff && p < 255)
		{
			retval[p]=pmatch[0][p];
			for (o=0; o<i; o++) {
				if (p > strlen(pmatch[o]))
					continue;
				if (retval[p] != pmatch[o][p]) {
					retval[p] = 0;
					diff=true;
				}
			}
			p++;
		}
		return retval;
	}

	return NULL;
}

qboolean Cmd_IsComplete (const char *command)
{

	// check for exact match
	if(Cmd_Find(command) != NULL)
		return true;

	if(Cmd_AliasFind(command) != NULL)
		return true;

	if(Cvar_FindVar(command) != NULL)
		return true;

	return false;
}


/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void	Cmd_ExecuteString (char *text)
{	
	cmd_function_t	*cmd;
	cmdalias_t		*a;

	Cmd_TokenizeString (text, true);
			
	// execute the command line
	if (!Cmd_Argc())
		return;		// no tokens

	// check functions
	cmd = Cmd_Find(cmd_argv[0]);
	if (cmd)
	{
		if (!cmd->function)
		{	// forward to server command
			Cmd_ExecuteString (va("cmd %s", text));
		}
		else
			cmd->function ();
		return;
	}

	// check alias
	a = Cmd_AliasFind(cmd_argv[0]);
	if (a)
	{
		if (++alias_count == ALIAS_LOOP_COUNT)
		{
			Com_Printf ("ALIAS_LOOP_COUNT\n");
			return;
		}
		Cbuf_InsertText (va("%s\n", a->value));
		return;
	}
	
	// check cvars
	if (Cvar_Command ())
		return;

	// send it as a server command if we are connected
	Cmd_ForwardToServer ();
}

/*
============
Cmd_List_f
============
*/
static int cmdsort( const void *_a, const void *_b )
{
	const cmd_function_t	*a = (const cmd_function_t *)_a;
	const cmd_function_t	*b = (const cmd_function_t *)_b;

	return strcmp (a->name, b->name);
}

void Cmd_List_f (void)
{
    cmd_function_t  *cmd;
    int		i, j, c;
    char	*filter = "*";
	int		len, count = 0;
	cmd_function_t	*sortedList;

    c = Cmd_Argc();

    if (c == 2)
	    filter = Cmd_Argv(1);

	for (cmd = cmd_functions, i = 0; cmd ; cmd = cmd->next, i++);

	len = i * sizeof(cmd_function_t);
	sortedList = Z_Malloc (len);
	
	for (cmd = cmd_functions, i = 0; cmd ; cmd = cmd->next, i++)
	{
		sortedList[i] = *cmd;
	}

	qsort (sortedList, i, sizeof(sortedList[0]), (int (*)(const void *, const void *))cmdsort);

	for (j = 0; j < i; j++)
	{
		cmd = &sortedList[j];
        if (c == 2 && !Com_WildCmp(filter, cmd->name, 1) && !strstr(cmd->name, filter))
			continue;

        Com_Printf("%s\n", cmd->name);
		count++;
    }

    if (c == 2)
		Com_Printf("%i cmds found (%i total cmds)\n", count, i);
    else
		Com_Printf("%i cmds\n", i);

	Z_Free (sortedList);
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
	// register our commands
	Cmd_AddCommand ("cmdlist",Cmd_List_f);
	Cmd_AddCommand ("exec",Cmd_Exec_f);
	Cmd_AddCommand ("echo",Cmd_Echo_f);
	Cmd_AddCommand ("alias",Cmd_Alias_f);
	Cmd_AddCommand ("unalias", Cmd_UnAlias_f);
	Cmd_AddCommand ("aliaslist",Cmd_Aliaslist_f);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
}
