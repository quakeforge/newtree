/*
	r_main.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <math.h>

#include "bothdefs.h"
#include "cl_cam.h"
#include "cl_main.h"
#include "console.h"
#include "cmd.h"
#include "locs.h"
#include "mathlib.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "screen.h"
#include "sound.h"
#include "sys.h"
#include "view.h"

//define    PASSAGES

void       *colormap;
vec3_t      viewlightvec;
alight_t    r_viewlighting = { 128, 192, viewlightvec };
float       r_time1;
int         r_numallocatededges;
qboolean    r_drawpolys;
qboolean    r_drawculledpolys;
qboolean    r_worldpolysbacktofront;
qboolean    r_recursiveaffinetriangles = true;
int         r_pixbytes = 1;
float       r_aliasuvscale = 1.0;
int         r_outofsurfaces;
int         r_outofedges;

qboolean    r_dowarp, r_dowarpold, r_viewchanged;

int         numbtofpolys;
btofpoly_t *pbtofpolys;
mvertex_t  *r_pcurrentvertbase;

int         c_surf;
int         r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
qboolean    r_surfsonstack;
int         r_clipflags;

byte       *r_warpbuffer;

byte       *r_stack_start;

qboolean    r_fov_greater_than_90;

entity_t    r_worldentity;

mplane_t    frustum[4];

//
// view origin
//
vec3_t      vup, base_vup;
vec3_t      vpn, base_vpn;
vec3_t      vright, base_vright;
vec3_t      r_origin;

//
// screen size info
//
refdef_t    r_refdef;
float       xcenter, ycenter;
float       xscale, yscale;
float       xscaleinv, yscaleinv;
float       xscaleshrink, yscaleshrink;
float       aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int         screenwidth;

float       pixelAspect;
float       screenAspect;
float       verticalFieldOfView;
float       xOrigin, yOrigin;

mplane_t    screenedge[4];

//
// refresh flags
//
int         r_framecount = 1;			// so frame counts initialized to 0

										// don't match
int         r_visframecount;
int         d_spanpixcount;
int         r_polycount;
int         r_drawnpolycount;
int         r_wholepolycount;

int        *pfrustum_indexes[4];
int         r_frustum_indexes[4 * 6];

int         reinit_surfcache = 1;		// if 1, surface cache is currently

										// empty and
								// must be reinitialized for current cache
								// size

mleaf_t    *r_viewleaf, *r_oldviewleaf;

float       r_aliastransition, r_resfudge;

int         d_lightstylevalue[256];		// 8.8 fraction of base light value

float       dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float       se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;

void        R_MarkLeaves (void);

cvar_t     *r_draworder;
cvar_t     *r_speeds;
cvar_t     *r_timegraph;
cvar_t     *r_netgraph;
cvar_t     *r_zgraph;
cvar_t     *r_graphheight;
cvar_t     *r_clearcolor;
cvar_t     *r_waterwarp;
cvar_t     *r_drawentities;
cvar_t     *r_drawviewmodel;
cvar_t     *r_particles;
cvar_t     *r_aliasstats;
cvar_t     *r_dspeeds;
cvar_t     *r_drawflat;
cvar_t     *r_ambient;
cvar_t     *r_reportsurfout;
cvar_t     *r_maxsurfs;
cvar_t     *r_numsurfs;
cvar_t     *r_reportedgeout;
cvar_t     *r_maxedges;
cvar_t     *r_numedges;
cvar_t     *r_aliastransbase;
cvar_t     *r_aliastransadj;

cvar_t     *gl_dlight_lightmap;
cvar_t     *gl_dlight_polyblend;
cvar_t     *gl_sky_divide;

extern cvar_t *scr_fov;

void        CreatePassages (void);
void        SetVisibilityByPassages (void);

void        R_NetGraph (void);
void        R_ZGraph (void);

/*
	R_Textures_Init
*/
void
R_Textures_Init (void)
{
	int         x, y, m;
	byte       *dest;

// create a simple checkerboard texture for the default
	r_notexture_mip =
		Hunk_AllocName (sizeof (texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2,
						"notexture");

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof (texture_t);

	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for (m = 0; m < 4; m++) {
		dest = (byte *) r_notexture_mip + r_notexture_mip->offsets[m];
		for (y = 0; y < (16 >> m); y++)
			for (x = 0; x < (16 >> m); x++) {
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}
}

void        R_LoadSky_f (void);

/*
	R_Init
*/
void
R_Init (void)
{
	int         dummy;

	allowskybox = false;				// server decides this  --KB

	// get stack position so we can guess if we are going to overflow
	r_stack_start = (byte *) & dummy;

	R_InitTurb ();

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f, "Tests the current refresh rate for the current location");
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f, "Load a pointfile to determine map leaks");
	Cmd_AddCommand ("loadsky", R_LoadSky_f, "Load a skybox");

	Cvar_SetValue (r_maxedges, (float) NUMSTACKEDGES);
	Cvar_SetValue (r_maxsurfs, (float) NUMSTACKSURFACES);

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
		view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
		view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	R_InitParticles ();

// TODO: collect 386-specific code in one place
#ifdef USE_INTEL_ASM
	Sys_MakeCodeWriteable ((long) R_EdgeCodeStart,
						   (long) R_EdgeCodeEnd - (long) R_EdgeCodeStart);
#endif // USE_INTEL_ASM

	D_Init ();
}

