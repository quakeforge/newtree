/*
	progs.h

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

#ifndef _PROGS_H
#define _PROGS_H

#include "gcc_attr.h"
#include "protocol.h"
#include "pr_comp.h"			// defs shared with qcc
#include "progdefs.h"			// generated by program cdefs
#include "link.h"
#include "quakeio.h"

typedef union eval_s
{
	string_t		string;
	float			_float;
	float			vector[3];
	func_t			function;
	int				_int;
	int				edict;
} eval_t;	

#define	MAX_ENT_LEAFS	16
typedef struct edict_s
{
	qboolean	free;
	link_t		area;				// linked to a division node or leaf
	
	int			num_leafs;
	short		leafnums[MAX_ENT_LEAFS];

	entity_state_t	baseline;
	
	float		freetime;			// sv.time when the object was freed
	entvars_t	v;					// C exported fields from progs
// other fields from progs come immediately after
} edict_t;
#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,area)

struct progs_s;

//============================================================================

void PR_Init (void);

void PR_ExecuteProgram (struct progs_s *pr, func_t fnum);
void PR_LoadProgs (struct progs_s *pr);

void PR_Profile_f (void);

edict_t *ED_Alloc (struct progs_s *pr);
void ED_Free (struct progs_s *pr, edict_t *ed);

char	*ED_NewString (struct progs_s *pr, char *string);
// returns a copy of the string allocated from the server's string heap

void ED_Print (struct progs_s *pr, edict_t *ed);
void ED_Write (struct progs_s *pr, QFile *f, edict_t *ed);
char *ED_ParseEdict (struct progs_s *pr, char *data, edict_t *ent);

void ED_WriteGlobals (struct progs_s *pr, QFile *f);
void ED_ParseGlobals (struct progs_s *pr, char *data);

void ED_LoadFromFile (struct progs_s *pr, char *data);

ddef_t *ED_FindField (struct progs_s *pr, char *name);
dfunction_t *ED_FindFunction (struct progs_s *pr, char *name);


//define EDICT_NUM(p,n) ((edict_t *)(*(p)->edicts+ (n)*(p)->pr_edict_size))
//define NUM_FOR_EDICT(p,e) (((byte *)(e) - *(p)->edicts)/(p)->pr_edict_size)

edict_t *EDICT_NUM(struct progs_s *pr, int n);
int NUM_FOR_EDICT(struct progs_s *pr, edict_t *e);

#define	NEXT_EDICT(p,e) ((edict_t *)( (byte *)e + (p)->pr_edict_size))

#define	EDICT_TO_PROG(p,e) ((byte *)e - (byte *)*(p)->edicts)
#define PROG_TO_EDICT(p,e) ((edict_t *)((byte *)*(p)->edicts + e))

//============================================================================

#define	G_FLOAT(p,o) ((p)->pr_globals[o])
#define	G_INT(p,o) (*(int *)&(p)->pr_globals[o])
#define	G_EDICT(p,o) ((edict_t *)((byte *)*(p)->edicts+ *(int *)&(p)->pr_globals[o]))
#define G_EDICTNUM(p,o) NUM_FOR_EDICT(p,G_EDICT(p, o))
#define	G_VECTOR(p,o) (&(p)->pr_globals[o])
#define	G_STRING(p,o) (PR_GetString(pr,*(string_t *)&(p)->pr_globals[o]))
#define	G_FUNCTION(p,o) (*(func_t *)&(p)->pr_globals[o])

#define	E_FLOAT(e,o) (((float*)&e->v)[o])
#define	E_INT(e,o) (*(int *)&((float*)&e->v)[o])
#define	E_VECTOR(e,o) (&((float*)&e->v)[o])
#define	E_STRING(e,o) (PR_GetString(pr,*(string_t *)&((float*)&e->v)[o]))

extern	int		type_size[8];

typedef void (*builtin_t) (struct progs_s *pr);
extern	builtin_t *pr_builtins;
extern int pr_numbuiltins;

int FindFieldOffset (struct progs_s *pr, char *field);

extern func_t	EndFrame;	// 2000-01-02 EndFrame function by Maddes/FrikaC

extern func_t SpectatorConnect;
extern func_t SpectatorThink;
extern func_t SpectatorDisconnect;

void PR_RunError (struct progs_s *pr, char *error, ...) __attribute__((format(printf,2,3)));

void ED_PrintEdicts (struct progs_s *pr);
void ED_PrintNum (struct progs_s *pr, int ent);
void ED_Count (struct progs_s *pr);
void ED_PrintEdict_f (void);
void ED_PrintEdicts_f (void);
void ED_Count_f (void);
void PR_Profile (struct progs_s *pr);

eval_t *GetEdictFieldValue(struct progs_s *pr, edict_t *ed, char *field);

//
// PR STrings stuff
//
#define MAX_PRSTR 1024

char *PR_GetString(struct progs_s *pr, int num);
int PR_SetString(struct progs_s *pr, char *s);

//============================================================================

#define MAX_STACK_DEPTH		32
#define LOCALSTACK_SIZE		2048

typedef struct {
	int         s;
	dfunction_t *f;
} prstack_t;

typedef struct progs_s {
	dprograms_t		*progs;
	dfunction_t		*pr_functions;
	char			*pr_strings;
	ddef_t			*pr_globaldefs;
	ddef_t			*pr_fielddefs;
	dstatement_t	*pr_statements;
	globalvars_t	*pr_global_struct;
	float			*pr_globals;			// same as pr_global_struct

	int				pr_edict_size;	// in bytes
	int				pr_edictareasize; // LordHavoc: for bounds checking

	int				pr_argc;

	qboolean		pr_trace;
	dfunction_t		*pr_xfunction;
	int				pr_xstatement;

	char			*pr_strtbl[MAX_PRSTR];
	int				num_prstr;

	prstack_t		pr_stack[MAX_STACK_DEPTH];
	int				pr_depth;

	int				localstack[LOCALSTACK_SIZE];
	int				localstack_used;

	edict_t			**edicts;
	int				*num_edicts;
	double			*time;
	int				null_bad;
} progs_t;

#endif // _PROGS_H
