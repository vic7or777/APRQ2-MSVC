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
// cvar.c -- dynamic variable tracking

#include "qcommon.h"

cvar_t	*cvar_vars;

#define CVAR_HASH_SIZE	128
static cvar_t *cvarHash[CVAR_HASH_SIZE];

qboolean CL_CheatsOK(void);

static unsigned int CvarHashValue( const char *name )
{
	int i;
	unsigned int hash = 0;

	for( i = 0; name[i]; i++ )
		hash += tolower(name[i]) * (i+119);

	return hash & (CVAR_HASH_SIZE-1);
}

/*
============
Cvar_InfoValidate
============
*/
static qboolean Cvar_InfoValidate (const char *s)
{
	if ( !s )
		return false;
	if (strchr (s, '\\'))
		return false;
	if (strchr (s, '\"'))
		return false;
	if (strchr (s, ';'))
		return false;
	return true;
}

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (const char *var_name)
{
	cvar_t	*var;
	unsigned int hash;

	hash = CvarHashValue(var_name);
	for (var = cvarHash[hash]; var; var = var->hashNext)
		if (!Q_stricmp (var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue (const char *var_name)
{
	const cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return var->value;
}


/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (const char *var_name)
{
	const cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return "";
	return var->string;
}


/*
============
Cvar_CommandCompletion
============
*/
void Cvar_CommandCompletion ( const char *partial, void(*callback)(const char *name, const char *value) )
{
	const cvar_t		*cvar;
	int			len;
	
	len = strlen(partial);
	
	for ( cvar = cvar_vars ; cvar ; cvar = cvar->next ) {
		if (!Q_strnicmp (partial,cvar->name, len))
			callback( cvar->name, cvar->string );
	}
}


/*
============
Cvar_Get

If the variable already exists, the value will not be set
The flags will be or'ed in if the variable exists.
============
*/
cvar_t *Cvar_Get (const char *var_name, const char *var_value, int flags)
{
	cvar_t	*var;
	int		hash;
	
	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_name))
		{
			Com_Printf("invalid info cvar name\n");
			return NULL;
		}
	}

	hash = CvarHashValue(var_name);
	for (var = cvarHash[hash]; var; var = var->hashNext)
		if (!Q_stricmp (var_name, var->name))
			break;

	if (var)
	{
		if ( ( var->flags & CVAR_USER_CREATED ) && !( flags & CVAR_USER_CREATED ) ) {
			var->flags &= ~CVAR_USER_CREATED;
			Z_Free( var->resetString );
			var->resetString = CopyString ( var_value, TAGMALLOC_CVAR );
		}
		var->flags |= flags;

		return var;
	}

	if (!var_value)
		return NULL;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_value))
		{
			Com_Printf("invalid info cvar value\n");
			return NULL;
		}
	}

	var = Z_TagMalloc (sizeof(*var), TAGMALLOC_CVAR);
	var->name = CopyString (var_name, TAGMALLOC_CVAR);
	var->string = CopyString (var_value, TAGMALLOC_CVAR);
	var->resetString = CopyString (var_value, TAGMALLOC_CVAR);
	var->latched_string = NULL;
	var->modified = true;
	var->OnChange = NULL;
	var->value = atof (var->string);
	var->integer = Q_rint(var->value);

	// link the variable in
	var->next = cvar_vars;
	cvar_vars = var;
	var->hashNext = cvarHash[hash];
	cvarHash[hash] = var;

	var->flags = flags;

	return var;
}

