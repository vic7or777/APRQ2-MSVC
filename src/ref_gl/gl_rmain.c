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
// r_main.c
#include "gl_local.h"

viddef_t	vid;

unsigned int QGL_TEXTURE0, QGL_TEXTURE1;

model_t		*r_worldmodel;

double		gldepthmin, gldepthmax;

glconfig_t gl_config;
glstate_t  gl_state;

image_t		*r_notexture;		// use for bad textures
image_t		*r_particletexture;	// little dot for particles
image_t		*r_caustictexture;	//Water caustic texture
image_t		*r_bholetexture;
image_t		*r_shelltexture;

entity_t	*currententity;
model_t		*currentmodel;

cplane_t	frustum[4];

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_alias_polys;

static float		v_blend[4];			// final blending color

void GL_Strings_f( void );

//
// view origin
//
static float	r_ModelViewMatrix[16];
static float	r_ProjectionMatrix[16];
float			r_WorldViewMatrix[16];

vec3_t	viewAxis[3];
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

//float	r_world_matrix[16];
//float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_newrefdef;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_lefthand;

cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

//cvar_t	*gl_nosubimage;
cvar_t	*gl_allow_software;

cvar_t	*gl_vertex_arrays;

cvar_t	*gl_particle_min_size;
cvar_t	*gl_particle_max_size;
cvar_t	*gl_particle_size;
cvar_t	*gl_particle_att_a;
cvar_t	*gl_particle_att_b;
cvar_t	*gl_particle_att_c;

cvar_t	*gl_ext_swapinterval;
cvar_t	*gl_ext_palettedtexture;
cvar_t	*gl_ext_multitexture;
cvar_t	*gl_ext_pointparameters;
cvar_t	*gl_ext_compiled_vertex_array;

cvar_t	*gl_bitdepth;
cvar_t	*gl_drawbuffer;
cvar_t  *gl_driver;
cvar_t	*gl_lightmap;
cvar_t	*gl_shadows;
cvar_t	*gl_mode;
cvar_t	*gl_dynamic;
cvar_t  *gl_monolightmap;
cvar_t	*gl_modulate;
cvar_t	*gl_nobind;
cvar_t	*gl_round_down;
cvar_t	*gl_picmip;
cvar_t	*gl_skymip;
cvar_t	*gl_showtris;
cvar_t	*gl_ztrick;
cvar_t	*gl_finish;
cvar_t	*gl_clear;
cvar_t	*gl_cull;
cvar_t	*gl_polyblend;
cvar_t	*gl_flashblend;
//cvar_t	*gl_playermip;
cvar_t  *gl_saturatelighting;
cvar_t	*gl_swapinterval;
cvar_t	*gl_texturemode;
cvar_t	*gl_texturealphamode;
cvar_t	*gl_texturesolidmode;
cvar_t	*gl_lockpvs;

cvar_t	*gl_3dlabs_broken;

cvar_t	*gl_eff_world_wireframe;
cvar_t	*gl_eff_world_bg_type;
cvar_t	*gl_eff_world_bg_color_r;
cvar_t	*gl_eff_world_bg_color_g;
cvar_t	*gl_eff_world_bg_color_b;
cvar_t	*gl_eff_world_lines_color_r;
cvar_t	*gl_eff_world_lines_color_g;
cvar_t	*gl_eff_world_lines_color_b;

extern cvar_t	*vid_fullscreen;
extern cvar_t	*vid_gamma;
void VID_Restart_f (void);

//Added cvar's -Maniac
cvar_t *skydistance; // DMP - skybox size change
cvar_t *gl_screenshot_quality;

cvar_t 	*gl_replacewal;
cvar_t  *gl_replacepcx;
cvar_t	*gl_stainmaps;				// stainmaps
cvar_t	*gl_motionblur;				// motionblur
cvar_t	*gl_waterwaves;
cvar_t	*gl_fontshadow;
cvar_t	*gl_particle;
cvar_t	*gl_sgis_mipmap;
cvar_t	*gl_ext_texture_compression;
cvar_t	*gl_celshading;				//celshading
cvar_t	*gl_celshading_width;
cvar_t	*gl_scale;
cvar_t	*gl_watercaustics;
cvar_t	*gl_fog;
cvar_t	*gl_fog_density;
cvar_t	*gl_decals;
cvar_t	*gl_decals_time;
cvar_t	*gl_coloredlightmaps;
cvar_t	*r_customwidth;
cvar_t	*r_customheight;
cvar_t	*gl_gammapics;
cvar_t	*gl_shelleffect;

cvar_t	*gl_ext_texture_filter_anisotropic;
cvar_t	*gl_ext_max_anisotropy;

static void R_ModeList_f( void );
static qboolean isWideScreen = false;

// vertex arrays
static float	tex_array[MAX_ARRAY][2];
static float	vert_array[MAX_ARRAY][3];
static float	col_array[MAX_ARRAY][4];
//End

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (const vec3_t mins, const vec3_t maxs)
{
	int		i;
	cplane_t *p;

	if (r_nocull->integer)
		return false;

	for (i=0,p=frustum ; i<4; i++,p++)
	{
		switch (p->signbits)
		{
		case 0:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		default:
			return false;
		}
	}

	return false;
}


void R_RotateForEntity (const entity_t *e) {
    qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    qglRotatef (e->angles[1],  0, 0, 1);
    qglRotatef (-e->angles[0],  0, 1, 0);
    qglRotatef (-e->angles[2],  1, 0, 0);
}

static void myGlMultMatrix( const float *a, const float *b, float *out ) {
	int		i, j;

	for ( i = 0 ; i < 4 ; i++ ) {
		for ( j = 0 ; j < 4 ; j++ ) {
			out[ i * 4 + j ] =
				a [ i * 4 + 0 ] * b [ 0 * 4 + j ]
				+ a [ i * 4 + 1 ] * b [ 1 * 4 + j ]
				+ a [ i * 4 + 2 ] * b [ 2 * 4 + j ]
				+ a [ i * 4 + 3 ] * b [ 3 * 4 + j ];
		}
	}
}

