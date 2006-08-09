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
// models.c -- model loading and caching

#include "gl_local.h"

static model_t	*loadmodel;
static int		modfilelen;

static void Mod_LoadAliasMD2Model (model_t *mod, void *buffer);
static void Mod_LoadAliasMD3Model (model_t *mod, void *buffer);
static void Mod_LoadSpriteModel (model_t *mod, void *buffer);
static void Mod_LoadBrushModel (model_t *mod, void *buffer);

static byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
static model_t	mod_known[MAX_MOD_KNOWN];
static int		mod_numknown;

#define MODELS_HASH_SIZE 32
static model_t *mod_hash[MODELS_HASH_SIZE];

// the inline * models from the current map are kept seperate
static model_t	mod_inline[MAX_MOD_KNOWN];

int		registration_sequence;

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (const vec3_t p, const model_t *model)
{
	mnode_t	*node;
	float	d;
	cplane_t	*plane;
	
	if (!model || !model->nodes)
		Com_Error (ERR_DROP, "Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents != CONTENTS_NODE)
			return (mleaf_t *)node;
		plane = node->plane;
		if ( plane->type < 3 )
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (p, plane->normal) - plane->dist;

		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
static byte *Mod_DecompressVis (const byte *in, const model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->vis->numclusters+7)>>3;	
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
	
	return decompressed;
}

/*
==============
Mod_ClusterPVS
==============
*/
byte *Mod_ClusterPVS (int cluster, const model_t *model)
{
	if (cluster == -1 || !model->vis)
		return mod_novis;
	return Mod_DecompressVis ( (byte *)model->vis + model->vis->bitofs[cluster][DVIS_PVS],
		model);
}


//===============================================================================

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f (void)
{
	int		i;
	model_t	*mod;
	int		total = 0;

	Com_Printf ("Loaded models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		Com_Printf ("%8i : %s\n",mod->extradatasize, mod->name);
		total += mod->extradatasize;
	}
	Com_Printf ("Total resident: %i\n", total);
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
	GL_ClearDecals (); //Decals
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
static model_t *Mod_ForName (const char *name, qboolean crash)
{
	model_t	*mod;
	unsigned *buf;
	int		i;
	unsigned int hash;
	
	if (!name || !name[0])
		Com_Error (ERR_DROP, "Mod_ForName: NULL name");
		
	//
	// inline models are grabbed only from worldmodel
	//
	if (name[0] == '*')
	{
		i = atoi(name+1);
		if (i < 1 || !r_worldmodel || i >= r_worldmodel->numsubmodels)
			Com_Error (ERR_DROP, "Mod_ForName: bad inline model number");
		return &mod_inline[i];
	}

	//
	// search the currently loaded models
	//
	hash = Com_HashKey(name, MODELS_HASH_SIZE);
	for (mod=mod_hash[hash]; mod; mod = mod->hashNext)
	{
		if (!strcmp (mod->name, name) )
			return mod;
	}
	
	//
	// find a free model slot spot
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!mod->name[0])
			break;	// free spot

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Com_Error (ERR_DROP, "Mod_ForName: mod_numknown == MAX_MOD_KNOWN");
		mod_numknown++;
	}
	Q_strncpyz (mod->name, name, sizeof(mod->name));

	//
	// load the file
	//
	modfilelen = FS_LoadFile(mod->name, (void **)&buf);
	if (!buf)
	{
		if (crash)
			Com_Error (ERR_DROP, "Mod_ForName: %s not found", mod->name);
		memset (mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	loadmodel = mod;

	//
	// fill it in
	//


	// call the apropriate loader
	
	switch (LittleLong(*(unsigned *)buf))
	{
	case IDALIASHEADER:
		Mod_LoadAliasMD2Model (mod, buf);
		break;
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;
	case IDBSPHEADER:
		loadmodel->extradata = Hunk_Begin (0x1000000);
		Mod_LoadBrushModel (mod, buf);
		loadmodel->extradatasize = Hunk_End ();
		break;
	case IDMD3HEADER:
		Mod_LoadAliasMD3Model (mod, buf);
		break;
	default:
		Com_Error (ERR_DROP,"Mod_ForName: unknown fileid for %s", mod->name);
		break;
	}

	mod->hashNext = mod_hash[hash];
	mod_hash[hash] = mod;

	FS_FreeFile (buf);

	return mod;
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

static byte	*mod_base;


/*
=================
Mod_LoadLighting
=================
*/
static void Mod_LoadLighting (const lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->lightdata = Hunk_Alloc (l->filelen);
	loadmodel->staindata = Hunk_Alloc (l->filelen);	// Stainmaps
	memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
	memcpy (loadmodel->staindata, mod_base + l->fileofs, l->filelen);	// Stainmaps
}


/*
=================
Mod_LoadVisibility
=================
*/
static void Mod_LoadVisibility (const lump_t *l)
{
#ifndef ENDIAN_LITTLE
	int		i;
#endif
	if (!l->filelen)
	{
		loadmodel->vis = NULL;
		return;
	}
	loadmodel->vis = Hunk_Alloc ( l->filelen);	
	memcpy (loadmodel->vis, mod_base + l->fileofs, l->filelen);
#ifndef ENDIAN_LITTLE
	loadmodel->vis->numclusters = LittleLong (loadmodel->vis->numclusters);
	for (i=0 ; i<loadmodel->vis->numclusters ; i++)
	{
		loadmodel->vis->bitofs[i][0] = LittleLong (loadmodel->vis->bitofs[i][0]);
		loadmodel->vis->bitofs[i][1] = LittleLong (loadmodel->vis->bitofs[i][1]);
	}
#endif
}


/*
=================
Mod_LoadVertexes
=================
*/
static void Mod_LoadVertexes (const lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "Mod_LoadBmodel: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
RadiusFromBounds
=================
*/
static float RadiusFromBounds (const vec3_t mins, const vec3_t maxs)
{
	vec3_t	corner;
	float	val1, val2;

	val1 = (float)fabs(mins[0]);
	val2 = (float)fabs(maxs[0]);
	corner[0] = (val1 > val2) ? val1 : val2;
	val1 = (float)fabs(mins[1]);
	val2 = (float)fabs(maxs[1]);
	corner[1] = (val1 > val2) ? val1 : val2;
	val1 = (float)fabs(mins[2]);
	val2 = (float)fabs(maxs[2]);
	corner[2] = (val1 > val2) ? val1 : val2;

	return (float)VectorLength(corner);
}


/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels (const lump_t *l)
{
	dmodel_t	*in;
	mmodel_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "Mod_LoadSubmodels: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		// spread the mins / maxs by a pixel
		out->mins[0] = LittleFloat (in->mins[0]) - 1;
		out->mins[1] = LittleFloat (in->mins[1]) - 1;
		out->mins[2] = LittleFloat (in->mins[2]) - 1;
		out->maxs[0] = LittleFloat (in->maxs[0]) + 1;
		out->maxs[1] = LittleFloat (in->maxs[1]) + 1;
		out->maxs[2] = LittleFloat (in->maxs[2]) + 1;

		out->radius = RadiusFromBounds (out->mins, out->maxs);
		out->headnode = LittleLong (in->headnode);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
static void Mod_LoadEdges (const lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "Mod_LoadEdges: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc (count * sizeof(*out));	

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = LittleShort(in->v[0]);
		out->v[1] = LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
static void Mod_LoadTexinfo (const lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out, *step;
	int 	i, count;
	char	name[MAX_QPATH];
	int		next;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "Mod_LoadTexinfo: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->vecs[0][0] = LittleFloat (in->vecs[0][0]);
		out->vecs[0][1] = LittleFloat (in->vecs[0][1]);
		out->vecs[0][2] = LittleFloat (in->vecs[0][2]);
		out->vecs[0][3] = LittleFloat (in->vecs[0][3]);

		out->vecs[1][0] = LittleFloat (in->vecs[1][0]);
		out->vecs[1][1] = LittleFloat (in->vecs[1][1]);
		out->vecs[1][2] = LittleFloat (in->vecs[1][2]);
		out->vecs[1][3] = LittleFloat (in->vecs[1][3]);

		out->flags = LittleLong (in->flags);
		next = LittleLong (in->nexttexinfo);
		if (next > 0)
			out->next = loadmodel->texinfo + next;
		else
		    out->next = NULL;
		
		Com_sprintf (name, sizeof(name), "textures/%s.wal", in->texture);

		out->image = GL_FindImage (name, it_wall);
		if (!out->image)
		{
			Com_Printf ("Couldn't load %s\n", name);
			out->image = r_notexture;
		}
	}

	// count animation frames
	for (i=0 ; i<count ; i++)
	{
		out = &loadmodel->texinfo[i];
		out->numframes = 1;
		for (step = out->next ; step && step != out ; step=step->next)
			out->numframes++;
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
static void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = (int)floor(mins[i] / 16);
		bmaxs[i] = (int)ceil(maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
	}
}


void GL_BuildPolygonFromSurface(msurface_t *fa);
void GL_CreateSurfaceLightmap (msurface_t *surf);
void GL_EndBuildingLightmaps (void);
void GL_BeginBuildingLightmaps (void);

/*
=================
Mod_LoadFaces
=================
*/
static void Mod_LoadFaces (const lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;
	int			ti;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "Mod_LoadFaces: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	currentmodel = loadmodel;

	//GL_BeginBuildingLightmaps (loadmodel);
	GL_BeginBuildingLightmaps ();

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;
		out->polys = NULL;

		out->texturechain = NULL;
		out->lightmapchain = NULL;
		out->dlight_s = 0;
		out->dlight_t = 0;
		out->dlightframe = 0;
		out->dlightbits = 0;

		out->visframe = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		ti = LittleShort (in->texinfo);
		if (ti < 0 || ti >= loadmodel->numtexinfo)
			Com_Error (ERR_DROP, "Mod_LoadFaces: bad texinfo number");
		out->texinfo = loadmodel->texinfo + ti;

		CalcSurfaceExtents (out);
				
	// lighting info

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
		{
			out->samples = NULL;
			out->stain_samples = NULL;
		}
		else
		{
			out->samples = loadmodel->lightdata + i;
			out->stain_samples = loadmodel->staindata + i;
		}
		
		// set the drawing flags
		if (out->texinfo->flags & SURF_WARP)
			out->flags |= SURF_DRAWTURB;

		// create lightmaps and polygons
		if ( !(out->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP) ) )
			GL_CreateSurfaceLightmap (out);

		GL_BuildPolygonFromSurface(out);

	}

	GL_EndBuildingLightmaps ();
}


/*
=================
Mod_SetParent
=================
*/
static void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;

	if (node->contents != CONTENTS_NODE)
		return;

	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes (const lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "Mod_LoadNodes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->minmaxs[0] = LittleShort (in->mins[0]);
		out->minmaxs[1] = LittleShort (in->mins[1]);
		out->minmaxs[2] = LittleShort (in->mins[2]);

		out->minmaxs[3] = LittleShort (in->maxs[0]);
		out->minmaxs[4] = LittleShort (in->maxs[1]);
		out->minmaxs[5] = LittleShort (in->maxs[2]);
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		out->contents = CONTENTS_NODE;	// differentiate from leafs

		out->parent = NULL;
		out->visframe = 0;

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs (const lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "Mod_LoadLeafs: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->minmaxs[0] = LittleShort (in->mins[0]);
		out->minmaxs[1] = LittleShort (in->mins[1]);
		out->minmaxs[2] = LittleShort (in->mins[2]);

		out->minmaxs[3] = LittleShort (in->maxs[0]);
		out->minmaxs[4] = LittleShort (in->maxs[1]);
		out->minmaxs[5] = LittleShort (in->maxs[2]);

		out->contents = LittleLong(in->contents);

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstleafface);
		out->nummarksurfaces = LittleShort(in->numleaffaces);

		out->parent = NULL;
		out->visframe = 0;
		// gl underwater warp
		if (out->contents & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA/*|CONTENTS_THINWATER*/) )
		{
			for (j=0; j<out->nummarksurfaces; j++)
			{
				if ((out->firstmarksurface[j]->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP)))
					continue;

				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
			}
		}
	}	
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
static void Mod_LoadMarksurfaces (const lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "Mod_LoadMarksurfaces: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j < 0 ||  j >= loadmodel->numsurfaces)
			Com_Error (ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
static void Mod_LoadSurfedges (const lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "Mod_LoadSurfedges: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		Com_Error (ERR_DROP, "Mod_LoadSurfedges: bad surfedges count in %s: %i",
		loadmodel->name, count);

	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}


/*
=================
Mod_LoadPlanes
=================
*/
static void Mod_LoadPlanes (const lump_t *l)
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadPlanes: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	
	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
		out->pad[0] = out->pad[1] = 0;
	}
}

/*
=================
Mod_LoadBrushModel
=================
*/
static void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i;
	dheader_t	*header;
	mmodel_t 	*bm;
	
	loadmodel->type = mod_brush;
	if (loadmodel != mod_known)
		Com_Error (ERR_DROP, "Loaded a brush model after the world");

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
		Com_Error (ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

// swap all the lumps
	mod_base = (byte *)header;

#ifndef ENDIAN_LITTLE
	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);
#endif
// load into heap
	
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_LEAFFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	mod->numframes = 2;		// regular and alternate animation
	
//
// set up the submodels
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		model_t	*starmod;

		bm = &mod->submodels[i];
		starmod = &mod_inline[i];

		*starmod = *loadmodel;
		
		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->firstnode = bm->headnode;
		if (starmod->firstnode >= loadmodel->numnodes)
			Com_Error (ERR_DROP, "Inline model %i has bad firstnode", i);

		VectorCopy (bm->maxs, starmod->maxs);
		VectorCopy (bm->mins, starmod->mins);
		starmod->radius = bm->radius;
	
		if (i == 0)
			*loadmodel = *starmod;

//		starmod->numleafs = bm->visleafs;
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasMD2Model
=================
*/
//#define Mod_Malloc(size) Z_TagMalloc(size, TAGMALLOC_RENDER_IMAGE)
#define Mod_Malloc(size) Hunk_Alloc(size)

static void Mod_LoadAliasMD2Model (model_t *mod, void *buffer)
{
	int					i, j, k;
	int					version, framesize;
	float				skinwidth, skinheight;
	int					numverts, numindexes, numFrames, numSkins, numTris, numVertsr, bufsize;
	int					indremap[MD2_MAX_TRIANGLES*3];
	index_t				ptempindex[MD2_MAX_TRIANGLES*3], ptempstindex[MD2_MAX_TRIANGLES*3], ptemp2index[MD2_MAX_TRIANGLES*3];
	dmdl_t				*pinmodel;
	dstvert_t			*pinst;
	dtriangle_t			*pintri;
	daliasframe_t		*pinframe;
	index_t				*poutindex;
	maliasmodel_t		*poutmodel;
	maliasmesh_t		*poutmesh;
	vec2_t				*poutcoord;
	maliasframe_t		*poutframe;
	maliasvertex_t		*poutvertex;
	maliasskin_t		*poutskin;
	byte				*buf;

	pinmodel = ( dmdl_t * )buffer;
	version = LittleLong( pinmodel->version );
	framesize = LittleLong( pinmodel->framesize );

	if( version != ALIAS_VERSION )
		Com_Error( ERR_DROP, "%s has wrong version number (%i should be %i)\n",
				 mod->name, version, ALIAS_VERSION );

	// byte swap the header fields and sanity check
	skinwidth = LittleLong( pinmodel->skinwidth );
	skinheight = LittleLong( pinmodel->skinheight );

	if( skinwidth <= 0 )
		Com_Error( ERR_DROP, "model %s has invalid skin width\n", mod->name );
	if( skinheight <= 0 )
		Com_Error( ERR_DROP, "model %s has invalid skin height\n", mod->name );

	numFrames = LittleLong( pinmodel->num_frames );
	if( numFrames > MD2_MAX_FRAMES )
		Com_Error( ERR_DROP, "model %s has too many frames\n", mod->name );
	else if( numFrames <= 0 )
		Com_Error( ERR_DROP, "model %s has no frames\n", mod->name );

	numSkins = LittleLong( pinmodel->num_skins );
	if( numSkins > MD2_MAX_SKINS )
		Com_Error( ERR_DROP, "model %s has too many skins\n", mod->name );
	else if( numSkins < 0 )
		Com_Error( ERR_DROP, "model %s has invalid number of skins\n", mod->name );

	numVertsr = LittleLong( pinmodel->num_xyz );
	if( numVertsr <= 0 )
		Com_Error( ERR_DROP, "model %s has no vertices\n", mod->name );
	else if( numVertsr > MD2_MAX_VERTS )
		Com_Error( ERR_DROP, "model %s has too many vertices\n", mod->name );

	numTris = LittleLong( pinmodel->num_tris );
	if( numTris > MD2_MAX_TRIANGLES )
		Com_Error( ERR_DROP, "model %s has too many triangles\n", mod->name );
	else if( numTris <= 0 )
		Com_Error( ERR_DROP, "model %s has no triangles\n", mod->name );

	numindexes = numTris * 3;
//
// load triangle lists
//
	pintri = ( dtriangle_t * )( ( byte * )pinmodel + LittleLong( pinmodel->ofs_tris ) );
	pinst = ( dstvert_t * ) ( ( byte * )pinmodel + LittleLong( pinmodel->ofs_st ) );

	for( i = 0, k = 0; i < numTris; i++, k += 3 ) {
		ptempindex[k+0] = ( index_t )LittleShort( pintri[i].index_xyz[0] );
		ptempindex[k+1] = ( index_t )LittleShort( pintri[i].index_xyz[1] );
		ptempindex[k+2] = ( index_t )LittleShort( pintri[i].index_xyz[2] );

		ptempstindex[k+0] = ( index_t )LittleShort( pintri[i].index_st[0] );
		ptempstindex[k+1] = ( index_t )LittleShort( pintri[i].index_st[1] );
		ptempstindex[k+2] = ( index_t )LittleShort( pintri[i].index_st[2] );
	}

//
// build list of unique vertexes
//
	numverts = 0;
	memset( indremap, -1, MD2_MAX_TRIANGLES * 3 * sizeof(int) );

	for( i = 0; i < numindexes; i++ ) {
		if( indremap[i] != -1 )
			continue;

		// remap duplicates
		for( j = i + 1; j < numindexes; j++ ) {
			if( (ptempindex[j] == ptempindex[i])
				&& (pinst[ptempstindex[j]].s == pinst[ptempstindex[i]].s)
				&& (pinst[ptempstindex[j]].t == pinst[ptempstindex[i]].t) ) {
				indremap[j] = i;
				ptemp2index[j] = numverts;
			}
		}

		// add unique vertex
		indremap[i] = i;
		ptemp2index[i] = numverts++;
	}

	Com_DPrintf( "%s: remapped %i verts to %i (%i tris)\n", mod->name, numVertsr, numverts, numTris );

	bufsize = ( sizeof(maliasmodel_t) + sizeof(maliasmesh_t) +
		numindexes * sizeof(index_t) + //indexes
		numverts * sizeof(vec2_t) + //stcoords
		numFrames * (sizeof(maliasframe_t) + numverts * sizeof(maliasvertex_t)) + //frames
		numSkins * sizeof(maliasskin_t)); //skins

	mod->type = mod_alias;
	mod->radius = 0;
	ClearBounds( mod->mins, mod->maxs );

	bufsize = (bufsize+31)&~31;
	mod->extradata = Hunk_Begin(bufsize);
	buf = Hunk_Alloc(bufsize);
	mod->extradatasize = Hunk_End();

	poutmodel = (maliasmodel_t *)buf; buf += sizeof(maliasmodel_t);
	poutmodel->numtags = 0;
	poutmodel->tags = NULL;
	poutmodel->nummeshes = 1;
	poutmodel->numframes = numFrames;

	poutmesh = poutmodel->meshes = (maliasmesh_t *)buf; buf += sizeof(maliasmesh_t);

	Q_strncpyz( poutmesh->name, "default", MD2_MAX_SKINNAME );
	poutmesh->numskins = numSkins;
	poutmesh->numverts = numverts;
	poutmesh->numtris = numTris;

	poutindex = poutmesh->indexes = (index_t *)buf; buf += numindexes * sizeof(index_t);

	for(i=0; i<numindexes; i++)
		poutindex[i] = ptemp2index[i];
//
// load base s and t vertices
//
	skinwidth = 1.0f / skinwidth;
	skinheight = 1.0f / skinheight;
	poutcoord = poutmesh->stcoords = (vec2_t *)buf; buf += numverts * sizeof(vec2_t);
	for( i = 0; i < numindexes; i++ ) {
		if( indremap[i] == i ) {
			poutcoord[poutindex[i]][0] = ((float)LittleShort( pinst[ptempstindex[i]].s ) + 0.5f) * skinwidth;
			poutcoord[poutindex[i]][1] = ((float)LittleShort( pinst[ptempstindex[i]].t ) + 0.5f) * skinheight;
		}
	}

//
// load the frames
//
	poutframe = poutmodel->frames = (maliasframe_t *)buf;
	buf += poutmodel->numframes * sizeof(maliasframe_t);
	poutvertex = poutmesh->vertexes = (maliasvertex_t *)buf;
	buf += poutmodel->numframes * poutmesh->numverts * sizeof(maliasvertex_t);

	for( i = 0; i < poutmodel->numframes; i++, poutframe++, poutvertex += numverts ) {
		pinframe = ( daliasframe_t * )( ( byte * )pinmodel + LittleLong( pinmodel->ofs_frames ) + i * framesize );

		poutframe->scale[0] = LittleFloat( pinframe->scale[0] );
		poutframe->scale[1] = LittleFloat( pinframe->scale[1] );
		poutframe->scale[2] = LittleFloat( pinframe->scale[2] );
		poutframe->translate[0] = LittleFloat( pinframe->translate[0] );
		poutframe->translate[1] = LittleFloat( pinframe->translate[1] );
		poutframe->translate[2] = LittleFloat( pinframe->translate[2] );

		for( j = 0; j < numindexes; j++ ) {		// verts are all 8 bit, so no swapping needed
			if( indremap[j] == j ) {
				poutvertex[poutindex[j]].point[0] = (short)pinframe->verts[ptempindex[j]].v[0];
				poutvertex[poutindex[j]].point[1] = (short)pinframe->verts[ptempindex[j]].v[1];
				poutvertex[poutindex[j]].point[2] = (short)pinframe->verts[ptempindex[j]].v[2];
				ByteToDir(pinframe->verts[ptempindex[j]].lightnormalindex, poutvertex[poutindex[j]].normal);
				//poutvertex[poutindex[j]].lightnormalindex = pinframe->verts[ptempindex[j]].lightnormalindex;
			}
		}

		//Mod_AliasCalculateVertexNormals( numindexes, poutindex, numverts, poutvertex );

		VectorCopy( poutframe->translate, poutframe->mins );
		VectorMA( poutframe->translate, 255, poutframe->scale, poutframe->maxs );
		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );

		mod->radius = max( mod->radius, poutframe->radius );
		AddPointToBounds( poutframe->mins, mod->mins, mod->maxs );
		AddPointToBounds( poutframe->maxs, mod->mins, mod->maxs );
	}

	// register all skins
	poutskin = poutmesh->skins = (maliasskin_t *)buf;
	for( i = 0; i < poutmesh->numskins; i++, poutskin++ ) {
		if( LittleLong( pinmodel->ofs_skins ) == -1 )
			continue;
		mod->skins[i] = poutskin->image = GL_FindImage ((char *)pinmodel + LittleLong( pinmodel->ofs_skins ) + i*MD2_MAX_SKINNAME, it_skin );
	}
}


