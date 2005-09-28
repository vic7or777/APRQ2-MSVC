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
// cl_parse.c  -- parse a message received from the server

#include "client.h"

cvar_t	*cl_timestamps;
cvar_t	*cl_timestampsformat;
cvar_t	*cl_highlight;
cvar_t	*cl_highlightmsg;
cvar_t	*cl_highlightcolor;
cvar_t	*cl_highlightnames;
cvar_t	*ignorewaves;
cvar_t	*cl_textcolors;
cvar_t	*cl_mychatcolor;

cvar_t	*cl_autoscreenshot;

extern cvar_t *cl_customlimitmsg;
extern cvar_t *name;

void CL_VersionReply (const char *s);
qboolean CL_ChatParse (const char *s, int *clinu, int *skip2, int *mm2);
void CL_HighlightNames( char *s );
int CL_Highlight ( const char *s, int *color );
void CL_ParseAutoScreenshot (const char *s);
char *CL_Timestamp( qboolean chat );
qboolean CL_Ignore(const char *s);

void CL_Reconnect_f (void);

const char *svc_strings[256] =
{
	"svc_bad",

	"svc_muzzleflash",
	"svc_muzzleflash2",
	"svc_temp_entity",
	"svc_layout",
	"svc_inventory",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",	
	"svc_centerprint",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_frame",
	"svc_zpacket",
	"svc_zdownload"
};

//=============================================================================

void CL_DownloadFileName(char *dest, int destlen, char *fn)
{
	//if (strncmp(fn, "players", 7) == 0)
	//	Com_sprintf (dest, destlen, "%s/%s", BASEDIRNAME, fn);
	//else
		Com_sprintf (dest, destlen, "%s/%s", FS_Gamedir(), fn);
}

void CL_FinishDownload (void)
{
	int r;
	char	oldn[MAX_OSPATH];
	char	newn[MAX_OSPATH];

	fclose (cls.download);

	// rename the temp file to it's final name
	CL_DownloadFileName(oldn, sizeof(oldn), cls.downloadtempname);
	CL_DownloadFileName(newn, sizeof(newn), cls.downloadname);

	r = rename (oldn, newn);
	if (r)
		Com_Printf ("failed to rename.\n");

	cls.download = NULL;
	cls.downloadpercent = 0;
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean	CL_CheckOrDownloadFile (const char *filename)
{
	FILE *fp;
	int		length;
	char	*p;
	char	name[MAX_OSPATH];
	static char lastfilename[MAX_OSPATH] = {0};


	//r1: don't attempt same file many times
	if (!strcmp (filename, lastfilename))
		return true;

	strcpy (lastfilename, filename);

	if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to download a path with ..\n");
		return true;
	}

	if (strchr (filename, ' '))
	{
		Com_Printf ("Refusing to check a path containing spaces (%s)\n", filename);
		return true;
	}

	if (strchr (filename, ':'))
	{
		Com_Printf ("Refusing to check a path containing a colon (%s)\n", filename);
		return true;
	}

	if (filename[0] == '/')
	{
		Com_Printf ("Refusing to check a path starting with / (%s)\n", filename);
		return true;
	}

	if (FS_LoadFile (filename, NULL) != -1)
	{	// it exists, no need to download
		return true;
	}

	strcpy (cls.downloadname, filename);

	//r1: fix \ to /
	p = cls.downloadname;
	while ((p = strchr(p, '\\')))
		*p = '/';

	length = (int)strlen(cls.downloadname);

	//normalize path
	p = cls.downloadname;
	while ((p = strstr (p, "./")))
	{
		memmove (p, p+2, length - (p - cls.downloadname) - 1);
		length -= 2;
	}

	//r1: verify we are giving the server a legal path
	if (cls.downloadname[length-1] == '/')
	{
		Com_Printf ("Refusing to download bad path (%s)\n", filename);
		return true;
	}

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

//ZOID
	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

//	FS_CreatePath (name);

	fp = fopen (name, "r+b");
	if (fp)
	{ // it exists
		int len;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		cls.download = fp;

		// give the server an offset to start the download
		Com_Printf ("Resuming %s\n", cls.downloadname);

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
#ifdef R1Q2_PROTOCOL
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
			MSG_WriteString (&cls.netchan.message, va("download \"%s\" %i udp-zlib", cls.downloadname, len));
		else
#endif
			MSG_WriteString (&cls.netchan.message, va("download \"%s\" %i", cls.downloadname, len));
	}
	else
	{
		Com_Printf ("Downloading %s\n", cls.downloadname);

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
#ifdef R1Q2_PROTOCOL
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
			MSG_WriteString (&cls.netchan.message, va("download \"%s\" 0 udp-zlib", cls.downloadname));
		else
#endif
			MSG_WriteString (&cls.netchan.message, va("download \"%s\"", cls.downloadname));
	}

	cls.downloadnumber++;

	return false;
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void	CL_Download_f (void)
{
	char filename[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: download <filename>\n");
		return;
	}

	if (cls.state <= ca_connecting)
	{
		Com_Printf ("Not connected.\n");
		return;
	}

	Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));

	if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to download a path with ..\n");
		return;
	}

	if (FS_LoadFile (filename, NULL) != -1)
	{	// it exists, no need to download
		Com_Printf("File already exists.\n");
		return;
	}

	Q_strncpyz (cls.downloadname, filename, sizeof(cls.downloadname));
	Com_Printf ("Downloading %s\n", cls.downloadname);

	// download to a temp name, and only rename to the real name when done,
	// so if interrupted a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
