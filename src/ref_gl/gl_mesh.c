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
// gl_mesh.c: triangle model functions

#include "gl_local.h"

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

#define NUMVERTEXNORMALS	162

static const float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

//typedef float vec4_t[4];

static	vec4_t	s_lerped[MAX_VERTS];

static vec3_t	shadevector;
static float	shadelight[3];

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
static const float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anormtab.h"
;

static const float	*shadedots = r_avertexnormal_dots[0];
extern  vec3_t lightspot;

static void GL_LerpVerts( int nverts, const dtrivertx_t *v, const dtrivertx_t *ov, float *lerp, float move[3], float frontv[3], float backv[3] )
{
	int i;

	for (i=0 ; i < nverts; i++, v++, ov++, lerp+=4)
	{
		lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0];
		lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1];
		lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2];
	}
}

static void GL_LerpShellVerts( int nverts, const dtrivertx_t *v, const dtrivertx_t *ov, const dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3] )
{
	int i;
	float scale;

	if (currententity->flags & RF_WEAPONMODEL)
	{
		if(gl_shelleffect->integer)
			scale = 0.5f;
		else
			scale = 0.66f;
	}
	else
		scale = POWERSUIT_SCALE;

	for (i=0 ; i < nverts; i++, v++, ov++, lerp+=4 )
	{
		const float *normal = r_avertexnormals[verts[i].lightnormalindex];

		lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0] + normal[0] * scale;
		lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1] + normal[1] * scale;
		lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2] + normal[2] * scale; 
	}
}

/*============================
Cellshading from q2max
Discoloda's cellshading outline routine
=============================*/
#define OUTLINEDROPOFF 700.0 //distance away for it to stop
static void GL_DrawOutLine (const dmdl_t *paliashdr) 
{
	int		*order;
	float	scale;
	int		count;
 
	scale = (float)Distance( r_newrefdef.vieworg, currententity->origin)*(r_newrefdef.fov_y/90.0);
	scale = (OUTLINEDROPOFF-scale) / OUTLINEDROPOFF;

	if( scale <= 0 || scale >= 1)
		return;

	if( gl_celshading_width->value > 10)
		Cvar_Set("gl_celshading_width", "10");

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	qglCullFace (GL_BACK);
	qglEnable(GL_BLEND);
	qglColor4f (0, 0, 0, scale);
	qglLineWidth(gl_celshading_width->value * scale);

	//Now Draw...
	for (count = *order++; count; count = *order++)
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		}
		else
			qglBegin (GL_TRIANGLE_STRIP);

		do
		{
			qglVertex3fv (s_lerped[order[2]]);
			order += 3;
		} while (--count);

		qglEnd ();
	}

	qglLineWidth(1);
	qglDisable(GL_BLEND);
	qglCullFace(GL_FRONT);
	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
}

