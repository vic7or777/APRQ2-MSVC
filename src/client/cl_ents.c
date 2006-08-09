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
// cl_ents.c -- entity parsing and management

#include "client.h"

extern	struct model_s	*cl_mod_powerscreen;

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

/*
=================
CL_ParseEntityBits

Returns the entity number and the header bits
=================
*/
//int	bitcounts[32];	/// just for protocol profiling
int CL_ParseEntityBits (sizebuf_t *msg, unsigned int *bits)
{
	unsigned int b, total, number;

	total = MSG_ReadByte (msg);
	if (total & U_MOREBITS1)
	{
		b = MSG_ReadByte (msg);
		total |= b<<8;
	}
	if (total & U_MOREBITS2)
	{
		b = MSG_ReadByte (msg);
		total |= b<<16;
	}
	if (total & U_MOREBITS3)
	{
		b = MSG_ReadByte (msg);
		total |= b<<24;
	}

	// count the bits for net profiling
	/*for (i=0 ; i<32 ; i++)
		if (total&(1<<i))
			bitcounts[i]++;*/

	if (total & U_NUMBER16)
	{
		number = MSG_ReadShort (msg);
		if (number >= MAX_EDICTS)
			Com_Error (ERR_DROP, "CL_ParseEntityBits: Bad entity number %u", number);
	}
	else
	{
		number = MSG_ReadByte (msg);
	}

	*bits = total;

	return (int)number;
}

/*
==================
CL_ParseDelta

Can go from either a baseline or a previous packet_entity
==================
*/
void CL_ParseDelta (sizebuf_t *msg, const entity_state_t *from, entity_state_t *to, int number, int bits)
{
	// set everything to the state we are delta'ing from
	*to = *from;

#ifdef R1Q2_PROTOCOL
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION)
		VectorCopy (from->origin, to->old_origin);
	else if (!(bits & U_OLDORIGIN) && !(from->renderfx & RF_BEAM))
		VectorCopy (from->origin, to->old_origin);
#else
	VectorCopy (from->origin, to->old_origin);
