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

#include "qcommon.h"

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

//
// in memory
//

typedef struct packfile_s
{
	char		name[MAX_QPATH];
	int			filepos, filelen;

	struct packfile_s *hashNext;
} packfile_t;

typedef struct pack_s
{
	char		filename[MAX_OSPATH];
	FILE		*handle;
	int			numfiles;
	packfile_t	*files;
	packfile_t	**fileHash;
	int			hashSize;
} pack_t;

char	fs_gamedir[MAX_OSPATH];
cvar_t	*fs_basedir;
//cvar_t	*fs_cddir;
cvar_t	*fs_gamedirvar;
cvar_t	*fs_allpakloading;

typedef struct searchpath_s
{
	char		filename[MAX_OSPATH];
	pack_t		*pack;		// only one of filename / pack will be used
	struct		searchpath_s *next;
} searchpath_t;

static searchpath_t	*fs_searchpaths;
static searchpath_t	*fs_base_searchpaths;	// without gamedirs

#define FS_MAX_HASH_SIZE 1024

/*

All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

*/

/*
===========
FS_PackHashKey
===========
*/
static int FS_HashKey( const char *name, int hashSize )
{
	int i;
	unsigned int c, hash = 0;

	for( i = 0; name[i]; i++ ) {
		c = tolower(name[i]);
		if( c == '\\' )
			c = '/';
		hash += c * (i+119);
	}
	return hash & (hashSize - 1);
}

/*
================
FS_filelength
================
*/
int FS_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}


/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
void FS_CreatePath (char *path)
{
	char	*ofs;
	
	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
			Sys_Mkdir (path);
			*ofs = '/';
		}
	}
}

/*
==============
FS_FCloseFile

For some reason, other dll's can't just cal fclose()
on files returned by FS_FOpenFile...
==============
*/
void FS_FCloseFile (FILE *f)
{
	fclose (f);
}


// RAFAEL
/*
	Developer_searchpath
*/
int	Developer_searchpath (void)
{
	searchpath_t	*search;
	
	for (search = fs_searchpaths ; search ; search = search->next)
	{
		if (strstr (search->filename, "xatrix"))
			return 1;

		if (strstr (search->filename, "rogue"))
			return 2;
	}
	return 0;
}


/*
===========
FS_FOpenFile

Finds the file in the search path.
returns filesize and an open FILE *
Used for streaming data out of either a pak file or
a seperate file.
===========
*/
int file_from_pak = 0;

int FS_FOpenFile (const char *filename, FILE **file)
{
	unsigned int	hash;
	searchpath_t	*search;
	char			netpath[MAX_OSPATH];
	pack_t			*pak;
	packfile_t		*pakfile;

	file_from_pak = 0;

//
// search through the path, one element at a time
//
	for (search = fs_searchpaths ; search ; search = search->next)
	{
	// is the element a pak file?
		if (search->pack)
		{
		// look through all the pak file elements
			pak = search->pack;
			hash = FS_HashKey(filename, pak->hashSize);
			for ( pakfile = pak->fileHash[hash]; pakfile; pakfile = pakfile->hashNext)
				if (!Q_stricmp( pakfile->name, filename ) )	{	// found it!
					file_from_pak = 1;
					Com_DPrintf ("PackFile: %s : %s\n",pak->filename, filename);
					// open a new file on the pakfile
					*file = fopen (pak->filename, "rb");
					if (!*file)
						Com_Error (ERR_FATAL, "Couldn't reopen %s", pak->filename);	
					fseek (*file, pakfile->filepos, SEEK_SET);
					return pakfile->filelen;
				}
		}
		else
		{		
	// check a file in the directory tree
			
			Com_sprintf (netpath, sizeof(netpath), "%s/%s",search->filename, filename);
			
			*file = fopen (netpath, "rb");
#ifndef _WIN32
			if (!*file)
			{
				Q_strlwr(netpath);
				*file = fopen (netpath, "rb");
			}
#endif
			if (!*file)
				continue;
			
			Com_DPrintf ("FindFile: %s\n",netpath);

			return FS_filelength (*file);
		}
		
	}
	
	Com_DPrintf ("FindFile: can't find %s\n", filename);
	
	file = NULL;
	return -1;
}