/*
=============
GL_DrawAliasFrameLerp

interpolates between two frames and origins
FIXME: batch lerp all vertexes
=============
*/
static void GL_DrawAliasFrameLerp (const dmdl_t *paliashdr, float backlerp)
{
	daliasframe_t	*frame, *oldframe;
	dtrivertx_t		*ov, *verts;
	int				i, *order, count, index_xyz;
	float			l, frontlerp, *lerp = s_lerped[0], alpha = 1;
	vec3_t			move, delta, frontv, backv;

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + currententity->frame * paliashdr->framesize);
	verts = frame->verts;

	oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + currententity->oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	frontlerp = 1.0f - backlerp;

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract (currententity->oldorigin, currententity->origin, delta);
	move[0] = DotProduct(currententity->axis[0],delta) + oldframe->translate[0];
	move[1] = DotProduct(currententity->axis[1],delta) + oldframe->translate[1];
	move[2] = DotProduct(currententity->axis[2],delta) + oldframe->translate[2];

	move[0] = backlerp*move[0] + frontlerp*frame->translate[0];
	move[1] = backlerp*move[1] + frontlerp*frame->translate[1];
	move[2] = backlerp*move[2] + frontlerp*frame->translate[2];

	VectorScale(frame->scale, frontlerp, frontv);
	VectorScale(oldframe->scale, backlerp, backv);

	if (currententity->flags & RF_TRANSLUCENT)
		alpha = currententity->alpha;

	if(currententity->flags & (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
	{
		GL_LerpShellVerts( paliashdr->num_xyz, verts, ov, verts, lerp, move, frontv, backv );

		if(gl_shelleffect->integer) {
			GL_Bind(r_shelltexture->texnum);
			qglColor4f( shadelight[0], shadelight[1], shadelight[2], 1);

			qglBlendFunc (GL_SRC_ALPHA, GL_ONE);

			for (count = *order++; count; count = *order++)
			{
				// get the vertex count and primitive type
				if (count < 0) {
					count = -count;
					qglBegin (GL_TRIANGLE_FAN);
				}
				else
					qglBegin (GL_TRIANGLE_STRIP);

				do
				{
					index_xyz = order[2];
					order += 3;
					qglTexCoord2f ((s_lerped[index_xyz][1] + s_lerped[index_xyz][0]) / 23.0 - (float)cos(r_newrefdef.time*0.3f),
									s_lerped[index_xyz][2] / 23.0 - (float)sin(r_newrefdef.time*0.3f));
					qglVertex3fv (s_lerped[index_xyz]);
				} while (--count);

				qglEnd ();
			}
			qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else {
			qglDisable( GL_TEXTURE_2D );
			qglColor4f( shadelight[0], shadelight[1], shadelight[2], alpha);
			
			for (count = *order++; count; count = *order++)
			{
				// get the vertex count and primitive type
				if (count < 0) {
					count = -count;
					qglBegin (GL_TRIANGLE_FAN);
				}
				else
					qglBegin (GL_TRIANGLE_STRIP);

				do
				{
					index_xyz = order[2];
					order += 3;
					qglVertex3fv (s_lerped[index_xyz]);
				} while (--count);

				qglEnd ();
			}
			qglEnable( GL_TEXTURE_2D );
		}
		return;
	}

	GL_LerpVerts( paliashdr->num_xyz, verts, ov, lerp, move, frontv, backv );

	if ( gl_vertex_arrays->integer )
	{
		float colorArray[MAX_VERTS*4];

		qglEnableClientState( GL_VERTEX_ARRAY );
		qglVertexPointer( 3, GL_FLOAT, 16, s_lerped );	// padded for SIMD

		qglEnableClientState( GL_COLOR_ARRAY );
		qglColorPointer( 4, GL_FLOAT, 0, colorArray );

		// pre light everything
		for ( i = 0; i < paliashdr->num_xyz; i++ )
		{
			float l = shadedots[verts[i].lightnormalindex];

			colorArray[i*4+0] = l * shadelight[0];
			colorArray[i*4+1] = l * shadelight[1];
			colorArray[i*4+2] = l * shadelight[2];
			colorArray[i*4+3] = alpha;
		}

		if ( qglLockArraysEXT != 0 )
			qglLockArraysEXT( 0, paliashdr->num_xyz );

		for (count = *order++; count; count = *order++)
		{
			// get the vertex count and primitive type
			if (count < 0)
			{
				count = -count;
				qglBegin (GL_TRIANGLE_FAN);
			}
			else
				qglBegin (GL_TRIANGLE_STRIP);

			do
			{
				// texture coordinates come from the draw list
				qglTexCoord2f (((float *)order)[0], ((float *)order)[1]);
				// normals and vertexes come from the frame list
				qglArrayElement( order[2] );
				order += 3;
			} while (--count);

			qglEnd ();
		}

		if ( qglUnlockArraysEXT != 0 )
			qglUnlockArraysEXT();
	}
	else
	{
		for (count = *order++; count; count = *order++)
		{
			// get the vertex count and primitive type
			if (count < 0)
			{
				count = -count;
				qglBegin (GL_TRIANGLE_FAN);
			}
			else
				qglBegin (GL_TRIANGLE_STRIP);

			do
			{
				// texture coordinates come from the draw list
				qglTexCoord2f (((float *)order)[0], ((float *)order)[1]);
				index_xyz = order[2];
				// normals and vertexes come from the frame list
				l = shadedots[verts[index_xyz].lightnormalindex];		
				qglColor4f (l* shadelight[0], l*shadelight[1], l*shadelight[2], alpha);

				qglVertex3fv (s_lerped[index_xyz]);
				order += 3;
			} while (--count);

			qglEnd ();
		}
	}
}

/*
=============
GL_DrawAliasShadow
=============
*/
static void GL_DrawAliasShadow (const dmdl_t *paliashdr)
{
	int		*order;
	vec3_t	point;
	float	height = 0, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];
	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);
	height = -lheight + 1.0f;

	if (gl_state.stencil && gl_shadows->integer == 2) {
		height = -lheight + 0.1f;
		qglEnable( GL_STENCIL_TEST );
		qglStencilFunc( GL_EQUAL, 1, 2 );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
	}

	for (count = *order++; count; count = *order++)
	{
		// get the vertex count and primitive type
		if (count < 0)
		{
			count = -count;
			qglBegin (GL_TRIANGLE_FAN);
		}
		else
			qglBegin (GL_TRIANGLE_STRIP);

		do
		{
			// normals and vertexes come from the frame list
			VectorCopy ( s_lerped[order[2]], point );

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;

			qglVertex3fv (point);

			order += 3;

		} while (--count);

		qglEnd ();
	}

	if (gl_state.stencil && gl_shadows->integer == 2)
		qglDisable(GL_STENCIL_TEST);
}