#ifdef R1Q2_PROTOCOL
	if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
		MSG_WriteString (&cls.netchan.message, va("download \"%s\" 0 udp-zlib", cls.downloadname));
	else
#endif
		MSG_WriteString (&cls.netchan.message, va("download \"%s\" 0", cls.downloadname));

	cls.downloadnumber++;
}

/*
======================
CL_RegisterSounds
======================
*/
void CL_RegisterSounds (void)
{
	int		i;

	S_BeginRegistration ();
	CL_RegisterTEntSounds ();
	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!cl.configstrings[CS_SOUNDS+i][0])
			break;
		cl.sound_precache[i] = S_RegisterSound (cl.configstrings[CS_SOUNDS+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	S_EndRegistration ();
}


/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload (qboolean dataIsCompressed)
{
	int		size, percent;
	char	name[MAX_OSPATH];
//	int		r;

	// read the data
	size = MSG_ReadShort (&net_message);
	percent = MSG_ReadByte (&net_message);
	if (size < 0)
	{
		if (size == -1)
			Com_Printf ("Server does not have this file.\n");
		else
			Com_Printf ("Bad download data from server.\n");

		*cls.downloadtempname = 0;

		if (cls.download)
		{
			// if here, we tried to resume a file but the server said no
			fclose (cls.download);
			cls.download = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download)
	{
		if (!*cls.downloadtempname)
		{
			Com_Printf ("Received download packet without request. Ignored.\n");
			net_message.readcount += size;
			return;
		}

		CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

		FS_CreatePath (name);

		cls.download = fopen (name, "wb");
		if (!cls.download)
		{
			net_message.readcount += size;
			Com_Printf ("Failed to open %s\n", cls.downloadtempname);
			CL_RequestNextDownload ();
			return;
		}
	}

#ifdef R1Q2_PROTOCOL
	if (dataIsCompressed)
	{
		int			uncompressedLen;
		byte		uncompressed[0xFFFF];

		uncompressedLen = MSG_ReadShort (&net_message);

		if (!uncompressedLen)
			Com_Error (ERR_DROP, "uncompressedLen == 0");

		ZLibDecompress (net_message_buffer + net_message.readcount, size, uncompressed, uncompressedLen, -15);
		fwrite (uncompressed, 1, uncompressedLen, cls.download);
		Com_DPrintf ("svc_zdownload(%s): %d -> %d\n", cls.downloadname, size, uncompressedLen);
	}
	else
	{
#endif
		fwrite (net_message_buffer + net_message.readcount, 1, size, cls.download);
#ifdef R1Q2_PROTOCOL
	}
#endif

	//fwrite (net_message.data + net_message.readcount, 1, size, cls.download);
	net_message.readcount += size;

	if (percent != 100)
	{
		// request next block
// change display routines by zoid

		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
	}
	else
	{
		CL_FinishDownload();

		// get another file if needed
		CL_RequestNextDownload ();
	}
}


/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData
==================
*/
qboolean CL_ParseServerData (void)
{
	extern cvar_t	*fs_gamedirvar;
	char	*str;
	int		i;
	
	Com_DPrintf ("Serverdata packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();
	cls.state = ca_connected;

// parse protocol version number
	i = MSG_ReadLong (&net_message);
	cls.serverProtocol = i;

	if (cl.attractloop)
	{
		if(i == ENHANCED_PROTOCOL_VERSION)
			Com_Error (ERR_DROP, "This demo was recorded with R1Q2 protocol. Use R1Q2 to playback.\n");
	}
#ifdef R1Q2_PROTOCOL
	else if (i != ORIGINAL_PROTOCOL_VERSION && i != ENHANCED_PROTOCOL_VERSION)
		Com_Error (ERR_DROP, "Server is using unknown protocol %d.", i);
#else
	else if (i != ORIGINAL_PROTOCOL_VERSION)
		Com_Error (ERR_DROP, "Server is using unknown protocol %d.", i);
#endif

	cl.servercount = MSG_ReadLong (&net_message);
	cl.attractloop = MSG_ReadByte (&net_message);

	// game directory
	str = MSG_ReadString (&net_message);
	Q_strncpyz (cl.gamedir, str, sizeof(cl.gamedir));

	// set gamedir
	if ((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str))) || (!*str && (fs_gamedirvar->string || *fs_gamedirvar->string)))
		Cvar_Set("game", str);

	// parse player entity number
	cl.playernum = MSG_ReadShort (&net_message);

	// get the full level name
	str = MSG_ReadString (&net_message);

#ifdef R1Q2_PROTOCOL
	if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
	{
		int		newVersion;
		cl.enhancedServer = MSG_ReadByte (&net_message);

		newVersion = MSG_ReadShort (&net_message);
		if (newVersion != CURRENT_ENHANCED_COMPATIBILITY_NUMBER &&
			newVersion != 1902) //this version works also
		{
			Com_Printf ("Protocol 35 version mismatch (%d != %d), falling back to 34.\n", newVersion, CURRENT_ENHANCED_COMPATIBILITY_NUMBER);
			CL_Disconnect();
			cls.serverProtocol = ORIGINAL_PROTOCOL_VERSION;
			CL_Reconnect_f ();
			return false;
		}

		if (newVersion >= 1903)
		{
			cl.advancedDeltas = MSG_ReadByte (&net_message);
			pm_strafehack = MSG_ReadByte (&net_message);
			if(cl.advancedDeltas)
			{
				Com_Printf ("Server have advanced deltas enabled, those arent supported yet. Falling back to 34 protocol.\n");
				CL_Disconnect();
				cls.serverProtocol = ORIGINAL_PROTOCOL_VERSION;
				CL_Reconnect_f ();
				return false;
			}
		}
		else
		{
			cl.advancedDeltas = false;
			pm_strafehack = false;
		}
		pm_enhanced = cl.enhancedServer;
	}
	else
#endif
	{
		cl.advancedDeltas = false;
		pm_enhanced = false;
		pm_strafehack = false;
	}

	if (cl.playernum == -1)
	{	// playing a cinematic or showing a pic, not a level
		SCR_PlayCinematic (str);
	}
	else
	{
		// seperate the printfs so the server message can have a color
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Com_Printf ("%c%s\n", 2, str);

		// need to prep refresh at next oportunity
		cl.refresh_prepped = false;
	}

	return true;
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (void)
{
	entity_state_t	*es;
	unsigned		bits;
	int				newnum;
	entity_state_t	nullstate;

	memset (&nullstate, 0, sizeof(nullstate));

	newnum = CL_ParseEntityBits (&bits);

	es = &cl_entities[newnum].baseline;
	CL_ParseDelta (&nullstate, es, newnum, bits);
}

#ifdef R1Q2_PROTOCOL
void CL_ParseZPacket (void)
{
	byte buff_in[MAX_MSGLEN];
	byte buff_out[0xFFFF];
	sizebuf_t sb, old;
	int compressed_len = MSG_ReadShort (&net_message);
	int uncompressed_len = MSG_ReadShort (&net_message);
	
	if (uncompressed_len <= 0)
		Com_Error (ERR_DROP, "CL_ParseZPacket: uncompressed_len <= 0");

	if (compressed_len <= 0)
		Com_Error (ERR_DROP, "CL_ParseZPacket: compressed_len <= 0");

	MSG_ReadData (&net_message, buff_in, compressed_len);

	SZ_Init (&sb, buff_out, uncompressed_len);
	sb.cursize = ZLibDecompress (buff_in, compressed_len, buff_out, uncompressed_len, -15);

	old = net_message;
	net_message = sb;
	CL_ParseServerMessage ();
	net_message = old;

	Com_DPrintf ("Got a ZPacket, %d->%d\n", uncompressed_len + 4, compressed_len);
}
#endif

/*
================
CL_LoadClientinfo

================
*/
void CL_LoadClientinfo (clientinfo_t *ci, char *s)
{
	int i;
	char		*t;
	char		model_name[MAX_QPATH];
	char		skin_name[MAX_QPATH];
	char		model_filename[MAX_QPATH];
	char		skin_filename[MAX_QPATH];
	char		weapon_filename[MAX_QPATH];

	Q_strncpyz (ci->cinfo, s, sizeof(ci->cinfo));

	// isolate the player's name
	Q_strncpyz (ci->name, s, sizeof(ci->name));

	t = strstr (s, "\\");
	if (t)
	{
		ci->name[t-s] = 0;
		s = t+1;
	}

	t = s;
	while (*t)
	{
		if (!isprint (*t))
		{
			*s = 0;
			break;
		}
		t++;
	}

	if (cl_noskins->integer || *s == 0)
	{
		strcpy (model_filename, "players/male/tris.md2");
		strcpy (weapon_filename, "players/male/weapon.md2");
		strcpy (skin_filename, "players/male/grunt.pcx");
		strcpy (ci->iconname, "/players/male/grunt_i.pcx");
		ci->model = R_RegisterModel (model_filename);
		memset(ci->weaponmodel, 0, sizeof(ci->weaponmodel));
		ci->weaponmodel[0] = R_RegisterModel (weapon_filename);
		ci->skin = R_RegisterSkin (skin_filename);
		ci->icon = Draw_FindPic (ci->iconname);
	}
	else
	{
		// isolate the model name
		Q_strncpyz (model_name, s, sizeof(model_name));
		t = strstr(model_name, "/");
		if (!t)
			t = strstr(model_name, "\\");
		if (!t)
			t = model_name;
		*t = 0;

		// isolate the skin name
		Q_strncpyz (skin_name, s + strlen(model_name) + 1, sizeof(skin_name));

		// model file
		Com_sprintf (model_filename, sizeof(model_filename), "players/%s/tris.md2", model_name);
		ci->model = R_RegisterModel (model_filename);
		if (!ci->model)
		{
			strcpy(model_name, "male");
			Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
			ci->model = R_RegisterModel (model_filename);
		}

		// skin file
		Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
		ci->skin = R_RegisterSkin (skin_filename);

		// if we don't have the skin and the model wasn't male,
		// see if the male has it (this is for CTF's skins)
 		if (!ci->skin && Q_stricmp(model_name, "male"))
		{
			// change model to male
			strcpy(model_name, "male");
			Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
			ci->model = R_RegisterModel (model_filename);

			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
			ci->skin = R_RegisterSkin (skin_filename);
		}

		// if we still don't have a skin, it means that the male model didn't have
		// it, so default to grunt
		if (!ci->skin) {
			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/grunt.pcx", model_name);
			ci->skin = R_RegisterSkin (skin_filename);
		}

		// weapon file
		for (i = 0; i < num_cl_weaponmodels; i++) {
			Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
			ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
			if (!ci->weaponmodel[i] && strcmp(model_name, "cyborg") == 0) {
				// try male
				Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male/%s", cl_weaponmodels[i]);
				ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
			}
			if (!cl_vwep->integer)
				break; // only one when vwep is off
		}

		// icon file
		Com_sprintf (ci->iconname, sizeof(ci->iconname), "/players/%s/%s_i.pcx", model_name, skin_name);
		ci->icon = Draw_FindPic (ci->iconname);
	}

	// must have loaded all data types to be valud
	if (!ci->skin || !ci->icon || !ci->model || !ci->weaponmodel[0])
	{
		ci->skin = NULL;
		ci->icon = NULL;
		ci->model = NULL;
		ci->weaponmodel[0] = NULL;
		return;
	}
}

/*
================
CL_ParseClientinfo

Load the skin, icon, and model for a client
================
*/
void CL_ParseClientinfo (int player)
{
	char			*s;
	clientinfo_t	*ci;

	s = cl.configstrings[player+CS_PLAYERSKINS];

	ci = &cl.clientinfo[player];

	CL_LoadClientinfo (ci, s);
}


/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString (void)
{
	int		i, length;
	char	*s;
	char	olds[MAX_QPATH];

	i = MSG_ReadShort (&net_message);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
	s = MSG_ReadString(&net_message);

	Q_strncpyz (olds, cl.configstrings[i], sizeof(olds));

	//Q_strncpyz (cl.configstrings[i], s, sizeof(cl.configstrings[i]));

	length = strlen(s);

	if (i != CS_NAME && i < CS_GENERAL)
	{
		if (i >= CS_STATUSBAR && i < CS_AIRACCEL)
		{
			Q_strncpyz (cl.configstrings[i], s, (sizeof(cl.configstrings[i]) * (CS_AIRACCEL - i)));
		}
		else
		{
			if (length >= MAX_QPATH)
				Com_Printf ("WARNING: Configstring %d of length %d exceeds MAX_QPATH.\n", i, length);
			Q_strncpyz (cl.configstrings[i], s, sizeof(cl.configstrings[i]));
		}
	}
	else
	{
		strcpy (cl.configstrings[i], s);
	}

	// do something apropriate
	if(i == CS_AIRACCEL)
	{
		pm_airaccelerate = atof(cl.configstrings[CS_AIRACCEL]);
	}
	else if (i >= CS_LIGHTS && i < CS_LIGHTS+MAX_LIGHTSTYLES)
	{
		CL_SetLightstyle (i - CS_LIGHTS);
	}
	else if (i == CS_CDTRACK)
	{
#ifdef CD_AUDIO
		if (cl.refresh_prepped)
			CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
#endif
	}
	else if (i >= CS_MODELS && i < CS_MODELS+MAX_MODELS)
	{
		if( i == CS_MODELS + 1 ) {
			if( strlen( s ) > 9 ) {
				Q_strncpyz( cls.mapname, s + 5, sizeof( cls.mapname ) ); // skip "maps/"
				cls.mapname[strlen( cls.mapname ) - 4] = 0; // cut off ".bsp"
			}

		}
		if (cl.refresh_prepped)
		{
			cl.model_draw[i-CS_MODELS] = R_RegisterModel (cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i-CS_MODELS] = CM_InlineModel (cl.configstrings[i]);
			else
				cl.model_clip[i-CS_MODELS] = NULL;
		}
	}
	else if (i >= CS_SOUNDS && i < CS_SOUNDS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound (cl.configstrings[i]);
	}
	else if (i >= CS_IMAGES && i < CS_IMAGES+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.image_precache[i-CS_IMAGES] = Draw_FindPic (cl.configstrings[i]);
	}
	else if (i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS+MAX_CLIENTS)
	{
		if (cl.refresh_prepped && strcmp(olds, s))
			CL_ParseClientinfo (i-CS_PLAYERSKINS);
	}
}


/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos_v;
	float	*pos;
    int 	channel, ent;
    int 	sound_num, flags;
    float 	volume, attenuation, ofs;

	flags = MSG_ReadByte (&net_message);
	sound_num = MSG_ReadByte (&net_message);

    if (flags & SND_VOLUME)
		volume = MSG_ReadByte (&net_message) * ONEDIV255;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (flags & SND_ATTENUATION)
		attenuation = MSG_ReadByte (&net_message) * ONEDIV64;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;	

    if (flags & SND_OFFSET)
		ofs = MSG_ReadByte (&net_message) / 1000.0;
	else
		ofs = 0;

	if (flags & SND_ENT)
	{	// entity reletive
		channel = MSG_ReadShort(&net_message); 
		ent = channel>>3;
		if (ent < 0 || ent > MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & SND_POS)
	{	// positioned in space
		MSG_ReadPos (&net_message, pos_v);
 
		pos = pos_v;
	}
	else	// use entity number
		pos = NULL;

	if (!cl.sound_precache[sound_num])
		return;

	S_StartSound (pos, ent, channel, cl.sound_precache[sound_num], volume, attenuation, ofs);
}       


void SHOWNET(const char *s)
{
	if (cl_shownet->integer >= 2)
		Com_Printf ("%3i:%s\n", net_message.readcount-1, s);
}

static void CL_ParsePrint( char *s, int i )
{
	char texti[MAX_STRING_CHARS];
	int color = 0, highlight = 0, skip = 0, client = 0, mm2 = 0;
	char *timestamp = NULL;

	Q_strncpyz(texti, s, sizeof(texti));
	Q_strlwr(texti);

	if (i == PRINT_CHAT)
	{
		if( CL_ChatParse(s, &client, &skip, &mm2) )
		{
			if(!strcmp(cl.clientinfo[client].name, name->string)) //Own chat text
			{
				if(cl_mychatcolor->integer && cl_textcolors->integer)
					color = cl_mychatcolor->integer;
			}
			else
			{
				if(CL_Ignore(cl.clientinfo[client].name)) //do not show ignored msg
					return;
				
				highlight = CL_Highlight(texti+skip, &color);
			}
			CL_VersionReply(texti+skip);
		}
		else
			CL_VersionReply(texti);

		if(highlight & 1)
			S_StartLocalSound ("misc/talk1.wav");
		else
			S_StartLocalSound ("misc/talk.wav");

		SCR_AddToChatHUD(s, mm2 == 1); //Chathud

		timestamp = CL_Timestamp(true);
		if(color) {
			con.ormask = 0;
			clamp (color, 1, 7);
			if(timestamp)
				Com_Printf (S_ONE_COLOR "^%i%s %s", color, timestamp, s);
			else
				Com_Printf (S_ONE_COLOR "^%i%s", color, s);
		}
		else {
			con.ormask = 128;
			if(timestamp)
				Com_Printf("%s %s", timestamp, s);
			else
				Com_Printf("%s", s);
		}

		con.ormask = 0;
		return;
	}

	if(!cl.attractloop)
		Cmd_ExecTrigger( s ); //Triggers

	if (i == PRINT_HIGH)
	{
		if (ignorewaves->integer && (!strcmp(texti, "flipoff\n") || !strcmp(texti, "salute\n") || !strcmp(texti, "taunt\n") || !strcmp(texti, "wave\n") || !strcmp(texti, "point\n")))
			return;

		CL_ParseAutoScreenshot (texti);		
		CL_ParseAutoRecord (texti);

	}

	CL_HighlightNames(s);

	timestamp = CL_Timestamp(false);
	if(timestamp)
		Com_Printf("%s %s", timestamp, s);
	else
		Com_Printf("%s", s);
}

/*
=====================
CL_ParseServerMessage
=====================
*/
#ifdef R1Q2_PROTOCOL
#define WRITEDEMOMSG \
	if(cls.demorecording && cls.serverProtocol != ORIGINAL_PROTOCOL_VERSION) \
		CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
#else
#define WRITEDEMOMSG
#endif

void CL_ParseServerMessage (void)
{
	int			cmd, extrabits = 0;
	char		*s;
	int			i, oldReadCount;


    if (cls.doscreenshot == 2)
	{
		cls.doscreenshot = 0;
		if (cl_autoscreenshot->integer == 1)
			  Cbuf_AddText("screenshot\n");
		else if (cl_autoscreenshot->integer == 2)
			  Cbuf_AddText("screenshotjpg\n");
    }

//
// if recording demos, copy the message out
//
	if (cl_shownet->integer == 1)
		Com_Printf ("%i ",net_message.cursize);
	else if (cl_shownet->integer >= 2)
		Com_Printf ("------------------\n");

//
// parse the message
//
	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Bad server message (%d>%d)", net_message.readcount, net_message.cursize);
			break;
		}

		oldReadCount = net_message.readcount;

		cmd = MSG_ReadByte (&net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;
		}

#ifdef R1Q2_PROTOCOL
		//r1: more hacky bit stealing in the name of bandwidth
		extrabits = cmd & 0xE0;
		cmd &= 0x1F;
#endif

		if (cl_shownet->integer >= 2)
		{
			if (!svc_strings[cmd])
				Com_Printf ("%3i:BAD CMD %i\n", net_message.readcount-1,cmd);
			else
				SHOWNET(svc_strings[cmd]);
		}
	
	// other commands
		switch (cmd)
		{
		case svc_frame:
			CL_ParseFrame (extrabits);
			//CL_ParseFrame ();
			break;

		case svc_muzzleflash:
			CL_ParseMuzzleFlash ();
			WRITEDEMOMSG
			break;

		case svc_muzzleflash2:
			CL_ParseMuzzleFlash2 ();
			WRITEDEMOMSG
			break;

		case svc_temp_entity:
			CL_ParseTEnt ();
			WRITEDEMOMSG
			break;

		case svc_sound:
			CL_ParseStartSoundPacket();
			WRITEDEMOMSG
			break;

		case svc_print:
			i = MSG_ReadByte( &net_message );
			s = MSG_ReadString( &net_message );
			WRITEDEMOMSG
			CL_ParsePrint(s, i);
			break;
			
		case svc_centerprint:
			s = MSG_ReadString (&net_message);
			if (!strcmp(s, "ACTION!\n")) //Hack to show roundtime in aq2 mod -Maniac
				cls.roundtime = cl.time;

			SCR_CenterPrint (s);
			WRITEDEMOMSG
			break;
			
		case svc_stufftext:
			s = MSG_ReadString (&net_message);
			Com_DPrintf ("stufftext: %s\n", s);
			Cbuf_AddText (s);
			break;
			
		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
			if (!CL_ParseServerData())
				return;
			WRITEDEMOMSG
			break;
			
		case svc_configstring:
			CL_ParseConfigString ();
			WRITEDEMOMSG
			break;
			
		case svc_spawnbaseline:
			CL_ParseBaseline ();
			WRITEDEMOMSG
			break;

		case svc_download:
			CL_ParseDownload (false);
			WRITEDEMOMSG
			break;

		case svc_inventory:
			CL_ParseInventory ();
			WRITEDEMOMSG
			break;

		case svc_layout:
			s = MSG_ReadString (&net_message);
			WRITEDEMOMSG
			Q_strncpyz (cl.layout, s, sizeof(cl.layout));
			break;

#ifdef R1Q2_PROTOCOL
		// ************** r1q2 specific BEGIN ****************
		case svc_zpacket:
			CL_ParseZPacket();
			break;

		case svc_zdownload:
			CL_ParseDownload(true);
			break;
		// ************** r1q2 specific END ******************
#endif

		case svc_playerinfo:
		case svc_packetentities:
		case svc_deltapacketentities:
			Com_Error (ERR_DROP, "Out of place frame data");
			break;

		case svc_nop:
			break;
			
		case svc_reconnect:
			Com_Printf ("Server disconnected, reconnecting\n");
			if (cls.download) {
				//ZOID, close download
				fclose (cls.download);
				cls.download = NULL;
			}
			if(cl_autorecord->integer && cls.demorecording)
				CL_Stop_f();

			cls.downloadname[0] = 0;

			cls.state = ca_connecting;
			cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately

            SCR_ClearChatHUD_f();
			break;

		case svc_disconnect:
#ifdef R1Q2_PROTOCOL
			//r1 uuuuugly...
			if (cls.serverProtocol != ORIGINAL_PROTOCOL_VERSION && cls.realtime - cls.connect_time < 30000)
			{
				Com_Printf ("Disconnected by server, assuming protocol mismatch. Reconnecting with protocol 34.\nPlease be sure that you and the server are using the latest build of R1Q2.\n");
				CL_Disconnect();
				cls.serverProtocol = ORIGINAL_PROTOCOL_VERSION;
				CL_Reconnect_f ();
				return;
			}
			else
#endif
			{
				Com_Error (ERR_DISCONNECT, "Server disconnected\n");
			}
			WRITEDEMOMSG

			SCR_ClearChatHUD_f();
			break;

		default:
#ifdef R1Q2_PROTOCOL
			if (cls.serverProtocol != ORIGINAL_PROTOCOL_VERSION && cls.realtime - cls.connect_time < 30000)
			{
				Com_Printf ("Unknown command byte %d, assuming protocol mismatch. Reconnecting with protocol 34.\nPlease be sure that you and the server are using the latest build of R1Q2.\n", cmd);
				CL_Disconnect();
				cls.serverProtocol = ORIGINAL_PROTOCOL_VERSION;
				CL_Reconnect_f ();
				return;
			}
#endif
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Unknown command byte %d (0x%.2x)", cmd, cmd);
			break;
		}
	}

#ifdef R1Q2_PROTOCOL
	//flush this frame
	if(cls.demorecording && cls.serverProtocol != ORIGINAL_PROTOCOL_VERSION)
		CL_WriteDemoMessage (NULL, 0, true);
#endif
}

