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

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	vec3_t		position;
} mvertex_t;

typedef struct
{
	vec3_t		mins, maxs;
	float		radius;
	int			headnode;
	int			firstface, numfaces;
} mmodel_t;


#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWTURB		0x10
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct
{
	unsigned short	v[2];
} medge_t;

typedef struct mtexinfo_s
{
	float		vecs[2][4];
	int			flags;
	int			numframes;
	struct mtexinfo_s	*next;		// animation chain
	image_t		*image;
} mtexinfo_t;

#define	VERTEXSIZE	7

typedef struct glpoly_s
{
	int		numverts;
	float	verts[4][VERTEXSIZE];	// variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct msurface_s
{
	int			visframe;		// should be drawn when node is crossed

	cplane_t	*plane;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges
	
	int			texturemins[2];
	int			extents[2];

	int			light_s, light_t;	// gl lightmap coordinates
	int			dlight_s, dlight_t; // gl lightmap coordinates for dynamic lightmaps

	glpoly_t	*polys;				// multiple if warped
	struct	msurface_s	*texturechain;
	struct  msurface_s	*lightmapchain;

	mtexinfo_t	*texinfo;
	
// lighting info
	int			dlightframe;
	int			dlightbits;

	int			lightmaptexturenum;
	byte		styles[MAXLIGHTMAPS];
	float		cached_light[MAXLIGHTMAPS];	// values currently used in lightmap
	byte		*samples;		// [numstyles*surfsize]
	byte		*stain_samples;		// stainmapping

	// Vic's awesome decals
	int				fragmentframe;
} msurface_t;

#define CONTENTS_NODE -1

typedef struct mnode_s
{
// common with leaf
	int			contents;		// CONTENTS_NODE, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current
	
	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// node specific
	cplane_t	*plane;
	struct mnode_s	*children[2];	

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
} mnode_t;



typedef struct mleaf_s
{
// common with node
	int			contents;		// will be something other than CONTENTS_NODE
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// leaf specific
	int			cluster;
	int			area;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
} mleaf_t;


//===================================================================

//
// Whole model
//

typedef enum {mod_bad, mod_brush, mod_sprite, mod_alias} modtype_t;

typedef struct model_s
{
	char		name[MAX_QPATH];

	int			registration_sequence;

	modtype_t	type;
	int			numframes;
	
	int			flags;

//
// volume occupied by the model graphics
//		
	vec3_t		mins, maxs;
	float		radius;

//
// brush model
//
	int			firstmodelsurface, nummodelsurfaces;

	int			numsubmodels;
	mmodel_t	*submodels;

	int			numplanes;
	cplane_t	*planes;

	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numvertexes;
	mvertex_t	*vertexes;

	int			numedges;
	medge_t		*edges;

	int			numnodes;
	int			firstnode;
	mnode_t		*nodes;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			numsurfedges;
	int			*surfedges;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	dvis_t		*vis;

	byte		*lightdata;
	byte		*staindata;

	// for alias models and skins
	image_t		*skins[MD2_MAX_SKINS];

	struct model_s *hashNext;

	int			extradatasize;
	void		*extradata;
} model_t;

//============================================================================

void	Mod_Init (void);

mleaf_t *Mod_PointInLeaf (const float *p, const model_t *model);
byte	*Mod_ClusterPVS (int cluster, const model_t *model);

void	Mod_Modellist_f (void);

void	*Hunk_Begin (int maxsize);
void	*Hunk_Alloc (int size);
int		Hunk_End (void);
void	Hunk_Free (void *base);

void	R_ShutdownModels (void);
void	Mod_Free (model_t *mod);



//Sprite model
typedef struct
{
	int			width, height;
	int			origin_x, origin_y;			// raster coordinates inside pic

	char		name[SPRITE_MAX_NAME];
/*	shader_t	*shader;

	float		mins[3], maxs[3];
	float		radius;*/
} mspriteframe_t;

typedef struct 
{
	int				numframes;
	mspriteframe_t	*frames;
} mspritemodel_t;

//Alias models
#define ALIAS_MAX_VERTS		4096
#define ALIAS_MAX_LODS		4

typedef unsigned int index_t;

typedef struct maliasvertex_s {
	short			point[3];
	//byte			latlong[2];		// use bytes to keep 8-byte alignment
	vec3_t			normal;
	//byte			lightnormalindex;
} maliasvertex_t;

typedef struct maliasframe_s {
	vec3_t			mins;
	vec3_t			maxs;

	vec3_t			scale;
	vec3_t			translate;
	float			radius;
} maliasframe_t;

typedef struct maliastag_s {
	char			name[MAX_QPATH];
	quat_t			quat;
	vec3_t			origin;
} maliastag_t;

typedef struct maliasskin_s {
	char			name[MAX_QPATH];
	image_t			*image;
//	shader_t		*shader;
} maliasskin_t;

typedef struct maliasmesh_s {
	char			name[MAX_QPATH];

	int				numverts;
	maliasvertex_t	*vertexes;
	vec2_t			*stcoords;

	int				numtris;
	index_t			*indexes;

	int				numskins;
	maliasskin_t	*skins;
} maliasmesh_t;

typedef struct maliasmodel_s {
	int				numframes;
	maliasframe_t	*frames;

	int				numtags;
	maliastag_t		*tags;

	int				nummeshes;
	maliasmesh_t	*meshes;
} maliasmodel_t;