/*
** R_CullAliasModel
*/
static qboolean R_CullAliasModel( vec3_t bbox[8] )
{
	int i, f, aggregatemask = ~0;
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	vec3_t		mins, maxs;
	dmdl_t		*paliashdr;
	daliasframe_t *pframe, *poldframe;

	paliashdr = (dmdl_t *)currentmodel->extradata;
	pframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + paliashdr->ofs_frames +
									  currententity->frame * paliashdr->framesize);

	poldframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + paliashdr->ofs_frames +
									  currententity->oldframe * paliashdr->framesize);

	// compute axially aligned mins and maxs
	if ( pframe == poldframe )
	{
		VectorCopy(pframe->translate, mins);
		VectorMA(mins, 255, pframe->scale, maxs);
	}
	else
	{
		VectorCopy(pframe->translate, thismins);
		VectorMA(thismins, 255, pframe->scale, thismaxs);

		VectorCopy(poldframe->translate, oldmins);
		VectorMA(oldmins, 255, poldframe->scale, oldmaxs);

		mins[0] = min(thismins[0], oldmins[0]);
		mins[1] = min(thismins[1], oldmins[1]);
		mins[2] = min(thismins[2], oldmins[2]);

		maxs[0] = max(thismaxs[0], oldmaxs[0]);
		maxs[1] = max(thismaxs[1], oldmaxs[1]);
		maxs[2] = max(thismaxs[2], oldmaxs[2]);
	}

	// rotate the bounding box
	for ( i = 0; i < 8; i++ )
	{
		vec3_t tmp;
		int mask = 0;

		tmp[0] = ( i & 1 ) ? mins[0] : maxs[0];
		tmp[1] = ( i & 2 ) ? mins[1] : maxs[1];
		tmp[2] = ( i & 4 ) ? mins[2] : maxs[2];

		bbox[i][0] = currententity->axis[0][0]*tmp[0] + currententity->axis[1][0]*tmp[1] + currententity->axis[2][0]*tmp[2] + currententity->origin[0]; 
		bbox[i][1] = currententity->axis[0][1]*tmp[0] + currententity->axis[1][1]*tmp[1] + currententity->axis[2][1]*tmp[2] + currententity->origin[1]; 
		bbox[i][2] = currententity->axis[0][2]*tmp[0] + currententity->axis[1][2]*tmp[1] + currententity->axis[2][2]*tmp[2] + currententity->origin[2];

		for ( f = 0; f < 4; f++ ) {
			if ( DotProduct( frustum[f].normal, bbox[i] ) < frustum[f].dist )
				mask |= ( 1 << f );
		}

		aggregatemask &= mask;
	}

	if ( aggregatemask )
		return true;

	return false;
}

