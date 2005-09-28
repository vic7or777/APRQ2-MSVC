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
// gl_warp.c -- sky and water polygons

#include "gl_local.h"

static char		skyname[MAX_QPATH];
static float	skyrotate;
static vec3_t	skyaxis;
static image_t	*sky_images[6];


// speed up sin calculations - Ed
float	r_turbsin[] =
{
	#include "warpsin.h"
};
#define TURBSCALE (256.0 / M_TWOPI)
#define TURBSIN(f, s) r_turbsin[((int)(((f)*(s) + r_newrefdef.time) * TURBSCALE) & 255)]

/*
=============
EmitWaterPolys

Does a water warp
=============
*/
void EmitWaterPolys (const msurface_t *fa)
{
	vec3_t		wv;   // Water waves
	float		*v;
	int			i, nv;
	float		st[2];
	float		scroll = 0, rdt = r_newrefdef.time;


	if (fa->texinfo->flags & SURF_FLOWING)
		scroll = -64 * ( (r_newrefdef.time*0.5) - (int)(r_newrefdef.time*0.5) );

	v = fa->polys->verts[0];
	nv = fa->polys->numverts;

	qglBegin (GL_TRIANGLE_FAN);
	for (i=0 ; i<nv ; i++, v+=VERTEXSIZE)
	{
			st[0] = (v[3] + TURBSIN(v[4], 0.125) + scroll) * ONEDIV64;
			st[1] = (v[4] + TURBSIN(v[3], 0.125)) * ONEDIV64;
			qglTexCoord2fv (st);

			//=============== Water waves ============
			if (!(fa->texinfo->flags & SURF_FLOWING) && gl_waterwaves->value)
			{
				wv[0] =v[0];
				wv[1] =v[1];
				#if !id386
				wv[2] = v[2] + gl_waterwaves->value *sin(v[0]*0.025+r_newrefdef.time)*sin(v[2]*0.05+r_newrefdef.time)
						+ gl_waterwaves->value *sin(v[1]*0.025+r_newrefdef.time*2)*sin(v[2]*0.05+r_newrefdef.time);
				#else
				wv[2] = v[2] + gl_waterwaves->value *sin(v[0]*0.025+rdt)*sin(v[2]*0.05+rdt)
						+ gl_waterwaves->value *sin(v[1]*0.025+rdt*2)*sin(v[2]*0.05+rdt);
				#endif

				qglVertex3fv (wv);
			}
			else
			{
			//============= Water waves end. ==============
				qglVertex3fv (v);
			}
	}
	qglEnd ();
}


//===================================================================


static const vec3_t skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};

// 1 = s, 2 = t, 3 = 2048
static const int st_to_vec[6][3] =
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down
};

// s = [0]/[2], t = [1]/[2]
static const int vec_to_st[6][3] =
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}
};

static float	skymins[2][6], skymaxs[2][6];
static float	sky_min, sky_max;

static void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int		i,j;
	vec3_t	v = {0,0,0}, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	// decide which face it maps to
	for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
		VectorAdd (vp, v, v);

	VectorSet(av, fabs(v[0]), fabs(v[1]), fabs(v[2]));
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		if (dv < 0.001)
			continue;	// don't divide by zero

		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j -1] / dv;
		else
			s = vecs[j-1] / dv;

		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j -1] / dv;
		else
			t = vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

#define	ON_EPSILON		0.1			// point on plane side epsilon
#define	MAX_CLIP_VERTS	64