void R_RotateForEntity2 (const entity_t *ent) {
	float	glMatrix[16];

	glMatrix[0] = ent->axis[0][0];
	glMatrix[4] = ent->axis[1][0];
	glMatrix[8] = ent->axis[2][0];
	glMatrix[12] = ent->origin[0];

	glMatrix[1] = ent->axis[0][1];
	glMatrix[5] = ent->axis[1][1];
	glMatrix[9] = ent->axis[2][1];
	glMatrix[13] = ent->origin[1];

	glMatrix[2] = ent->axis[0][2];
	glMatrix[6] = ent->axis[1][2];
	glMatrix[10] = ent->axis[2][2];
	glMatrix[14] = ent->origin[2];

	glMatrix[3] = 0;
	glMatrix[7] = 0;
	glMatrix[11] = 0;
	glMatrix[15] = 1;

	myGlMultMatrix (glMatrix, r_WorldViewMatrix, r_ModelViewMatrix);

	qglLoadMatrixf (r_ModelViewMatrix);
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/


/*
=================
R_DrawSpriteModel

=================
*/
static void R_DrawSpriteModel (void)
{
	float alpha = 1.0F;
	vec3_t	point;
	dsprframe_t	*frame;
	float		*up, *right;
	dsprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = (dsprite_t *)currentmodel->extradata;

	currententity->frame %= psprite->numframes;

	frame = &psprite->frames[currententity->frame];


	// normal sprite
	up = vup;
	right = vright;

	if (currententity->flags & RF_TRANSLUCENT && currententity->alpha < 1.0F)
	{
		alpha = currententity->alpha;
		qglEnable(GL_BLEND);
		qglColor4f( 1, 1, 1, alpha );
	}
	else
		qglEnable(GL_ALPHA_TEST);

    GL_Bind(currentmodel->skins[currententity->frame]->texnum);

	GL_TexEnv( GL_MODULATE );

	qglBegin (GL_QUADS);

	qglTexCoord2i (0, 1);
	VectorMA (currententity->origin, -frame->origin_y, up, point);
	VectorMA (point, -frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2i (0, 0);
	VectorMA (currententity->origin, frame->height - frame->origin_y, up, point);
	VectorMA (point, -frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2i (1, 0);
	VectorMA (currententity->origin, frame->height - frame->origin_y, up, point);
	VectorMA (point, frame->width - frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2i (1, 1);
	VectorMA (currententity->origin, -frame->origin_y, up, point);
	VectorMA (point, frame->width - frame->origin_x, right, point);
	qglVertex3fv (point);
	
	qglEnd ();

	GL_TexEnv( GL_REPLACE );

	if ( alpha < 1.0F )
	{
		qglDisable(GL_BLEND);
		qglColor4fv(colorWhite);
	}
	else
		qglDisable(GL_ALPHA_TEST);

}

//==================================================================================

/*
=============
R_DrawNullModel
=============
*/
static void R_DrawNullModel (void)
{
	vec3_t	shadelight;
	int		i;

	if ( currententity->flags & RF_FULLBRIGHT )
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
	else
		R_LightPoint (currententity->origin, shadelight);

	qglDisable (GL_TEXTURE_2D);
	qglColor3fv (shadelight);

	qglBegin (GL_TRIANGLE_FAN);

	qglVertex3f (currententity->origin[0], currententity->origin[1], currententity->origin[2]-16);
	for (i=0 ; i<=4 ; i++)
		qglVertex3f ( currententity->origin[0] + 16*(float)cos(i*M_PI_DIV_2),
					currententity->origin[1] + 16*(float)sin(i*M_PI_DIV_2), 
					currententity->origin[2]);
	qglEnd ();

	qglBegin (GL_TRIANGLE_FAN);
	qglVertex3f (currententity->origin[0], currententity->origin[1], currententity->origin[2]+16);
	for (i=4 ; i>=0 ; i--)
		qglVertex3f ( currententity->origin[0] + 16*(float)cos(i*M_PI_DIV_2),
					currententity->origin[1] + 16*(float)sin(i*M_PI_DIV_2), 
					currententity->origin[2]);

	qglEnd ();

	qglColor3fv(color_table[COLOR_WHITE]);
	qglEnable (GL_TEXTURE_2D);
}

/*
=============
R_DrawEntitiesOnList
=============
*/
static void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities->integer)
		return;

	// draw non-transparent first
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];
		if (currententity->flags & RF_TRANSLUCENT)
			continue;	// solid

		if ( currententity->flags & RF_BEAM )
		{
			R_DrawBeam ();
			continue;
		}

		currentmodel = currententity->model;
		if (!currentmodel)
		{
			R_DrawNullModel ();
			continue;
		}
		switch (currentmodel->type)
		{
		case mod_alias:
			R_DrawAliasModel ();
			break;
		case mod_brush:
			R_DrawBrushModel ();
			break;
		case mod_sprite:
			R_DrawSpriteModel ();
			break;
		default:
			Com_Error (ERR_DROP, "Bad modeltype");
			break;
		}
	}

	// draw transparent entities, we could sort these if it ever becomes a problem...
	qglDepthMask (GL_FALSE);		// no z writes
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];
		if (!(currententity->flags & RF_TRANSLUCENT))
			continue;	// solid

		if ( currententity->flags & RF_BEAM )
		{
			R_DrawBeam ();
			continue;
		}

		currentmodel = currententity->model;
		if (!currentmodel)
		{
			R_DrawNullModel ();
			continue;
		}
		switch (currentmodel->type)
		{
		case mod_alias:
			R_DrawAliasModel ();
			break;
		case mod_brush:
			R_DrawBrushModel ();
			break;
		case mod_sprite:
			R_DrawSpriteModel ();
			break;
		default:
			Com_Error (ERR_DROP, "Bad modeltype");
			break;
		}
	}
	qglDepthMask (GL_TRUE);		// back to writing

}

/*
=====================
GL_DrawParticles
=====================
*/
static void GL_DrawParticles( int num_particles, const particle_t particles[], const unsigned colortable[768] )
{
	const particle_t *p;
	int				i;
	vec3_t			r_pup, r_pright, corner, tmp1, tmp2;
	float			scale;
	byte			color[4];

    GL_Bind(r_particletexture->texnum);
	qglDepthMask( GL_FALSE );		// no z buffering
	qglEnable( GL_BLEND );
	GL_TexEnv( GL_MODULATE );
	qglBegin( GL_QUADS );

	VectorScale (vup, 1.5f, r_pup);
	VectorScale (vright, 1.5f, r_pright);

	for ( p = particles, i=0 ; i < num_particles ; i++,p++)
	{
		// hack a scale up to keep particles from disapearing
		scale = ( p->origin[0] - r_origin[0] ) * vpn[0] + 
			    ( p->origin[1] - r_origin[1] ) * vpn[1] +
			    ( p->origin[2] - r_origin[2] ) * vpn[2];

		scale = (scale < 20) ? 1 : 1 + scale * 0.004f;

		*(int *)color = colortable[p->color];
		color[3] = p->alpha * 255;

		qglColor4ubv( color );

		VectorScale(r_pup, scale, tmp1);
		VectorScale(r_pright, scale, tmp2);
		corner[0] = p->origin[0] + (tmp1[0]+tmp2[0])*(-0.5f);
		corner[1] = p->origin[1] + (tmp1[1]+tmp2[1])*(-0.5f);
		corner[2] = p->origin[2] + (tmp1[2]+tmp2[2])*(-0.5f);

		qglTexCoord2i( 1, 1 );
		qglVertex3fv( corner );

		qglTexCoord2i( 0, 1 );
		qglVertex3f( corner[0] + tmp1[0], 
					 corner[1] + tmp1[1], 
					 corner[2] + tmp1[2]);

		qglTexCoord2i( 0, 0 );
		qglVertex3f( corner[0] + tmp1[0] + tmp2[0], 
					 corner[1] + tmp1[1] + tmp2[1],
					 corner[2] + tmp1[2] + tmp2[2]);

		qglTexCoord2i( 1, 0 );
		qglVertex3f( corner[0] + tmp2[0], 
					 corner[1] + tmp2[1], 
					 corner[2] + tmp2[2]);

	}

	qglEnd ();
	qglDisable(GL_BLEND);
	qglColor4fv(colorWhite);
	qglDepthMask( GL_TRUE );		// back to normal Z buffering
	GL_TexEnv( GL_REPLACE );
}

/*
===============
R_DrawParticles
===============
*/
static void R_DrawParticles (void)
{
	if (gl_particle->integer)
	{
		const particle_t *p;
		int				i,k;
		vec3_t			up, right;
		float			scale, r,g,b;

		qglDepthMask( GL_FALSE );		// no z buffering
		qglEnable(GL_BLEND);
		GL_TexEnv( GL_MODULATE );
		qglEnable( GL_TEXTURE_2D );
		qglBlendFunc   (GL_SRC_ALPHA, GL_ONE);	
	
		// Vertex arrays 
		qglEnableClientState (GL_VERTEX_ARRAY);
		qglEnableClientState (GL_TEXTURE_COORD_ARRAY);
		qglEnableClientState (GL_COLOR_ARRAY);

		qglTexCoordPointer (2, GL_FLOAT, sizeof(tex_array[0]), tex_array[0]);
		qglVertexPointer (3, GL_FLOAT, sizeof(vert_array[0]), vert_array[0]);
		qglColorPointer (4, GL_FLOAT, sizeof(col_array[0]), col_array[0]);

		GL_Bind(r_particletexture->texnum);


		VectorScale (vup, 1.5, up);
		VectorScale (vright, 1.5, right);

		for ( p = r_newrefdef.particles, i=0, k=0 ; i < r_newrefdef.num_particles; i++,p++,k+=4)
		{
			// hack a scale up to keep particles from disapearing
			scale = ( p->origin[0] - r_origin[0] ) * vpn[0] + 
				    ( p->origin[1] - r_origin[1] ) * vpn[1] +
				    ( p->origin[2] - r_origin[2] ) * vpn[2];

			scale = (scale < 20) ? 1 : 1 + scale * 0.0004f;

			r = d_8to24tablef[p->color & 0xFF][0];
			g = d_8to24tablef[p->color & 0xFF][1];
			b = d_8to24tablef[p->color & 0xFF][2];

			Vector2Set(tex_array[k], 0, 0);
			Vector4Set(col_array[k], r, g, b, p->alpha);
			VectorSet(vert_array[k], p->origin[0] - (up[0]*scale) - (right[0]*scale), p->origin[1] - (up[1]*scale) - (right[1]*scale), p->origin[2] - (up[2]*scale) - (right[2]*scale));

			Vector2Set(tex_array[k+1], 1, 0);
			Vector4Set(col_array[k+1], r, g, b, p->alpha);
			VectorSet(vert_array[k+1],p->origin[0] + (up[0]*scale) - (right[0]*scale), p->origin[1] + (up[1]*scale) - (right[1]*scale), p->origin[2] + (up[2]*scale) - (right[2]*scale));

			Vector2Set(tex_array[k+2], 1, 1);
			Vector4Set(col_array[k+2], r, g, b, p->alpha);
			VectorSet(vert_array[k+2], p->origin[0] + (right[0]*scale) + (up[0]*scale), p->origin[1] + (right[1]*scale) + (up[1]*scale), p->origin[2] + (right[2]*scale) + (up[2]*scale));

			Vector2Set(tex_array[k+3], 0, 1);
			Vector4Set(col_array[k+3], r, g, b, p->alpha);
			VectorSet(vert_array[k+3], p->origin[0] + (right[0]*scale) - (up[0]*scale), p->origin[1] + (right[1]*scale) - (up[1]*scale), p->origin[2] + (right[2]*scale) - (up[2]*scale));
		}

		qglDrawArrays(GL_QUADS,0,k);

		qglDisable(GL_BLEND);
		qglColor4fv(colorWhite);
		qglDepthMask(GL_TRUE);		// back to normal Z buffering
		GL_TexEnv(GL_REPLACE);
		qglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		qglDisableClientState (GL_VERTEX_ARRAY);
		qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
		qglDisableClientState(GL_COLOR_ARRAY);
	}
	else if ( gl_ext_pointparameters->integer && qglPointParameterfEXT )
	{
		int i;
		unsigned char color[4];
		const particle_t *p;

		qglDepthMask( GL_FALSE );
		qglEnable(GL_BLEND);
		qglDisable( GL_TEXTURE_2D );

		qglPointSize( gl_particle_size->value );

		qglBegin( GL_POINTS );
		for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ )
		{
			*(int *)color = d_8to24table[p->color];
			color[3] = p->alpha*255;

			qglColor4ubv( color );

			qglVertex3fv( p->origin );
		}
		qglEnd();

		qglDisable(GL_BLEND);
		qglColor4fv(colorWhite);
		qglDepthMask( GL_TRUE );
		qglEnable( GL_TEXTURE_2D );

	}
	else
	{
		GL_DrawParticles( r_newrefdef.num_particles, r_newrefdef.particles, d_8to24table );
	}
}