#endif

	to->number = number;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte (msg);
	if (bits & U_MODEL2)
		to->modelindex2 = MSG_ReadByte (msg);
	if (bits & U_MODEL3)
		to->modelindex3 = MSG_ReadByte (msg);
	if (bits & U_MODEL4)
		to->modelindex4 = MSG_ReadByte (msg);
		
	if (bits & U_FRAME8)
		to->frame = MSG_ReadByte (msg);
	if (bits & U_FRAME16)
		to->frame = MSG_ReadShort (msg);

	if ((bits & U_SKIN8) && (bits & U_SKIN16))		//used for laser colors
		to->skinnum = MSG_ReadLong(msg);
	else if (bits & U_SKIN8)
		to->skinnum = MSG_ReadByte(msg);
	else if (bits & U_SKIN16)
		to->skinnum = MSG_ReadShort(msg);

	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16) )
		to->effects = MSG_ReadLong(msg);
	else if (bits & U_EFFECTS8)
		to->effects = MSG_ReadByte(msg);
	else if (bits & U_EFFECTS16)
		to->effects = MSG_ReadShort(msg);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16) )
		to->renderfx = MSG_ReadLong(msg);
	else if (bits & U_RENDERFX8)
		to->renderfx = MSG_ReadByte(msg);
	else if (bits & U_RENDERFX16)
		to->renderfx = MSG_ReadShort(msg);

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord (msg);
	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord (msg);
	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord (msg);
		
	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle(msg);
	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle(msg);
	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle(msg);

	if (bits & U_OLDORIGIN)
		MSG_ReadPos (msg, to->old_origin);

	if (bits & U_SOUND)
		to->sound = MSG_ReadByte (msg);

	if (bits & U_EVENT)
		to->event = MSG_ReadByte (msg);
	else
		to->event = 0;

	if (bits & U_SOLID)
		to->solid = MSG_ReadShort (msg);
}

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
static void CL_DeltaEntity (sizebuf_t *msg, frame_t *frame, int newnum, const entity_state_t *old, int bits)
{
	centity_t	*ent;
	entity_state_t	*state;

	ent = &cl_entities[newnum];

	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];
	cl.parse_entities++;
	frame->num_entities++;

	CL_ParseDelta(msg, old, state, newnum, bits);

	// some data changes will force no lerping
	if (state->modelindex != ent->current.modelindex
		|| state->modelindex2 != ent->current.modelindex2
		|| state->modelindex3 != ent->current.modelindex3
		|| state->modelindex4 != ent->current.modelindex4
		|| abs(state->origin[0] - ent->current.origin[0]) > 512
		|| abs(state->origin[1] - ent->current.origin[1]) > 512
		|| abs(state->origin[2] - ent->current.origin[2]) > 512
		|| state->event == EV_PLAYER_TELEPORT
		|| state->event == EV_OTHER_TELEPORT
		)
	{
		ent->serverframe = -99;
	}

	if (ent->serverframe != cl.frame.serverframe - 1)
	{	// wasn't in last update, so initialize some things
		ent->trailcount = 1024;		// for diminishing rocket / grenade trails
		// duplicate the current state so lerping doesn't hurt anything
		ent->prev = *state;
		if (state->event == EV_OTHER_TELEPORT)
		{
			VectorCopy (state->origin, ent->prev.origin);
			VectorCopy (state->origin, ent->lerp_origin);
		}
		else
		{
			VectorCopy (state->old_origin, ent->prev.origin);
			VectorCopy (state->old_origin, ent->lerp_origin);
		}
	}
	else
	{	// shuffle the last state to previous
		ent->prev = ent->current;
	}

	ent->serverframe = cl.frame.serverframe;
	ent->current = *state;
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
static void CL_ParsePacketEntities (sizebuf_t *msg, const frame_t *oldframe, frame_t *newframe)
{
	int			newnum;
	unsigned int	bits;
	entity_state_t	*oldstate  = NULL;
	int			oldindex = 0, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	// delta from the entities present in oldframe
	if (!oldframe)
		oldnum = 99999;
	else
	{
		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while (1)
	{
		newnum = CL_ParseEntityBits(msg, &bits);

		if (msg->readcount > msg->cursize)
			Com_Error (ERR_DROP,"CL_ParsePacketEntities: end of message");

		if (!newnum)
			break;

		while (oldnum < newnum)
		{	// one or more entities from the old packet are unchanged
			if (cl_shownet->integer == 3)
				Com_Printf ("   unchanged: %i\n", oldnum);
			CL_DeltaEntity(msg, newframe, oldnum, oldstate, 0);
			
			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}

		if (bits & U_REMOVE)
		{	// the entity present in oldframe is not in the current frame
			if (cl_shownet->integer == 3)
				Com_Printf ("   remove: %i\n", newnum);
			if (oldnum != newnum)
				Com_DPrintf ("U_REMOVE: oldnum != newnum\n");

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum == newnum)
		{	// delta from previous state
			if (cl_shownet->integer == 3)
				Com_Printf ("   delta: %i\n", newnum);
			CL_DeltaEntity(msg, newframe, newnum, oldstate, bits);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum > newnum)
		{	// delta from baseline
			if (cl_shownet->integer == 3)
				Com_Printf ("   baseline: %i\n", newnum);
			CL_DeltaEntity(msg, newframe, newnum, &cl_entities[newnum].baseline, bits);
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while (oldnum != 99999)
	{	// one or more entities from the old packet are unchanged
		if (cl_shownet->integer == 3)
			Com_Printf ("   unchanged: %i\n", oldnum);
		CL_DeltaEntity(msg, newframe, oldnum, oldstate, 0);
		
		oldindex++;

		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}

#ifdef R1Q2_PROTOCOL
void CL_DemoDeltaEntity (const entity_state_t *from, const entity_state_t *to, sizebuf_t *buf, qboolean force, qboolean newentity)
{
	int		bits;

	if (to == NULL)
	{
		bits = U_REMOVE;
		if (from->number >= 256)
			bits |= U_NUMBER16 | U_MOREBITS1;

		MSG_WriteByte (buf, bits&255 );
		if (bits & 0x0000ff00)
			MSG_WriteByte (buf, (bits>>8)&255 );

		if (bits & U_NUMBER16)
			MSG_WriteShort (buf, from->number);
		else
			MSG_WriteByte (buf, from->number);

		return;
	}

	if (!to->number)
		Com_Error (ERR_FATAL, "Unset entity number");

	if (to->number >= MAX_EDICTS)
		Com_Error (ERR_FATAL, "Entity number >= MAX_EDICTS");

// send an update
	bits = 0;

	if (to->number >= 256)
		bits |= U_NUMBER16;		// number8 is implicit otherwise

	if (to->origin[0] != from->origin[0])
		bits |= U_ORIGIN1;
	if (to->origin[1] != from->origin[1])
		bits |= U_ORIGIN2;
	if (to->origin[2] != from->origin[2])
		bits |= U_ORIGIN3;

	if ( to->angles[0] != from->angles[0] )
		bits |= U_ANGLE1;		
	if ( to->angles[1] != from->angles[1] )
		bits |= U_ANGLE2;
	if ( to->angles[2] != from->angles[2] )
		bits |= U_ANGLE3;
		
	if ( to->skinnum != from->skinnum )
	{
		if ((unsigned)to->skinnum < 256)
			bits |= U_SKIN8;
		else if ((unsigned)to->skinnum < 0x10000)
			bits |= U_SKIN16;
		else
			bits |= (U_SKIN8|U_SKIN16);
	}
		
	if ( to->frame != from->frame )
	{
		if (to->frame < 256)
			bits |= U_FRAME8;
		else
			bits |= U_FRAME16;
	}

	if ( to->effects != from->effects )
	{
		if (to->effects < 256)
			bits |= U_EFFECTS8;
		else if (to->effects < 0x8000)
			bits |= U_EFFECTS16;
		else
			bits |= U_EFFECTS8|U_EFFECTS16;
	}
	
	if ( to->renderfx != from->renderfx )
	{
		if (to->renderfx < 256)
			bits |= U_RENDERFX8;
		else if (to->renderfx < 0x8000)
			bits |= U_RENDERFX16;
		else
			bits |= U_RENDERFX8|U_RENDERFX16;
	}
	
	if ( to->solid != from->solid )
		bits |= U_SOLID;

	// event is not delta compressed, just 0 compressed
	if ( to->event  )
		bits |= U_EVENT;
	
	if ( to->modelindex != from->modelindex )
		bits |= U_MODEL;
	if ( to->modelindex2 != from->modelindex2 )
		bits |= U_MODEL2;
	if ( to->modelindex3 != from->modelindex3 )
		bits |= U_MODEL3;
	if ( to->modelindex4 != from->modelindex4 )
		bits |= U_MODEL4;

	if ( to->sound != from->sound )
		bits |= U_SOUND;

	if (newentity || (to->renderfx & RF_BEAM))
	{
		if (!VectorCompare (from->old_origin, to->old_origin))
			bits |= U_OLDORIGIN;
	}

	//
	// write the message
	//
	if (!bits && !force)
		return;		// nothing to send!

	//----------

	if (bits & 0xff000000)
		bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x00ff0000)
		bits |= U_MOREBITS2 | U_MOREBITS1;
	else if (bits & 0x0000ff00)
		bits |= U_MOREBITS1;

	MSG_WriteByte (buf, bits&255 );

	if (bits & 0xff000000)
	{
		MSG_WriteByte (buf, (bits>>8)&255 );
		MSG_WriteByte (buf, (bits>>16)&255 );
		MSG_WriteByte (buf, (bits>>24)&255 );
	}
	else if (bits & 0x00ff0000)
	{
		MSG_WriteByte (buf, (bits>>8)&255 );
		MSG_WriteByte (buf, (bits>>16)&255 );
	}
	else if (bits & 0x0000ff00)
	{
		MSG_WriteByte (buf, (bits>>8)&255 );
	}

	//----------

	if (bits & U_NUMBER16)
		MSG_WriteShort (buf, to->number);
	else
		MSG_WriteByte (buf, to->number);

	if (bits & U_MODEL)
		MSG_WriteByte (buf, to->modelindex);
	if (bits & U_MODEL2)
		MSG_WriteByte (buf, to->modelindex2);
	if (bits & U_MODEL3)
		MSG_WriteByte (buf, to->modelindex3);
	if (bits & U_MODEL4)
		MSG_WriteByte (buf, to->modelindex4);

	if (bits & U_FRAME8)
		MSG_WriteByte (buf, to->frame);
	if (bits & U_FRAME16)
		MSG_WriteShort (buf, to->frame);

	if ((bits & U_SKIN8) && (bits & U_SKIN16))		//used for laser colors
		MSG_WriteLong (buf, to->skinnum);
	else if (bits & U_SKIN8)
		MSG_WriteByte (buf, to->skinnum);
	else if (bits & U_SKIN16)
		MSG_WriteShort (buf, to->skinnum);


	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16) )
		MSG_WriteLong (buf, to->effects);
	else if (bits & U_EFFECTS8)
		MSG_WriteByte (buf, to->effects);
	else if (bits & U_EFFECTS16)
		MSG_WriteShort (buf, to->effects);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16) )
		MSG_WriteLong (buf, to->renderfx);
	else if (bits & U_RENDERFX8)
		MSG_WriteByte (buf, to->renderfx);
	else if (bits & U_RENDERFX16)
		MSG_WriteShort (buf, to->renderfx);

	if (bits & U_ORIGIN1)
		MSG_WriteCoord (buf, to->origin[0]);		
	if (bits & U_ORIGIN2)
		MSG_WriteCoord (buf, to->origin[1]);
	if (bits & U_ORIGIN3)
		MSG_WriteCoord (buf, to->origin[2]);

	if (bits & U_ANGLE1)
		MSG_WriteAngle(buf, to->angles[0]);
	if (bits & U_ANGLE2)
		MSG_WriteAngle(buf, to->angles[1]);
	if (bits & U_ANGLE3)
		MSG_WriteAngle(buf, to->angles[2]);

	if (bits & U_OLDORIGIN)
	{
		MSG_WriteCoord (buf, to->old_origin[0]);
		MSG_WriteCoord (buf, to->old_origin[1]);
		MSG_WriteCoord (buf, to->old_origin[2]);
	}

	if (bits & U_SOUND)
		MSG_WriteByte (buf, to->sound);
	if (bits & U_EVENT)
		MSG_WriteByte (buf, to->event);
	if (bits & U_SOLID)
		MSG_WriteShort (buf, to->solid);
}