void
R_Init_Cvars (void)
{
	D_Init_Cvars ();

	r_draworder = Cvar_Get ("r_draworder", "0", CVAR_NONE, NULL, "Toggles drawing order");
	r_speeds = Cvar_Get ("r_speeds", "0", CVAR_NONE, NULL, "Toggles the displaying of drawing time and"
		"statistics of what is currently being viewed");
	r_timegraph = Cvar_Get ("r_timegraph", "0", CVAR_NONE, NULL, "Toggle the display of a performance graph");
	r_netgraph = Cvar_Get ("r_netgraph", "0", CVAR_NONE, NULL, "Toggle the display of a graph showing network performance");
	r_zgraph = Cvar_Get ("r_zgraph", "0", CVAR_NONE, NULL, "Toggle the graph that reports the changes of z-axis position");
	r_graphheight = Cvar_Get ("r_graphheight", "15", CVAR_NONE, NULL, "Set the number of lines displayed in the various graphs");
	r_drawflat = Cvar_Get ("r_drawflat", "0", CVAR_NONE, NULL, "Toggles the drawing of textures");
	r_ambient = Cvar_Get ("r_ambient", "0", CVAR_NONE, NULL, "Determines the ambient lighting for a level");
	r_clearcolor = Cvar_Get ("r_clearcolor", "2", CVAR_NONE, NULL, "This sets the color for areas outside of the current map");
	r_waterwarp = Cvar_Get ("r_waterwarp", "1", CVAR_NONE, NULL, "Toggles whether surfaces are warped in a liquid.");
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_NONE, NULL, "Toggles the drawing of entities.");
	r_drawviewmodel = Cvar_Get ("r_drawviewmodel", "1", CVAR_ARCHIVE, NULL, "Toggles the drawing of your weapon");
	r_particles = Cvar_Get ("r_particles", "1", CVAR_ARCHIVE, NULL, "Toggles drawing of particles.");
	r_aliasstats = Cvar_Get ("r_polymodelstats", "0", CVAR_NONE, NULL, "Toggles the displays of number of polygon models current being viewed");
	r_dspeeds = Cvar_Get ("r_dspeeds", "0", CVAR_NONE, NULL, "Toggles the display of drawing speed information");
	r_reportsurfout = Cvar_Get ("r_reportsurfout", "0", CVAR_NONE, NULL, "Toggle the display of how many surfaces where not displayed");
	r_maxsurfs = Cvar_Get ("r_maxsurfs", "0", CVAR_NONE, NULL, "Sets the maximum number of surfaces");
	r_numsurfs = Cvar_Get ("r_numsurfs", "0", CVAR_NONE, NULL, "Toggles the displaying of number of surfaces currently being viewed");
	r_reportedgeout = Cvar_Get ("r_reportedgeout", "0", CVAR_NONE, NULL, "Toggle the display of how many edges where not displayed");
	r_maxedges = Cvar_Get ("r_maxedges", "0", CVAR_NONE, NULL, "Sets the maximum number of surfaces");
	r_numedges = Cvar_Get ("r_numedges", "0", CVAR_NONE, NULL, "Toggles the displaying of number of edges currently being viewed");
	r_aliastransbase = Cvar_Get ("r_aliastransbase", "200", CVAR_NONE, NULL, "Determines how much of an alias model is clipped away and how much is viewable");
	r_aliastransadj = Cvar_Get ("r_aliastransadj", "100", CVAR_NONE, NULL, "Determines how much of an alias model is clipped away and how much is viewable.");
	gl_sky_divide = Cvar_Get ("gl_sky_divide", "1", CVAR_ARCHIVE, NULL,
		"subdivide sky polys");
}

