/*
	cmd.c

	script command processing module

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
# include <config.h>
#endif
#include "sys.h"
#include "cmd.h"
#include "cvar.h"
#include "sizebuf.h"
#include "console.h"
#include "qargs.h"
#include "quakefs.h"
#include "commdef.h"
#include "zone.h"

#include <string.h>
#include <ctype.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

void Cmd_ForwardToServer (void);

#define	MAX_ALIAS_NAME	32

typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	char	name[MAX_ALIAS_NAME];
	char	*value;
} cmdalias_t;

cmdalias_t	*cmd_alias;

qboolean	cmd_wait;

cvar_t	*cl_warncmd;
//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f (void)
{
	cmd_wait = true;
}

/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

sizebuf_t	cmd_text;
byte		cmd_text_buf[8192];

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	cmd_text.data = cmd_text_buf;
	cmd_text.maxsize = sizeof(cmd_text_buf);
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (char *text)
{
	int		l;

	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Con_Printf ("Cbuf_AddText: overflow\n");
		return;
	}
	SZ_Write (&cmd_text, text, strlen (text));
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (char *text)
{
	char	*temp;
	int		templen;

// copy off any commands still remaining in the exec buffer
	templen = cmd_text.cursize;
	if (templen)
	{
		temp = malloc (templen);
		memcpy (temp, cmd_text.data, templen);
		SZ_Clear (&cmd_text);
	}
	else
		temp = NULL;	// shut up compiler

// add the entire text of the file
	Cbuf_AddText (text);
	SZ_Write (&cmd_text, "\n", 1);
// add the copied off data
	if (templen)
	{
		SZ_Write (&cmd_text, temp, templen);
		free (temp);
	}
}

static void
extract_line(char *line)
{
	int		i;
	char	*text;
	int	quotes;

	// find a \n or ; line break
	text = (char *)cmd_text.data;
	quotes = 0;
	for (i=0 ; i< cmd_text.cursize ; i++) {
		if (text[i] == '"')
			quotes++;
		if ( !(quotes&1) &&  text[i] == ';')
			break;	// don't break if inside a quoted string
		if (text[i] == '\n' || text[i] == '\r')
			break;
	}

	memcpy (line, text, i);
	line[i] = '\0';
	// delete the text from the command buffer and move remaining commands down
	// this is necessary because commands (exec, alias) can insert data at the
	// beginning of the text buffer

	if (i == cmd_text.cursize)
		cmd_text.cursize = 0;
	else {
		i++;
		cmd_text.cursize -= i;
		memcpy (text, text+i, cmd_text.cursize);
	}
}

/*

	Cbuf_Execute

*/
void
Cbuf_Execute (void)
{
	char	line[1024] = {0};

	while (cmd_text.cursize) {
		extract_line (line);
		// execute the command line
		//Con_DPrintf("+%s\n",line),
		Cmd_ExecuteString (line);

		if (cmd_wait)
		{	// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	}
}
/*

	Cbuf_Execute

*/
void
Cbuf_Execute_Sets (void)
{
	char	line[1024] = {0};

	while (cmd_text.cursize) {
		extract_line (line);
		// execute the command line
		if (strncmp(line,"set",3)==0
			&& isspace((int) line[3]))
			//Con_DPrintf("+%s\n",line),
			Cmd_ExecuteString (line);
	}
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void Cmd_StuffCmds_f (void)
{
	int		i, j;
	int		s;
	char	*build, c;

	s = strlen (com_cmdline);
	if (!s)
		return;

// pull out the commands
	build = malloc (s+1);
	build[0] = 0;

	for (i=0 ; i<s-1 ; i++)
	{
		if (com_cmdline[i] == '+')
		{
			i++;

			for (j=i ; ((com_cmdline[j] != '+')
						&& (com_cmdline[j] != '-')
						&& (com_cmdline[j] != 0)) ; j++)
				;

			c = com_cmdline[j];
			com_cmdline[j] = 0;

			strcat (build, com_cmdline+i);
			strcat (build, "\n");
			com_cmdline[j] = c;
			i = j-1;
		}
	}

	//Con_Printf("[\n%s]\n",build);

	if (build[0])
		Cbuf_InsertText (build);

	free (build);
}

/*

  Cmd_Exec_File

*/
void
Cmd_Exec_File (char *path)
{
	char	*f;
	int		mark;
	int		len;
	char	base[32];
	FILE   *file;

	if ((file = fopen (path, "r")) != NULL) {
		// extract the filename base name for hunk tag
		COM_FileBase (path, base);
		len = COM_filelength (file);
		mark = Hunk_LowMark ();
		f = (char *)Hunk_AllocName (len+1, base);
		if (f) {
			f[len] = 0;
			fread (f, 1, len, file);
			fclose (file);
			Cbuf_InsertText (f);
		}
		Hunk_FreeToLowMark (mark);
	}
}

/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f (void)
{
	char	*f;
	int		mark;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	// FIXME: is this safe freeing the hunk here???
	mark = Hunk_LowMark ();
	f = (char *)COM_LoadHunkFile (Cmd_Argv(1));
	if (!f)
	{
		Con_Printf ("couldn't exec %s\n",Cmd_Argv(1));
		return;
	}
	if (!Cvar_Command () && (cl_warncmd->value || developer->value))
		Con_Printf ("execing %s\n",Cmd_Argv(1));

	Cbuf_InsertText (f);
	Hunk_FreeToLowMark (mark);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	int		i;

	for (i=1 ; i<Cmd_Argc() ; i++)
		Con_Printf ("%s ",Cmd_Argv(i));
	Con_Printf ("\n");
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/

char *CopyString (char *in)
{
	char	*out;

	out = malloc (strlen(in)+1);
	strcpy (out, in);
	return out;
}

void Cmd_Alias_f (void)
{
	cmdalias_t	*a;
	char		cmd[1024];
	int			i, c;
	char		*s;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("Current alias commands:\n");
		for (a = cmd_alias ; a ; a=a->next)
			Con_Printf ("%s : %s\n", a->name, a->value);
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Con_Printf ("Alias name is too long\n");
		return;
	}

	// if the alias allready exists, reuse it
	for (a = cmd_alias ; a ; a=a->next)
	{
		if (!strcmp(s, a->name))
		{
			free (a->value);
			break;
		}
	}

	if (!a)
	{
		a = malloc (sizeof(cmdalias_t));
		a->next = cmd_alias;
		cmd_alias = a;
	}
	strcpy (a->name, s);

// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	c = Cmd_Argc();
	for (i=2 ; i< c ; i++)
	{
		strcat (cmd, Cmd_Argv(i));
		if (i != c)
			strcat (cmd, " ");
	}
	strcat (cmd, "\n");

	a->value = CopyString (cmd);
}

void Cmd_UnAlias_f (void)
{
	cmdalias_t	*a, *prev;
	char		*s;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("unalias <alias>: erase an existing alias\n");
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Con_Printf ("Alias name is too long\n");
		return;
	}

	prev = cmd_alias;
	for (a = cmd_alias ; a ; a = a->next)
	{
		if (!strcmp(s, a->name))
		{
			free (a->value);
			prev->next = a->next;
			if (a == cmd_alias)
				cmd_alias = a->next;
			free (a);
			return;
		}
		prev = a;
	}
	Con_Printf ("Unknown alias \"%s\"\n", s);
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char					*name;
	xcommand_t				function;
} cmd_function_t;


#define	MAX_ARGS		80

static	int			cmd_argc;
static	char		*cmd_argv[MAX_ARGS];
static	char		*cmd_null_string = "";
static	char		*cmd_args = NULL;



static	cmd_function_t	*cmd_functions;		// possible commands to execute

/*
============
Cmd_Argc
============
*/
int		Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char	*Cmd_Argv (int arg)
{
	if ( arg >= cmd_argc )
		return cmd_null_string;
	return cmd_argv[arg];
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char		*Cmd_Args (void)
{
	if (!cmd_args)
		return "";
	return cmd_args;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/
void Cmd_TokenizeString (char *text)
{
	int		i;

// clear the args from the last string
	for (i=0 ; i<cmd_argc ; i++)
		free (cmd_argv[i]);

	cmd_argc = 0;
	cmd_args = NULL;

	while (1)
	{
// skip whitespace up to a /n
		while (*text && *text <= ' ' && *text != '\n')
		{
			text++;
		}

		if (*text == '\n')
		{	// a newline seperates commands in the buffer
			text++;
			break;
		}

		if (!*text)
			return;

		if (cmd_argc == 1)
			 cmd_args = text;

		text = COM_Parse (text);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS)
		{
			cmd_argv[cmd_argc] = malloc (strlen(com_token)+1);
			strcpy (cmd_argv[cmd_argc], com_token);
			cmd_argc++;
		}
	}

}


/*
============
Cmd_AddCommand
============
*/
void	Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
	cmd_function_t	*cmd;

	if (host_initialized)	// because hunk allocation would get stomped
		Sys_Error ("Cmd_AddCommand after host_initialized");

// fail if the command is a variable name
	if (Cvar_VariableString(cmd_name)[0])
	{
		Con_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

// fail if the command already exists
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!strcmp (cmd_name, cmd->name))
		{
			Con_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = Hunk_Alloc (sizeof(cmd_function_t));
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

/*
============
Cmd_Exists
============
*/
qboolean	Cmd_Exists (char *cmd_name)
{
	cmd_function_t	*cmd;

	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!strcmp (cmd_name,cmd->name))
			return true;
	}

	return false;
}



/*
============
Cmd_CompleteCommand
============
*/
char *Cmd_CompleteCommand (char *partial)
{
	cmd_function_t	*cmd;
	int				len;
	cmdalias_t		*a;

	len = strlen(partial);

	if (!len)
		return NULL;

// check for exact match
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
		if (!strcasecmp (partial, cmd->name))
			return cmd->name;
	for (a=cmd_alias ; a ; a=a->next)
		if (!strcasecmp (partial, a->name))
			return a->name;

// check for partial match
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
		if (!strncasecmp (partial, cmd->name, len))
			return cmd->name;
	for (a=cmd_alias ; a ; a=a->next)
		if (!strncasecmp (partial, a->name, len))
			return a->name;

	return NULL;
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void Cmd_ExecuteString (char *text)
{
	cmd_function_t	*cmd;
	cmdalias_t		*a;

	Cmd_TokenizeString (text);

// execute the command line
	if (!Cmd_Argc())
		return;		// no tokens

// check functions
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
	{
		if (!strcasecmp (cmd_argv[0],cmd->name))
		{
			if (!cmd->function)
				Cmd_ForwardToServer ();
			else
				cmd->function ();
			return;
		}
	}

// Tonik: check cvars
	if (Cvar_Command())
		return;

// check alias
	for (a=cmd_alias ; a ; a=a->next)
	{
		if (!strcasecmp (cmd_argv[0], a->name))
		{
			Cbuf_InsertText (a->value);
			return;
		}
	}
	
	if (cl_warncmd->value || developer->value)
		Con_Printf ("Unknown command \"%s\"\n", Cmd_Argv(0));

}



/*
================
Cmd_CheckParm

Returns the position (1 to argc-1) in the command's argument list
where the given parameter apears, or 0 if not present
================
*/
int Cmd_CheckParm (char *parm)
{
	int i;

	if (!parm)
		Sys_Error ("Cmd_CheckParm: NULL");

	for (i = 1; i < Cmd_Argc (); i++)
		if (! strcasecmp (parm, Cmd_Argv (i)))
			return i;

	return 0;
}

void Cmd_CmdList_f (void)
{
	cmd_function_t	*cmd;
	int	i;

	for (cmd=cmd_functions, i=0 ; cmd ; cmd=cmd->next, i++)
	{
		Con_Printf("%s\n", cmd->name);
	}

	Con_Printf ("------------\n%d commands\n", i);
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
//
// register our commands
//
	Cmd_AddCommand ("stuffcmds",Cmd_StuffCmds_f);
	Cmd_AddCommand ("exec",Cmd_Exec_f);
	Cmd_AddCommand ("echo",Cmd_Echo_f);
	Cmd_AddCommand ("alias",Cmd_Alias_f);
	Cmd_AddCommand ("unalias",Cmd_UnAlias_f);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
	Cmd_AddCommand ("cmdlist", Cmd_CmdList_f);
}


char		com_token[MAX_COM_TOKEN];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int		c;
	int		len;
	char	buf[255];
	int		i;
	cvar_t	*var, *bestvar;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;			// end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse a regular word
	do
	{
		// FIXME: only check for $'s when parsing
		// command-line text (not entity data!)
		if (c == '$')
		{
			data++;

			// Copy the text after '$' to a temp buffer
			i = 0;
			buf[0] = 0;
			bestvar = NULL;
			while ((c = *data) > 32)
			{
				if (c == '$')
					break;
				data++;
				buf[i++] = c;
				buf[i] = 0;
				if ((var = Cvar_FindVar(buf)))
					bestvar = var;
			}

			if (bestvar)
			{
				// check buffer size
				if (len + strlen(bestvar->string) >= MAX_COM_TOKEN-1)
					break;

				strcpy(&com_token[len], bestvar->string);
				len += strlen(bestvar->string);
				i = strlen(bestvar->name);
				while (buf[i])
					com_token[len++] = buf[i++];
			}
			else
			{
				// no matching cvar name was found
				com_token[len++] = '$';
				if (len + strlen(buf) >= MAX_COM_TOKEN-1)
					break;
				strcpy (&com_token[len], buf);
				len += strlen(buf);
			}
		}
		else
		{
			com_token[len] = c;
			data++;
			len++;
			if (len >= MAX_COM_TOKEN-1)
				break;
		}

		c = *data;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}