/*
=================
Mod_StripLODSuffix
=================
*/
void Mod_StripLODSuffix( char *name )
{
	int len, lodnum;

	len = strlen( name );
	if( len <= 2 )
		return;

	lodnum = atoi( &name[len - 1] );
	if( lodnum < MD3_ALIAS_MAX_LODS ) {
		if( name[len-2] == '_' )
			name[len-2] = 0;
	}
}

/*
=================
Mod_LoadAliasMD3Model
=================
*/

static vec_t Quat_Normalize( quat_t q )
{
	vec_t length;

	length = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
	if( length != 0.0f ) {
		vec_t ilength = 1.0f / (float)sqrt( length );
		q[0] *= ilength;
		q[1] *= ilength;
		q[2] *= ilength;
		q[3] *= ilength;
	}

	return length;
}

static void Matrix_Quat( vec3_t m[3], quat_t q )
{
	vec_t tr, s;

	tr = m[0][0] + m[1][1] + m[2][2];
	if( tr > 0.00001f ) {
		s = (float)sqrt( tr + 1.0f );
		q[3] = s * 0.5f; s = 0.5f / s;
		q[0] = (m[2][1] - m[1][2]) * s;
		q[1] = (m[0][2] - m[2][0]) * s;
		q[2] = (m[1][0] - m[0][1]) * s;
	} else {
		int i = 0, j, k;

		if (m[1][1] > m[0][0]) i = 1;
		if (m[2][2] > m[i][i]) i = 2;
		j = (i + 1) % 3;
		k = (i + 2) % 3;

		s = (float)sqrt( m[i][i] - (m[j][j] + m[k][k]) + 1.0f );

		q[i] = s * 0.5f;
		if(s != 0.0f)
			s = 0.5f / s;
		q[j] = (m[j][i] + m[i][j]) * s;
		q[k] = (m[k][i] + m[i][k]) * s;
		q[3] = (m[k][j] - m[j][k]) * s;
	}

	Quat_Normalize( q );
}

