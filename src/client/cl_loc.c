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

LOC support by NiceAss, modified by Maniac
*/
#include "client.h"

#define MAX_LOCATIONS		256
#define LOC_LABEL_LEN		64

typedef struct
{
	char name[LOC_LABEL_LEN];
	short origin[3];
	qboolean used;
}loc_t;

loc_t locations[MAX_LOCATIONS];

static cvar_t	*cl_enablelocs;
static cvar_t	*cl_drawlocs;

//returns first free loc slot if theres a any
int CL_FreeLoc(void)
{
	int i;

	for (i = 0; i < MAX_LOCATIONS; i++) {
		if (locations[i].used == false)
			return i;
	}

	return -1;
}

void CL_LoadLoc(void)
{
	FILE *f;

	memset(locations, 0, sizeof(loc_t) * MAX_LOCATIONS);

	f = fopen(va("locs/%s.loc", cl.mapname), "r");
	if (!f)
		return;

	while (!feof(f)) {
		char *token1, *token2, *token3, *token4;
		char line[128], *nl;
		int index;

		// read a line
		fgets(line, sizeof(line), f);

		// skip comments
		if (line[0] == ':' || line[0] == ';' || line[0] == '/')
			continue;

		// overwrite new line characters with null
		nl = strchr(line, '\n');
		if (nl)
			*nl = '\0';

		// break the line up into 4 tokens
		token1 = line;

		token2 = strchr(token1, ' ');
		if (token2 == NULL)
			continue;
		*token2 = '\0';
		token2++;

		token3 = strchr(token2, ' ');
		if (token3 == NULL)
			continue;
		*token3 = '\0';
		token3++;

		token4 = strchr(token3, ' ');
		if (token4 == NULL)
			continue;
		*token4 = '\0';
		token4++;

		// copy the data to the structure
		index = CL_FreeLoc();

		if(index == -1)
		{
			Com_Printf("Already set maximum number of locations.\n");
			break; //no free slots
		}

		locations[index].origin[0] = atoi(token1);
		locations[index].origin[1] = atoi(token2);
		locations[index].origin[2] = atoi(token3);
		strcpy(locations[index].name, token4);
		locations[index].used = true;
	}

	Com_Printf("%s.loc found and loaded\n", cl.mapname);

	fclose(f);
}

// return the index of the closest location
int CL_LocIndex(short origin[3])
{
	short diff[3];
	float minDist = -1, dist;
	int locIndex = -1;
	int	i;

	for(i = 0; i < MAX_LOCATIONS; i++) {

		if(locations[i].used == false)
			continue;

		VectorSubtract( origin, locations[i].origin, diff );

		dist = diff[0]*diff[0]/64.0f + diff[1]*diff[1]/64.0f + diff[2]*diff[2]/64.0f;

		if(dist < minDist || minDist == -1)	{
			minDist = dist;
			locIndex = i;
		}
	}

	return locIndex;
}

void CL_LocDelete(int index)
{

	if (index < MAX_LOCATIONS && index >= 0 && locations[index].used)
	{
		locations[index].used = false;
		Com_Printf("Location number %i deleted.\n", index);
		return;
	}
	else
		Com_Printf("Cant find location number %i.\n", index);
}

void CL_LocAdd(char *name)
{
	int index = CL_FreeLoc();

	if(index == -1) {
		Com_Printf("Already set maximum number of locations\n");
		return;
	}

	locations[index].origin[0] = cl.frame.playerstate.pmove.origin[0];
	locations[index].origin[1] = cl.frame.playerstate.pmove.origin[1];
	locations[index].origin[2] = cl.frame.playerstate.pmove.origin[2];
	
	strcpy(locations[index].name, name);
	locations[index].used=true;

	Com_Printf("Location %i. %s added at (%d %d %d)\n", index, name, 
		cl.frame.playerstate.pmove.origin[0],
		cl.frame.playerstate.pmove.origin[1],
		cl.frame.playerstate.pmove.origin[2] );
}

void CL_LocWrite(char *filename)
{
	int i;
	FILE *f;

	Sys_Mkdir( "locs" );

	f = fopen(va("locs/%s.loc", filename), "w");
	if (!f) {
		Com_Printf("Warning: Unable to open locs/%s.loc for writing.\n", filename);
		return;
	}


	for(i=0;i<MAX_LOCATIONS;i++)
	{
		if(!locations[i].used)
			continue;

		fprintf(f, "%d %d %d %s\n",
			locations[i].origin[0], locations[i].origin[1], locations[i].origin[2], locations[i].name);
	}

	fclose(f);

	Com_Printf("locs/%s.loc successfully written.\n", filename);
}

void CL_LocPlace(void)
{
	trace_t tr;
	vec3_t end;
	short there[3];
	int	index1, index2 = -1;

	if(!cl_enablelocs->value)
		return;

	index1 = CL_LocIndex(cl.frame.playerstate.pmove.origin);

	VectorMA(cl.predicted_origin, 8192, cl.v_forward, end);
	tr = CM_BoxTrace(cl.predicted_origin, end, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID);
	there[0] = tr.endpos[0] * 8;
	there[1] = tr.endpos[1] * 8;
	there[2] = tr.endpos[2] * 8;
	index2 = CL_LocIndex(there);

	if (index1 != -1)
		strcpy(cl.loc_here, locations[index1].name);
	else
		strcpy(cl.loc_here, "%L");

	if (index2 != -1)
		strcpy(cl.loc_there, locations[index2].name);
	else
		strcpy(cl.loc_there, "%S");
}