//  reply to !version
void CL_VersionReply (const char *s)
{
	if (!strstr(s, "!version"))
		return;
	
	if (cls.lastSpamTime == 0 || cls.realtime > cls.lastSpamTime + 150000)
	{
		cls.spamTime = cls.realtime + (int)(random() * 1500);
	}
}

qboolean CL_ChatParse (const char *s, int *clinu, int *skip2, int *mm2) //Seperates nick & msg from chat msg
{
	int i, client = -1, skip = 0;
	char *tmp, *start = NULL;

	for( i = 0; i < MAX_CLIENTS; i++ ) 
	{ 
		if( cl.clientinfo[i].name[0] )
		{
			tmp = strstr( s, cl.clientinfo[i].name ); 

			if( tmp && ( tmp < start || client == -1 || ( tmp == start && strlen(cl.clientinfo[i].name) > strlen(cl.clientinfo[client].name) ) ) )
			{ 
				client = i; 
				start = tmp; 
			}
		}
	}

	if( start )
	{
		skip = strlen(s) - strlen(start);
		if(skip > 0) {
			if(s[skip - 1] == '(' && s[skip + strlen(cl.clientinfo[client].name)] == ')') //Hack and only work on aq2?
				*mm2 = 1;
		}

		// skip the name 
		start += strlen( cl.clientinfo[client].name );
		skip = strlen(s) - strlen(start);

		// walk to a space (after the colon) 
		while( *(start) != ' ' && *(start) != 0 )
			start++;

		if(*start)
			skip = strlen(s) - strlen(start);

		*skip2 = skip;
		*clinu = client;

		return true;
	}

	return false;
}