static void Mod_LoadAliasMD3Model ( model_t *mod, void *buffer )
{
	int					version, i, j, l;
	int					bufsize;
	dmd3header_t		*pinmodel;
	dmd3frame_t			*pinframe;
	dmd3tag_t			*pintag;
	dmd3mesh_t			*pinmesh;
	dmd3skin_t			*pinskin;
	dmd3coord_t			*pincoord;
	dmd3vertex_t		*pinvert;
	index_t				*pinindex, *poutindex;
	maliasvertex_t		*poutvert;
	vec2_t				*poutcoord;
	maliasskin_t		*poutskin;
	maliasmesh_t		*poutmesh;
	maliastag_t			*pouttag;
	maliasframe_t		*poutframe;
	maliasmodel_t		*poutmodel;
	float				lat, lng;
	byte				*buf;
	int					numFrames, numTags, numMeshes;
	int					numTris[MD3_MAX_MESHES], numSkins[MD3_MAX_MESHES], numVerts[MD3_MAX_MESHES];

	pinmodel = ( dmd3header_t * )buffer;
	version = LittleLong( pinmodel->version );

	if ( version != MD3_ALIAS_VERSION )
	{
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: %s has wrong version number (%i should be %i)\n",
				 mod->name, version, MD3_ALIAS_VERSION);
	}

	// byte swap the header fields and sanity check
	numFrames = LittleLong ( pinmodel->num_frames );
	if ( numFrames <= 0 )
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: model %s has no frames\n", mod->name );
	else if ( numFrames > MD3_MAX_FRAMES )
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: model %s has too many frames\n", mod->name );

	numTags = LittleLong ( pinmodel->num_tags );
	if ( numTags > MD3_MAX_TAGS )
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: model %s has too many tags\n", mod->name );
	else if ( numTags < 0 ) 
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: model %s has invalid number of tags\n", mod->name );

	numMeshes = LittleLong ( pinmodel->num_meshes );
	if ( numMeshes <= 0 )
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: model %s has no meshes\n", mod->name );
	else if ( numMeshes > MD3_MAX_MESHES )
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: model %s has too many meshes\n", mod->name );

	bufsize = sizeof(maliasmodel_t) + numFrames * (sizeof( maliasframe_t ) + sizeof( maliastag_t ) * numTags) + 
		numMeshes * sizeof( maliasmesh_t );

	pinmesh = ( dmd3mesh_t * )( ( byte * )pinmodel + LittleLong( pinmodel->ofs_meshes ) );
	for( i = 0; i < numMeshes; i++ ) {
		if( strncmp( (const char *)pinmesh->id, "IDP3", 4) )
			Com_Error( ERR_DROP, "mesh %s in model %s has wrong id (%s should be %s)\n",
					 pinmesh->name, mod->name, LittleLong( pinmesh->id ), IDMD3HEADER );

		numTris[i] = LittleLong( pinmesh->num_tris );
		numSkins[i] = LittleLong( pinmesh->num_skins );
		numVerts[i] = LittleLong( pinmesh->num_verts );

		if( numSkins[i] <= 0 )
			Com_Error( ERR_DROP, "mesh %i in model %s has no skins\n", i, mod->name );
		else if( numSkins[i] > MD3_MAX_SHADERS )
			Com_Error( ERR_DROP, "mesh %i in model %s has too many skins (%i > %i)\n", i, mod->name, numSkins[i], MD3_MAX_SHADERS);
		if( numTris[i] <= 0 )
			Com_Error( ERR_DROP, "mesh %i in model %s has no elements\n", i, mod->name );
		else if( numTris[i] > MD3_MAX_TRIANGLES )
			Com_Error( ERR_DROP, "mesh %i in model %s has too many triangles (%i > %i)\n", i, mod->name, numTris[i], MD3_MAX_TRIANGLES);
		if( numVerts[i] <= 0 )
			Com_Error( ERR_DROP, "mesh %i in model %s has no vertices\n", i, mod->name );
		else if( numVerts[i] > MD3_MAX_VERTS )
			Com_Error( ERR_DROP, "mesh %i in model %s has too many vertices (%i > %i)\n", i, mod->name, numVerts[i], MD3_MAX_VERTS);

		bufsize += sizeof(maliasskin_t) * numSkins[i] + numTris[i] * sizeof(index_t) * 3 +
			numVerts[i] * (sizeof(vec2_t) + sizeof(maliasvertex_t) * numFrames);

		pinmesh = ( dmd3mesh_t * )( ( byte * )pinmesh + LittleLong( pinmesh->meshsize ) );
	}

	mod->type = mod_alias;

	bufsize = (bufsize+31)&~31;
	mod->extradata = Hunk_Begin(bufsize);
	buf = Hunk_Alloc(bufsize);
	mod->extradatasize = Hunk_End();

	poutmodel = (maliasmodel_t *)buf; buf += sizeof(maliasmodel_t);

	poutmodel->numframes = numFrames;
	poutmodel->numtags = numTags;
	poutmodel->nummeshes = numMeshes;

