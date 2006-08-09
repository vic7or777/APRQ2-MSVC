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
//static vec3_t	shadevector;
static float	shadelight[3];

extern  vec3_t lightspot;

static vec3_t	tempVertexArray[MD3_MAX_MESHES][MD3_MAX_VERTS];
static vec3_t	default_lightdir = { 0.587785f, 0.425325f, 0.688191f }; // jitskm

extern vec2_t	texCoordArray[MAX_ARRAY];
extern vec4_t	colorArray[MAX_ARRAY];

/*============================
Cellshading from q2max
Discoloda's cellshading outline routine
=============================*/
#define OUTLINEDROPOFF 700.0f //distance away for it to stop
static void GL_DrawOutLine (const maliasmodel_t *paliashdr) 
{
	int		meshNum;
	float	scale;
	maliasmesh_t *mesh;
 
	scale = (float)Distance( r_newrefdef.vieworg, currententity->origin)*(r_newrefdef.fov_y/90.0f);
	scale = (OUTLINEDROPOFF-scale) / OUTLINEDROPOFF;

	if( scale <= 0 || scale >= 1)
		return;

	if( gl_celshading_width->value > 10)
		Cvar_Set("gl_celshading_width", "10");
	else if( gl_celshading_width->value < 0.5f)
		Cvar_Set("gl_celshading_width", "0.5");

	qglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	qglCullFace (GL_BACK);
	qglEnable(GL_BLEND);
	qglColor4f (0, 0, 0, scale);
	qglLineWidth(gl_celshading_width->value * scale);

	for (mesh = paliashdr->meshes, meshNum = 0; meshNum < paliashdr->nummeshes; meshNum++, mesh++)
	{
		qglVertexPointer (3, GL_FLOAT, sizeof(tempVertexArray[meshNum][0]), tempVertexArray[meshNum][0]);

		if(gl_state.compiledVertexArray) {
			qglLockArraysEXT(0, mesh->numverts);
			qglDrawElements (GL_TRIANGLES, mesh->numtris*3, GL_UNSIGNED_INT, mesh->indexes);
			qglUnlockArraysEXT ();
		} else {
			qglDrawElements (GL_TRIANGLES, mesh->numtris*3, GL_UNSIGNED_INT, mesh->indexes);
		}
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
static void GL_DrawAliasFrameLerp (const maliasmodel_t *paliashdr)
{
	maliasframe_t	*frame, *oldframe;
	maliasvertex_t	*ov, *v;
	int				i, meshNum;
	float			l, backlerp, frontlerp, alpha;
	vec3_t			move, delta, frontv, backv;
	vec3_t			tempNormalsArray[MD3_MAX_VERTS];
	maliasmesh_t *mesh;
	image_t		*skin;


	frame = paliashdr->frames + currententity->frame;
	oldframe = paliashdr->frames + currententity->oldframe;

	backlerp = currententity->backlerp;
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
	else
		alpha = 1.0f;

	for(mesh = paliashdr->meshes, meshNum = 0; meshNum < paliashdr->nummeshes; meshNum++, mesh++)
	{
		v = mesh->vertexes + currententity->frame*mesh->numverts;
		ov = mesh->vertexes + currententity->oldframe*mesh->numverts;

		c_alias_polys += mesh->numtris;

		qglVertexPointer (3, GL_FLOAT, sizeof(tempVertexArray[meshNum][0]), tempVertexArray[meshNum][0]);

		if(currententity->flags & (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
		{
			float scale = POWERSUIT_SCALE;

			if (currententity->flags & RF_WEAPONMODEL)
				scale = (gl_shelleffect->integer) ? 0.5f : 0.66f;

			for ( i = 0; i < mesh->numverts; i++, v++, ov++ )
			{
				tempNormalsArray[i][0] = v->normal[0] + (ov->normal[0] - v->normal[0])*backlerp;
				tempNormalsArray[i][1] = v->normal[1] + (ov->normal[1] - v->normal[1])*backlerp;
				tempNormalsArray[i][2] = v->normal[2] + (ov->normal[2] - v->normal[2])*backlerp;

				tempVertexArray[meshNum][i][0] = move[0] + ov->point[0]*backv[0] + v->point[0]*frontv[0] + tempNormalsArray[i][0] * scale;
				tempVertexArray[meshNum][i][1] = move[1] + ov->point[1]*backv[1] + v->point[1]*frontv[1] + tempNormalsArray[i][1] * scale;
				tempVertexArray[meshNum][i][2] = move[2] + ov->point[2]*backv[2] + v->point[2]*frontv[2] + tempNormalsArray[i][2] * scale;
			}

			if(gl_shelleffect->integer) {
				float time = (float)sin(r_newrefdef.time*0.3f);

				for(i = 0; i < mesh->numverts; i++) {
					texCoordArray[i][0] = mesh->stcoords[i][0] - time;
					texCoordArray[i][1] = mesh->stcoords[i][1] - time;
				}

				qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
				qglTexCoordPointer (2, GL_FLOAT, sizeof(texCoordArray[0]), texCoordArray[0]);

				GL_Bind(r_shelltexture->texnum);
				qglColor4f( shadelight[0], shadelight[1], shadelight[2], 1);

				qglBlendFunc (GL_SRC_ALPHA, GL_ONE);
				if(gl_state.compiledVertexArray) {
					qglLockArraysEXT(0, mesh->numverts);
					qglDrawElements (GL_TRIANGLES, mesh->numtris*3, GL_UNSIGNED_INT, mesh->indexes);
					qglUnlockArraysEXT ();
				} else {
					qglDrawElements (GL_TRIANGLES, mesh->numtris*3, GL_UNSIGNED_INT, mesh->indexes);
				}
				qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
				qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			} else {
				qglDisable( GL_TEXTURE_2D );
				qglColor4f( shadelight[0], shadelight[1], shadelight[2], alpha);

				if(gl_state.compiledVertexArray) {
					qglLockArraysEXT(0, mesh->numverts);
					qglDrawElements (GL_TRIANGLES, mesh->numtris*3, GL_UNSIGNED_INT, mesh->indexes);
					qglUnlockArraysEXT ();
				} else {
					qglDrawElements (GL_TRIANGLES, mesh->numtris*3, GL_UNSIGNED_INT, mesh->indexes);
				}
				qglEnable( GL_TEXTURE_2D );
			}
			continue;
		}

		if( currententity->frame == currententity->oldframe ) {
			for ( i = 0; i < mesh->numverts; i++, v++, ov++ ) {
				VectorCopy(v->normal, tempNormalsArray[i]);
				tempVertexArray[meshNum][i][0] = move[0] + v->point[0]*frame->scale[0];
				tempVertexArray[meshNum][i][1] = move[1] + v->point[1]*frame->scale[1];
				tempVertexArray[meshNum][i][2] = move[2] + v->point[2]*frame->scale[2];

				l = (DotProduct(tempNormalsArray[i], default_lightdir) + 1.0f) / 2.0f * 1.4f + 0.6f;
				colorArray[i][0] = l * shadelight[0];
				colorArray[i][1] = l * shadelight[1];
				colorArray[i][2] = l * shadelight[2];
				colorArray[i][3] = alpha;
			}
		} else {
			for ( i = 0; i < mesh->numverts; i++, v++, ov++ ) {
				tempNormalsArray[i][0] = v->normal[0] + (ov->normal[0] - v->normal[0])*backlerp;
				tempNormalsArray[i][1] = v->normal[1] + (ov->normal[1] - v->normal[1])*backlerp;
				tempNormalsArray[i][2] = v->normal[2] + (ov->normal[2] - v->normal[2])*backlerp;

				tempVertexArray[meshNum][i][0] = move[0] + ov->point[0]*backv[0] + v->point[0]*frontv[0];
				tempVertexArray[meshNum][i][1] = move[1] + ov->point[1]*backv[1] + v->point[1]*frontv[1];
				tempVertexArray[meshNum][i][2] = move[2] + ov->point[2]*backv[2] + v->point[2]*frontv[2];

				l = (DotProduct(tempNormalsArray[i], default_lightdir) + 1.0f) * 0.75f + 0.6f;
				colorArray[i][0] = l * shadelight[0];
				colorArray[i][1] = l * shadelight[1];
				colorArray[i][2] = l * shadelight[2];
				colorArray[i][3] = alpha;
			}
		}

		// select skin
		if (currententity->skin)
			skin = currententity->skin;	// custom player skin
		else {
			if (currententity->skinnum >= mesh->numskins)
				skin = mesh->skins[0].image;
			else {
				skin = mesh->skins[currententity->skinnum].image;
				if (!skin)
					skin = mesh->skins[0].image;
			}
		}
		if (!skin)
			skin = r_notexture;	// fallback...

		// draw it
		GL_Bind(skin->texnum);

		qglEnableClientState (GL_COLOR_ARRAY);
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		
		qglTexCoordPointer (2, GL_FLOAT, sizeof(mesh->stcoords[0]), mesh->stcoords[0]);

		if(gl_state.compiledVertexArray) {
			qglLockArraysEXT(0, mesh->numverts);
			qglDrawElements (GL_TRIANGLES, mesh->numtris*3, GL_UNSIGNED_INT, mesh->indexes);
			qglUnlockArraysEXT ();
		} else {
			qglDrawElements (GL_TRIANGLES, mesh->numtris*3, GL_UNSIGNED_INT, mesh->indexes);
		}

		qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
		qglDisableClientState(GL_COLOR_ARRAY);
	}
}

/*
=============
GL_DrawAliasShadow
=============
*/
static void GL_DrawAliasShadow (const maliasmodel_t *paliashdr)
{
	float	height = 0, lheight;
	int		meshNum, i;
	maliasmesh_t *mesh;
	//float an = currententity->angles[1]/180*M_PI;
	
	//VectorSet(shadevector, (float)cos(-an), (float)sin(-an), 1);
	//VectorNormalize (shadevector);


	lheight = currententity->origin[2] - lightspot[2];
	height = -lheight + 1.0f;

	if (gl_state.stencil && gl_shadows->integer == 2) {
		height = -lheight + 0.1f;
		qglEnable( GL_STENCIL_TEST );
		qglStencilFunc( GL_EQUAL, 128, 0xFF );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
	}

	for (mesh = paliashdr->meshes, meshNum = 0; meshNum < paliashdr->nummeshes; meshNum++, mesh++)
	{
		for(i=0; i < mesh->numverts; i++)
			tempVertexArray[meshNum][i][2] = height;
		
		qglVertexPointer (3, GL_FLOAT, sizeof(tempVertexArray[meshNum][0]), tempVertexArray[meshNum][0]);

		if(gl_state.compiledVertexArray) {
			qglLockArraysEXT(0, mesh->numverts);
			qglDrawElements (GL_TRIANGLES, mesh->numtris*3, GL_UNSIGNED_INT, mesh->indexes);
			qglUnlockArraysEXT ();
		} else {
			qglDrawElements (GL_TRIANGLES, mesh->numtris*3, GL_UNSIGNED_INT, mesh->indexes);
		}
	}

	if (gl_state.stencil && gl_shadows->integer == 2)
		qglDisable(GL_STENCIL_TEST);
}


/*
** R_CullAliasModel
*/
static qboolean R_CullAliasModel( const maliasmodel_t *paliashdr, vec3_t bbox[8])
{
	int i, f, aggregatemask = ~0, mask;
	float		*thismins, *oldmins, *thismaxs, *oldmaxs;
	vec3_t		mins, maxs, tmp;
	maliasframe_t *pframe, *poldframe;

	pframe = paliashdr->frames + currententity->frame;
	poldframe = paliashdr->frames + currententity->oldframe;


	// compute axially aligned mins and maxs
	if ( pframe == poldframe )
	{
		VectorCopy(pframe->mins, mins);
		VectorCopy(pframe->maxs, maxs);
	}
	else
	{
		thismins = pframe->mins;
		thismaxs = pframe->maxs;

		oldmins = poldframe->mins;
		oldmaxs = poldframe->maxs;

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
		tmp[0] = ( i & 1 ) ? mins[0] : maxs[0];
		tmp[1] = ( i & 2 ) ? mins[1] : maxs[1];
		tmp[2] = ( i & 4 ) ? mins[2] : maxs[2];

		bbox[i][0] = currententity->axis[0][0]*tmp[0] + currententity->axis[1][0]*tmp[1] + currententity->axis[2][0]*tmp[2] + currententity->origin[0]; 
		bbox[i][1] = currententity->axis[0][1]*tmp[0] + currententity->axis[1][1]*tmp[1] + currententity->axis[2][1]*tmp[2] + currententity->origin[1]; 
		bbox[i][2] = currententity->axis[0][2]*tmp[0] + currententity->axis[1][2]*tmp[1] + currententity->axis[2][2]*tmp[2] + currententity->origin[2];

		mask = 0;
		for ( f = 0; f < 4; f++ ) {
			if (DotProduct(frustum[f].normal, bbox[i]) < frustum[f].dist)
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
		if (currententity->flags & RF_SHELL_HALF_DAM)
			VectorSet(shadelight, 0.56f, 0.59f, 0.45f);
		else
			VectorClear (shadelight);

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
					r_lightlevel->value = 150.0f*shadelight[0];
				else
					r_lightlevel->value = 150.0f*shadelight[2];
			}
			else
			{
				if (shadelight[1] > shadelight[2])
					r_lightlevel->value = 150.0f*shadelight[1];
				else
					r_lightlevel->value = 150.0f*shadelight[2];
			}

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
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (void)
{
	maliasmodel_t		*paliashdr;
	vec3_t	bbox[8];

	paliashdr = (maliasmodel_t *)currentmodel->extradata;
	if ( ( currententity->frame >= paliashdr->numframes ) || ( currententity->frame < 0 ) )
	{
		Com_DPrintf ( "R_DrawAliasModel %s: no such frame %d\n", currentmodel->name, currententity->frame);
		currententity->frame = 0;
	}
	if ( ( currententity->oldframe >= paliashdr->numframes ) || ( currententity->oldframe < 0 ) )
	{
		Com_DPrintf ( "R_DrawAliasModel %s: no such oldframe %d\n", currentmodel->name, currententity->oldframe);
		currententity->oldframe = 0;
	}

	if (currententity->flags & RF_WEAPONMODEL)
	{
		if ( r_lefthand->integer == 2 )
			return;
	}
	else if ( R_CullAliasModel( paliashdr, bbox ) )
		return;


	GL_SetShadeLight();

	// draw all the triangles
	if (currententity->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));

	qglPushMatrix ();

	qglEnableClientState (GL_VERTEX_ARRAY);

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

	GL_TexEnv( GL_MODULATE );

	if ( currententity->flags & RF_TRANSLUCENT )
		qglEnable(GL_BLEND);

	if ( !r_lerpmodels->integer )
		currententity->backlerp = 0;

	GL_DrawAliasFrameLerp(paliashdr);

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
		qglRotatef (currententity->angles[2],  1, 0, 0);
		qglRotatef (-currententity->angles[0],  0, 1, 0);

		qglDisable (GL_TEXTURE_2D);
		qglEnable(GL_BLEND);
		qglColor4f (0,0,0,0.5f);
		GL_DrawAliasShadow (paliashdr);
		qglEnable (GL_TEXTURE_2D);
		qglDisable(GL_BLEND);
	}

	qglDisableClientState (GL_VERTEX_ARRAY);

	qglPopMatrix ();

	qglColor4fv(colorWhite);
 }