/*
	R_NewMap
*/
void
R_NewMap (void)
{
	int         i;

	memset (&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles ();

	r_cnumsurfs = r_maxsurfs->int_val;

	if (r_cnumsurfs <= MINSURFACES)
		r_cnumsurfs = MINSURFACES;

	if (r_cnumsurfs > NUMSTACKSURFACES) {
		surfaces = Hunk_AllocName (r_cnumsurfs * sizeof (surf_t), "surfaces");

		surface_p = surfaces;
		surf_max = &surfaces[r_cnumsurfs];
		r_surfsonstack = false;
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
		R_SurfacePatch ();
	} else {
		r_surfsonstack = true;
	}

	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;

	r_numallocatededges = r_maxedges->int_val;

	if (r_numallocatededges < MINEDGES)
		r_numallocatededges = MINEDGES;

	if (r_numallocatededges <= NUMSTACKEDGES) {
		auxedges = NULL;
	} else {
		auxedges = Hunk_AllocName (r_numallocatededges * sizeof (edge_t),
								   "edges");
	}

	r_dowarpold = false;
	r_viewchanged = false;
}


/*
	R_SetVrect
*/
void
R_SetVrect (vrect_t *pvrectin, vrect_t *pvrect, int lineadj)
{
	int         h;
	float       size;
	qboolean    full = false;

	if (scr_viewsize->int_val >= 100) {
		size = 100.0;
		full = true;
	} else
		size = scr_viewsize->int_val;

	if (cl.intermission) {
		full = true;
		size = 100.0;
		lineadj = 0;
	}
	size /= 100.0;

	if (!cl_sbar->int_val && full)
		h = pvrectin->height;
	else
		h = pvrectin->height - lineadj;

//  h = (!cl_sbar->int_val && size==1.0) ? pvrectin->height : (pvrectin->height - lineadj);
//  h = pvrectin->height - lineadj;
	if (full)
		pvrect->width = pvrectin->width;
	else
		pvrect->width = pvrectin->width * size;
	if (pvrect->width < 96) {
		size = 96.0 / pvrectin->width;
		pvrect->width = 96;				// min for icons
	}
	pvrect->width &= ~7;
	pvrect->height = pvrectin->height * size;
	if (cl_sbar->int_val || !full) {
		if (pvrect->height > pvrectin->height - lineadj)
			pvrect->height = pvrectin->height - lineadj;
	} else if (pvrect->height > pvrectin->height)
		pvrect->height = pvrectin->height;

	pvrect->height &= ~1;

	pvrect->x = (pvrectin->width - pvrect->width) / 2;
	if (full)
		pvrect->y = 0;
	else
		pvrect->y = (h - pvrect->height) / 2;
}


/*
	R_ViewChanged

	Called every time the vid structure or r_refdef changes.
	Guaranteed to be called before the first refresh
*/
void
R_ViewChanged (vrect_t *pvrect, int lineadj, float aspect)
{
	int         i;
	float       res_scale;

	r_viewchanged = true;

	R_SetVrect (pvrect, &r_refdef.vrect, lineadj);

	r_refdef.horizontalFieldOfView = 2.0 * tan (r_refdef.fov_x / 360 * M_PI);
	r_refdef.fvrectx = (float) r_refdef.vrect.x;
	r_refdef.fvrectx_adj = (float) r_refdef.vrect.x - 0.5;
	r_refdef.vrect_x_adj_shift20 = (r_refdef.vrect.x << 20) + (1 << 19) - 1;
	r_refdef.fvrecty = (float) r_refdef.vrect.y;
	r_refdef.fvrecty_adj = (float) r_refdef.vrect.y - 0.5;
	r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
	r_refdef.vrectright_adj_shift20 =
		(r_refdef.vrectright << 20) + (1 << 19) - 1;
	r_refdef.fvrectright = (float) r_refdef.vrectright;
	r_refdef.fvrectright_adj = (float) r_refdef.vrectright - 0.5;
	r_refdef.vrectrightedge = (float) r_refdef.vrectright - 0.99;
	r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
	r_refdef.fvrectbottom = (float) r_refdef.vrectbottom;
	r_refdef.fvrectbottom_adj = (float) r_refdef.vrectbottom - 0.5;

	r_refdef.aliasvrect.x = (int) (r_refdef.vrect.x * r_aliasuvscale);
	r_refdef.aliasvrect.y = (int) (r_refdef.vrect.y * r_aliasuvscale);
	r_refdef.aliasvrect.width = (int) (r_refdef.vrect.width * r_aliasuvscale);
	r_refdef.aliasvrect.height = (int) (r_refdef.vrect.height * r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x +
		r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y +
		r_refdef.aliasvrect.height;

	pixelAspect = aspect;
	xOrigin = r_refdef.xOrigin;
	yOrigin = r_refdef.yOrigin;

	screenAspect = r_refdef.vrect.width * pixelAspect / r_refdef.vrect.height;
// 320*200 1.0 pixelAspect = 1.6 screenAspect
// 320*240 1.0 pixelAspect = 1.3333 screenAspect
// proper 320*200 pixelAspect = 0.8333333

	verticalFieldOfView = r_refdef.horizontalFieldOfView / screenAspect;

// values for perspective projection
// if math were exact, the values would range from 0.5 to to range+0.5
// hopefully they wll be in the 0.000001 to range+.999999 and truncate
// the polygon rasterization will never render in the first row or column
// but will definately render in the [range] row and column, so adjust the
// buffer origin to get an exact edge to edge fill
	xcenter = ((float) r_refdef.vrect.width * XCENTERING) +
		r_refdef.vrect.x - 0.5;
	aliasxcenter = xcenter * r_aliasuvscale;
	ycenter = ((float) r_refdef.vrect.height * YCENTERING) +
		r_refdef.vrect.y - 0.5;
	aliasycenter = ycenter * r_aliasuvscale;

	xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	aliasxscale = xscale * r_aliasuvscale;
	xscaleinv = 1.0 / xscale;
	yscale = xscale * pixelAspect;
	aliasyscale = yscale * r_aliasuvscale;
	yscaleinv = 1.0 / yscale;
	xscaleshrink = (r_refdef.vrect.width - 6) / r_refdef.horizontalFieldOfView;
	yscaleshrink = xscaleshrink * pixelAspect;

// left side clip
	screenedge[0].normal[0] = -1.0 / (xOrigin * r_refdef.horizontalFieldOfView);
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[0].type = PLANE_ANYZ;

// right side clip
	screenedge[1].normal[0] =
		1.0 / ((1.0 - xOrigin) * r_refdef.horizontalFieldOfView);
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[1].type = PLANE_ANYZ;

// top side clip
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = -1.0 / (yOrigin * verticalFieldOfView);
	screenedge[2].normal[2] = 1;
	screenedge[2].type = PLANE_ANYZ;

// bottom side clip
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = 1.0 / ((1.0 - yOrigin) * verticalFieldOfView);
	screenedge[3].normal[2] = 1;
	screenedge[3].type = PLANE_ANYZ;

	for (i = 0; i < 4; i++)
		VectorNormalize (screenedge[i].normal);

	res_scale = sqrt ((double) (r_refdef.vrect.width * r_refdef.vrect.height) /
					  (320.0 * 152.0)) * (2.0 / r_refdef.horizontalFieldOfView);
	r_aliastransition = r_aliastransbase->value * res_scale;
	r_resfudge = r_aliastransadj->value * res_scale;

	if (scr_fov->value <= 90.0)
		r_fov_greater_than_90 = false;
	else
		r_fov_greater_than_90 = true;

// TODO: collect 386-specific code in one place
#ifdef USE_INTEL_ASM
	if (r_pixbytes == 1) {
		Sys_MakeCodeWriteable ((long) R_Surf8Start,
							   (long) R_Surf8End - (long) R_Surf8Start);
		colormap = vid.colormap;
		R_Surf8Patch ();
	} else {
		Sys_MakeCodeWriteable ((long) R_Surf16Start,
							   (long) R_Surf16End - (long) R_Surf16Start);
		colormap = vid.colormap16;
		R_Surf16Patch ();
	}
#endif // USE_INTEL_ASM

	D_ViewChanged ();
}


/*
	R_MarkLeaves
*/
void
R_MarkLeaves (void)
{
	byte       *vis;
	mnode_t    *node;
	int         i;

	if (r_oldviewleaf == r_viewleaf)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	for (i = 0; i < cl.worldmodel->numleafs; i++) {
		if (vis[i >> 3] & (1 << (i & 7))) {
			node = (mnode_t *) &cl.worldmodel->leafs[i + 1];
			do {
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

 /*
 =============
 R_ShowNearestLoc
 =============
 */
static void
R_ShowNearestLoc (void)
{
	location_t *nearloc;
	vec3_t trueloc;
	dlight_t   *dl;
	
	if (r_drawentities->int_val)
		return;
	nearloc = locs_find (cl.simorg);
	if (nearloc) {
		dl = CL_AllocDlight (4096);
		VectorCopy (nearloc->loc, dl->origin);
		dl->radius = 200;
		dl->die = cl.time + 0.1;
		dl->color[1]=1;
		
		VectorCopy(nearloc->loc,trueloc);
		R_RunParticleEffect(trueloc,252,10);
	}
}
			
/*
	R_DrawEntitiesOnList
*/
void
R_DrawEntitiesOnList (void)
{
	int         i, j;
	int         lnum;
	alight_t    lighting;

// FIXME: remove and do real lighting
	float       lightvec[3] = { -1, 0, 0 };
	vec3_t      dist;
	float       add;

	if (!r_drawentities->int_val) {
		R_ShowNearestLoc();
		return;
	}

	for (i = 0; i < cl_numvisedicts; i++) {
		currententity = cl_visedicts[i];

		switch (currententity->model->type) {
			case mod_sprite:
				VectorCopy (currententity->origin, r_entorigin);
				VectorSubtract (r_origin, r_entorigin, modelorg);
				R_DrawSprite ();
				break;

			case mod_alias:
				VectorCopy (currententity->origin, r_entorigin);
				VectorSubtract (r_origin, r_entorigin, modelorg);

				// see if the bounding box lets us trivially reject, also
				// sets
				// trivial accept status
				if (R_AliasCheckBBox ()) {
					j = R_LightPoint (currententity->origin);

					lighting.ambientlight = j;
					lighting.shadelight = j;

					lighting.plightvec = lightvec;

					for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
						if (cl_dlights[lnum].die >= cl.time) {
							VectorSubtract (currententity->origin,
											cl_dlights[lnum].origin, dist);
							add = cl_dlights[lnum].radius - Length (dist);

							if (add > 0)
								lighting.ambientlight += add;
						}
					}

					// clamp lighting so it doesn't overbright as much
					if (lighting.ambientlight > 128)
						lighting.ambientlight = 128;
					if (lighting.ambientlight + lighting.shadelight > 192)
						lighting.shadelight = 192 - lighting.ambientlight;

					R_AliasDrawModel (&lighting);
				}

				break;

			default:
				break;
		}
	}
}

/*
	R_DrawViewModel
*/
void
R_DrawViewModel (void)
{
// FIXME: remove and do real lighting
	float       lightvec[3] = { -1, 0, 0 };
	int         j;
	int         lnum;
	vec3_t      dist;
	float       add;
	dlight_t   *dl;

	if (!r_drawviewmodel->int_val
		|| !Cam_DrawViewModel ()
		|| !r_drawentities->int_val)
		return;

	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	VectorCopy (vup, viewlightvec);
	VectorInverse (viewlightvec);

	j = R_LightPoint (currententity->origin);

	if (j < 24)
		j = 24;							// always give some light on gun
	r_viewlighting.ambientlight = j;
	r_viewlighting.shadelight = j;

// add dynamic lights       
	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - Length (dist);
		if (add > 0)
			r_viewlighting.ambientlight += add;
	}

// clamp lighting so it doesn't overbright as much
	if (r_viewlighting.ambientlight > 128)
		r_viewlighting.ambientlight = 128;
	if (r_viewlighting.ambientlight + r_viewlighting.shadelight > 192)
		r_viewlighting.shadelight = 192 - r_viewlighting.ambientlight;

	r_viewlighting.plightvec = lightvec;

	R_AliasDrawModel (&r_viewlighting);
}


/*
	R_BmodelCheckBBox
*/
int
R_BmodelCheckBBox (model_t *clmodel, float *minmaxs)
{
	int         i, *pindex, clipflags;
	vec3_t      acceptpt, rejectpt;
	double      d;

	clipflags = 0;

	if (currententity->angles[0] || currententity->angles[1]
		|| currententity->angles[2]) {
		for (i = 0; i < 4; i++) {
			d = DotProduct (currententity->origin, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= -clmodel->radius)
				return BMODEL_FULLY_CLIPPED;

			if (d <= clmodel->radius)
				clipflags |= (1 << i);
		}
	} else {
		for (i = 0; i < 4; i++) {
			// generate accept and reject points
			// FIXME: do with fast look-ups or integer tests based on the
			// sign bit
			// of the floating point values

			pindex = pfrustum_indexes[i];

			rejectpt[0] = minmaxs[pindex[0]];
			rejectpt[1] = minmaxs[pindex[1]];
			rejectpt[2] = minmaxs[pindex[2]];

			d = DotProduct (rejectpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				return BMODEL_FULLY_CLIPPED;

			acceptpt[0] = minmaxs[pindex[3 + 0]];
			acceptpt[1] = minmaxs[pindex[3 + 1]];
			acceptpt[2] = minmaxs[pindex[3 + 2]];

			d = DotProduct (acceptpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				clipflags |= (1 << i);
		}
	}

	return clipflags;
}


/*
	R_DrawBEntitiesOnList
*/
void
R_DrawBEntitiesOnList (void)
{
	int         i, j, k, clipflags;
	vec3_t      oldorigin;
	model_t    *clmodel;
	float       minmaxs[6];

	if (!r_drawentities->int_val)
		return;

	VectorCopy (modelorg, oldorigin);
	insubmodel = true;
	r_dlightframecount = r_framecount;

	for (i = 0; i < cl_numvisedicts; i++) {
		currententity = cl_visedicts[i];

		switch (currententity->model->type) {
			case mod_brush:

				clmodel = currententity->model;

				// see if the bounding box lets us trivially reject, also
				// sets
				// trivial accept status
				for (j = 0; j < 3; j++) {
					minmaxs[j] = currententity->origin[j] + clmodel->mins[j];
					minmaxs[3 + j] = currententity->origin[j] +
						clmodel->maxs[j];
				}

				clipflags = R_BmodelCheckBBox (clmodel, minmaxs);

				if (clipflags != BMODEL_FULLY_CLIPPED) {
					VectorCopy (currententity->origin, r_entorigin);
					VectorSubtract (r_origin, r_entorigin, modelorg);
					// FIXME: is this needed?
					VectorCopy (modelorg, r_worldmodelorg);

					r_pcurrentvertbase = clmodel->vertexes;

					// FIXME: stop transforming twice
					R_RotateBmodel ();

					// calculate dynamic lighting for bmodel if it's not an
					// instanced model
					if (clmodel->firstmodelsurface != 0) {
						vec3_t      lightorigin;

						for (k = 0; k < MAX_DLIGHTS; k++) {
							if ((cl_dlights[k].die < cl.time) ||
								(!cl_dlights[k].radius)) continue;

							VectorSubtract (cl_dlights[k].origin,
											currententity->origin, lightorigin);
							R_MarkLights (lightorigin, &cl_dlights[k], 1 << k,
										  clmodel->nodes +
										  clmodel->hulls[0].firstclipnode);
						}
					}
					// if the driver wants polygons, deliver those.
					// Z-buffering is on
					// at this point, so no clipping to the world tree is
					// needed, just
					// frustum clipping
					if (r_drawpolys | r_drawculledpolys) {
						R_ZDrawSubmodelPolys (clmodel);
					} else {
						r_pefragtopnode = NULL;

						for (j = 0; j < 3; j++) {
							r_emins[j] = minmaxs[j];
							r_emaxs[j] = minmaxs[3 + j];
						}

						R_SplitEntityOnNode2 (cl.worldmodel->nodes);

						if (r_pefragtopnode) {
							currententity->topnode = r_pefragtopnode;

							if (r_pefragtopnode->contents >= 0) {
								// not a leaf; has to be clipped to the world 
								// BSP
								r_clipflags = clipflags;
								R_DrawSolidClippedSubmodelPolygons (clmodel);
							} else {
								// falls entirely in one leaf, so we just put 
								// all the
								// edges in the edge list and let 1/z sorting 
								// handle
								// drawing order
								R_DrawSubmodelPolygons (clmodel, clipflags);
							}

							currententity->topnode = NULL;
						}
					}

					// put back world rotation and frustum clipping     
					// FIXME: R_RotateBmodel should just work off base_vxx
					VectorCopy (base_vpn, vpn);
					VectorCopy (base_vup, vup);
					VectorCopy (base_vright, vright);
					VectorCopy (base_modelorg, modelorg);
					VectorCopy (oldorigin, modelorg);
					R_TransformFrustum ();
				}

				break;

			default:
				break;
		}
	}

	insubmodel = false;
}


/*
	R_EdgeDrawing
*/
void
R_EdgeDrawing (void)
{
	edge_t      ledges[NUMSTACKEDGES +

					   ((CACHE_SIZE - 1) / sizeof (edge_t)) + 1];
	surf_t      lsurfs[NUMSTACKSURFACES +

					   ((CACHE_SIZE - 1) / sizeof (surf_t)) + 1];

	if (auxedges) {
		r_edges = auxedges;
	} else {
		r_edges = (edge_t *)
			(((long) &ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	}

	if (r_surfsonstack) {
		surfaces = (surf_t *)
			(((long) &lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
		surf_max = &surfaces[r_cnumsurfs];
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
		R_SurfacePatch ();
	}

	R_BeginEdgeFrame ();

	if (r_dspeeds->int_val) {
		rw_time1 = Sys_DoubleTime ();
	}

	R_RenderWorld ();

	if (r_drawculledpolys)
		R_ScanEdges ();

// only the world can be drawn back to front with no z reads or compares, just
// z writes, so have the driver turn z compares on now
	D_TurnZOn ();

	if (r_dspeeds->int_val) {
		rw_time2 = Sys_DoubleTime ();
		db_time1 = rw_time2;
	}

	R_DrawBEntitiesOnList ();

	if (r_dspeeds->int_val) {
		db_time2 = Sys_DoubleTime ();
		se_time1 = db_time2;
	}

	if (!r_dspeeds->int_val) {
		VID_UnlockBuffer ();
		S_ExtraUpdate ();				// don't let sound get messed up if
										// going slow
		VID_LockBuffer ();
	}

	if (!(r_drawpolys | r_drawculledpolys))
		R_ScanEdges ();
}


/*
	R_RenderView

	r_refdef must be set before the first call
*/
void
R_RenderView_ (void)
{
	byte        warpbuffer[WARP_WIDTH * WARP_HEIGHT];

	r_warpbuffer = warpbuffer;

	if (r_timegraph->int_val || r_speeds->int_val || r_dspeeds->int_val)
		r_time1 = Sys_DoubleTime ();

	R_SetupFrame ();

#ifdef PASSAGES
	SetVisibilityByPassages ();
#else
	R_MarkLeaves ();					// done here so we know if we're in
										// water
#endif

// make FDIV fast. This reduces timing precision after we've been running for a
// while, so we don't do it globally.  This also sets chop mode, and we do it
// here so that setup stuff like the refresh area calculations match what's
// done in screen.c
	Sys_LowFPPrecision ();

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (!r_dspeeds->int_val) {
		VID_UnlockBuffer ();
		S_ExtraUpdate ();				// don't let sound get messed up if
										// going slow
		VID_LockBuffer ();
	}

	R_EdgeDrawing ();

	if (!r_dspeeds->int_val) {
		VID_UnlockBuffer ();
		S_ExtraUpdate ();				// don't let sound get messed up if
										// going slow
		VID_LockBuffer ();
	}

	if (r_dspeeds->int_val) {
		se_time2 = Sys_DoubleTime ();
		de_time1 = se_time2;
	}

	R_DrawEntitiesOnList ();

	if (r_dspeeds->int_val) {
		de_time2 = Sys_DoubleTime ();
		dv_time1 = de_time2;
	}

	R_DrawViewModel ();

	if (r_dspeeds->int_val) {
		dv_time2 = Sys_DoubleTime ();
		dp_time1 = Sys_DoubleTime ();
	}

	R_DrawParticles ();

	if (r_dspeeds->int_val)
		dp_time2 = Sys_DoubleTime ();

	if (r_dowarp)
		D_WarpScreen ();

	V_SetContentsColor (r_viewleaf->contents);

	if (r_timegraph->int_val)
		R_TimeGraph ();

	if (r_netgraph->int_val)
		R_NetGraph ();

	if (r_zgraph->int_val)
		R_ZGraph ();

	if (r_aliasstats->int_val)
		R_PrintAliasStats ();

	if (r_speeds->int_val)
		R_PrintTimes ();

	if (r_dspeeds->int_val)
		R_PrintDSpeeds ();

	if (r_reportsurfout->int_val && r_outofsurfaces)
		Con_Printf ("Short %d surfaces\n", r_outofsurfaces);

	if (r_reportedgeout->int_val && r_outofedges)
		Con_Printf ("Short roughly %d edges\n", r_outofedges * 2 / 3);

// back to high floating-point precision
	Sys_HighFPPrecision ();
}

void
R_RenderView (void)
{
	int         dummy;
	int         delta;

	delta = (byte *) & dummy - r_stack_start;
	if (delta < -10000 || delta > 10000)
		Sys_Error ("R_RenderView: called without enough stack");

	if (Hunk_LowMark () & 3)
		Sys_Error ("Hunk is missaligned");

	if ((long) (&dummy) & 3)
		Sys_Error ("Stack is missaligned");

	if ((long) (&r_warpbuffer) & 3)
		Sys_Error ("Globals are missaligned");

	R_RenderView_ ();
}

/*
	R_InitTurb
*/
void
R_InitTurb (void)
{
	int         i;

	for (i = 0; i < 1280; i++) {
		sintable[i] = AMP + sin (i * 3.14159 * 2 / CYCLE) * AMP;
		intsintable[i] = AMP2 + sin (i * 3.14159 * 2 / CYCLE) * AMP2;
		// AMP2 not 20
	}
}