//
// load the frames
//
	pinframe = ( dmd3frame_t * )( ( byte * )pinmodel + LittleLong( pinmodel->ofs_frames ) );
	poutframe = poutmodel->frames = ( maliasframe_t * )buf; buf += sizeof( maliasframe_t ) * poutmodel->numframes;
	for( i = 0; i < poutmodel->numframes; i++, pinframe++, poutframe++ ) {
		poutframe->scale[0] = poutframe->scale[1] = poutframe->scale[2] = MD3_XYZ_SCALE;
		poutframe->translate[0] = LittleFloat( pinframe->translate[0] );
		poutframe->translate[1] = LittleFloat( pinframe->translate[1] );
		poutframe->translate[2] = LittleFloat( pinframe->translate[2] );
		// never trust the modeler utility and recalculate bbox and radius
		ClearBounds( poutframe->mins, poutframe->maxs );
	}
	
//
// load the tags
//
	pintag = ( dmd3tag_t * )( ( byte * )pinmodel + LittleLong( pinmodel->ofs_tags ) );
	pouttag = poutmodel->tags = ( maliastag_t * )buf; buf += sizeof( maliastag_t ) * poutmodel->numframes * poutmodel->numtags;
	for( i = 0; i < poutmodel->numframes; i++ ) {
		for( l = 0; l < poutmodel->numtags; l++, pintag++, pouttag++ ) {
			for ( j = 0; j < 3; j++ ) {
				vec3_t axis[3];

				axis[0][j] = LittleFloat( pintag->axis[0][j] );
				axis[1][j] = LittleFloat( pintag->axis[1][j] );
				axis[2][j] = LittleFloat( pintag->axis[2][j] );
				Matrix_Quat( axis, pouttag->quat );
				//Quat_Normalize( pouttag->quat );
				pouttag->origin[j] = LittleFloat( pintag->origin[j] );
			}

			Q_strncpyz( pouttag->name, pintag->name, MD3_MAX_PATH );
		}
	}