/*
=================
FS_ReadFile

Properly handles partial reads
=================
*/
#ifdef CD_AUDIO
void CDAudio_Stop(void);
#endif
#define	MAX_READ	0x10000		// read in blocks of 64k
void FS_Read (void *buffer, int len, FILE *f)
{
	int		block, remaining;
	int		read;
	byte	*buf;
#ifdef CD_AUDIO
	int		tries = 0;
#endif

	buf = (byte *)buffer;

	// read in chunks for progress bar
	remaining = len;

	while (remaining)
	{
		block = remaining;
		if (block > MAX_READ)
			block = MAX_READ;
		read = fread (buf, 1, block, f);
		if (read == 0)
		{
			// we might have been trying to read from a CD
#ifdef CD_AUDIO
			if (!tries)
			{
				tries = 1;
				CDAudio_Stop();
			}
			else
#endif
				Com_Error (ERR_FATAL, "FS_Read: 0 bytes read");
		}

		if (read == -1)
			Com_Error (ERR_FATAL, "FS_Read: -1 bytes read");

		// do some progress bar thing here...

		remaining -= read;
		buf += read;
	}
}

/*
============
FS_LoadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_LoadFile (const char *path, void **buffer)
{
	FILE	*h;
	byte	*buf = NULL;
	int		len;


// look for it in the filesystem or pack files
	len = FS_FOpenFile (path, &h);
	if (!h)
	{
		if (buffer)
			*buffer = NULL;
		return -1;
	}

	if (!buffer)
	{
		fclose (h);
		return len;
	}

	buf = Z_TagMalloc(len+1, TAGMALLOC_FSLOADFILE);
	buf[len] = 0;
	*buffer = buf;

	FS_Read (buf, len, h);

	fclose (h);

	return len;
}


/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile (void *buffer)
{
	Z_Free (buffer);
}

/*
=================
FS_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
static pack_t *FS_LoadPackFile (const char *packfile)
{
	dpackheader_t	header;
	int				i;
	packfile_t		*file;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dpackfile_t		*info;
	int				hashSize;
	unsigned int	hash;

	packhandle = fopen(packfile, "rb");
	if (!packhandle)
		return NULL;

	fread (&header, 1, sizeof(header), packhandle);
	if (LittleLong(header.ident) != IDPAKHEADER) {
		Com_Error (ERR_FATAL, "FS_LoadPackFile: %s is not a packfile", packfile);
		return NULL;
	}

	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);
	if (numpackfiles <= 0) {
		Com_Printf("FS_LoadPackFile: '%s' has %i files", packfile, numpackfiles);
		return NULL;
	}

	if(fseek(packhandle, header.dirofs, SEEK_SET)) {
		Com_Printf("FS_LoadPackFile: fseek() to offset %u in %s failed (corrupt packfile?)", header.dirofs, packfile);
		return NULL;
	}

	info = Z_TagMalloc (numpackfiles * sizeof(dpackfile_t), TAGMALLOC_FSLOADPAK);
	if (fread(info, 1, header.dirlen, packhandle) != header.dirlen) {
		Com_Printf("FS_LoadPackFile: error reading packfile directory from %s (failed to read %u bytes at %u)", packfile, header.dirofs, header.dirlen);
		Z_Free(info);
		return NULL;
	}

	for(hashSize = 1; (hashSize < numpackfiles) && (hashSize < FS_MAX_HASH_SIZE); hashSize <<= 1);

	pack = Z_TagMalloc (sizeof(pack_t) + numpackfiles * sizeof(packfile_t) + hashSize * sizeof(packfile_t *), TAGMALLOC_FSLOADPAK);
	Q_strncpyz(pack->filename, packfile, sizeof(pack->filename));
	pack->files = ( packfile_t * )((byte *)pack + sizeof(pack_t));
	pack->fileHash = ( packfile_t **)((byte *)pack->files + numpackfiles * sizeof( packfile_t ));
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->hashSize = hashSize;

	for(i = 0; i<hashSize; i++)
		pack->fileHash[i] = NULL;
// parse the directory
	for (i=0, file=pack->files; i<numpackfiles; i++, file++)
	{
		Q_strncpyz (file->name, info[i].name, sizeof(file->name));
		file->filepos = LittleLong(info[i].filepos);
		file->filelen = LittleLong(info[i].filelen);

		hash = FS_HashKey(file->name, hashSize);
		file->hashNext = pack->fileHash[hash];
		pack->fileHash[hash] = file;
	}
	Z_Free(info);
	Com_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}


/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
static void FS_AddGameDirectory (const char *dir)
{
	int				i, numfiles;
	searchpath_t	*search;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];
	char			**searchnames;
	int   j;
	char buf[16];
	qboolean numberedpak;

	Q_strncpyz(fs_gamedir, dir, sizeof(fs_gamedir));

	//
	// add the directory to the search path
	//
	search = Z_TagMalloc (sizeof(searchpath_t), TAGMALLOC_SEARCHPATH);
	Q_strncpyz (search->filename, dir, sizeof(search->filename));
	search->pack = NULL;
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	//
	// add any pak files in the format pak0.pak pak1.pak, ...
	//
	for (i=0; i<100; i++) //0-99 pak loading
	{
		Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		pak = FS_LoadPackFile (pakfile);
		if (!pak)
			continue;
		search = Z_TagMalloc (sizeof(searchpath_t), TAGMALLOC_SEARCHPATH);
		search->filename[0] = 0;
		search->pack = pak;
		search->next = fs_searchpaths;
		fs_searchpaths = search;		
	}

	if(!fs_allpakloading->integer)
		return;

	Com_sprintf (pakfile, sizeof(pakfile), "%s/*.pak", dir);

	searchnames = FS_ListFiles (pakfile, &numfiles, 0, 0);
	if (searchnames)
	{
		for (i=0 ; i<numfiles - 1 ; i++)
		{
			if( Q_stricmp(searchnames[i]+strlen(searchnames[i])-4, ".pak") )
				continue;

			numberedpak = false;

			for (j=0; j<100; j++)
			{
				Com_sprintf( buf, sizeof(buf), "/pak%i.pak", j);
				if ( strstr(searchnames[i], buf) )
				{
					numberedpak = true;
					break;
				}
			}

			if (numberedpak)
				continue;

			pak = FS_LoadPackFile (searchnames[i]);
			if (!pak)
				continue;

			search = Z_TagMalloc (sizeof(searchpath_t), TAGMALLOC_SEARCHPATH);
			search->filename[0] = 0;
			search->pack = pak;
			search->next = fs_searchpaths;
			fs_searchpaths = search;
		}

		for (i=0 ; i<numfiles - 1 ; i++)
			Z_Free (searchnames[i]);

		Z_Free (searchnames);
	}
}

/*
================
FS_AddHomeAsGameDirectory

Use ~/.quake2/dir as fs_gamedir
================
*/
static void FS_AddHomeAsGameDirectory (const char *dir)
{
#ifndef _WIN32
	char gdir[MAX_OSPATH];
	char *homedir = getenv("HOME");

	if(homedir) {
		int len = snprintf(gdir,sizeof(gdir),"%s/.quake2/%s/", homedir, dir);
		Com_Printf("using %s for writing\n",gdir);
		FS_CreatePath (gdir);

		if ((len > 0) && (len < sizeof(gdir)) && (gdir[len-1] == '/'))
			gdir[len-1] = 0;

		FS_AddGameDirectory (gdir);
	}
#endif
}
/*
============
FS_Gamedir

Called to find where to write a file (demos, savegames, etc)
============
*/
char *FS_Gamedir (void)
{
	if (*fs_gamedir)
		return fs_gamedir;
	else
		return BASEDIRNAME;
}