static void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	const float	*norm;
	float	*v;
	qboolean	front = false, back = false;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS-2)
		Com_Error (ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS");

	if (stage == 6)
	{	// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	norm = skyclip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)) );
	newc[0] = newc[1] = 0;

	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if ( (sides[i] == SIDE_ON) || (sides[i+1] == SIDE_ON) || (sides[i+1] == sides[i]) )
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

/*
=================
R_AddSkySurface
=================
*/
void R_AddSkySurface (const msurface_t *fa)
{
	int			i;
	vec3_t		verts[MAX_CLIP_VERTS];
	const glpoly_t	*p;

	// calculate vertex values for sky box
	p = fa->polys;

	for (i=0 ; i<p->numverts ; i++)
		VectorSubtract (p->verts[i], r_origin, verts[i]);

	ClipSkyPolygon (p->numverts, verts[0], 0);
}


/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox (void)
{
	int		i;

	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}


static void MakeSkyVec (float s, float t, int axis)
{
	vec3_t		v, b;
	int			j, k;

	b[0] = s * skydistance->integer;
	b[1] = t * skydistance->integer;
	b[2] = skydistance->integer;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
	}

	// avoid bilerp seam
	s = (s+1)*0.5;
	t = (t+1)*0.5;

	if (s < sky_min)
		s = sky_min;
	else if (s > sky_max)
		s = sky_max;
	if (t < sky_min)
		t = sky_min;
	else if (t > sky_max)
		t = sky_max;

	t = 1.0 - t;
	qglTexCoord2f (s, t);
	qglVertex3fv (v);
}

/*
==============
R_DrawSkyBox
==============
*/
static const int	skytexorder[6] = {0,2,1,3,4,5};
void R_DrawSkyBox (void)
{
	int		i;

	if (skyrotate)
	{	// check for no sky at all
		for (i=0 ; i<6 ; i++)
			if (skymins[0][i] < skymaxs[0][i] && skymins[1][i] < skymaxs[1][i])
				break;
		if (i == 6)
			return;		// nothing visible
	}

	qglPushMatrix ();

	qglTranslatef (r_origin[0], r_origin[1], r_origin[2]);
	qglRotatef (r_newrefdef.time * skyrotate, skyaxis[0], skyaxis[1], skyaxis[2]);

	for (i=0 ; i<6 ; i++)
	{
		if (skyrotate)
		{	// hack, forces full sky to draw when rotating
			skymins[0][i] =	skymins[1][i] = -1;
			skymaxs[0][i] =	skymaxs[1][i] = 1;
		}

		if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])
			continue;

		GL_Bind (sky_images[skytexorder[i]]->texnum);

		qglBegin (GL_QUADS);
		MakeSkyVec (skymins[0][i], skymins[1][i], i);
		MakeSkyVec (skymins[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymins[1][i], i);
		qglEnd ();
	}

	qglPopMatrix ();
}


/*
============
R_SetSky
============
*/
// 3dstudio environment map names
static const char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
void R_SetSky (const char *name, float rotate, vec3_t axis)
{
	int		i;
	char	pathname[MAX_QPATH];

	Q_strncpyz (skyname, name, sizeof(skyname));
	skyrotate = rotate;
	VectorCopy (axis, skyaxis);

	for (i=0 ; i<6 ; i++)
	{
		// chop down rotating skies for less memory
		if (gl_skymip->integer || skyrotate)
			gl_picmip->integer++;

		if ( qglColorTableEXT && gl_ext_palettedtexture->integer )
			Com_sprintf (pathname, sizeof(pathname), "env/%s%s.pcx", skyname, suf[i]);
		else
			Com_sprintf (pathname, sizeof(pathname), "env/%s%s.tga", skyname, suf[i]);

		sky_images[i] = GL_FindImage (pathname, it_sky);
		if (!sky_images[i])
			sky_images[i] = r_notexture;

		if (gl_skymip->integer || skyrotate)
		{	// take less memory
			gl_picmip->integer--;
			sky_min = 0.00390625f;
			sky_max = 0.99609375f;
		}
		else	
		{
			sky_min = 0.001953125f;
			sky_max = 0.998046875f;
		}
	}
}

//Water caustics
void EmitCausticPolys (const msurface_t *fa)
{
	float		*v;
	int			i;
	float		txm, tym;


	txm = cos (r_newrefdef.time*0.3) * 0.3;
	tym = sin (r_newrefdef.time*-0.3) * 0.6;

	v = fa->polys->verts[0];	

	GL_SelectTexture(QGL_TEXTURE1);
	qglDisable(GL_TEXTURE_2D);
	GL_SelectTexture(QGL_TEXTURE0);
	qglEnable(GL_BLEND);

    qglBlendFunc(GL_ZERO, GL_SRC_COLOR);

	qglColor4f (1, 1, 1, 0.275f);

	GL_Bind(r_caustictexture->texnum);
	qglBegin (GL_POLYGON);

	for (i=0 ; i<fa->polys->numverts; i++, v+= VERTEXSIZE)
	{
		qglTexCoord2f (v[3]+txm, v[4]+tym);
		qglVertex3fv (v);

	}
	qglEnd ();


	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglColor4f (1,1,1,1);
	qglDisable(GL_BLEND);
	GL_SelectTexture(QGL_TEXTURE1);
	qglEnable(GL_TEXTURE_2D);
	GL_SelectTexture(QGL_TEXTURE0);
}