//
// load the meshes
//
	pinmesh = ( dmd3mesh_t * )( ( byte * )pinmodel + LittleLong( pinmodel->ofs_meshes ) );
	poutmesh = poutmodel->meshes = ( maliasmesh_t * )buf; buf += poutmodel->nummeshes * sizeof( maliasmesh_t );
	for( i = 0; i < poutmodel->nummeshes; i++, poutmesh++ )
	{
		Q_strncpyz( poutmesh->name, pinmesh->name, MD3_MAX_PATH );

		Mod_StripLODSuffix( poutmesh->name );

		poutmesh->numtris = numTris[i];
		poutmesh->numskins = numSkins[i];
		poutmesh->numverts = numVerts[i];

	//
	// load the skins
	//
		pinskin = ( dmd3skin_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_skins ) );
		poutskin = poutmesh->skins = ( maliasskin_t * )buf; buf += sizeof(maliasskin_t) * poutmesh->numskins;
		for( j = 0; j < poutmesh->numskins; j++, pinskin++, poutskin++ )
			poutskin->image = GL_FindImage(pinskin->name, it_skin );

	//
	// load the indexes
	//
		pinindex = ( index_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_indexes ) );
		poutindex = poutmesh->indexes = ( index_t * )buf; buf += poutmesh->numtris * sizeof(index_t) * 3;
		for( j = 0; j < poutmesh->numtris; j++, pinindex += 3, poutindex += 3 ) {
			poutindex[0] = (index_t)LittleLong( pinindex[0] );
			poutindex[1] = (index_t)LittleLong( pinindex[1] );
			poutindex[2] = (index_t)LittleLong( pinindex[2] );
		}

	//
	// load the texture coordinates
	//
		pincoord = ( dmd3coord_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_tcs ) );
		poutcoord = poutmesh->stcoords = ( vec2_t * )buf; buf += poutmesh->numverts * sizeof(vec2_t);
		for( j = 0; j < poutmesh->numverts; j++, pincoord++ ) {
			poutcoord[j][0] = LittleFloat( pincoord->st[0] );
			poutcoord[j][1] = LittleFloat( pincoord->st[1] );
		}

	//
	// load the vertexes and normals
	//
		pinvert = ( dmd3vertex_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_verts ) );
		poutvert = poutmesh->vertexes = ( maliasvertex_t * )buf; buf += poutmesh->numverts * sizeof(maliasvertex_t) * poutmodel->numframes;
		for( l = 0, poutframe = poutmodel->frames; l < poutmodel->numframes; l++, poutframe++, pinvert += poutmesh->numverts, poutvert += poutmesh->numverts ) {
			vec3_t v;

			for( j = 0; j < poutmesh->numverts; j++ ) {
				poutvert[j].point[0] = LittleShort( pinvert[j].point[0] );
				poutvert[j].point[1] = LittleShort( pinvert[j].point[1] );
				poutvert[j].point[2] = LittleShort( pinvert[j].point[2] );

				/*poutvert[j].latlong[0] = pinvert[j].norm[0];
				poutvert[j].latlong[1] = pinvert[j].norm[1];*/
				lng = (float)pinvert[j].norm[0] * M_PI/128.0f;
				lat = (float)pinvert[j].norm[1] * M_PI/128.0f;

				poutvert->normal[0] = (float)cos(lat) * (float)sin(lng);
				poutvert->normal[1] = (float)sin(lat) * (float)sin(lng);
				poutvert->normal[2] = (float)cos(lng);

				VectorCopy( poutvert[j].point, v );
				AddPointToBounds( v, poutframe->mins, poutframe->maxs );
			}
		}

		pinmesh = ( dmd3mesh_t * )( ( byte * )pinmesh + LittleLong( pinmesh->meshsize ) );
	}

	for(i = 0; i < poutmodel->meshes[0].numskins && i < MD2_MAX_SKINS; i++)
		mod->skins[i] = poutmodel->meshes[0].skins[i].image;