//r1: this fakes a protocol 34 packetentites write from the clients state instead
//of the server. used to write demo stream regardless of c/s protocol in use.
static void CL_DemoPacketEntities (sizebuf_t *buf, const frame_t /*@null@*/*from, const frame_t *to)
{
	const entity_state_t	*oldent = NULL;
	const entity_state_t	*newent = NULL;

	int				oldindex = 0, newindex = 0;
	int				oldnum, newnum;
	int				from_num_entities;

	//r1: pointless waste of byte since this is already inside an svc_frame
	MSG_WriteByte (buf, svc_packetentities);

	if (!from)
		from_num_entities = 0;
	else
		from_num_entities = from->num_entities;

	while (newindex < to->num_entities || oldindex < from_num_entities)
	{
		if (newindex >= to->num_entities)
			newnum = 9999;
		else
		{
			newent = &cl_parse_entities[(to->parse_entities +newindex)&(MAX_PARSE_ENTITIES-1)];
			newnum = newent->number;
		}

		if (oldindex >= from_num_entities)
			oldnum = 9999;
		else
		{
			//Com_Printf ("server: its in old entities!\n");
			oldent = &cl_parse_entities[(from->parse_entities+oldindex)&(MAX_PARSE_ENTITIES-1)];
			oldnum = oldent->number;
		}

		if (newnum == oldnum)
		{	// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping

			CL_DemoDeltaEntity (oldent, newent, buf, false, newent->number <= cl.maxclients);

			oldindex++;
			newindex++;
			continue;
		}
	
		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline
			CL_DemoDeltaEntity (&cl_entities[newnum].baseline, newent, buf, true, true);
			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
			CL_DemoDeltaEntity (oldent, NULL, buf, true, false);
			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (buf, 0);	// end of packetentities
}

static void CL_DemoDeltaPlayerstate (sizebuf_t *buf, const frame_t *from, frame_t *to)
{
	int				i;
	int				pflags;
	player_state_new_t	*ps;
	const player_state_new_t *ops;
	player_state_new_t	dummy;
	int				statbits;

	ps = &to->playerstate;
	if (!from)
	{
		memset(&dummy, 0, sizeof(dummy));
		ops = &dummy;
	}
	else
		ops = &from->playerstate;

	//
	// determine what needs to be sent
	//
	pflags = 0;

	if (ps->pmove.pm_type != ops->pmove.pm_type)
		pflags |= PS_M_TYPE;

	if (ps->pmove.origin[0] != ops->pmove.origin[0]
		|| ps->pmove.origin[1] != ops->pmove.origin[1]
		|| ps->pmove.origin[2] != ops->pmove.origin[2] )
		pflags |= PS_M_ORIGIN;

	if (ps->pmove.velocity[0] != ops->pmove.velocity[0]
		|| ps->pmove.velocity[1] != ops->pmove.velocity[1]
		|| ps->pmove.velocity[2] != ops->pmove.velocity[2] )
		pflags |= PS_M_VELOCITY;

	if (ps->pmove.pm_time != ops->pmove.pm_time)
		pflags |= PS_M_TIME;

	if (ps->pmove.pm_flags != ops->pmove.pm_flags)
		pflags |= PS_M_FLAGS;

	if (ps->pmove.gravity != ops->pmove.gravity)
		pflags |= PS_M_GRAVITY;

	if (ps->pmove.delta_angles[0] != ops->pmove.delta_angles[0]
		|| ps->pmove.delta_angles[1] != ops->pmove.delta_angles[1]
		|| ps->pmove.delta_angles[2] != ops->pmove.delta_angles[2] )
		pflags |= PS_M_DELTA_ANGLES;


	if (ps->viewoffset[0] != ops->viewoffset[0]
		|| ps->viewoffset[1] != ops->viewoffset[1]
		|| ps->viewoffset[2] != ops->viewoffset[2] )
		pflags |= PS_VIEWOFFSET;

	if (ps->viewangles[0] != ops->viewangles[0]
		|| ps->viewangles[1] != ops->viewangles[1]
		|| ps->viewangles[2] != ops->viewangles[2] )
		pflags |= PS_VIEWANGLES;

	if (ps->kick_angles[0] != ops->kick_angles[0]
		|| ps->kick_angles[1] != ops->kick_angles[1]
		|| ps->kick_angles[2] != ops->kick_angles[2] )
		pflags |= PS_KICKANGLES;

	if (ps->blend[0] != ops->blend[0]
		|| ps->blend[1] != ops->blend[1]
		|| ps->blend[2] != ops->blend[2]
		|| ps->blend[3] != ops->blend[3] )
		pflags |= PS_BLEND;

	if (ps->fov != ops->fov)
		pflags |= PS_FOV;

	if (ps->rdflags != ops->rdflags)
		pflags |= PS_RDFLAGS;

	if (ps->gunframe != ops->gunframe)
		pflags |= PS_WEAPONFRAME;

	pflags |= PS_WEAPONINDEX;

	//
	// write it
	//
	MSG_WriteByte(buf, svc_playerinfo);
	MSG_WriteShort(buf, pflags);

	//
	// write the pmove_state_t
	//
	if (pflags & PS_M_TYPE)
		MSG_WriteByte (buf, ps->pmove.pm_type);

	if (pflags & PS_M_ORIGIN)
	{
		MSG_WriteShort (buf, ps->pmove.origin[0]);
		MSG_WriteShort (buf, ps->pmove.origin[1]);
		MSG_WriteShort (buf, ps->pmove.origin[2]);
	}

	if (pflags & PS_M_VELOCITY)
	{
		MSG_WriteShort (buf, ps->pmove.velocity[0]);
		MSG_WriteShort (buf, ps->pmove.velocity[1]);
		MSG_WriteShort (buf, ps->pmove.velocity[2]);
	}

	if (pflags & PS_M_TIME)
		MSG_WriteByte (buf, ps->pmove.pm_time);

	if (pflags & PS_M_FLAGS)
		MSG_WriteByte (buf, ps->pmove.pm_flags);

	if (pflags & PS_M_GRAVITY)
		MSG_WriteShort (buf, ps->pmove.gravity);

	if (pflags & PS_M_DELTA_ANGLES)
	{
		MSG_WriteShort (buf, ps->pmove.delta_angles[0]);
		MSG_WriteShort (buf, ps->pmove.delta_angles[1]);
		MSG_WriteShort (buf, ps->pmove.delta_angles[2]);
	}

	//
	// write the rest of the player_state_t
	//
	if (pflags & PS_VIEWOFFSET)
	{
		MSG_WriteChar (buf, (int)(ps->viewoffset[0]*4));
		MSG_WriteChar (buf, (int)(ps->viewoffset[1]*4));
		MSG_WriteChar (buf, (int)(ps->viewoffset[2]*4));
	}

	if (pflags & PS_VIEWANGLES)
	{
		MSG_WriteAngle16 (buf, ps->viewangles[0]);
		MSG_WriteAngle16 (buf, ps->viewangles[1]);
		MSG_WriteAngle16 (buf, ps->viewangles[2]);
	}

	if (pflags & PS_KICKANGLES)
	{
		MSG_WriteChar (buf, (int)(ps->kick_angles[0]*4));
		MSG_WriteChar (buf, (int)(ps->kick_angles[1]*4));
		MSG_WriteChar (buf, (int)(ps->kick_angles[2]*4));
	}

	if (pflags & PS_WEAPONINDEX)
	{
		MSG_WriteByte (buf, ps->gunindex);
	}

	if (pflags & PS_WEAPONFRAME)
	{
		MSG_WriteByte (buf, ps->gunframe);
		MSG_WriteChar (buf, (int)(ps->gunoffset[0]*4));
		MSG_WriteChar (buf, (int)(ps->gunoffset[1]*4));
		MSG_WriteChar (buf, (int)(ps->gunoffset[2]*4));
		MSG_WriteChar (buf, (int)(ps->gunangles[0]*4));
		MSG_WriteChar (buf, (int)(ps->gunangles[1]*4));
		MSG_WriteChar (buf, (int)(ps->gunangles[2]*4));
	}

	if (pflags & PS_BLEND)
	{
		MSG_WriteByte (buf, (int)(ps->blend[0]*255));
		MSG_WriteByte (buf, (int)(ps->blend[1]*255));
		MSG_WriteByte (buf, (int)(ps->blend[2]*255));
		MSG_WriteByte (buf, (int)(ps->blend[3]*255));
	}

	if (pflags & PS_FOV)
		MSG_WriteByte (buf, (int)ps->fov);

	if (pflags & PS_RDFLAGS)
		MSG_WriteByte (buf, ps->rdflags);

	// send stats
	statbits = 0;
	for (i=0 ; i<MAX_STATS ; i++)
		if (ps->stats[i] != ops->stats[i])
			statbits |= 1<<i;
	MSG_WriteLong (buf, statbits);
	for (i=0 ; i<MAX_STATS ; i++)
		if (statbits & (1<<i) )
			MSG_WriteShort (buf, ps->stats[i]);
}
#endif

/*
===================
CL_ParsePlayerstate
===================
*/
static void CL_ParsePlayerstate (sizebuf_t *msg, const frame_t *oldframe, frame_t *newframe,  int extraflags)
{
	int			flags;
	player_state_new_t	*state;
	int			i;
	int			statbits;
	qboolean	enhanced = false;
#ifdef R1Q2_PROTOCOL
	enhanced = (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION);
#endif
	state = &newframe->playerstate;

	// clear to old value before delta parsing
	if (oldframe)
		*state = oldframe->playerstate;
	else
		memset (state, 0, sizeof(*state));

	flags = MSG_ReadShort (msg);

	//
	// parse the pmove_state_t
	//
	if (flags & PS_M_TYPE) {
		state->pmove.pm_type = MSG_ReadByte (msg);
		//if(state->pmove.pm_type >= PM_DEAD && oldframe && oldframe->playerstate.pmove.pm_type == PM_NORMAL && !cl.attractloop)
		//	Cmd_ExecTrigger("#dead");
	}

	if (flags & PS_M_ORIGIN)
	{
		if (!enhanced)
			extraflags |= EPS_PMOVE_ORIGIN2;
		state->pmove.origin[0] = MSG_ReadShort (msg);
		state->pmove.origin[1] = MSG_ReadShort (msg);
	}
	if (extraflags & EPS_PMOVE_ORIGIN2)
		state->pmove.origin[2] = MSG_ReadShort (msg);

	if (flags & PS_M_VELOCITY)
	{
		if (!enhanced)
			extraflags |= EPS_PMOVE_VELOCITY2;
		state->pmove.velocity[0] = MSG_ReadShort (msg);
		state->pmove.velocity[1] = MSG_ReadShort (msg);
	}
	if (extraflags & EPS_PMOVE_VELOCITY2)
		state->pmove.velocity[2] = MSG_ReadShort (msg);

	if (flags & PS_M_TIME)
		state->pmove.pm_time = MSG_ReadByte (msg);

	if (flags & PS_M_FLAGS)
		state->pmove.pm_flags = MSG_ReadByte (msg);

	if (flags & PS_M_GRAVITY)
		state->pmove.gravity = MSG_ReadShort (msg);

	if (flags & PS_M_DELTA_ANGLES)
	{
		state->pmove.delta_angles[0] = MSG_ReadShort (msg);
		state->pmove.delta_angles[1] = MSG_ReadShort (msg);
		state->pmove.delta_angles[2] = MSG_ReadShort (msg);
	}

	if (cl.attractloop)
		state->pmove.pm_type = PM_FREEZE;		// demo playback

	//
	// parse the rest of the player_state_t
	//
	if (flags & PS_VIEWOFFSET)
	{
		state->viewoffset[0] = MSG_ReadChar (msg) * 0.25f;
		state->viewoffset[1] = MSG_ReadChar (msg) * 0.25f;
		state->viewoffset[2] = MSG_ReadChar (msg) * 0.25f;
	}

	if (flags & PS_VIEWANGLES)
	{
		if (!enhanced)
			extraflags |= EPS_VIEWANGLE2;
		state->viewangles[0] = MSG_ReadAngle16 (msg);
		state->viewangles[1] = MSG_ReadAngle16 (msg);
	}
	if (extraflags & EPS_VIEWANGLE2)
		state->viewangles[2] = MSG_ReadAngle16 (msg);

	if (flags & PS_KICKANGLES)
	{
		state->kick_angles[0] = MSG_ReadChar (msg) * 0.25f;
		state->kick_angles[1] = MSG_ReadChar (msg) * 0.25f;
		state->kick_angles[2] = MSG_ReadChar (msg) * 0.25f;
	}

	if (flags & PS_WEAPONINDEX)
	{
		state->gunindex = MSG_ReadByte (msg);
	}

	if (flags & PS_WEAPONFRAME)
	{
		if (!enhanced)
			extraflags |= EPS_GUNOFFSET|EPS_GUNANGLES;
		state->gunframe = MSG_ReadByte (msg);
	}
	if (extraflags & EPS_GUNOFFSET)
	{
		state->gunoffset[0] = MSG_ReadChar (msg)*0.25f;
		state->gunoffset[1] = MSG_ReadChar (msg)*0.25f;
		state->gunoffset[2] = MSG_ReadChar (msg)*0.25f;
	}

	if (extraflags & EPS_GUNANGLES)
	{
		state->gunangles[0] = MSG_ReadChar (msg)*0.25f;
		state->gunangles[1] = MSG_ReadChar (msg)*0.25f;
		state->gunangles[2] = MSG_ReadChar (msg)*0.25f;
	}

	if (flags & PS_BLEND)
	{
		state->blend[0] = MSG_ReadByte (msg)*ONEDIV255;
		state->blend[1] = MSG_ReadByte (msg)*ONEDIV255;
		state->blend[2] = MSG_ReadByte (msg)*ONEDIV255;
		state->blend[3] = MSG_ReadByte (msg)*ONEDIV255;
	}

	if (flags & PS_FOV)
		state->fov = (float)MSG_ReadByte (msg);

	if (flags & PS_RDFLAGS)
		state->rdflags = MSG_ReadByte (msg);

	//r1q2 extensions
#ifdef R1Q2_PROTOCOL
	if (enhanced)
	{
		if (flags & PS_BBOX)
		{
			int x, zd, zu;
			int solid;

			solid = MSG_ReadShort (msg);

			x = 8*(solid & 31);
			zd = 8*((solid>>5) & 31);
			zu = 8*((solid>>10) & 63) - 32;

			state->mins[0] = state->mins[1] = -(float)x;
			state->maxs[0] = state->maxs[1] = (float)x;
			state->mins[2] = -(float)zd;
			state->maxs[2] = (float)zu;
			Com_Printf ("received bbox from server: (%f, %f, %f), (%f, %f, %f)\n", state->mins[0], state->mins[1], state->mins[2], state->maxs[0], state->maxs[1], state->maxs[2]);
		}
	}
#endif

	if (!enhanced)
		extraflags |= EPS_STATS;

	if (extraflags & EPS_STATS)
	{
		// parse stats
		statbits = MSG_ReadLong (msg);
		if (statbits)
		{
			for (i=0 ; i<MAX_STATS ; i++)
				if (statbits & (1<<i) )
					state->stats[i] = MSG_ReadShort(msg);
		}
	}
}


/*
==================
CL_FireEntityEvents

==================
*/
void CL_FireEntityEvents (const frame_t *frame)
{
	entity_state_t		*s1;
	int					pnum, num;

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++)
	{
		num = (frame->parse_entities + pnum)&(MAX_PARSE_ENTITIES-1);
		s1 = &cl_parse_entities[num];
		if (s1->event)
			CL_EntityEvent (s1);

		// EF_TELEPORTER acts like an event, but is not cleared each frame
		if (s1->effects & EF_TELEPORTER)
			CL_TeleporterParticles (s1);
	}
}