void CL_AddViewLocs(void)
{
	int index = CL_LocIndex(cl.frame.playerstate.pmove.origin);
	int i;
	int num = 0;

	if (!cl_drawlocs->value || !cl_enablelocs->value)
		return;

	for (i = 0; i < MAX_LOCATIONS; i++) {
		int dist;
		entity_t ent;

		if (locations[i].used == false)
			continue;

		dist = (cl.frame.playerstate.pmove.origin[0] - locations[i].origin[0]) *
		    (cl.frame.playerstate.pmove.origin[0] - locations[i].origin[0]) +
		    (cl.frame.playerstate.pmove.origin[1] - locations[i].origin[1]) *
		    (cl.frame.playerstate.pmove.origin[1] - locations[i].origin[1]) +
		    (cl.frame.playerstate.pmove.origin[2] - locations[i].origin[2]) *
		    (cl.frame.playerstate.pmove.origin[2] - locations[i].origin[2]);

		if (dist > 4000 * 4000)
			continue;

		memset(&ent, 0, sizeof(entity_t));
		ent.origin[0] = locations[i].origin[0] * 0.125f;
		ent.origin[1] = locations[i].origin[1] * 0.125f;
		ent.origin[2] = locations[i].origin[2] * 0.125f;
		ent.skinnum = 0;
		ent.skin = NULL;
		ent.model = NULL;

		if (i == index)
			ent.origin[2] += sin(cl.time * 0.01f) * 10.0f;

		V_AddEntity(&ent);
		num++;
	}
}

// printf all markers in current location buffer
void CL_LocList(void)
{
	int i;

	for(i=0;i<MAX_LOCATIONS;i++) {
		if(locations[i].used)
			Com_Printf("Location Marker: %i at (%i,%i,%i) = %s\n", i, locations[i].origin[0],locations[i].origin[1],locations[i].origin[2],locations[i].name);
	}
}

qboolean CL_LocsFound(void)
{
	int i;

	for(i=0;i<MAX_LOCATIONS;i++) {
		if(locations[i].used)
			return true;
	}

	return false;
}

/*
=============================================
			LOC COMMANDS
=============================================
*/
void CL_LocAdd_f(void)
{
	if(Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s <label>\n", Cmd_Argv(0));
		return;
	}

	CL_LocAdd(Cmd_Argv(1));
}

void CL_LocDel_f(void)
{
	if(Cmd_Argc()!=2)
	{
		Com_Printf("Usage: %s <index>\n", Cmd_Argv(0));
		return;
	}

	CL_LocDelete(atoi(Cmd_Argv(1)));
}

void CL_LocList_f(void)
{
	CL_LocList();
}

void CL_LocWrite_f(void) 
{
	if(Cmd_Argc()!=2)
	{
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	CL_LocWrite(Cmd_Argv(1));
}

void CL_SayTeam_Loc(void)
{
	char ostr[1024] ;
	char outmsg[1024];
	char buf[1024];
	char *p ;
	char *msg ;

	if (!CL_LocsFound() || !cl_enablelocs->value)
	{
		Com_sprintf(ostr,sizeof(outmsg), "%s %s\n", "say_team", Cmd_Args());
    	Cmd_ExecuteString(ostr);
		return ;
	}
	if(Cmd_Argc()<=1) {
			return ;
	}
	Com_sprintf(buf, sizeof(buf), "%s", Cmd_Args());	// copy the argv to the buffer.
	
	msg = buf ;
	p = outmsg ;

	if (*msg == '\"') {
		msg[strlen(msg) - 1] = 0;
		msg++;
	}
	for (p = outmsg; *msg && (p - outmsg) < sizeof(outmsg) - 2; msg++) {
		if (*msg == '%') {
			switch (*++msg) {
				case 'l' :
				case 'L' :
					if (strlen(cl.loc_here) + (p - outmsg) < sizeof(outmsg) - 2) {
						strcpy(p, cl.loc_here);
						p += strlen(cl.loc_here);
					}
					break;
				case 's' :
				case 'S' :
					if (strlen(cl.loc_there) + (p - outmsg) < sizeof(outmsg) - 2) {
						strcpy(p, cl.loc_there);
						p += strlen(cl.loc_there);
					}
					break;
				default :
					*p++ = '%';
					*p++ = *msg;
			}
		} else
			*p++ = *msg;
	}
	*p = 0;
	Com_sprintf(ostr,sizeof(outmsg), "%s %s\n", "say_team", outmsg);
	Cmd_ExecuteString(ostr) ;
}

/*
==============
LOC_Init
==============
*/
void CL_InitLocs( void )
{
	cl_enablelocs = Cvar_Get("cl_enablelocs", "0", CVAR_ARCHIVE);
	cl_drawlocs = Cvar_Get("cl_drawlocs", "0", 0);

	Cmd_AddCommand ("loc_add", CL_LocAdd_f);
	Cmd_AddCommand ("loc_list", CL_LocList_f);
	Cmd_AddCommand ("loc_save", CL_LocWrite_f);
	Cmd_AddCommand ("loc_del", CL_LocDel_f);
	Cmd_AddCommand ("say_teamloc" , CL_SayTeam_Loc);

}