/*
============
R_PolyBlend
============
*/
static void R_PolyBlend (void)
{
	if (!gl_polyblend->integer)
		return;

	if (v_blend[3] < 0.01f)
		return;

	qglEnable(GL_BLEND);
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, 1, 1, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	qglColor4fv (v_blend);

	qglBegin (GL_TRIANGLES);
	qglVertex2i (-5, -5);
	qglVertex2i (10, -5);
	qglVertex2i (-5, 10);
	qglEnd ();

	qglDisable(GL_BLEND);
	qglEnable (GL_TEXTURE_2D);

	qglColor4fv(colorWhite);
}

//=======================================================================

static void SetPlaneSignbits(cplane_t *out)
{
	int	bits = 0, j;

	// for fast box on planeside test
	for (j=0; j<3; j++) {
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	out->signbits = bits;
}


static void R_SetFrustum (void)
{
	int		i;
	float	xs, xc;
	float	ang;

	if(isWideScreen)
		ang = r_newrefdef.fov_x / 180 * M_PI * 0.625f;
	else
		ang = r_newrefdef.fov_x / 180 * M_PI * 0.5f;

	xs = (float)sin( ang );
	xc = (float)cos( ang );

	VectorScale( viewAxis[0], xs, frustum[1].normal );
	VectorMA( frustum[1].normal, xc, viewAxis[1], frustum[0].normal );
	VectorMA( frustum[1].normal, -xc, viewAxis[1], frustum[1].normal );

	ang = r_newrefdef.fov_y / 180 * M_PI * 0.5f;
	xs = (float)sin( ang );
	xc = (float)cos( ang );

	VectorScale( viewAxis[0], xs, frustum[3].normal );
	VectorMA( frustum[3].normal, xc, viewAxis[2], frustum[2].normal );
	VectorMA( frustum[3].normal, -xc, viewAxis[2], frustum[3].normal );

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		SetPlaneSignbits (&frustum[i]);
	}
}

//=======================================================================

/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame (void)
{
	//int i;
	mleaf_t	*leaf;

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_newrefdef.vieworg, r_origin);
	AngleVectors (r_newrefdef.viewangles, vpn, vright, vup);
	VectorCopy(vpn, viewAxis[0]);
	VectorNegate(vright, viewAxis[1]);
	VectorCopy(vup, viewAxis[2]);

	// current viewcluster
	if ( !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{
			// look down a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{
			// look up a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
	}
	else // clear out the portion of the screen that the NOWORLDMODEL defines
	{
		qglEnable( GL_SCISSOR_TEST );
		qglScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );
		qglClear( GL_DEPTH_BUFFER_BIT );
		qglDisable( GL_SCISSOR_TEST );
	}

	Vector4Copy(r_newrefdef.blend, v_blend);
}


static float skybox_farz = 4096;

static void R_SetupProjection( void )
{
	float	xmin, xmax, ymin, ymax;
	float	width, height, depth;
	float	zNear, zFar;

	// set up projection matrix
	zNear	= 4;
	zFar	= skybox_farz;

	ymax = zNear * (float)tan( r_newrefdef.fov_y * M_PI_DIV_360 );
	ymin = -ymax;

	xmax = ymax * r_newrefdef.width / r_newrefdef.height;
	xmin = -xmax;

	if(gl_state.camera_separation) {
		xmin += -( 2 * gl_state.camera_separation ) / zNear;
		xmax += -( 2 * gl_state.camera_separation ) / zNear;
	}

	width = xmax - xmin;
	height = ymax - ymin;
	depth = zFar - zNear;

	r_ProjectionMatrix[0] = 2 * zNear / width;
	r_ProjectionMatrix[4] = 0;
	r_ProjectionMatrix[8] = ( xmax + xmin ) / width;	// normally 0
	r_ProjectionMatrix[12] = 0;

	r_ProjectionMatrix[1] = 0;
	r_ProjectionMatrix[5] = 2 * zNear / height;
	r_ProjectionMatrix[9] = ( ymax + ymin ) / height;	// normally 0
	r_ProjectionMatrix[13] = 0;

	r_ProjectionMatrix[2] = 0;
	r_ProjectionMatrix[6] = 0;
	r_ProjectionMatrix[10] = -( zFar + zNear ) / depth;
	r_ProjectionMatrix[14] = -2 * zFar * zNear / depth;

	r_ProjectionMatrix[3] = 0;
	r_ProjectionMatrix[7] = 0;
	r_ProjectionMatrix[11] = -1;
	r_ProjectionMatrix[15] = 0;
}

/*
=============
R_SetupModelviewMatrix
=============
*/
static void R_SetupModelviewMatrix( void )
{
	r_WorldViewMatrix[ 0] = -viewAxis[1][0];
	r_WorldViewMatrix[ 1] = viewAxis[2][0];
	r_WorldViewMatrix[ 2] = -viewAxis[0][0];
	r_WorldViewMatrix[ 3] = 0.0;
	r_WorldViewMatrix[ 4] = -viewAxis[1][1];
	r_WorldViewMatrix[ 5] = viewAxis[2][1];
	r_WorldViewMatrix[ 6] = -viewAxis[0][1];
	r_WorldViewMatrix[ 7] = 0.0;
	r_WorldViewMatrix[ 8] = -viewAxis[1][2];
	r_WorldViewMatrix[ 9] = viewAxis[2][2];
	r_WorldViewMatrix[10] = -viewAxis[0][2];
	r_WorldViewMatrix[11] = 0.0;
	r_WorldViewMatrix[12] = DotProduct(r_origin, viewAxis[1]);
	r_WorldViewMatrix[13] = -DotProduct(r_origin, viewAxis[2]);
	r_WorldViewMatrix[14] = DotProduct(r_origin, viewAxis[0]);
	r_WorldViewMatrix[15] = 1.0;
}

/*
=============
R_SetupGL
=============
*/
static void R_SetupGL (void)
{
	// set up viewport
	qglViewport( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );

	// set up projection matrix
	R_SetupProjection();
	qglMatrixMode(GL_PROJECTION);
    qglLoadMatrixf (r_ProjectionMatrix);

	// set up the world view matrix
	R_SetupModelviewMatrix();
	qglMatrixMode (GL_MODELVIEW);
	qglLoadMatrixf(r_WorldViewMatrix);

	// set drawing parms
	if (gl_cull->integer)
		qglEnable(GL_CULL_FACE);

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);
}