/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame (sizebuf_t *msg, int extrabits)
{
	int			cmd;
	int			len;
	int			extraflags = 0;
	int			serverframe;
	frame_t		*old;

	memset (&cl.frame, 0, sizeof(cl.frame));


	//HACK: we steal last bits of this int for the offset
	//if serverframe gets that high then the server has been on the same map
	//for over 19 days... how often will this legitimately happen, and do we
	//really need the possibility of the server running same map for 13 years...
	serverframe = MSG_ReadLong (msg);

#ifdef R1Q2_PROTOCOL
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION)
	{
#endif
		cl.frame.serverframe = serverframe;
		cl.frame.deltaframe = MSG_ReadLong (msg);
#ifdef R1Q2_PROTOCOL
	}
	else
	{
		unsigned int	offset;
		
		offset = serverframe & 0xF8000000;
		offset >>= 27;
		
		serverframe &= 0x07FFFFFF;

		cl.frame.serverframe = serverframe;

		if (offset == 31)
			cl.frame.deltaframe = -1;
		else
			cl.frame.deltaframe = serverframe - offset;
	}
#endif
	//cl.frame.servertime = cl.frame.serverframe*100;
	cl.serverTime = cl.frame.serverframe*100;
	//cl.frame.servertime = (cl.frame.serverframe - cl.initial_server_frame) *100;


#ifdef R1Q2_PROTOCOL
	//HACK UGLY SHIT
	//moving the extrabits from cmd over so that the 4 that come from extraflags (surpressCount) don't conflict
	extraflags = extrabits >> 1;