void CL_HighlightNames( char *s )
{
	char *t;
	int i, j, nro = 0, temp = 0;
	int ord[MAX_CLIENTS] = { 0 };

	if(!cl_highlightnames->integer)
		return;
	
	for( i = 0; i < MAX_CLIENTS; i++ ) {
		if(strlen(cl.clientinfo[i].name) > 1)
			ord[nro++] = i;
	}

	//Put nick list to order by lenght, longest first
	for( i = 0; i < nro; i++ ) {
		for( j = i+1; j < nro; j++) {
			if( strlen(cl.clientinfo[ord[j]].name) > strlen(cl.clientinfo[ord[i]].name) )
			{
				temp = ord[i];
				ord[i] = ord[j];
				ord[j] = temp;
			}
		}
	}
	
	// highlight peoples names
	for( i = 0; i < nro; i++ ) 
	{
		char *tmp = strstr( s, cl.clientinfo[ord[i]].name );

		if( tmp )
		{
			for( t = tmp; t < tmp + strlen( cl.clientinfo[ord[i]].name ); t++ )
				*t |= 128;
		}
	}
}

int CL_Highlight ( const char *s, int *color )
{
	int highlight = 0;

	if(!cl_highlight->integer)
		return 0;

	if (strlen(cl_highlightmsg->string) < 2)
		return 0;

	if (strstr(s, cl_highlightmsg->string))
	{
		highlight = cl_highlight->integer;

		if(highlight & 2 && cl_textcolors->integer && cl_highlightcolor->integer)
			*color = cl_highlightcolor->integer;

		return highlight;
	}

	return 0;
}