//
// calculate model bounds
//
	poutframe = poutmodel->frames;
	for( i = 0; i < poutmodel->numframes; i++, poutframe++ ) {
		VectorMA( poutframe->translate, MD3_XYZ_SCALE, poutframe->mins, poutframe->mins );
		VectorMA( poutframe->translate, MD3_XYZ_SCALE, poutframe->maxs, poutframe->maxs );
		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );

		AddPointToBounds( poutframe->mins, mod->mins, mod->maxs );
		AddPointToBounds( poutframe->maxs, mod->mins, mod->maxs );
		mod->radius = max( mod->radius, poutframe->radius );
	}
}

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

/*
=================
Mod_LoadSpriteModel
=================
*/
static void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	dsprite_t		*sprin;
	dsprframe_t		*sprinframe;
	mspritemodel_t	*sprout;
	mspriteframe_t	*sproutframe;
	int				i, numFrames, bufsize;

	sprin = (dsprite_t *)buffer;

	if (LittleLong(sprin->version) != SPRITE_VERSION)
		Com_Error (ERR_DROP, "sprite %s has wrong version number (%i should be %i)",
				 mod->name, LittleLong(sprin->version), SPRITE_VERSION);

	numFrames = LittleLong (sprin->numframes);
	if (numFrames > SPRITE_MAX_FRAMES)
		Com_Error (ERR_DROP, "sprite %s has too many frames (%i > %i)", mod->name, numFrames, SPRITE_MAX_FRAMES);
	else if (numFrames <= 0)
		Com_Error (ERR_DROP, "sprite %s has no frames", mod->name);

	bufsize = sizeof(mspritemodel_t) + sizeof(mspriteframe_t) * numFrames;
	bufsize = (bufsize+31)&~31;
	mod->extradata = Hunk_Begin(bufsize);
	sprout = (mspritemodel_t *)Hunk_Alloc(bufsize);
	mod->extradatasize = Hunk_End();

	sprout->numframes = numFrames;

	sprinframe = sprin->frames;
	sprout->frames = sproutframe = (mspriteframe_t *)((byte *)sprout + sizeof(mspritemodel_t));

	// byte swap everything
	for (i=0 ; i<sprout->numframes ; i++, sprinframe++, sproutframe++ )
	{
		sproutframe->width = LittleLong( sprinframe->width );
		sproutframe->height = LittleLong( sprinframe->height );
		sproutframe->origin_x = LittleLong( sprinframe->origin_x );
		sproutframe->origin_y = LittleLong( sprinframe->origin_y );
		Q_strncpyz(sproutframe->name, sprinframe->name, sizeof(sproutframe->name));

		mod->skins[i] = GL_FindImage(sproutframe->name, it_sprite);
	}

	mod->type = mod_sprite;
}