#endif

	// BIG HACK to let old demos continue to work
	if (cls.serverProtocol != 26)
	{
		byte	data;
		data = MSG_ReadByte (msg);

#ifdef R1Q2_PROTOCOL
		//r1: HACK to get extra 4 bits of otherwise unused data
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
		{
			cl.surpressCount = (data & 0x0F);
			extraflags |= (data & 0xF0) >> 4;
		}
		else
#endif
			cl.surpressCount = data;
	}

	if (cl_shownet->integer == 3)
		Com_Printf ("   frame:%i  delta:%i\n", cl.frame.serverframe, cl.frame.deltaframe);

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if (cl.frame.deltaframe <= 0)
	{
		cl.frame.valid = true;		// uncompressed frame
		old = NULL;
		cls.demowaiting = false;	// we can start recording now
	}
	else
	{
		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];
		if (!old->valid)
		{	// should never happen
			Com_Printf ("Delta from invalid frame (not supposed to happen!).\n");
		}
		if (old->serverframe != cl.frame.deltaframe)
		{	// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Com_DPrintf ("Delta frame too old.\n");
		}
		else if (cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES-128)
		{
			Com_DPrintf ("Delta parse_entities too old.\n");
		}
		else
			cl.frame.valid = true;	// valid delta parse
	}

	// clamp time 
	if (cl.time > cl.serverTime)
		cl.time = cl.serverTime;
	else if (cl.time < cl.serverTime - 100)
		cl.time = cl.serverTime - 100;

	// read areabits
	len = MSG_ReadByte (msg);
	MSG_ReadData (msg, &cl.frame.areabits, len);

	// read playerinfo
#ifdef R1Q2_PROTOCOL
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION)
	{
#endif
		cmd = MSG_ReadByte (msg);
		if (cmd != svc_playerinfo)
			Com_Error (ERR_DROP, "CL_ParseFrame: 0x%.2x not playerinfo", cmd);

		SHOWNET(msg, svc_strings[cmd]);
#ifdef R1Q2_PROTOCOL
	}
#endif
	CL_ParsePlayerstate (msg, old, &cl.frame, extraflags);

	// read packet entities
#ifdef R1Q2_PROTOCOL
	if (cls.serverProtocol != ENHANCED_PROTOCOL_VERSION)
	{
#endif
		cmd = MSG_ReadByte (msg);
		if (cmd != svc_packetentities)
			Com_Error (ERR_DROP, "CL_ParseFrame: 0x%.2x not packetentities", cmd);
		SHOWNET(msg, svc_strings[cmd]);
#ifdef R1Q2_PROTOCOL
	}
#endif
	CL_ParsePacketEntities(msg, old, &cl.frame);

#ifdef R1Q2_PROTOCOL
	//r1: now write protocol 34 compatible delta from our localstate for demo.
	if (cls.demorecording && cls.serverProtocol != ORIGINAL_PROTOCOL_VERSION)
	{
		sizebuf_t	fakeMsg;
		byte		fakeDemoFrame[1300];

		//do it
		SZ_Init (&fakeMsg, fakeDemoFrame, sizeof(fakeDemoFrame));
		fakeMsg.allowoverflow = true;

		//svc_frame header shit
		MSG_WriteByte (&fakeMsg, svc_frame);
		MSG_WriteLong (&fakeMsg, cl.frame.serverframe);
		MSG_WriteLong (&fakeMsg, cl.frame.deltaframe);
		MSG_WriteByte (&fakeMsg, cl.surpressCount);

		//areabits
		MSG_WriteByte (&fakeMsg, len);
		SZ_Write (&fakeMsg, &cl.frame.areabits, len);

		//delta ps
		CL_DemoDeltaPlayerstate (&fakeMsg, old, &cl.frame);

		//delta pe
		CL_DemoPacketEntities (&fakeMsg, old, &cl.frame);

		//copy to demobuff
		if (!fakeMsg.overflowed)
		{
			if (fakeMsg.cursize + cl.demoBuff.cursize > cl.demoBuff.maxsize)
				Com_DPrintf ("Discarded a demoframe of %d bytes.\n", fakeMsg.cursize);
			else
				SZ_Write (&cl.demoBuff, fakeDemoFrame, fakeMsg.cursize);
		}
	}
#endif

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if (cl.frame.valid)
	{
		// getting a valid frame message ends the connection process
		if (cls.state != ca_active)
		{
			cls.state = ca_active;

			//cl.time = cl.serverTime;
			//cl.initial_server_frame = cl.frame.serverframe;
			//cl.frame.servertime = (cl.frame.serverframe - cl.initial_server_frame) * 100;

			cl.force_refdef = true;
			VectorScale(cl.frame.playerstate.pmove.origin, 0.125f, cl.predicted_origin);
			VectorCopy (cl.frame.playerstate.viewangles, cl.predicted_angles);

			SCR_ClearLagometer();
			SCR_ClearChatHUD_f();

			if (cls.disable_servercount != cl.servercount && cl.refresh_prepped)
				SCR_EndLoadingPlaque ();	// get rid of loading plaque

			if(!cl.attractloop) {
				Cmd_ExecTrigger( "#cl_enterlevel" );
				CL_StartAutoRecord();
			}

			cl.sound_prepped = true;	// can start mixing ambient sounds
		}

		// fire entity events
		CL_FireEntityEvents (&cl.frame);
		CL_CheckPredictionError ();
	}
}

/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARMS

==========================================================================
*/
extern int Developer_searchpath( void );
/*
===============
CL_AddPacketEntities

===============
*/
void CL_AddPacketEntities (const frame_t *frame)
{
	entity_t			ent = {0};
	const entity_state_t		*s1;
	float				autorotate;
	int					i;
	int					pnum;
	centity_t			*cent;
	int					autoanim;
	const clientinfo_t		*ci;
	unsigned int		effects, renderfx;

	// bonus items rotate at a fixed rate
	autorotate = anglemod(cl.time*0.1f);

	// brush models can auto animate their frames
	autoanim = cl.time/500;

	//memset (&ent, 0, sizeof(ent));

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++)
	{
		s1 = &cl_parse_entities[(frame->parse_entities+pnum)&(MAX_PARSE_ENTITIES-1)];

		cent = &cl_entities[s1->number];

		effects = s1->effects;
		renderfx = s1->renderfx;

			// set frame
		if (effects & EF_ANIM01)
			ent.frame = autoanim & 1;
		else if (effects & EF_ANIM23)
			ent.frame = 2 + (autoanim & 1);
		else if (effects & EF_ANIM_ALL)
			ent.frame = autoanim;
		else if (effects & EF_ANIM_ALLFAST)
			ent.frame = cl.time / 100;
		else
			ent.frame = s1->frame;

		// quad and pent can do different things on client
		if (effects & EF_PENT)
		{
			effects &= ~EF_PENT;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_RED;
		}

		if (effects & EF_QUAD)
		{
			effects &= ~EF_QUAD;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_BLUE;
		}
//======
// PMM
		if (effects & EF_DOUBLE)
		{
			effects &= ~EF_DOUBLE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_DOUBLE;
		}

		if (effects & EF_HALF_DAMAGE)
		{
			effects &= ~EF_HALF_DAMAGE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_HALF_DAM;
		}
// pmm
//======
		ent.oldframe = cent->prev.frame;
		ent.backlerp = 1.0f - cl.lerpfrac;

		if (renderfx & (RF_FRAMELERP|RF_BEAM))
		{	// step origin discretely, because the frames
			// do the animation properly
			VectorCopy (cent->current.origin, ent.origin);
			VectorCopy (cent->current.old_origin, ent.oldorigin);
		}
		else
		{	// interpolate origin
			ent.origin[0] = ent.oldorigin[0] = cent->prev.origin[0] + cl.lerpfrac * (cent->current.origin[0] - cent->prev.origin[0]);
			ent.origin[1] = ent.oldorigin[1] = cent->prev.origin[1] + cl.lerpfrac * (cent->current.origin[1] - cent->prev.origin[1]);
			ent.origin[2] = ent.oldorigin[2] = cent->prev.origin[2] + cl.lerpfrac * (cent->current.origin[2] - cent->prev.origin[2]);
		}

		// create a new entity
	
		// tweak the color of beams
		if ( renderfx & RF_BEAM )
		{	// the four beam colors are encoded in 32 bits of skinnum (hack)
			ent.alpha = 0.30f;
			ent.skinnum = (s1->skinnum >> ((rand() % 4)*8)) & 0xff;
			ent.model = NULL;
		}
		else
		{
			// set skin
			if (s1->modelindex == 255)
			{	// use custom player skin
				ent.skinnum = 0;
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				ent.skin = ci->skin;
				ent.model = ci->model;
				if (!ent.skin || !ent.model)
				{
					ent.skin = cl.baseclientinfo.skin;
					ent.model = cl.baseclientinfo.model;
				}

//============
//PGM
				if (renderfx & RF_USE_DISGUISE)
				{
					if(!strncmp((char *)ent.skin, "players/male", 12))
					{
						ent.skin = R_RegisterSkin ("players/male/disguise.pcx");
						ent.model = R_RegisterModel ("players/male/tris.md2");
					}
					else if(!strncmp((char *)ent.skin, "players/female", 14))
					{
						ent.skin = R_RegisterSkin ("players/female/disguise.pcx");
						ent.model = R_RegisterModel ("players/female/tris.md2");
					}
					else if(!strncmp((char *)ent.skin, "players/cyborg", 14))
					{
						ent.skin = R_RegisterSkin ("players/cyborg/disguise.pcx");
						ent.model = R_RegisterModel ("players/cyborg/tris.md2");
					}
				}
//PGM
//============
			}
			else
			{
				ent.skinnum = s1->skinnum;
				ent.skin = NULL;
				ent.model = cl.model_draw[s1->modelindex];
			}
		}

		// only used for black hole model right now, FIXME: do better
		if (renderfx & RF_TRANSLUCENT)
			ent.alpha = 0.70f;

		// render effects (fullbright, translucent, etc)
		if ((effects & EF_COLOR_SHELL))
			ent.flags = 0;	// renderfx go on color shell entity
		else
			ent.flags = renderfx;

		// calculate angles
		if (effects & EF_ROTATE) // some bonus items auto-rotate
			VectorSet(ent.angles, 0, autorotate, 0);

		// RAFAEL
		else if (effects & EF_SPINNINGLIGHTS)
		{
			vec3_t forward;
			vec3_t start;
			ent.angles[0] = 0;
			ent.angles[1] = anglemod(cl.time*0.5f) + s1->angles[1];
			ent.angles[2] = 180;

			AngleVectors (ent.angles, forward, NULL, NULL);
			VectorMA (ent.origin, 64, forward, start);
			V_AddLight (start, 100, 1, 0, 0);
		}
		else
		{	// interpolate angles
			float	a1, a2;

			for (i=0 ; i<3 ; i++)
			{
				a1 = cent->current.angles[i];
				a2 = cent->prev.angles[i];
				ent.angles[i] = LerpAngle (a2, a1, cl.lerpfrac);
			}
		}

		if (s1->number == cl.playernum+1)
		{
			ent.flags |= RF_VIEWERMODEL;	// only draw from mirrors
			// FIXME: still pass to refresh

			if (effects & EF_FLAG1)
				V_AddLight (ent.origin, 225, 1.0f, 0.1f, 0.1f);
			else if (effects & EF_FLAG2)
				V_AddLight (ent.origin, 225, 0.1f, 0.1f, 1.0f);
			else if (effects & EF_TAGTRAIL)						//PGM
				V_AddLight (ent.origin, 225, 1.0f, 1.0f, 0.0f);	//PGM
			else if (effects & EF_TRACKERTRAIL)					//PGM
				V_AddLight (ent.origin, 225, -1.0f, -1.0f, -1.0f);	//PGM

			continue;
		}

		// if set to invisible, skip
		if (!s1->modelindex)
			continue;

		if (effects & EF_BFG)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.30f;
		}

		// RAFAEL
		if (effects & EF_PLASMA)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.6f;
		}

		if (effects & EF_SPHERETRANS)
		{
			ent.flags |= RF_TRANSLUCENT;
			// PMM - *sigh*  yet more EF overloading
			if (effects & EF_TRACKERTRAIL)
				ent.alpha = 0.6f;
			else
				ent.alpha = 0.3f;
		}
//pmm

		// add to refresh list
		AnglesToAxis(ent.angles, ent.axis);
		V_AddEntity (&ent);


		// color shells generate a seperate entity for the main model
		if (effects & EF_COLOR_SHELL)
		{
			// PMM - at this point, all of the shells have been handled
			// if we're in the rogue pack, set up the custom mixing, otherwise just
			// keep going
			if(Developer_searchpath() == 2)
			{
				// all of the solo colors are fine.  we need to catch any of the combinations that look bad
				// (double & half) and turn them into the appropriate color, and make double/quad something special
				if (renderfx & RF_SHELL_HALF_DAM)
				{
					// ditch the half damage shell if any of red, blue, or double are on
					if (renderfx & (RF_SHELL_RED|RF_SHELL_BLUE|RF_SHELL_DOUBLE))
						renderfx &= ~RF_SHELL_HALF_DAM;
				}

				if (renderfx & RF_SHELL_DOUBLE)
				{
					// lose the yellow shell if we have a red, blue, or green shell
					if (renderfx & (RF_SHELL_RED|RF_SHELL_BLUE|RF_SHELL_GREEN))
						renderfx &= ~RF_SHELL_DOUBLE;
					// if we have a red shell, turn it to purple by adding blue
					if (renderfx & RF_SHELL_RED)
						renderfx |= RF_SHELL_BLUE;
					// if we have a blue shell (and not a red shell), turn it to cyan by adding green
					else if (renderfx & RF_SHELL_BLUE) {
						// go to green if it's on already, otherwise do cyan (flash green)
						if (renderfx & RF_SHELL_GREEN)
							renderfx &= ~RF_SHELL_BLUE;
						else
							renderfx |= RF_SHELL_GREEN;
					}
				}
			}
			ent.flags = renderfx | RF_TRANSLUCENT;
			ent.alpha = 0.30f;
			V_AddEntity (&ent);

#ifdef GL_QUAKE
			// duplicate for linked models
			if (s1->modelindex2)
			{
				if (s1->modelindex2 == 255)
				{// custom weapon
					ci = &cl.clientinfo[s1->skinnum & 0xff];
					i = (s1->skinnum >> 8); // 0 is default weapon model
					if (!cl_vwep->value || i > MAX_CLIENTWEAPONMODELS - 1)
						i = 0;
					ent.model = ci->weaponmodel[i];
					if (!ent.model) {
						if (i != 0)
							ent.model = ci->weaponmodel[0];
						if (!ent.model)
							ent.model = cl.baseclientinfo.weaponmodel[0];
					}
				}
				else
					ent.model = cl.model_draw[s1->modelindex2];

				V_AddEntity (&ent);
			}
			if (s1->modelindex3)
			{
				ent.model = cl.model_draw[s1->modelindex3];
				V_AddEntity (&ent);
			}
			if (s1->modelindex4)
			{
				ent.model = cl.model_draw[s1->modelindex4];
				V_AddEntity (&ent);
			}
#endif
		}

		ent.skin = NULL;		// never use a custom skin on others
		ent.skinnum = ent.flags = ent.alpha = 0;

		// duplicate for linked models
		if (s1->modelindex2)
		{
			if (s1->modelindex2 == 255)
			{	// custom weapon
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				i = (s1->skinnum >> 8); // 0 is default weapon model
				if (!cl_vwep->integer || i > MAX_CLIENTWEAPONMODELS - 1)
					i = 0;
				ent.model = ci->weaponmodel[i];
				if (!ent.model) {
					if (i != 0)
						ent.model = ci->weaponmodel[0];
					if (!ent.model)
						ent.model = cl.baseclientinfo.weaponmodel[0];
				}
			}
			else
				ent.model = cl.model_draw[s1->modelindex2];

			// PMM - check for the defender sphere shell .. make it translucent
			// replaces the previous version which used the high bit on modelindex2 to determine transparency
			if (!Q_stricmp (cl.configstrings[CS_MODELS+(s1->modelindex2)], "models/items/shell/tris.md2"))
			{
				ent.alpha = 0.32f;
				ent.flags = RF_TRANSLUCENT;
			}
			// pmm
			V_AddEntity (&ent);

			//PGM - make sure these get reset.
			ent.flags = ent.alpha = 0;
			//PGM
		}
		if (s1->modelindex3)
		{
			ent.model = cl.model_draw[s1->modelindex3];
			V_AddEntity (&ent);
		}
		if (s1->modelindex4)
		{
			ent.model = cl.model_draw[s1->modelindex4];
			V_AddEntity (&ent);
		}

		if ( effects & EF_POWERSCREEN )
		{
			ent.model = cl_mod_powerscreen;
			ent.oldframe = 0;
			ent.frame = 0;
			ent.flags |= (RF_TRANSLUCENT | RF_SHELL_GREEN);
			ent.alpha = 0.30f;
			V_AddEntity (&ent);
		}

		// add automatic particle trails
		if ( (effects&~EF_ROTATE) )
		{
			if (effects & EF_ROCKET)
			{
				CL_RocketTrail (cent->lerp_origin, ent.origin, cent);
				V_AddLight (ent.origin, 200, 1, 1, 0);
			}
			// PGM - Do not reorder EF_BLASTER and EF_HYPERBLASTER. 
			// EF_BLASTER | EF_TRACKER is a special case for EF_BLASTER2... Cheese!
			else if (effects & EF_BLASTER)
			{
//				CL_BlasterTrail (cent->lerp_origin, ent.origin);
//PGM
				if (effects & EF_TRACKER)	// lame... problematic?
				{
					CL_BlasterTrail2 (cent->lerp_origin, ent.origin);
					V_AddLight (ent.origin, 200, 0, 1, 0);		
				}
				else
				{
					CL_BlasterTrail (cent->lerp_origin, ent.origin);
					V_AddLight (ent.origin, 200, 1, 1, 0);
				}
//PGM
			}
			else if (effects & EF_HYPERBLASTER)
			{
				if (effects & EF_TRACKER)						// PGM	overloaded for blaster2.
					V_AddLight (ent.origin, 200, 0, 1, 0);		// PGM
				else											// PGM
					V_AddLight (ent.origin, 200, 1, 1, 0);
			}
			else if (effects & EF_GIB)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);
			}
			else if (effects & EF_GRENADE)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);
			}
			else if (effects & EF_FLIES)
			{
				CL_FlyEffect (cent, ent.origin);
			}
			else if (effects & EF_BFG)
			{
				static const int bfg_lightramp[6] = {300, 400, 600, 300, 150, 75};

				if (effects & EF_ANIM_ALLFAST)
				{
					CL_BfgParticles (&ent);
					i = 200;
				}
				else
				{
					i = bfg_lightramp[s1->frame];
				}
				V_AddLight (ent.origin, i, 0, 1, 0);
			}
			// RAFAEL
			else if (effects & EF_TRAP)
			{
				ent.origin[2] += 32;
				CL_TrapParticles (&ent);
				i = (rand()%100) + 100;
				V_AddLight (ent.origin, i, 1.0f, 0.8f, 0.1f);
			}
			else if (effects & EF_FLAG1)
			{
				CL_FlagTrail (cent->lerp_origin, ent.origin, 242);
				V_AddLight (ent.origin, 225, 1.0f, 0.1f, 0.1f);
			}
			else if (effects & EF_FLAG2)
			{
				CL_FlagTrail (cent->lerp_origin, ent.origin, 115);
				V_AddLight (ent.origin, 225, 0.1f, 0.1f, 1.0f);
			}