char *CL_Timestamp( qboolean chat )
{
    static char timebuf[32];
    time_t clock;

	if(!cl_timestamps->integer || (!chat && cl_timestamps->integer < 2))
		return NULL;

	time( &clock );
	strftime( timebuf, sizeof(timebuf), cl_timestampsformat->string, localtime( &clock ) );
	return timebuf;
}

//Ignore and Unignore
#define MAX_I_NLENGHT	16
#define MAX_I_NICKS		32
char ignorelist[MAX_I_NICKS][MAX_I_NLENGHT];

void CL_Ignore_f(void) 
{
	int i, c, len;
	char *nick;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: \"%s <id>\" to ignore player name.\n", Cmd_Argv(0));
		Com_Printf("Current list of players in server:\n");
		Com_Printf("ID  Name             Ignored\n");
		Com_Printf("--  ---------------  -------\n");
		for (i=0;i<MAX_CLIENTS;i++) 
		{ 
			if (cl.clientinfo[i].name[0] && strcmp(cl.clientinfo[i].name, name->string)) 
			{ 
				Com_Printf("%2i  %s", i, cl.clientinfo[i].name);

				for(c=0; c<MAX_I_NICKS; c++)
				{
					if(!strcmp(cl.clientinfo[i].name, ignorelist[c]))
					{
						len = 15 - strlen(cl.clientinfo[i].name);
						for(c=0; c<len; c++)
							Com_Printf(" ");

						Com_Printf("    yes");
						break;
					}
				}
				Com_Printf("\n");
			} 
		}
		return; 
	}

	c = atoi(Cmd_Argv(1));
	if(c < 0 || c >= MAX_CLIENTS)
	{
		Com_Printf("Bad player id\n");
		return;
	}

	nick = cl.clientinfo[c].name;
	if (!*nick) 
	{ 
		// player not found
		Com_Printf("Cant find player with id number [%i]\n", c);
		return;
	}

	if(strlen(nick) >= MAX_I_NLENGHT)
	{
		Com_Printf("Ignore: Name is too long\n");
		return;
	}

	if(!strcmp(nick, name->string))
	{
		Com_Printf("You cant ignore yourself!\n");
		return;
	}

	// see if player is already in ignore list
	for(i=0; i<MAX_I_NICKS; i++)
	{
		if(!strcmp(nick, ignorelist[i]))
		{
			Com_Printf("Player [%s] is already in ignorelist. Type \"unignorenick %i\" to remove it.\n", nick, i);
			return;
		}
	}

	for(i=0; i<MAX_I_NICKS; i++) // find a free slot
	{
		if(!ignorelist[i][0])
		{
			strcpy(ignorelist[i], nick);
			Com_Printf("Player [%s] is now ignored!\n", nick);
			return;
		}
	}

	Com_Printf("All ignore slots is full.\n");
}