/*
=============
R_Clear
=============
*/
static void R_Clear (void)
{
	int	bits = 0;

	if (gl_clear->integer)
		bits |= GL_COLOR_BUFFER_BIT;

	if (gl_state.stencil && gl_shadows->integer == 2)
	{
		qglClearStencil(1);
		bits |= GL_STENCIL_BUFFER_BIT;
	}

	if (gl_ztrick->integer)
	{
		static int trickframe = 0;

		if (bits)
			qglClear (bits);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			qglDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		bits |= GL_DEPTH_BUFFER_BIT;
		qglClear(bits);

		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc (GL_LEQUAL);
	}

	qglDepthRange (gldepthmin, gldepthmax);
}

/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/

void R_ApplyStains(void);

static void R_RenderView (refdef_t *fd)
{
	static const float fogcolor[4] = {0.09f,0.1f,0.12f,1.0f};

	if (r_norefresh->integer)
		return;

	r_newrefdef = *fd;

	r_newrefdef.width *= gl_scale->value;
	r_newrefdef.height *= gl_scale->value;
	r_newrefdef.x *= gl_scale->value;
	r_newrefdef.y *= gl_scale->value;

	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		Com_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	c_brush_polys = c_alias_polys = 0;

	R_PushDlights ();

	if (gl_finish->integer)
		qglFinish ();

	if (gl_stainmaps->integer && r_newrefdef.num_newstains > 0)
		R_ApplyStains ();

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();

	R_AddDecals (); //Decals

	R_DrawEntitiesOnList ();

	R_RenderDlights ();

	R_DrawParticles ();

	R_DrawAlphaSurfaces ();

	R_PolyBlend();

	if (r_speeds->integer)
	{
		Com_Printf ("%4i wpoly %4i epoly %i tex %i lmaps\n",
			c_brush_polys, 
			c_alias_polys, 
			c_visible_textures, 
			c_visible_lightmaps); 
	}

	if(gl_fog->integer)
	{
		qglFogi		(GL_FOG_MODE, GL_EXP2);
		qglFogf		(GL_FOG_DENSITY, gl_fog_density->value*2/4096);
		qglFogfv	(GL_FOG_COLOR, fogcolor);
		qglEnable	(GL_FOG);
	}
}

void R_MotionBlur(void)
{
	static unsigned int blurtex = 0;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if (!blurtex)
	{
		qglGenTextures(1,&blurtex);
		qglBindTexture(GL_TEXTURE_RECTANGLE_NV,blurtex);
		qglCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV,0,GL_RGB,0,0,r_newrefdef.width,r_newrefdef.height,0);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		return;
	}

    qglMatrixMode(GL_PROJECTION); 
    qglLoadIdentity (); 
    qglOrtho  (0, r_newrefdef.width, r_newrefdef.height, 0, -99999, 99999); 
    qglMatrixMode(GL_MODELVIEW); 
    qglLoadIdentity (); 

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	GL_TexEnv(GL_MODULATE);
	qglEnable(GL_TEXTURE_RECTANGLE_NV);
	qglColor4f (1,1,1,0.5f);
	
	qglBegin(GL_QUADS);
	qglTexCoord2i(0, r_newrefdef.height);
	qglVertex2i(0,0);
	qglTexCoord2i(r_newrefdef.width, r_newrefdef.height);
	qglVertex2i(r_newrefdef.width, 0);
	qglTexCoord2i(r_newrefdef.width, 0);
	qglVertex2i(r_newrefdef.width, r_newrefdef.height);
	qglTexCoord2i(0,0);
	qglVertex2i(0, r_newrefdef.height);
	qglEnd();

	qglDisable(GL_TEXTURE_RECTANGLE_NV);
	GL_TexEnv( GL_REPLACE );
	qglDisable(GL_BLEND);
	qglEnable(GL_ALPHA_TEST);
	qglColor4fv(colorWhite);

	qglBindTexture(GL_TEXTURE_RECTANGLE_NV,blurtex);
	qglCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV,0,GL_RGB,0,0,r_newrefdef.width,r_newrefdef.height,0);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void	R_SetGL2D (void)
{
	// set 2D virtual screen size
	qglViewport (0,0, vid.width, vid.height);

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();

	qglOrtho(0, vid.width / gl_scale->value, vid.height / gl_scale->value, 0, -99999, 99999);
//	qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);

	qglEnable(GL_ALPHA_TEST);
}

/*
====================
R_SetLightLevel

====================
*/
static void R_SetLightLevel (void)
{
	vec3_t		shadelight;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	// save off light value for server to look at (BIG HACK!)

	R_LightPoint (r_newrefdef.vieworg, shadelight);

	// pick the greatest component, which should be the same
	// as the mono value returned by software
	if (shadelight[0] > shadelight[1])
	{
		if (shadelight[0] > shadelight[2])
			r_lightlevel->integer = 150*shadelight[0];
		else
			r_lightlevel->integer = 150*shadelight[2];
	}
	else
	{
		if (shadelight[1] > shadelight[2])
			r_lightlevel->integer = 150*shadelight[1];
		else
			r_lightlevel->integer = 150*shadelight[2];
	}

}

/*
=====================
R_RenderFrame
=====================
*/
void R_RenderFrame (refdef_t *fd)
{
	R_RenderView( fd );
	R_SetLightLevel ();

	if (gl_motionblur->integer && gl_state.tex_rectangle)
		R_MotionBlur();

	R_SetGL2D ();
}


static void OnChange_Scale(cvar_t *self, const char *oldValue)
{
	int width, height;

	if( self->value < 1.0f ) {
		Cvar_Set( self->name, "1" );
		if((float)atof(oldValue) == 1.0f)
			return;
	}

	// get the current resolution
	R_GetModeInfo(&width, &height, gl_mode->integer);
	// lie to client about new scaled window size
	VID_NewWindow( width / self->value, height / self->value );
}

static void OnChange_WaterWaves(cvar_t *self, const char *oldValue)
{
	if (self->value < 0)
		Cvar_Set(self->name, "0");
	else if (self->value > 4)
		Cvar_Set(self->name, "4");
}

// texturemode stuff
static void OnChange_TexMode(cvar_t *self, const char *oldValue)
{
	GL_TextureMode( self->string );
	self->modified = false;
}

static void OnChange_TexAlphaMode(cvar_t *self, const char *oldValue)
{
	GL_TextureAlphaMode( self->string );
	self->modified = false;
}

static void OnChange_TexSolidMode(cvar_t *self, const char *oldValue)
{
	GL_TextureSolidMode( self->string );
	self->modified = false;
}


static void OnChange_Skydistace(cvar_t *self, const char *oldValue)
{
	GLdouble boxsize;

	boxsize = self->integer;
	boxsize -= 252 * ceil(boxsize / 2300);
	skybox_farz = 1.0;
	while (skybox_farz < boxsize)  // make this value a power-of-2
	{
		skybox_farz *= 2.0;
		if (skybox_farz >= 65536.0)  // don't make it larger than this
			break;
  	}
	skybox_farz *= 2.0;	// double since boxsize is distance from camera to edge of skybox
	if (skybox_farz >= 65536.0)  // don't make it larger than this
		skybox_farz = 65536.0;
}

static void OnChange_Fog(cvar_t *self, const char *oldValue)
{
	qglDisable(GL_FOG);
}

static void OnChangeCustomWH(cvar_t *self, const char *oldValue)
{
	if(gl_state.prev_mode == -1)
		gl_state.prev_mode = 3;
}

static void OnChangeMaxAnisotropy(cvar_t *self, const char *oldValue)
{
	int		i;
	image_t	*image;

	if(!gl_config.anisotropic)
		return;

	if(self->integer > gl_config.maxAnisotropic)
	{
		Cvar_SetValue(self->name, gl_config.maxAnisotropic);
		if(atoi(oldValue) == gl_config.maxAnisotropic)
			return;
	}
	else if (self->integer < 0)
	{
		Cvar_Set(self->name, "0");
		if(atoi(oldValue) == 0)
			return;
	}

	for (i=0, image=gltextures; i<numgltextures ; i++, image++)
	{
		if (image->type != it_pic && image->type != it_sky )
		{
			GL_Bind (image->texnum);
			qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, self->integer);
		}
	}
}

extern float r_turbsin[256];