static void GL_SetShadeLight(void)
{
	int i;
	// get lighting information
	//
	// PMM - rewrote, reordered to handle new shells & mixing
	// PMM - 3.20 code .. replaced with original way of doing it to keep mod authors happy
	if ( currententity->flags & (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE) )
	{
		VectorClear (shadelight);
		if (currententity->flags & RF_SHELL_HALF_DAM)
			VectorSet(shadelight, 0.56f, 0.59f, 0.45f);

		if ( currententity->flags & RF_SHELL_DOUBLE )
			shadelight[0] = 0.9f, shadelight[1] = 0.7f;

		if ( currententity->flags & RF_SHELL_RED )
			shadelight[0] = 1.0f;
		if ( currententity->flags & RF_SHELL_GREEN )
			shadelight[1] = 1.0f;
		if ( currententity->flags & RF_SHELL_BLUE )
			shadelight[2] = 1.0f;
	}
	else if ( currententity->flags & RF_FULLBRIGHT )
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0f;
	}
	else
	{
		R_LightPoint (currententity->origin, shadelight);
		// player lighting hack for communication back to server, big hack!
		if ( currententity->flags & RF_WEAPONMODEL )
		{
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
		
		if ( gl_monolightmap->string[0] != '0' )
		{
			float s = shadelight[0];

			if (s < shadelight[1])
				s = shadelight[1];
			if (s < shadelight[2])
				s = shadelight[2];

			shadelight[0] = shadelight[1] = shadelight[2] = s;
		}
	}

	if ( currententity->flags & RF_MINLIGHT )
	{
		for (i=0 ; i<3 ; i++)
			if (shadelight[i] > 0.1f)
				break;
		if (i == 3)
			shadelight[0] = shadelight[1] = shadelight[2] = 0.1f;
	}

	if ( currententity->flags & RF_GLOW )
	{	// bonus items will pulse with time
		float	scale, min;

		scale = 0.1f * (float)sin(r_newrefdef.time*7);
		for (i=0 ; i<3 ; i++)
		{
			min = shadelight[i] * 0.8f;
			shadelight[i] += scale;
			if (shadelight[i] < min)
				shadelight[i] = min;
		}
	}

	if ( r_newrefdef.rdflags & RDF_IRGOGGLES && currententity->flags & RF_IR_VISIBLE)
	{
		shadelight[0] = 1.0f;
		shadelight[1] = shadelight[2] = 0.0f;
	}

	shadedots = r_avertexnormal_dots[((int)(currententity->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (void)
{
	dmdl_t		*paliashdr;
	vec3_t		bbox[8];
	image_t		*skin;

	paliashdr = (dmdl_t *)currentmodel->extradata;
	if ( ( currententity->frame >= paliashdr->num_frames ) || ( currententity->frame < 0 ) )
	{
		Com_DPrintf ( "R_DrawAliasModel %s: no such frame %d\n", currentmodel->name, currententity->frame);
		currententity->frame = 0;
	}
	if ( ( currententity->oldframe >= paliashdr->num_frames ) || ( currententity->oldframe < 0 ) )
	{
		Com_DPrintf ( "R_DrawAliasModel %s: no such oldframe %d\n", currentmodel->name, currententity->oldframe);
		currententity->oldframe = 0;
	}

	if (currententity->flags & RF_WEAPONMODEL)
	{
		if ( r_lefthand->integer == 2 )
			return;
	}
	else if ( R_CullAliasModel( bbox ) )
		return;


	c_alias_polys += paliashdr->num_tris;

	GL_SetShadeLight();

	// draw all the triangles
	if (currententity->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	qglPushMatrix ();

	if (currententity->flags & RF_CULLHACK)
	{
		qglFrontFace (GL_CW);
		R_RotateForEntity2 (currententity);
	}
	else
	{
		currententity->angles[PITCH] = -currententity->angles[PITCH];	// sigh.
		R_RotateForEntity (currententity);
		currententity->angles[PITCH] = -currententity->angles[PITCH];	// sigh.
	}

	// select skin
	if (currententity->skin)
		skin = currententity->skin;	// custom player skin
	else {
		if (currententity->skinnum >= MAX_MD2SKINS)
			skin = currentmodel->skins[0];
		else {
			skin = currentmodel->skins[currententity->skinnum];
			if (!skin)
				skin = currentmodel->skins[0];
		}
	}
	if (!skin)
		skin = r_notexture;	// fallback...

	// draw it
	GL_Bind(skin->texnum);

	GL_TexEnv( GL_MODULATE );

	if ( currententity->flags & RF_TRANSLUCENT )
		qglEnable(GL_BLEND);

	if ( !r_lerpmodels->integer )
		currententity->backlerp = 0;

	GL_DrawAliasFrameLerp (paliashdr, currententity->backlerp);

	if ( currententity->flags & RF_TRANSLUCENT )
		qglDisable(GL_BLEND);
	else if	(gl_celshading->integer)
		GL_DrawOutLine (paliashdr);

	GL_TexEnv( GL_REPLACE );

	if (currententity->flags & RF_CULLHACK)
		qglFrontFace (GL_CCW);

	if (currententity->flags & RF_DEPTHHACK)
		qglDepthRange (gldepthmin, gldepthmax);

	if (gl_shadows->integer && !(currententity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL)))
	{
		float an = currententity->angles[1]/180*M_PI;
		VectorSet(shadevector, (float)cos(-an), (float)sin(-an), 1);
		VectorNormalize (shadevector);

		qglRotatef (currententity->angles[2],  1, 0, 0);
		qglRotatef (-currententity->angles[0],  0, 1, 0);

		qglDisable (GL_TEXTURE_2D);
		qglEnable(GL_BLEND);
		qglColor4f (0,0,0,0.5);
		GL_DrawAliasShadow (paliashdr);
		qglEnable (GL_TEXTURE_2D);
		qglDisable(GL_BLEND);
	}

	qglPopMatrix ();

	qglColor4fv(colorWhite);
 
}