void CL_Unignore_f(void)
{
	int i = 0, c = 0;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: \"%s <id>\" to unignore player name that match with id.\n",Cmd_Argv(0));
		Com_Printf("Current ignores:\n");

		for(i=0;i<MAX_I_NICKS;i++)
		{
			if(ignorelist[i][0])
			{
				c = 1;
				break;
			}
		}

		if(c)
		{
			Com_Printf("ID  Name\n");
			Com_Printf("--  ---------------\n");
			for(i=0; i < MAX_I_NICKS; i++)
				if(ignorelist[i][0])
					Com_Printf("%2i  %s\n", i, ignorelist[i]);
		}
		else
			Com_Printf("Ignorelist is empty.\n");

		return; 
	}

	if(!Q_stricmp(Cmd_Argv(1), "all"))
	{
		for(i=0; i < MAX_I_NICKS; i++)
		{
			if(ignorelist[i][0])
			{
				ignorelist[i][0] = 0;
				c++;
			}
		}
		if(c)
			Com_Printf("Removed %i nicks from ignorelist.\n", c);
		else
			Com_Printf("Ignorelist is already empty.\n");

		return;

	}

	c = atoi(Cmd_Argv(1));
	if(c < 0 || c >= MAX_CLIENTS)
	{
		Com_Printf("Bad player id\n");
		return;
	}

	if(ignorelist[c][0])
	{
		Com_Printf("Player [%s] removed from ignorelist\n", ignorelist[c]);
		ignorelist[c][0] = 0;
	}
	else
		Com_Printf("Cant find player with id number [%i] in ignore list\n", c);

}