//=============================================================================

static void Mod_RemoveHash (model_t *mod)
{
	model_t	*entry, **back;
	unsigned int  hash;

	hash = Com_HashKey(mod->name, MODELS_HASH_SIZE);
	for(back=&mod_hash[hash], entry=mod_hash[hash]; entry; back=&entry->hashNext, entry=entry->hashNext ) {
		if( entry == mod ) {
			*back = entry->hashNext;
			break;
		}
	}
}
/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginRegistration

Specifies the model that will be used as the world
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginRegistration (const char *map)
{
	char	fullname[MAX_QPATH];
	cvar_t	*flushmap;

	registration_sequence++;

	Com_sprintf (fullname, sizeof(fullname), "maps/%s.bsp", map);

	GL_ClearDecals (); //Decals

	// explicitly free the old map if different
	// this guarantees that mod_known[0] is the world map
	flushmap = Cvar_Get ("flushmap", "0", 0);
	if ( mod_known[0].name[0] && (strcmp(mod_known[0].name, fullname) || flushmap->integer)) {
		Mod_RemoveHash(&mod_known[0]);
		Mod_Free (&mod_known[0]);
	}
	r_worldmodel = Mod_ForName(fullname, true);

	r_framecount = 1;
	r_oldviewcluster = r_viewcluster = -1;		// force markleafs
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RegisterModel

@@@@@@@@@@@@@@@@@@@@@
*/
struct model_s *R_RegisterModel (const char *name)
{
	model_t			*mod;
	int				i, k;
	mspritemodel_t	*sprout;
	maliasmodel_t	*pheader;

	i = strlen(name);
	if (gl_replacemd2->integer && i > 4 && !strcmp(name + i - 4, ".md2"))
	{
		char	s[MAX_QPATH];

		Q_strncpyz(s, name, sizeof(s));
		s[strlen(s) - 1] = '3';
		mod = Mod_ForName(s, false);
		if (!mod)
			mod = Mod_ForName (name, false);
	}
	else
		mod = Mod_ForName (name, false);

	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		// register any images used by the models
		switch (mod->type) {
		case mod_sprite:
			sprout = (mspritemodel_t *)mod->extradata;
			for (i=0 ; i<sprout->numframes ; i++) {
				if (mod->skins[i])
					mod->skins[i]->registration_sequence = registration_sequence;
			}
			break;
		case mod_alias:
			pheader = (maliasmodel_t *)mod->extradata;
			for (i = 0; i < pheader->nummeshes; i++)
			{
				for (k = 0; k < pheader->meshes[i].numskins; k++)
				{
					if (pheader->meshes[i].skins[k].image)
						pheader->meshes[i].skins[k].image->registration_sequence = registration_sequence;
				}
			}

			mod->numframes = pheader->numframes;
			break;
		case mod_brush:
			for (i=0 ; i<mod->numtexinfo ; i++)
				mod->texinfo[i].image->registration_sequence = registration_sequence;

			break;
		default:
			break;
		}
	}
	return mod;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_EndRegistration

@@@@@@@@@@@@@@@@@@@@@
*/
void R_EndRegistration (void)
{
	int		i;
	model_t	*mod;

	for (i=0, mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (mod->registration_sequence != registration_sequence)
		{	// don't need this model
			Mod_RemoveHash(mod);
			Mod_Free (mod);
		}
	}

	GL_FreeUnusedImages ();
}


//=============================================================================


/*
================
Mod_Free
================
*/
void Mod_Free (model_t *mod)
{
	Hunk_Free (mod->extradata);
	memset (mod, 0, sizeof(*mod));
}

/*
================
Mod_FreeAll
================
*/
void R_ShutdownModels (void)
{
	int		i;

	for (i=0 ; i<mod_numknown ; i++)
	{
		if (mod_known[i].extradata)
			Hunk_Free(mod_known[i].extradata);
	}
	r_worldmodel = NULL;
	mod_numknown = 0;
	memset(mod_known, 0, sizeof(mod_known));
	memset(mod_hash, 0, sizeof(mod_hash));
	GL_ClearDecals (); //Decals
}