static void R_Register( void )
{
	int j;
	static qboolean render_initialized = false;

	Cvar_GetLatchedVars (CVAR_LATCHVIDEO);

	if(render_initialized)
		return;

	for ( j = 0; j < 256; j++ )
	{
		r_turbsin[j] *= 0.5f;
	}

	r_lefthand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_norefresh = Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = Cvar_Get ("r_fullbright", "0", CVAR_CHEAT);
	r_drawentities = Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = Cvar_Get ("r_drawworld", "1", CVAR_CHEAT);
	r_novis = Cvar_Get ("r_novis", "0", 0);
	r_nocull = Cvar_Get ("r_nocull", "0", 0);
	r_lerpmodels = Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = Cvar_Get ("r_speeds", "0", 0);

	r_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);

	//gl_nosubimage = Cvar_Get( "gl_nosubimage", "0", 0 );
	gl_allow_software = Cvar_Get( "gl_allow_software", "0", CVAR_LATCHVIDEO );

	gl_particle_min_size = Cvar_Get( "gl_particle_min_size", "2", CVAR_ARCHIVE );
	gl_particle_max_size = Cvar_Get( "gl_particle_max_size", "40", CVAR_ARCHIVE );
	gl_particle_size = Cvar_Get( "gl_particle_size", "40", CVAR_ARCHIVE );
	gl_particle_att_a = Cvar_Get( "gl_particle_att_a", "0.01", CVAR_ARCHIVE );
	gl_particle_att_b = Cvar_Get( "gl_particle_att_b", "0.0", CVAR_ARCHIVE );
	gl_particle_att_c = Cvar_Get( "gl_particle_att_c", "0.01", CVAR_ARCHIVE );

	gl_modulate = Cvar_Get ("gl_modulate", "1", CVAR_ARCHIVE );
	gl_bitdepth = Cvar_Get( "gl_bitdepth", "0", CVAR_ARCHIVE|CVAR_LATCHVIDEO );
	gl_mode = Cvar_Get( "gl_mode", "3", CVAR_ARCHIVE );

	gl_lightmap = Cvar_Get ("gl_lightmap", "0", CVAR_CHEAT);
	gl_shadows = Cvar_Get ("gl_shadows", "0", CVAR_ARCHIVE );
	gl_dynamic = Cvar_Get ("gl_dynamic", "1", 0);
	gl_nobind = Cvar_Get ("gl_nobind", "0", 0);
	gl_round_down = Cvar_Get ("gl_round_down", "1", CVAR_ARCHIVE|CVAR_LATCHVIDEO);
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_ARCHIVE|CVAR_LATCHVIDEO);
	gl_skymip = Cvar_Get ("gl_skymip", "0", CVAR_ARCHIVE|CVAR_LATCHVIDEO);
	gl_showtris = Cvar_Get ("gl_showtris", "0", 0);
	gl_ztrick = Cvar_Get ("gl_ztrick", "0", 0);
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);
	gl_clear = Cvar_Get ("gl_clear", "0", 0);
	gl_cull = Cvar_Get ("gl_cull", "1", 0);
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", 0);
	gl_flashblend = Cvar_Get ("gl_flashblend", "0", 0);
//	gl_playermip = Cvar_Get ("gl_playermip", "0", 0);
	gl_monolightmap = Cvar_Get( "gl_monolightmap", "0", 0 );
	gl_driver = Cvar_Get( "gl_driver", GL_DRIVERNAME, CVAR_ARCHIVE|CVAR_LATCHVIDEO);	
	gl_texturemode = Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	gl_texturealphamode = Cvar_Get( "gl_texturealphamode", "default", CVAR_ARCHIVE );
	gl_texturesolidmode = Cvar_Get( "gl_texturesolidmode", "default", CVAR_ARCHIVE );
	gl_lockpvs = Cvar_Get( "gl_lockpvs", "0", CVAR_CHEAT );

	gl_vertex_arrays = Cvar_Get( "gl_vertex_arrays", "0", CVAR_ARCHIVE );

	gl_ext_swapinterval = Cvar_Get( "gl_ext_swapinterval", "1", CVAR_ARCHIVE|CVAR_LATCHVIDEO );
	gl_ext_palettedtexture = Cvar_Get( "gl_ext_palettedtexture", "0", CVAR_ARCHIVE|CVAR_LATCHVIDEO );
	gl_ext_multitexture = Cvar_Get( "gl_ext_multitexture", "1", CVAR_ARCHIVE|CVAR_LATCHVIDEO );
	gl_ext_pointparameters = Cvar_Get( "gl_ext_pointparameters", "0", CVAR_ARCHIVE|CVAR_LATCHVIDEO );
	gl_ext_compiled_vertex_array = Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE|CVAR_LATCHVIDEO );

	gl_drawbuffer = Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 );
	gl_swapinterval = Cvar_Get( "gl_swapinterval", "1", CVAR_ARCHIVE );

	gl_saturatelighting = Cvar_Get( "gl_saturatelighting", "0", CVAR_CHEAT );

	gl_3dlabs_broken = Cvar_Get( "gl_3dlabs_broken", "1", CVAR_ARCHIVE|CVAR_LATCHVIDEO );

//	vid_fullscreen = Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
//	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );

	//Added cvar's -Maniac
	skydistance = Cvar_Get("skydistance", "2300", CVAR_ARCHIVE ); // DMP - skybox size change

	gl_replacewal = Cvar_Get( "gl_replacewal", "0", CVAR_ARCHIVE|CVAR_LATCHVIDEO);
	gl_replacepcx = Cvar_Get( "gl_replacepcx", "0", CVAR_ARCHIVE|CVAR_LATCHVIDEO);
	gl_fontshadow = Cvar_Get( "gl_fontshadow", "0", CVAR_ARCHIVE);

	gl_stainmaps = Cvar_Get( "gl_stainmaps", "0", CVAR_ARCHIVE );
	gl_sgis_mipmap = Cvar_Get( "gl_sgis_mipmap", "0", CVAR_ARCHIVE|CVAR_LATCHVIDEO );
	gl_ext_texture_compression = Cvar_Get( "gl_ext_texture_compression", "0", CVAR_ARCHIVE|CVAR_LATCHVIDEO );
	gl_celshading = Cvar_Get ( "gl_celshading", "0", CVAR_ARCHIVE ); //Celshading
	gl_celshading_width = Cvar_Get ( "gl_celshading_width", "6", CVAR_ARCHIVE );
	gl_scale = Cvar_Get ("gl_scale", "1", CVAR_ARCHIVE);

	gl_watercaustics = Cvar_Get ("gl_watercaustics", "0", 0);

    gl_screenshot_quality = Cvar_Get( "gl_screenshot_quality", "85", CVAR_ARCHIVE );
	gl_motionblur = Cvar_Get( "gl_motionblur", "0", 0 );	// motionblur
	gl_waterwaves = Cvar_Get( "gl_waterwaves", "0", CVAR_ARCHIVE );	// waterwave
	gl_particle = Cvar_Get ( "gl_particle", "0", CVAR_ARCHIVE );

	gl_decals = Cvar_Get( "gl_decals", "0", CVAR_ARCHIVE );
	gl_decals_time = Cvar_Get( "gl_decals_time", "30", CVAR_ARCHIVE );
	
	gl_gammapics = Cvar_Get( "gl_gammapics", "1", CVAR_ARCHIVE|CVAR_LATCHVIDEO );
	gl_fog = Cvar_Get ("gl_fog", "0", 0);
	gl_fog_density = Cvar_Get ("gl_fog_density", "1", 0);
	gl_coloredlightmaps = Cvar_Get( "gl_coloredlightmaps", "1", CVAR_ARCHIVE|CVAR_LATCHVIDEO);
    Cmd_AddCommand( "screenshotjpg", GL_ScreenShot_f );

	gl_eff_world_wireframe = Cvar_Get( "gl_eff_world_wireframe", "0", CVAR_CHEAT );
	gl_eff_world_bg_type = Cvar_Get( "gl_eff_world_bg_type", "0", CVAR_CHEAT );
	gl_eff_world_bg_color_r = Cvar_Get( "gl_eff_world_bg_color_r", "255", 0 );
	gl_eff_world_bg_color_g = Cvar_Get( "gl_eff_world_bg_color_g", "255", 0 );
	gl_eff_world_bg_color_b = Cvar_Get( "gl_eff_world_bg_color_b", "255", 0 );
	gl_eff_world_lines_color_r = Cvar_Get( "gl_eff_world_lines_color_r", "0", 0 );
	gl_eff_world_lines_color_g = Cvar_Get( "gl_eff_world_lines_color_g", "255", 0 );
	gl_eff_world_lines_color_b = Cvar_Get( "gl_eff_world_lines_color_b", "0", 0 );

	gl_shelleffect = Cvar_Get ("gl_shelleffect", "0", CVAR_ARCHIVE);

	gl_ext_texture_filter_anisotropic = Cvar_Get("gl_ext_texture_filter_anisotropic", "0", CVAR_ARCHIVE|CVAR_LATCHVIDEO);
	gl_ext_max_anisotropy = Cvar_Get("gl_ext_max_anisotropy", "2", CVAR_ARCHIVE);
	gl_ext_max_anisotropy->OnChange = OnChangeMaxAnisotropy;

	gl_texturemode->OnChange = OnChange_TexMode;
	gl_texturealphamode->OnChange = OnChange_TexAlphaMode;
	gl_texturesolidmode->OnChange = OnChange_TexSolidMode;

	r_customwidth = Cvar_Get ("r_customwidth",  "1024", CVAR_ARCHIVE);
	r_customheight = Cvar_Get ("r_customheight", "768", CVAR_ARCHIVE);
	r_customwidth->OnChange = OnChangeCustomWH;
	r_customheight->OnChange = OnChangeCustomWH;

	Cmd_AddCommand( "modelist", R_ModeList_f );

	if( gl_scale->value < 1.0f )
		Cvar_Set( "gl_scale", "1" );

	gl_waterwaves->OnChange = OnChange_WaterWaves;
	gl_scale->OnChange = OnChange_Scale;
	skydistance->OnChange = OnChange_Skydistace;
	gl_fog->OnChange = OnChange_Fog;
	OnChange_WaterWaves(gl_waterwaves, gl_waterwaves->resetString);
	OnChange_Skydistace(skydistance, skydistance->resetString);
	//End

	Cmd_AddCommand( "imagelist", GL_ImageList_f );
	Cmd_AddCommand( "screenshot", GL_ScreenShot_f );
	Cmd_AddCommand( "modellist", Mod_Modellist_f );
	Cmd_AddCommand( "gl_strings", GL_Strings_f );

	render_initialized = true;
}

