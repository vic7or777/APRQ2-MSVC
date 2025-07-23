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

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

//===============================================================================

//static int	hunkcount;

static byte	*membase;
static int	hunkmaxsize;
static int	cursize;

#define	VIRTUAL_ALLOC

void *Hunk_Begin (int maxsize)
{
	// reserve a huge chunk of memory, but don't commit any yet
	cursize = 0;
	hunkmaxsize = maxsize;
#ifdef VIRTUAL_ALLOC
	membase = VirtualAlloc (NULL, maxsize, MEM_RESERVE, PAGE_NOACCESS);
#else
	membase = malloc (maxsize);
	memset (membase, 0, maxsize);
#endif
	if (!membase)
		Sys_Error ("VirtualAlloc reserve failed");
	return (void *)membase;
}

void *Hunk_Alloc (int size)
{
#ifdef VIRTUAL_ALLOC
	void	*buf;
#endif

	// round to cacheline
	size = (size+31)&~31;

#ifdef VIRTUAL_ALLOC
	// commit pages as needed
//	buf = VirtualAlloc (membase+cursize, size, MEM_COMMIT, PAGE_READWRITE);
	buf = VirtualAlloc (membase, cursize+size, MEM_COMMIT, PAGE_READWRITE);
	if (!buf)
	{
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		Sys_Error ("VirtualAlloc commit failed.\n%s", (char *)buf);
		LocalFree(buf);
	}
#endif
	cursize += size;
	if (cursize > hunkmaxsize)
		Sys_Error ("Hunk_Alloc overflow");

	return (void *)(membase+cursize-size);
}

int Hunk_End (void)
{

	// free the remaining unused virtual memory
#if 0
	void	*buf;

	// write protect it
	buf = VirtualAlloc (membase, cursize, MEM_COMMIT, PAGE_READONLY);
	if (!buf)
		Sys_Error ("VirtualAlloc commit failed");
#endif

	//hunkcount++;
//Com_Printf ("hunkcount: %i\n", hunkcount);
	return cursize;
}

void Hunk_Free (void *base)
{
	if ( base )
#ifdef VIRTUAL_ALLOC
		VirtualFree (base, 0, MEM_RELEASE);
#else
		free (base);
#endif

	//hunkcount--;
}

//===============================================================================


/*
================
Sys_Milliseconds
================
*/
int	curtime;

int Sys_Milliseconds (void)
{
	static int		base;
	static qboolean	initialized = false;

	if (!initialized)
	{	// let base retain 16 bits of effectively random data
		base = timeGetTime() & 0xffff0000;
		initialized = true;
	}

	curtime = timeGetTime() - base;

	return curtime;
}

void Sys_Mkdir (const char *path)
{
	_mkdir (path);
}

//============================================

static char	findbase[MAX_OSPATH];
static char	findpath[MAX_OSPATH];
static HANDLE	findhandle = INVALID_HANDLE_VALUE;

static qboolean CompareAttributes( unsigned found, unsigned musthave, unsigned canthave )
{
	if (found & FILE_ATTRIBUTE_DIRECTORY) {
		if (canthave & SFF_SUBDIR)
			return false;
	}
	else if (musthave & SFF_SUBDIR)
		return false;

	if (found & FILE_ATTRIBUTE_HIDDEN) {
		if (canthave & SFF_HIDDEN)
			return false;
	}
	else if (musthave & SFF_HIDDEN)
		return false;

	if (found & FILE_ATTRIBUTE_SYSTEM) {
		if (canthave & SFF_SYSTEM)
			return false;
	}
	else if (musthave & SFF_SYSTEM)
		return false;

	return true;
}

char *Sys_FindFirst (const char *path, unsigned musthave, unsigned canthave )
{
	WIN32_FIND_DATA	findinfo;

	if (findhandle != INVALID_HANDLE_VALUE)
		Sys_Error ("Sys_FindFirst without close");

	COM_FilePath (path, findbase);
	findhandle = FindFirstFile(path, &findinfo);

	if (findhandle == INVALID_HANDLE_VALUE)
		return NULL;

	do
	{
		if (CompareAttributes(findinfo.dwFileAttributes, musthave, canthave))
		{
			Com_sprintf (findpath, sizeof(findpath), "%s/%s", findbase, findinfo.cFileName);
			return findpath;
		}
	}
	while(FindNextFile(findhandle, &findinfo));

	return NULL; 
}

char *Sys_FindNext ( unsigned musthave, unsigned canthave )
{
	WIN32_FIND_DATA	findinfo;

	if (findhandle == INVALID_HANDLE_VALUE)
		return NULL;

	while (FindNextFile(findhandle, &findinfo))
	{
		if (CompareAttributes(findinfo.dwFileAttributes, musthave, canthave))
		{
			Com_sprintf (findpath, sizeof(findpath), "%s/%s", findbase, findinfo.cFileName);
			return findpath;
		}
	}

	return NULL; 
}


void Sys_FindClose (void)
{
	if (findhandle != INVALID_HANDLE_VALUE)
		FindClose(findhandle);

	findhandle = INVALID_HANDLE_VALUE;
}


//============================================