/*
=============
FS_ExecAutoexec
=============
*/
void FS_ExecAutoexec (void)
{
	char *dir;
	char name [MAX_QPATH];

	dir = Cvar_VariableString("gamedir");
	if (*dir)
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, dir); 
	else
		Com_sprintf(name, sizeof(name), "%s/%s/autoexec.cfg", fs_basedir->string, BASEDIRNAME); 
	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
		Cbuf_AddText ("exec autoexec.cfg\n");
	Sys_FindClose();
}


/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
void FS_SetGamedir (const char *dir)
{
	searchpath_t	*next;

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":") )
	{
		Com_Printf ("Gamedir should be a single filename, not a path\n");
		return;
	}

	//
	// free up any current game dir info
	//
	while (fs_searchpaths != fs_base_searchpaths)
	{
		if (fs_searchpaths->pack)
		{
			fclose (fs_searchpaths->pack->handle);
			Z_Free (fs_searchpaths->pack);
		}
		next = fs_searchpaths->next;
		Z_Free (fs_searchpaths);
		fs_searchpaths = next;
	}

	//
	// flush all data, so it will be forced to reload
	//
	if (dedicated && !dedicated->integer)
		Cbuf_AddText ("vid_restart\nsnd_restart\n");

	Com_sprintf (fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, dir);

	if (!strcmp(dir,BASEDIRNAME) || (*dir == 0))
	{
		Cvar_FullSet ("gamedir", "", CVAR_SERVERINFO|CVAR_NOSET);
		Cvar_FullSet ("game", "", CVAR_LATCH|CVAR_SERVERINFO);
	}
	else
	{
		Cvar_FullSet ("gamedir", dir, CVAR_SERVERINFO|CVAR_NOSET);
		//if (fs_cddir->string[0])
		//	FS_AddGameDirectory (va("%s/%s", fs_cddir->string, dir) );
		FS_AddGameDirectory (va("%s/%s", fs_basedir->string, dir) );
		FS_AddHomeAsGameDirectory(dir);
	}
}