/*
============
Cvar_Set2
============
*/
static cvar_t *Cvar_Set2 (const char *var_name, const char *value, qboolean force)
{
	cvar_t	*var;
	char	*oldValue;

	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, 0);
	}

	if (var->flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (value))
		{
			Com_Printf("invalid info cvar value\n");
			return var;
		}
	}

	if (!value ) {
		value = var->resetString;
	}

	if (!strcmp(value,var->string)) {
		if (var->latched_string)
		{
			Z_Free (var->latched_string);
			var->latched_string = NULL;
		}
		return var;
	}

	if (!force)
	{
		if (var->flags & CVAR_NOSET)
		{
			Com_Printf ("%s is write protected.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_LATCH)
		{
			if (var->latched_string)
			{
				if (strcmp(value, var->latched_string) == 0)
					return var;
				Z_Free (var->latched_string);
				var->latched_string = NULL;
			}

			if (strcmp(value, var->string) == 0)
				return var;

			if (Com_ServerState())
			{
				Com_Printf ("%s will be changed for next game.\n", var_name);
				var->latched_string = CopyString(value, TAGMALLOC_CVAR);
			}
			else
			{
				var->string = CopyString(value, TAGMALLOC_CVAR);
				var->value = atof (var->string);
				var->integer = Q_rint(var->value);
				if (!strcmp(var->name, "game"))
				{
					FS_SetGamedir (var->string);
					FS_ExecAutoexec ();
				}
			}
			return var;
		}
		if (var->flags & (CVAR_LATCHVIDEO|CVAR_LATCHSOUND))
		{
			if (var->latched_string)
			{
				if (strcmp (value, var->latched_string) == 0)
					return var;
				Z_Free (var->latched_string);
				var->latched_string = NULL;
			}

			if (strcmp (value, var->string) == 0)
				return var;

			Com_Printf ("%s will be changed upon %s.\n", var_name, (var->flags & CVAR_LATCHVIDEO) ? "vid_restart" : "snd_restart");
			var->latched_string = CopyString (value, TAGMALLOC_CVAR);

			return var;
		}
		if ( (var->flags & CVAR_CHEAT) && !CL_CheatsOK() )
		{
			Com_Printf ("%s is cheat protected.\n", var_name);
			return var;
		}
	}
	else
	{
		if (var->latched_string)
		{
			Z_Free (var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!strcmp(value, var->string))
		return var;		// not changed

	var->modified = true;

	oldValue = var->string;
	
	var->string = CopyString(value, TAGMALLOC_CVAR);
	var->value = atof (var->string);
	var->integer = Q_rint(var->value);

	if(!force && var->OnChange)
		var->OnChange(var, oldValue);

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity

	Z_Free (oldValue);	// free the old value string

	return var;
}

/*
============
Cvar_ForceSet
============
*/
cvar_t *Cvar_SetLatched (const char *var_name, const char *value)
{
	return Cvar_Set2 (var_name, value, false);
}

/*
============
Cvar_Set
============
*/
cvar_t *Cvar_Set (const char *var_name, const char *value)
{
	return Cvar_Set2 (var_name, value, true);
}

/*
============
Cvar_FullSet
============
*/
cvar_t *Cvar_FullSet (const char *var_name, const char *value, int flags)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, flags);
	}

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity

	Z_Free (var->string);	// free the old value string

	var->string = CopyString(value, TAGMALLOC_CVAR);
	var->value = atof (var->string);
	var->integer = Q_rint(var->value);
	var->flags = flags;

	return var;
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (const char *var_name, float value)
{
	char	val[32];

	if (value == (int)value)
		Com_sprintf (val, sizeof(val), "%i",(int)value);
	else
		Com_sprintf (val, sizeof(val), "%f",value);
	Cvar_Set (var_name, val);
}


void Cvar_SetDefault (const char *var_name)
{ 

	const cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (var)
	{
		Cvar_Set (var->name, var->resetString);
	}
}

/*
============
Cvar_SetCheatState

Any testing variables will be reset to the safe values
============
*/
void Cvar_SetCheatState( void ) {
	cvar_t	*var;

	if(CL_CheatsOK())
		return;
	// set all default vars to the safe value
	for ( var = cvar_vars ; var ; var = var->next ) {
		if ( !(var->flags & CVAR_CHEAT) )
			continue;
		// the CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here 
		// because of a different var->latchedString
		if (var->latched_string)
		{
			Z_Free(var->latched_string);
			var->latched_string = NULL;
		}
		if (strcmp(var->resetString, var->string)) {
		   Cvar_Set2( var->name, var->resetString, true );
		}

	}
}

/*
============
Cvar_GetLatchedVars

Any variables with latched values will now be updated
============
*/
void Cvar_GetLatchedVars (int flags)
{
	cvar_t	*var;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!(var->flags & flags))
			continue;

		if (!var->latched_string)
			continue;

		Z_Free (var->string);
		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = atof(var->string);
		var->integer = Q_rint(var->value);
		if (!strcmp(var->name, "game"))
		{
			FS_SetGamedir (var->string);
			FS_ExecAutoexec ();
		}
	}
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command (void)
{
	const cvar_t			*v;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;
		
// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Com_Printf (S_ENABLE_COLOR "\"%s%s%s\" is \"%s%s%s\" default: \"%s%s%s\"\n", S_COLOR_CYAN, v->name, S_COLOR_WHITE, S_COLOR_CYAN, v->string, S_COLOR_WHITE, S_COLOR_CYAN, v->resetString, S_COLOR_WHITE);

		if (v->latched_string)
			Com_Printf (S_ENABLE_COLOR "latched: \"%s%s%s\"\n", S_COLOR_CYAN, v->latched_string, S_COLOR_WHITE);

		return true;
	}

	Cvar_Set2 (v->name, Cmd_Argv(1), false);
	return true;
}