//======
//ROGUE
			else if (effects & EF_TAGTRAIL)
			{
				CL_TagTrail (cent->lerp_origin, ent.origin, 220);
				V_AddLight (ent.origin, 225, 1.0f, 1.0f, 0.0f);
			}
			else if (effects & EF_TRACKERTRAIL)
			{
				if (effects & EF_TRACKER)
				{
					float intensity;

					intensity = 50 + (500 * ((float)sin(cl.time/500.0f) + 1.0f));
					// FIXME - check out this effect in rendition
#ifdef GL_QUAKE
						V_AddLight (ent.origin, intensity, -1.0f, -1.0f, -1.0f);
#else
						V_AddLight (ent.origin, -1.0f * intensity, 1.0f, 1.0f, 1.0f);
#endif
				}
				else
				{
					CL_Tracker_Shell (cent->lerp_origin);
					V_AddLight (ent.origin, 155, -1.0f, -1.0f, -1.0f);
				}
			}
			else if (effects & EF_TRACKER)
			{
				CL_TrackerTrail (cent->lerp_origin, ent.origin, 0);
				// FIXME - check out this effect in rendition
#ifdef GL_QUAKE
					V_AddLight (ent.origin, 200, -1, -1, -1);
#else
					V_AddLight (ent.origin, -200, 1, 1, 1);
#endif
			}
//ROGUE
//======
			// RAFAEL
			else if (effects & EF_GREENGIB)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);				
			}
			// RAFAEL
			else if (effects & EF_IONRIPPER)
			{
				CL_IonripperTrail (cent->lerp_origin, ent.origin);
				V_AddLight (ent.origin, 100, 1, 0.5f, 0.5f);
			}
			// RAFAEL
			else if (effects & EF_BLUEHYPERBLASTER)
			{
				V_AddLight (ent.origin, 200, 0, 0, 1);
			}
			// RAFAEL
			else if (effects & EF_PLASMA)
			{
				if (effects & EF_ANIM_ALLFAST)
					CL_BlasterTrail (cent->lerp_origin, ent.origin);

				V_AddLight (ent.origin, 130, 1, 0.5f, 0.5f);
			}
		}

		VectorCopy (ent.origin, cent->lerp_origin);
	}
}



/*
==============
CL_AddViewWeapon
==============
*/
extern cvar_t *cl_gunalpha;
extern cvar_t *hand;
//cvar_t *cl_gunx, *cl_guny, *cl_gunz;

static void CL_AddViewWeapon (const player_state_new_t *ps, const player_state_new_t *ops)
{
	entity_t	gun = {0};		// view model
	int			i;
#ifdef GL_QUAKE
	int pnum;
	entity_state_t *s1;
#endif

	// allow the gun to be completely removed
	if (!cl_gun->integer)
		return;

	// don't draw gun if in wide angle view
	if (ps->fov > 90 && cl_gun->integer != 2)
		return;

	//memset (&gun, 0, sizeof(gun));

	gun.model = cl.model_draw[ps->gunindex];
	if (!gun.model)
		return;

	// set up gun position
	for (i=0 ; i<3 ; i++)
	{
		gun.origin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i] + cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
		gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle (ops->gunangles[i], ps->gunangles[i], cl.lerpfrac);
	}

/*	if(!cl_gunx) {
		cl_gunx = Cvar_Get("cl_gunx", "0", 0);
		cl_guny = Cvar_Get("cl_guny", "0", 0);
		cl_gunz = Cvar_Get("cl_gunz", "0", 0);
	}
	VectorMA( gun.origin, cl_gunx->value, cl.v_forward, gun.origin );
	VectorMA( gun.origin, cl_guny->value, cl.v_right, gun.origin );
	VectorMA( gun.origin, cl_gunz->value, cl.v_up, gun.origin );
*/
	gun.frame = ps->gunframe;
	if (gun.frame == 0)
		gun.oldframe = 0;	// just changed weapons, don't lerp from old
	else
		gun.oldframe = ops->gunframe;

	gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;

	if(cl_gunalpha->value < 1.0f)
	{
		gun.flags |= RF_TRANSLUCENT;
		gun.alpha = cl_gunalpha->value;
	}
#ifdef GL_QUAKE
	AnglesToAxis(gun.angles, gun.axis);
	if (hand->integer == 1) {
		gun.flags |= RF_CULLHACK;
		VectorInverse (gun.axis[1]);
	}
#endif
	gun.backlerp = 1.0f - cl.lerpfrac;
	VectorCopy (gun.origin, gun.oldorigin);	// don't lerp at all
	V_AddEntity (&gun);