/*
================
R_GetModeInfo
================
*/
typedef struct vidmode_s
{
	const char *description;
	int         width, height;
	int         mode;
} vidmode_t;

static const vidmode_t r_vidModes[] =
{
	{ "Mode 0: 320x240",	 320,  240,	0  },
	{ "Mode 1: 400x300",	 400,  300,	1  },
	{ "Mode 2: 512x384",	 512,  384,	2  },
	{ "Mode 3: 640x480",	 640,  480,	3  },
	{ "Mode 4: 800x600",	 800,  600,	4  },
	{ "Mode 5: 960x720",	 960,  720,	5  },
	{ "Mode 6: 1024x768",	1024,  768,	6  },
	{ "Mode 7: 1152x864",	1152,  864,	7  },
	{ "Mode 8: 1280x960",	1280,  960,	8  },
	{ "Mode 9: 1600x1200",	1600, 1200,	9  },
	{ "Mode 10: 2048x1536",	2048, 1536,	10 },
	{ "Mode 11: 1024x480",	1024,  480,	11 },
	{ "Mode 12: 1280x768",	1280,  768,	12 },
	{ "Mode 13: 1280x1024",	1280, 1024,	13 }
};

static const int s_numVidModes = ( sizeof( r_vidModes ) / sizeof( r_vidModes[0] ) );

qboolean R_GetModeInfo( int *width, int *height, int mode )
{
	if ( mode < -1 || mode >= s_numVidModes )
		return false;

	if ( mode == -1 ) {
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		//*windowAspect = r_customaspect->value;
		return true;
	}
	*width  = r_vidModes[mode].width;
	*height = r_vidModes[mode].height;

	return true;
}

static void R_ModeList_f( void )
{
	int i;

	Com_Printf("%sMode -1: Custom resolution (%dx%d)\n", -1 == gl_mode->integer ? "\x02" : "", r_customwidth->integer, r_customheight->integer);
	for ( i = 0; i < s_numVidModes; i++ ) {
		Com_Printf("%s%s\n", i == gl_mode->integer ? "\x02" : "", r_vidModes[i].description);
	}
}

qboolean R_IsWideScreen(void)
{
	return isWideScreen;
}

static void R_CheckWideScreen(void)
{
	if( (gl_mode->integer == -1 && (r_customwidth->value/r_customheight->value == 16.0f/9.0f)) ||
		(float)((float)vid.width/(float)vid.height) == (float)(16.0f/9.0f)) {
		Com_Printf("WideScreen Mode\n");
		isWideScreen = true;
	}
}

/*
==================
R_SetMode
==================
*/
qboolean R_SetMode (void)
{
	rserr_t err;
	qboolean fullscreen;

#ifdef _WIN32
	if ( vid_fullscreen->modified && !gl_config.allow_cds )
	{
		Com_Printf ( "R_SetMode: CDS not allowed with this driver.\n" );
		Cvar_SetValue( "vid_fullscreen", !vid_fullscreen->integer );
		vid_fullscreen->modified = false;
	}
#endif
	fullscreen = vid_fullscreen->integer;
	vid_fullscreen->modified = false;
	gl_mode->modified = false;
	isWideScreen = false;

	if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->integer, fullscreen ) ) == rserr_ok )
	{
		gl_state.prev_mode = gl_mode->integer;
	}
	else
	{
		if ( err == rserr_invalid_fullscreen )
		{
			Cvar_Set( "vid_fullscreen", "0");
			vid_fullscreen->modified = false;
			Com_Printf ( "R_SetMode: fullscreen unavailable in this mode (%i).\n", gl_mode->integer );
			if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->integer, false ) ) == rserr_ok ) {
				R_CheckWideScreen();
				return true;
			}
		}
		else if ( err == rserr_invalid_mode )
		{
			Cvar_SetValue( "gl_mode", gl_state.prev_mode );
			gl_mode->modified = false;
			Com_Printf ( "R_SetMode: invalid mode (%i).\n", gl_mode->integer );
		}

		// try setting it back to something safe
		if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_state.prev_mode, false ) ) != rserr_ok )
		{
			Com_Printf ( "R_SetMode: could not revert to safe mode.\n" );
			return false;
		}
	}
	R_CheckWideScreen();

	return true;
}

/*
===============
R_Init
===============
*/
char qglLastError[128];

static const char *GetQGLErrorString (unsigned int error)
{
	switch (error) {
	case GL_INVALID_ENUM:		return "INVALID ENUM";
	case GL_INVALID_OPERATION:	return "INVALID OPERATION";
	case GL_INVALID_VALUE:		return "INVALID VALUE";
	case GL_NO_ERROR:			return "NO ERROR";
	case GL_OUT_OF_MEMORY:		return "OUT OF MEMORY";
	case GL_STACK_OVERFLOW:		return "STACK OVERFLOW";
	case GL_STACK_UNDERFLOW:	return "STACK UNDERFLOW";
	}

	return "unknown";
}