/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console
============
*/
static void Cvar_Set_f (void)
{
	int		c;
	int		flags;
	cvar_t	*var;

	c = Cmd_Argc();
	if (c != 3 && c != 4)
	{
		Com_Printf ("usage: set <variable> <value> [u / s]\n");
		return;
	}

	if (c == 4)
	{
		if (!strcmp(Cmd_Argv(3), "u"))
			flags = CVAR_USERINFO;
		else if (!strcmp(Cmd_Argv(3), "s"))
			flags = CVAR_SERVERINFO;
		else
		{
			Com_Printf ("flags can only be 'u' or 's'\n");
			return;
		}
		Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), flags);
	}
	else
	{
		var = Cvar_FindVar (Cmd_Argv(1));
		if (!var)
		{	// create it
			Cvar_Get (Cmd_Argv(1), Cmd_Argv(2), CVAR_USER_CREATED);
		}
		else {
			Cvar_Set2 (Cmd_Argv(1), Cmd_Argv(2), false);
		}
	}
}

/*
=============
Cvar_Toggle_f
Toggles the given variable's value between 0 and 1
=============
*/
static void Cvar_Toggle_f (void) 
{ 
	cvar_t *var;
	int i;

	if ( (Cmd_Argc() != 2) && (Cmd_Argc() != 4) )
	{ 
		Com_Printf("Usage: %s <cvar> [option1 option2 option3 ...]\n", Cmd_Argv(0)); 
		return; 
	}

	var = Cvar_FindVar(Cmd_Argv(1)); 
	if (!var) 
	{ 
		Com_Printf("Cvar %s does not exist.\n",Cmd_Argv(1)); 
		return; 
	} 

	if (Cmd_Argc() == 2)
	{
		if (var->integer != 0 && var->integer != 1)
		{
			Com_Printf ("toggle: cvar is not a binary variable\n");
			return;
		}
		Cvar_Set2 (var->name, var->integer ? "0" : "1", false);
		return;
	}

	for (i = 2; i < Cmd_Argc(); i++)
	{
		if (!Q_stricmp(var->string, Cmd_Argv(i)))
		{
			if (i == Cmd_Argc() -1)
			{
				Cvar_Set2(Cmd_Argv(1), Cmd_Argv(2), false);
				return;
			}
			else
			{
				Cvar_Set2(Cmd_Argv(1), Cmd_Argv(i+1), false);
				return;
			}
		}
	}
}