qboolean CL_Ignore(const char *s)
{
	int i;

	for(i=0; i<MAX_I_NICKS; i++)
	{
		if(ignorelist[i][0])
		{
			if(!strcmp(s, ignorelist[i]))
				return true;
		}
	}
	return false;
}

void CL_ParseAutoScreenshot (const char *s)
{
	if(!cl_autoscreenshot->integer)
		return;

	if (strstr(s, "timelimit hit")) {
		SCR_ClearChatHUD_f();
		cls.doscreenshot = 1;
	}
	else if (strstr(s, "capturelimit hit")) {
		SCR_ClearChatHUD_f();
		cls.doscreenshot = 1;
	}
	else if (strstr(s, "fraglimit hit")) {
		SCR_ClearChatHUD_f();
		cls.doscreenshot = 1;
	}
	else if (strstr(s, cl_customlimitmsg->string) ) {
		SCR_ClearChatHUD_f();
		cls.doscreenshot = 1;
	}

}

static void OnChange_HLColor (cvar_t *self, const char *oldValue)
{
	if(cl_highlightcolor->integer < 0)
		Cvar_SetValue ("cl_highlightcolor", 0);
	else if(cl_highlightcolor->integer > 7)
		Cvar_SetValue ("cl_highlightcolor", 7);
}