/*
** FS_ListFiles
*/
char **FS_ListFiles( const char *findname, int *numfiles, unsigned musthave, unsigned canthave )
{
	char *s;
	int nfiles = 0;
	char **list = 0;

	s = Sys_FindFirst( findname, musthave, canthave );
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
			nfiles++;
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	if ( !nfiles )
		return NULL;

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	list = Z_TagMalloc( sizeof( char * ) * nfiles, TAGMALLOC_FILELIST );
	memset( list, 0, sizeof( char * ) * nfiles );

	s = Sys_FindFirst( findname, musthave, canthave );
	nfiles = 0;
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
		{
			list[nfiles] = CopyString( s, TAGMALLOC_FILELIST );
#ifdef _WIN32
			Q_strlwr( list[nfiles] );
#endif
			nfiles++;
		}
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	return list;
}

/*
** FS_Dir_f
*/
static void FS_Dir_f( void )
{
	char	*path = NULL;
	char	findname[1024];
	char	wildcard[1024] = "*.*";
	char	**dirnames;
	int		ndirs;

	if ( Cmd_Argc() != 1 )
	{
		strcpy( wildcard, Cmd_Argv( 1 ) );
	}

	while ( ( path = FS_NextPath( path ) ) != NULL )
	{
		char *tmp = findname;

		Com_sprintf( findname, sizeof(findname), "%s/%s", path, wildcard );

		while ( *tmp != 0 )
		{
			if ( *tmp == '\\' ) 
				*tmp = '/';
			tmp++;
		}
		Com_Printf( "Directory of %s\n", findname );
		Com_Printf( "----\n" );

		if ( ( dirnames = FS_ListFiles( findname, &ndirs, 0, 0 ) ) != 0 )
		{
			int i;

			for ( i = 0; i < ndirs-1; i++ )
			{
				if ( strrchr( dirnames[i], '/' ) )
					Com_Printf( "%s\n", strrchr( dirnames[i], '/' ) + 1 );
				else
					Com_Printf( "%s\n", dirnames[i] );

				Z_Free( dirnames[i] );
			}
			Z_Free( dirnames );
		}
		Com_Printf( "\n" );
	}
}

/*
============
FS_Path_f

============
*/
static void FS_Path_f (void)
{
	searchpath_t	*s;

	Com_Printf ("Current search path:\n");
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s == fs_base_searchpaths)
			Com_Printf ("----------\n");
		if (s->pack)
			Com_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf ("%s\n", s->filename);
	}
}

#if 0
typedef struct packList_s
{
	char *pakname;
	char *filename;
} packList_t;

