/*
	pmove.h

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

#ifndef _PMOVE_H
#define _PMOVE_H

#include "protocol.h"
#include "mathlib.h"
#include "model.h"
#include "progs.h"

#define STOP_EPSILON 0.1

typedef struct
{
	vec3_t	normal;
	float	dist;
} plane_t;

typedef struct
{
	qboolean	allsolid;		// if true, plane is not valid
	qboolean	startsolid;		// if true, the initial point was in a solid area
	qboolean	inopen, inwater;
	float		fraction;		// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;			// final position
	plane_t		plane;			// surface normal at impact

// Dabb: only difference between server and client
//	int			ent;		// vs servers edict_t *ent;

	int			entnum;

	edict_t		*ent;			// entity the surface is on
} trace_t;

#define	MAX_PHYSENTS	32
typedef struct
{
	vec3_t	origin;
	model_t	*model;		// only for bsp models
	vec3_t	mins, maxs;	// only for non-bsp models
	int		info;		// for client or server to identify
} physent_t;

typedef struct
{
	int		sequence;	// just for debugging prints

	// player state
	vec3_t	origin;
	vec3_t	angles;
	vec3_t	velocity;
	int		oldbuttons;
	float		waterjumptime;
	qboolean	dead;
	qboolean	flying;
	int		spectator;

	// world state
	int		numphysent;
	physent_t	physents[MAX_PHYSENTS];	// 0 should be the world

	// input
	usercmd_t	cmd;

	// results
	int		numtouch;
	int		touchindex[MAX_PHYSENTS];
} playermove_t;

typedef struct {
	float	gravity;
	float	stopspeed;
	float	maxspeed;
	float	spectatormaxspeed;
	float	accelerate;
	float	airaccelerate;
	float	wateraccelerate;
	float	friction;
	float	waterfriction;
	float	entgravity;
} movevars_t;

extern	struct cvar_s	*no_pogo_stick;
extern	movevars_t		movevars;
extern	playermove_t	pmove;
extern	int		onground;
extern	int		waterlevel;
extern	int		watertype;

void PlayerMove (void);
void Pmove_Init (void);
void Pmove_Init_Cvars (void);

int HullPointContents (hull_t *hull, int num, vec3_t p);

int PM_PointContents (vec3_t point);
qboolean PM_TestPlayerPosition (vec3_t point);
trace_t PM_PlayerMove (vec3_t start, vec3_t stop);

#endif // _PMOVE_H