static void OnChange_MyColor (cvar_t *self, const char *oldValue)
{
	if(cl_mychatcolor->integer < 0)
		Cvar_SetValue ("cl_mychatcolor", 0);
	else if(cl_mychatcolor->integer > 7)
		Cvar_SetValue ("cl_mychatcolor", 7);
}

void CL_InitParse( void )
{
	cl_timestamps = Cvar_Get("cl_timestamps","0", CVAR_ARCHIVE);
	cl_timestampsformat = Cvar_Get("cl_timestampsformat", "[%H:%M:%S]", 0);

	cl_highlight = Cvar_Get ("cl_highlight",  "0", CVAR_ARCHIVE);
	cl_highlightmsg = Cvar_Get ("cl_highlightmsg",  "", CVAR_ARCHIVE);
	cl_highlightcolor = Cvar_Get ("cl_highlightcolor", "0", CVAR_ARCHIVE);
	cl_highlightnames = Cvar_Get ("cl_highlightnames",  "0", CVAR_ARCHIVE);

	cl_textcolors = Cvar_Get ("cl_textcolors", "0", CVAR_ARCHIVE);
	cl_mychatcolor = Cvar_Get ("cl_mychatcolor", "0", CVAR_ARCHIVE);

	cl_highlightcolor->OnChange = OnChange_HLColor;
	cl_mychatcolor->OnChange = OnChange_MyColor;
	OnChange_HLColor(cl_highlightcolor, cl_highlightcolor->resetString);
	OnChange_MyColor(cl_mychatcolor, cl_mychatcolor->resetString);

	cl_autoscreenshot = Cvar_Get ("cl_autoscreenshot", "0", 0);
	ignorewaves = Cvar_Get ("ignorewaves", "0", 0);

	Cmd_AddCommand ("ignorenick", CL_Ignore_f);
	Cmd_AddCommand ("unignorenick", CL_Unignore_f);
}