static void Cvar_Increase_f (void)
{
	cvar_t	*var;
	char	val[32];

	if (Cmd_Argc() != 3) {
		Com_Printf ("inc <cvar> <value>\n");
		return;
	}

	var = Cvar_FindVar(Cmd_Argv(1));
	if (!var) {
		Com_Printf ("Unknown cvar '%s'\n", Cmd_Argv(1));
		return;
	}
	
	Com_sprintf (val, sizeof(val), "%f", var->value+atof(Cmd_Argv(2)));
	Cvar_Set2(Cmd_Argv(1), val, false);
}

/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (FILE *f)
{
	const cvar_t	*var;
	char	buffer[1024];

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & CVAR_ARCHIVE)
		{
			if(var->flags & (CVAR_LATCHVIDEO|CVAR_LATCHSOUND) && var->latched_string)
				Com_sprintf (buffer, sizeof(buffer), "set %s \"%s\"\n", var->name, var->latched_string);
			else
				Com_sprintf (buffer, sizeof(buffer), "set %s \"%s\"\n", var->name, var->string);
			fprintf (f, "%s", buffer);
		}
	}
}

/*
============
Cvar_List_f

============
*/
static int CvarSort( const cvar_t *a, const cvar_t *b )
{
	return strcmp (a->name, b->name);
}

static void Cvar_List_f (void)
{
    const cvar_t  *var;
    int		i, j, c;
    char    *filter = "*";
	int		count = 0;
	cvar_t	*sortedList;

    c = Cmd_Argc();
    if (c > 1)
		filter = Cmd_Argv(1);

	for (var = cvar_vars, i = 0; var ; var = var->next, i++);

	sortedList = Z_TagMalloc (i * sizeof(cvar_t), TAGMALLOC_CVAR);

	for (var = cvar_vars, i = 0; var ; var = var->next, i++)
		sortedList[i] = *var;

	qsort (sortedList, i, sizeof(sortedList[0]), (int (*)(const void *, const void *))CvarSort);

	for (j = 0; j < i; j++)
	{
		var = &sortedList[j];
		if (c > 1 && !Com_WildCmp(filter, var->name, true) && !strstr(var->name, filter))
			continue;

		if (var->flags & CVAR_ARCHIVE)
			Com_Printf("*");
		else
			Com_Printf(" ");

		if (var->flags & CVAR_USERINFO)
			Com_Printf("U");
		else
			Com_Printf(" ");

		if (var->flags & CVAR_SERVERINFO)
			Com_Printf("S");
		else
			Com_Printf(" ");

		if (var->flags & CVAR_NOSET)
			Com_Printf ("-");
		else if (var->flags & (CVAR_LATCH|CVAR_LATCHVIDEO|CVAR_LATCHSOUND))
			Com_Printf ("L");
		else
			Com_Printf (" ");

		Com_Printf(" %s \"%s\"\n", var->name, var->string);
		count++;
    }

	if (c > 1)
		Com_Printf("%i cvars found (%i total cvars)\n", count, i);
	else
		Com_Printf("%i cvars\n", i);

	Z_Free (sortedList);
}

qboolean userinfo_modified;


static char	*Cvar_BitInfo (int bit)
{
	static char	info[MAX_INFO_STRING];
	const cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & bit)
			Info_SetValueForKey (info, var->name, var->string);
	}
	return info;
}

// returns an info string containing all the CVAR_USERINFO cvars
char	*Cvar_Userinfo (void)
{
	return Cvar_BitInfo (CVAR_USERINFO);
}

// returns an info string containing all the CVAR_SERVERINFO cvars
char	*Cvar_Serverinfo (void)
{
	return Cvar_BitInfo (CVAR_SERVERINFO);
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init (void)
{
	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_AddCommand ("inc", Cvar_Increase_f);
	Cmd_AddCommand ("cvarlist", Cvar_List_f);
}