#ifdef GL_QUAKE
	for (pnum = 0 ; pnum<cl.frame.num_entities ; pnum++)
	{
		s1 = &cl_parse_entities[(cl.frame.parse_entities+pnum)&(MAX_PARSE_ENTITIES-1)];
		if (s1->number != cl.playernum + 1)
			continue;

		if (s1->effects & (EF_COLOR_SHELL|EF_QUAD|EF_PENT|EF_DOUBLE|EF_HALF_DAMAGE))
		{
			gun.flags |= (RF_TRANSLUCENT|s1->renderfx);
			if (s1->effects & EF_PENT)
				gun.flags |= RF_SHELL_RED;
			if (s1->effects & EF_QUAD)
				gun.flags |= RF_SHELL_BLUE;
			if (s1->effects & EF_DOUBLE)
				gun.flags |= RF_SHELL_DOUBLE;
			if (s1->effects & EF_HALF_DAMAGE)
				gun.flags |= RF_SHELL_HALF_DAM;
			gun.alpha = 0.1f;
			V_AddEntity(&gun);
		}
		break;
	}
#endif
}


/*
===============
CL_CalcViewValues

Sets cl.refdef view values
===============
*/
static void CL_CalcViewValues (void)
{
	int			i;
	float		lerp, backlerp;
	//centity_t	*ent;
	const frame_t		*oldframe;
	const player_state_new_t	*ps, *ops;

	// find the previous frame to interpolate from
	ps = &cl.frame.playerstate;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];
	if (oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid)
		oldframe = &cl.frame;		// previous frame was dropped or involid
	ops = &oldframe->playerstate;

	// see if the player entity was teleported this frame
	if ( fabs(ops->pmove.origin[0] - ps->pmove.origin[0]) > 2048
		|| abs(ops->pmove.origin[1] - ps->pmove.origin[1]) > 2048
		|| abs(ops->pmove.origin[2] - ps->pmove.origin[2]) > 2048)
		ops = ps;		// don't interpolate

	//ent = &cl_entities[cl.playernum+1];
	lerp = cl.lerpfrac;

	// calculate the origin
	if (cl_predict->integer && !(cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION))
	{	// use predicted values
		unsigned	delta;

		backlerp = 1.0f - lerp;
		for (i=0 ; i<3 ; i++)
		{
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i]
				+ lerp * (ps->viewoffset[i] - ops->viewoffset[i])
				- backlerp * cl.prediction_error[i];
		}

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;
		if (delta < 100)
			cl.refdef.vieworg[2] -= cl.predicted_step * (100 - delta) * 0.01f;
	}
	else
	{	// just use interpolated values
		for (i=0 ; i<3 ; i++)
			cl.refdef.vieworg[i] = ops->pmove.origin[i]*0.125f + ops->viewoffset[i] 
				+ lerp * (ps->pmove.origin[i]*0.125f + ps->viewoffset[i] 
				- (ops->pmove.origin[i]*0.125f + ops->viewoffset[i]) );
	}

	if (Com_ServerState() == ss_demo)
	{
		cl.refdef.viewangles[0] = LerpAngle (ops->viewangles[0], ps->viewangles[0], lerp);
		cl.refdef.viewangles[1] = LerpAngle (ops->viewangles[1], ps->viewangles[1], lerp);
		cl.refdef.viewangles[2] = LerpAngle (ops->viewangles[2], ps->viewangles[2], lerp);
	}
	// if not running a demo or on a locked frame, add the local angle movement
	else if ( cl.frame.playerstate.pmove.pm_type < PM_DEAD )
	{	// use predicted values
		VectorCopy (cl.predicted_angles, cl.refdef.viewangles);
	}
	else
	{	// just use interpolated values
#ifdef R1Q2_PROTOCOL
		if (cl.frame.playerstate.pmove.pm_type >= PM_DEAD && ops->pmove.pm_type < PM_DEAD)
		{
			//r1: fix for server no longer sending viewangles every frame.
			cl.refdef.viewangles[0] = LerpAngle (cl.predicted_angles[0], ps->viewangles[0], lerp);
			cl.refdef.viewangles[1] = LerpAngle (cl.predicted_angles[1], ps->viewangles[1], lerp);
			cl.refdef.viewangles[2] = LerpAngle (cl.predicted_angles[2], ps->viewangles[2], lerp);
		}
		else
		{
#endif
			cl.refdef.viewangles[0] = LerpAngle (ops->viewangles[0], ps->viewangles[0], lerp);
			cl.refdef.viewangles[1] = LerpAngle (ops->viewangles[1], ps->viewangles[1], lerp);
			cl.refdef.viewangles[2] = LerpAngle (ops->viewangles[2], ps->viewangles[2], lerp);
#ifdef R1Q2_PROTOCOL
		}
#endif
	}

	cl.refdef.viewangles[0] += LerpAngle (ops->kick_angles[0], ps->kick_angles[0], lerp);
	cl.refdef.viewangles[1] += LerpAngle (ops->kick_angles[1], ps->kick_angles[1], lerp);
	cl.refdef.viewangles[2] += LerpAngle (ops->kick_angles[2], ps->kick_angles[2], lerp);

	AngleVectors (cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

	// interpolate field of view
	cl.refdef.fov_x = ops->fov + lerp * (ps->fov - ops->fov);

	// don't interpolate blend color
	Vector4Copy (ps->blend, cl.refdef.blend);

	// add the weapon
	CL_AddViewWeapon (ps, ops);
}

/*
===============
CL_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CL_AddEntities (void)
{

	if (cl.time > cl.serverTime)
	{
		if (cl_showclamp->integer)
			Com_Printf ("high clamp %i\n", cl.time - cl.serverTime);
		cl.time = cl.serverTime;
		cl.lerpfrac = 1.0f;
	}
	else if (cl.time < cl.serverTime - 100)
	{
		if (cl_showclamp->integer)
			Com_Printf ("low clamp %i\n", cl.serverTime-100 - cl.time);
		cl.time = cl.serverTime - 100;
		cl.lerpfrac = 0;
	}
	else
		cl.lerpfrac = 1.0f - (cl.serverTime - cl.time) * 0.01f;

	if (cl_timedemo->integer)
		cl.lerpfrac = 1.0f;

	CL_CalcViewValues ();
	// PMM - moved this here so the heat beam has the right values for the vieworg, and can lock the beam to the gun
	CL_AddPacketEntities (&cl.frame);

	CL_AddViewLocs();

	CL_AddTEnts ();
	CL_AddParticles ();
	CL_AddDLights ();
	CL_AddLightStyles ();
}


/*
===============
CL_GetEntitySoundOrigin

Called to get the sound spatialization origin
===============
*/
void CL_GetEntitySoundOrigin (int ent, vec3_t org)
{
	centity_t	*cent;

	if (ent < 0 || ent >= MAX_EDICTS)
		Com_Error(ERR_DROP, "CL_GetEntityOrigin: ent = %i", ent);

	// Player entity
	if (ent == cl.playernum + 1)
	{
		VectorCopy (cl.refdef.vieworg, org);
		return;
	}

	cent = &cl_entities[ent];
	//VectorCopy(cent->lerp_origin, org);

	if (cent->current.renderfx & (RF_FRAMELERP|RF_BEAM))
	{
		// Calculate origin
		org[0] = cent->current.old_origin[0] + (cent->current.origin[0] - cent->current.old_origin[0]) * cl.lerpfrac;
		org[1] = cent->current.old_origin[1] + (cent->current.origin[1] - cent->current.old_origin[1]) * cl.lerpfrac;
		org[2] = cent->current.old_origin[2] + (cent->current.origin[2] - cent->current.old_origin[2]) * cl.lerpfrac;
	}
	else
	{
		// Calculate origin
		org[0] = cent->prev.origin[0] + (cent->current.origin[0] - cent->prev.origin[0]) * cl.lerpfrac;
		org[1] = cent->prev.origin[1] + (cent->current.origin[1] - cent->prev.origin[1]) * cl.lerpfrac;
		org[2] = cent->prev.origin[2] + (cent->current.origin[2] - cent->prev.origin[2]) * cl.lerpfrac;
	}

	// If a brush model, offset the origin
	if (cent->current.solid == 31)
	{
		vec3_t		midPoint;
		cmodel_t	*cmodel = cl.model_clip[cent->current.modelindex];

		if (!cmodel)
			return;

		VectorAvg(cmodel->mins, cmodel->maxs, midPoint);
		VectorAdd(org, midPoint, org);
	}
}