int R_Init( void *hinstance, void *hWnd )
{	
	char renderer_buffer[1000];
	char vendor_buffer[1000];
	unsigned int		err;

	Com_Printf ("ref_gl version: "REF_VERSION"\n");

	Draw_GetPalette ();

	R_Register();

	// initialize our QGL dynamic bindings
	if ( !QGL_Init( gl_driver->string ) )
	{
		QGL_Shutdown();
		if(strcmp(gl_driver->string, GL_DRIVERNAME)) {
			Com_Printf ("R_Init: Could not load \"%s\", trying \"%s\"\n", gl_driver->string, GL_DRIVERNAME );
			if( !QGL_Init( GL_DRIVERNAME ) ) {
				QGL_Shutdown();
			    Com_Error (ERR_FATAL, "R_Init: Could not load \"%s\" or \"%s\"", gl_driver->string, GL_DRIVERNAME);
				return -1;
			}
		} else {
			Com_Error (ERR_FATAL, "R_Init: Could not load \"%s\"", gl_driver->string );
			return -1;
		}
	}

	// initialize OS-specific parts of OpenGL
	if ( !GLimp_Init( hinstance, hWnd ) )
	{
		QGL_Shutdown();
		Com_Error (ERR_FATAL, "R_Init: GLimp_Init Failed!" );
		return -1;
	}

	// set our "safe" modes
	gl_state.prev_mode = 3;

	qglLastError[0] = 0;
	// create the window and set up the context
	if ( !R_SetMode () )
	{
		QGL_Shutdown();
		if(qglLastError[0])
			Com_Error (ERR_FATAL, "R_Init: Could not set resolution: %s", qglLastError);
		else
			Com_Error (ERR_FATAL, "R_Init: Could not set resolution!" );
		return -1;
	}

	// get our various GL strings
	gl_config.vendor_string = (const char *)qglGetString (GL_VENDOR);
	Com_Printf ("GL_VENDOR: %s\n", gl_config.vendor_string );
	gl_config.renderer_string = (const char *)qglGetString (GL_RENDERER);
	Com_Printf ("GL_RENDERER: %s\n", gl_config.renderer_string );
	gl_config.version_string = (const char *)qglGetString (GL_VERSION);
	Com_Printf ("GL_VERSION: %s\n", gl_config.version_string );
	gl_config.extensions_string = (const char *)qglGetString (GL_EXTENSIONS);
	//Com_Printf ("GL_EXTENSIONS: %s\n", gl_config.extensions_string );

	Q_strncpyz( renderer_buffer, gl_config.renderer_string, sizeof(renderer_buffer) );
	Q_strlwr( renderer_buffer );

	Q_strncpyz( vendor_buffer, gl_config.vendor_string, sizeof(vendor_buffer) );
	Q_strlwr( vendor_buffer );

	if ( strstr( renderer_buffer, "voodoo" ) )
	{
		if ( !strstr( renderer_buffer, "rush" ) )
			gl_config.renderer = GL_RENDERER_VOODOO;
		else
			gl_config.renderer = GL_RENDERER_VOODOO_RUSH;
	}
	else if ( strstr( vendor_buffer, "sgi" ) )
		gl_config.renderer = GL_RENDERER_SGI;
	else if ( strstr( renderer_buffer, "permedia" ) )
		gl_config.renderer = GL_RENDERER_PERMEDIA2;
	else if ( strstr( renderer_buffer, "glint" ) )
		gl_config.renderer = GL_RENDERER_GLINT_MX;
	else if ( strstr( renderer_buffer, "glzicd" ) )
		gl_config.renderer = GL_RENDERER_REALIZM;
	else if ( strstr( renderer_buffer, "gdi" ) )
		gl_config.renderer = GL_RENDERER_MCD;
	else if ( strstr( renderer_buffer, "pcx2" ) )
		gl_config.renderer = GL_RENDERER_PCX2;
	else if ( strstr( renderer_buffer, "verite" ) )
		gl_config.renderer = GL_RENDERER_RENDITION;
	else
		gl_config.renderer = GL_RENDERER_OTHER;

	if ( toupper( gl_monolightmap->string[1] ) != 'F' )
	{
		if ( gl_config.renderer == GL_RENDERER_PERMEDIA2 )
		{
			Cvar_Set( "gl_monolightmap", "A" );
			Com_Printf ( "...using gl_monolightmap 'a'\n" );
		}
		else if ( gl_config.renderer & GL_RENDERER_POWERVR ) 
			Cvar_Set( "gl_monolightmap", "0" );
		else
			Cvar_Set( "gl_monolightmap", "0" );
	}

	// power vr can't have anything stay in the framebuffer, so
	// the screen needs to redraw the tiled background every frame
	if ( gl_config.renderer & GL_RENDERER_POWERVR ) 
		Cvar_Set( "scr_drawall", "1" );
	else
		Cvar_Set( "scr_drawall", "0" );


//#ifdef __linux__
#if 0 && defined (__linux__)
	Cvar_Set( "gl_finish", "1" );
#endif

	// MCD has buffering issues
	if ( gl_config.renderer == GL_RENDERER_MCD )
		Cvar_Set( "gl_finish", "1" );


	gl_config.allow_cds = true;
	if ( (gl_config.renderer & GL_RENDERER_3DLABS) && gl_3dlabs_broken->integer)
		gl_config.allow_cds = false;

#ifdef _WIN32
	if ( gl_config.allow_cds )
		Com_Printf ( "...allowing CDS\n" );
	else
		Com_Printf ( "...disabling CDS\n" );
#endif

	// grab extensions
	if ( strstr( gl_config.extensions_string, "GL_EXT_compiled_vertex_array" ) || 
		 strstr( gl_config.extensions_string, "GL_SGI_compiled_vertex_array" ) )
	{
		Com_Printf ( "...enabling GL_EXT_compiled_vertex_array\n" );
		qglLockArraysEXT = ( void * ) qwglGetProcAddress( "glLockArraysEXT" );
		qglUnlockArraysEXT = ( void * ) qwglGetProcAddress( "glUnlockArraysEXT" );
	}
	else
	{
		Com_Printf ( "...GL_EXT_compiled_vertex_array not found\n" );
	}

#ifdef _WIN32
	if ( strstr( gl_config.extensions_string, "WGL_EXT_swap_control" ) )
	{
		qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qwglGetProcAddress( "wglSwapIntervalEXT" );
		Com_Printf ( "...enabling WGL_EXT_swap_control\n" );
	}
	else
	{
		Com_Printf ( "...WGL_EXT_swap_control not found\n" );
	}
#endif

	if ( strstr( gl_config.extensions_string, "GL_EXT_point_parameters" ) )
	{
		if ( gl_ext_pointparameters->integer )
		{
			qglPointParameterfEXT = ( void (APIENTRY *)( GLenum, GLfloat ) ) qwglGetProcAddress( "glPointParameterfEXT" );
			qglPointParameterfvEXT = ( void (APIENTRY *)( GLenum, const GLfloat * ) ) qwglGetProcAddress( "glPointParameterfvEXT" );
			Com_Printf ( "...using GL_EXT_point_parameters\n" );
		}
		else
		{
			Com_Printf ( "...ignoring GL_EXT_point_parameters\n" );
		}
	}
	else
	{
		Com_Printf ( "...GL_EXT_point_parameters not found\n" );
	}

#ifdef __linux__
	if ( strstr( gl_config.extensions_string, "3DFX_set_global_palette" ))
	{
		if ( gl_ext_palettedtexture->integer )
		{
			Com_Printf ( "...using 3DFX_set_global_palette\n" );
			qgl3DfxSetPaletteEXT = ( void ( APIENTRY * ) (GLuint *) )qwglGetProcAddress( "gl3DfxSetPaletteEXT" );
			qglColorTableEXT = Fake_glColorTableEXT;
		}
		else
		{
			Com_Printf ( "...ignoring 3DFX_set_global_palette\n" );
		}
	}
	else
	{
		Com_Printf ( "...3DFX_set_global_palette not found\n" );
	}
#endif

	if ( !qglColorTableEXT &&
		strstr( gl_config.extensions_string, "GL_EXT_paletted_texture" ) && 
		strstr( gl_config.extensions_string, "GL_EXT_shared_texture_palette" ) )
	{
		if ( gl_ext_palettedtexture->integer )
		{
			Com_Printf ( "...using GL_EXT_shared_texture_palette\n" );
			qglColorTableEXT = ( void ( APIENTRY * ) ( GLenum, GLenum, GLsizei, GLenum, GLenum, const GLvoid * ) ) qwglGetProcAddress( "glColorTableEXT" );
		}
		else
		{
			Com_Printf ( "...ignoring GL_EXT_shared_texture_palette\n" );
		}
	}
	else
	{
		Com_Printf ( "...GL_EXT_shared_texture_palette not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_ARB_multitexture" ) )
	{
		if ( gl_ext_multitexture->integer )
		{
			Com_Printf ( "...using GL_ARB_multitexture\n" );
			qglMTexCoord2fSGIS = ( void * ) qwglGetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = ( void * ) qwglGetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ( void * ) qwglGetProcAddress( "glClientActiveTextureARB" );
			QGL_TEXTURE0 = GL_TEXTURE0_ARB;
			QGL_TEXTURE1 = GL_TEXTURE1_ARB;
		}
		else
		{
			Com_Printf ( "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		Com_Printf ( "...GL_ARB_multitexture not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_SGIS_multitexture" ) )
	{
		if ( qglActiveTextureARB )
		{
			Com_Printf ( "...GL_SGIS_multitexture deprecated in favor of ARB_multitexture\n" );
		}
		else if ( gl_ext_multitexture->integer )
		{
			Com_Printf ( "...using GL_SGIS_multitexture\n" );
			qglMTexCoord2fSGIS = ( void * ) qwglGetProcAddress( "glMTexCoord2fSGIS" );
			qglSelectTextureSGIS = ( void * ) qwglGetProcAddress( "glSelectTextureSGIS" );
			QGL_TEXTURE0 = GL_TEXTURE0_SGIS;
			QGL_TEXTURE1 = GL_TEXTURE1_SGIS;
		}
		else
		{
			Com_Printf ( "...ignoring GL_SGIS_multitexture\n" );
		}
	}
	else
	{
		Com_Printf ( "...GL_SGIS_multitexture not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_NV_texture_rectangle" ) )
	{
		Com_Printf ( "...using GL_NV_texture_rectangle\n");
		gl_state.tex_rectangle = true;
	}
	else if (strstr(gl_config.extensions_string, "GL_EXT_texture_rectangle"))
	{
		Com_Printf ( "...using GL_EXT_texture_rectangle\n");
		gl_state.tex_rectangle = true;
	} else {
		Com_Printf ( "...GL_NV_texture_rectangle not found\n");
		gl_state.tex_rectangle = false;
	}

	if ( strstr( gl_config.extensions_string, "GL_SGIS_generate_mipmap" ) )
	{
		if(gl_sgis_mipmap->integer)
		{
			Com_Printf ( "...using GL_SGIS_generate_mipmap\n");
			gl_state.sgis_mipmap = true;
		}
		else
		{
			Com_Printf ( "...ignoring GL_SGIS_generate_mipmap\n");
			gl_state.sgis_mipmap = false;
		}
	} else {
		Com_Printf ( "...GL_SGIS_generate_mipmap not found\n");
		gl_state.sgis_mipmap = false;
	}

	// Heffo - ARB Texture Compression
	if ( strstr( gl_config.extensions_string, "GL_ARB_texture_compression" ) )
	{
		if(gl_ext_texture_compression->integer)
		{
			Com_Printf ( "...using GL_ARB_texture_compression\n");
			gl_state.texture_compression = true;
			qglHint(GL_TEXTURE_COMPRESSION_HINT_ARB, GL_NICEST);
		}
		else
		{
			Com_Printf ( "...ignoring GL_ARB_texture_compression\n");
			gl_state.texture_compression = false;
		}
	}
	else
	{
		Com_Printf ( "...GL_ARB_texture_compression not found\n");
		gl_state.texture_compression = false;
	}

	// retreive information

	//Anisotropic
	gl_config.anisotropic = false;
	if (strstr( gl_config.extensions_string, "GL_EXT_texture_filter_anisotropic")) {
		if(gl_ext_texture_filter_anisotropic->integer)
		{
			qglGetIntegerv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_config.maxAnisotropic);
			if (gl_config.maxAnisotropic <= 0) {
				Com_Printf ("...GL_EXT_texture_filter_anisotropic not properly supported!\n");
				gl_config.maxAnisotropic = 0;
			}
			else {
				Com_Printf ("...enabling GL_EXT_texture_filter_anisotropic\n");
				Com_Printf ("Maximum Anisotropy: %i\n", gl_config.maxAnisotropic);
				gl_config.anisotropic = true;

				if(gl_ext_max_anisotropy->integer > gl_config.maxAnisotropic)
					Cvar_SetValue("gl_ext_max_anisotropy", gl_config.maxAnisotropic);
				else if(gl_ext_max_anisotropy->integer < 0)
					Cvar_Set("gl_ext_max_anisotropy", "0");
			}
		}
		else
			Com_Printf ("...ignoring GL_EXT_texture_filter_anisotropic\n");
	}
	else
		Com_Printf ("...GL_EXT_texture_filter_anisotropic not found\n");

	if(gl_ext_palettedtexture->integer && qglColorTableEXT)
	{
		gl_state.maxtexsize = 256;
	}
	else
	{
		qglGetIntegerv (GL_MAX_TEXTURE_SIZE, &gl_state.maxtexsize);
		if(gl_state.maxtexsize < 256)
			gl_state.maxtexsize = 256;
	}
	Com_Printf ( "Maximum Texture Size: %ix%i\n", gl_state.maxtexsize, gl_state.maxtexsize);

	GL_SetDefaultState();

	/*
	** draw our stereo patterns
	*/
#if 0 // commented out until H3D pays us the money they owe us
	GL_DrawStereoPattern();
#endif

	GL_InitImages ();

	Mod_Init ();
	R_InitParticleTexture ();
	Draw_InitLocal ();

	err = qglGetError();
	if ( err != GL_NO_ERROR )
		Com_Printf ("glGetError() = '%s' (0x%x)\n", GetQGLErrorString(err), err);

	return 0;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown (void)
{	
	/*Cmd_RemoveCommand ("modellist");
	Cmd_RemoveCommand ("screenshot");
	Cmd_RemoveCommand ("imagelist");
	Cmd_RemoveCommand ("gl_strings");

	Cmd_RemoveCommand ("screenshotjpg");*/


	Mod_FreeAll ();

	GL_ShutdownImages ();

	// shut down OS specific OpenGL stuff like contexts, etc.
	GLimp_Shutdown();

	// shutdown our QGL subsystem
	QGL_Shutdown();
}



/*
===============
R_BeginFrame
===============
*/
void R_BeginFrame( float camera_separation )
{

	gl_state.camera_separation = camera_separation;

	/*
	** change modes if necessary
	*/
	if ( gl_mode->modified || vid_fullscreen->modified )
	{	// FIXME: only restart if CDS is required
		VID_Restart_f ();
	}

	// update 3Dfx gamma -- it is expected that a user will do a vid_restart after tweaking this value
	if ( vid_gamma->modified )
	{
		vid_gamma->modified = false;

		if ( gl_config.renderer & ( GL_RENDERER_VOODOO ) )
		{
			char envbuffer[1024];
			float g;

			g = 2.00f * ( 0.8f - ( vid_gamma->value - 0.5f ) ) + 1.0F;
			Com_sprintf( envbuffer, sizeof(envbuffer), "SSTV2_GAMMA=%f", g );
			putenv( envbuffer );
			Com_sprintf( envbuffer, sizeof(envbuffer), "SST_GAMMA=%f", g );
			putenv( envbuffer );
		}
	}

	GLimp_BeginFrame( camera_separation );

	// go into 2D mode
	R_SetGL2D();

	if ( gl_drawbuffer->modified )
	{
		gl_drawbuffer->modified = false;

		if ( gl_state.camera_separation == 0 || !gl_state.stereo_enabled )
		{
			if ( Q_stricmp( gl_drawbuffer->string, "GL_FRONT" ) == 0 )
				qglDrawBuffer( GL_FRONT );
			else
				qglDrawBuffer( GL_BACK );
		}
	}

	// swapinterval stuff
	GL_UpdateSwapInterval();

	// clear screen if desired
	R_Clear();
}

/*
=============
R_SetPalette
=============
*/
unsigned r_rawpalette[256];

void R_CinematicSetPalette ( const unsigned char *palette)
{
	int		i;

	byte *rp = ( byte * ) r_rawpalette;

	if ( palette )
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = palette[i*3+0];
			rp[i*4+1] = palette[i*3+1];
			rp[i*4+2] = palette[i*3+2];
			rp[i*4+3] = 0xff;
		}
	}
	else
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = d_8to24table[i] & 0xff;
			rp[i*4+1] = ( d_8to24table[i] >> 8 ) & 0xff;
			rp[i*4+2] = ( d_8to24table[i] >> 16 ) & 0xff;
			rp[i*4+3] = 0xff;
		}
	}

	GL_SetTexturePalette( r_rawpalette );

	qglClearColor (0, 0, 0, 0);
	qglClear (GL_COLOR_BUFFER_BIT);
	qglClearColor( 1, 0.2f, 0, 1 );
}