packList_t *FS_ListPakFiles (const char *findname, int *numfiles)
{
	searchpath_t	*search;
	pack_t			*pak;
	int				i = 0, j = 0, count = 0;
	packList_t		*fileList = NULL;
	qboolean		skip = false;


	for (search = fs_searchpaths ; search ; search = search->next) {
		if (search->pack) {	// look through all the pak file elements
			for (i = 0, pak = search->pack; i < pak->numfiles; i++)	{
				if (Com_WildCmp(findname, pak->files[i].name, true ) )
					count++;
			}
		}
	}

	if(!count)
		return NULL;

	fileList = Z_TagMalloc (sizeof(packList_t) * count, TAGMALLOC_FILELIST);
	count = 0;

	for (search = fs_searchpaths ; search ; search = search->next)
	{
		if (search->pack) {	// look through all the pak file elements
			for (i = 0, pak = search->pack; i < pak->numfiles; i++)
			{
				if ( Com_WildCmp(findname, pak->files[i].name, true ) )
				{
					skip = false;
					for(j = 0; j < count; j++) { //Check so dont add same file from different pak to list
						if( !Q_stricmp(fileList[j].filename, pak->files[i].name) ) {
							skip = true;
							break;
						}
					}

					if(skip)
						continue;

					fileList[count].pakname = CopyString(pak->filename, TAGMALLOC_FILELIST);
					fileList[count].filename = CopyString(pak->files[i].name, TAGMALLOC_FILELIST);
					count++;
				}
			}
		}
	}
	*numfiles = count;
	return fileList;
}

void FS_PakFind_f (void)
{
	packList_t *fileList = NULL;
	int	i = 0, numFiles = 0;

	if ( Cmd_Argc() != 2 )
	{
		Com_Printf("Usage: pakfind <string>\n" );
		return;
	}

	fileList = FS_ListPakFiles ( Cmd_Argv(1), &numFiles );

	if(!fileList)
	{
		Com_Printf("Didnt find any files matching to '%s'\n", Cmd_Argv(1));
		return;
	}

	for(i = 0; i < numFiles; i++)
	{
		Com_Printf ("PackFile: %s (%s)\n", fileList[i].filename, fileList[i].pakname);
		Z_Free(fileList[i].pakname);
		Z_Free (fileList[i].filename);
	}
	Z_Free (fileList);

	Com_Printf("%i files found\n", numFiles);
}
#endif

/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char *FS_NextPath(char *prevpath)
{
	searchpath_t *s;
	char *prev;

	prev = NULL; // fs_gamedir is the first directory in the searchpath
	for (s = fs_searchpaths; s; s = s->next) {
		if (s->pack)
			continue;
		if (prevpath == NULL)
			return s->filename;
		if (prevpath == prev)
			return s->filename;
		prev = s->filename;
	}

	return NULL;
}


static void FS_PakList_f( void )
{
	searchpath_t *s;

	for (s = fs_searchpaths; s; s = s->next)
	{
		if (s->pack)
			Com_Printf ("PackFile: %s (%i files)\n", s->pack, s->pack->numfiles);
	}
}

/*
================
FS_InitFilesystem
================
*/
void FS_InitFilesystem (void)
{
	Cmd_AddCommand ("path", FS_Path_f);
	Cmd_AddCommand ("dir", FS_Dir_f );

	Cmd_AddCommand ("paklist", FS_PakList_f );
//	Cmd_AddCommand ("pakfind", FS_PakFind_f );

	//
	// basedir <path>
	// allows the game to run from outside the data tree
	//
	fs_basedir = Cvar_Get ("basedir", ".", CVAR_NOSET);
	fs_allpakloading = Cvar_Get ("fs_allpakloading", "0", CVAR_ARCHIVE);

	//
	// cddir <path>
	// Logically concatenates the cddir after the basedir for 
	// allows the game to run from outside the data tree
	//
	//fs_cddir = Cvar_Get ("cddir", "", CVAR_NOSET);
	//if (fs_cddir->string[0])
	//	FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_cddir->string) );

	//
	// add baseq2 to search path
	//
	FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_basedir->string));

	//
	// then add a '.quake2/baseq2' directory in home directory by default
	//
	FS_AddHomeAsGameDirectory(BASEDIRNAME);

	// any set gamedirs will be freed up to here
	fs_base_searchpaths = fs_searchpaths;

	// check for game override
	fs_gamedirvar = Cvar_Get ("game", "", CVAR_LATCH|CVAR_SERVERINFO);
	if (fs_gamedirvar->string[0])
		FS_SetGamedir (fs_gamedirvar->string);
}