/*
** R_DrawBeam
*/
void R_DrawBeam (void)
{
#define NUM_BEAM_SEGS 6

	int	i;
	float r, g, b;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	VectorCopy(currententity->oldorigin, oldorigin);

	VectorCopy(currententity->origin, origin);

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, currententity->frame * 0.5, perpvec );

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	qglDisable( GL_TEXTURE_2D );
	qglEnable(GL_BLEND);
	qglDepthMask( GL_FALSE );

	r = ( d_8to24table[currententity->skinnum & 0xFF] ) & 0xFF;
	g = ( d_8to24table[currententity->skinnum & 0xFF] >> 8 ) & 0xFF;
	b = ( d_8to24table[currententity->skinnum & 0xFF] >> 16 ) & 0xFF;

	r *= ONEDIV255;
	g *= ONEDIV255;
	b *= ONEDIV255;

	qglColor4f( r, g, b, currententity->alpha );

	qglBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		qglVertex3fv( start_points[i] );
		qglVertex3fv( end_points[i] );
		qglVertex3fv( start_points[(i+1)%NUM_BEAM_SEGS] );
		qglVertex3fv( end_points[(i+1)%NUM_BEAM_SEGS] );
	}
	qglEnd();

	qglEnable( GL_TEXTURE_2D );
	qglDisable(GL_BLEND);
	qglDepthMask( GL_TRUE );
	qglColor4fv(colorWhite);
}

