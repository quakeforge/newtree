/*
	menu.c

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
# include <config.h>
#endif
#include "sys.h"
#include "quakedef.h"
#ifdef _WIN32
#include "winquake.h"
#endif
#include "cl_slist.h"
#include "keys.h"
#include "menu.h"
#include "vid.h"
#include "draw.h"
#include "cmd.h"
#include "screen.h"
#include "client.h"
#include "console.h"
#include "input.h"
#include "view.h"

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

enum {m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup, m_net, m_options, m_video, m_keys, m_help, m_quit, m_serialconfig, m_modemconfig, m_lanconfig, m_gameoptions, m_search, m_sedit} m_state;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_SEdit_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_Help_f (void);
	void M_Menu_Quit_f (void);
void M_Menu_SerialConfig_f (void);
	void M_Menu_ModemConfig_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_Search_f (void);
void M_Menu_ServerList_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_Setup_Draw (void);
		void M_Net_Draw (void);
	void M_Options_Draw (void);
		void M_Keys_Draw (void);
		void M_Video_Draw (void);
	void M_Help_Draw (void);
	void M_Quit_Draw (void);
void M_SerialConfig_Draw (void);
	void M_ModemConfig_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_Search_Draw (void);
void M_ServerList_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
	void M_MultiPlayer_Key (int key);
		void M_Setup_Key (int key);
		void M_Net_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
		void M_Video_Key (int key);
	void M_Help_Key (int key);
	void M_Quit_Key (int key);
void M_SerialConfig_Key (int key);
	void M_ModemConfig_Key (int key);
void M_LanConfig_Key (int key);
void M_GameOptions_Key (int key);
void M_Search_Key (int key);
void M_ServerList_Key (int key);

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound
qboolean	m_recursiveDraw;

int			m_return_state;
qboolean	m_return_onerror;
char		m_return_reason [32];

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)
#define SerialConfig	(m_net_cursor == 0)
#define DirectConfig	(m_net_cursor == 1)
#define	IPXConfig		(m_net_cursor == 2)
#define	TCPIPConfig		(m_net_cursor == 3)

void M_ConfigureNetSubsystem(void);

//=============================================================================
/* Support Routines */

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character8 ( cx + ((vid.width - 320)>>1), line, num);
}

void M_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, (*str)+128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawTransPic (int x, int y, qpic_t *pic)
{
	Draw_TransPic (x + ((vid.width - 320)>>1), y, pic);
}

void M_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x + ((vid.width - 320)>>1), y, pic);
}

byte identityTable[256];
byte translationTable[256];

void M_BuildTranslationTable(int top, int bottom)
{
	int		j;
	byte	*dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)	// the artists made some backwards ranges.  sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[TOP_RANGE+j] = source[top+15-j];

	if (bottom < 128)
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[BOTTOM_RANGE+j] = source[bottom+15-j];		
}


void M_DrawTransPicTranslate (int x, int y, qpic_t *pic)
{
	Draw_TransPicTranslate (x + ((vid.width - 320)>>1), y, pic, translationTable);
}


void M_DrawTextBox (int x, int y, int width, int lines)
{
	qpic_t	*p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	M_DrawTransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	M_DrawTransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		M_DrawTransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			M_DrawTransPic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		M_DrawTransPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	M_DrawTransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	M_DrawTransPic (cx, cy+8, p);
}

//=============================================================================

int m_save_demonum;
		
/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	if (key_dest == key_console)
	{
		Con_ToggleConsole_f ();
		if (key_dest == key_console) //Still suck on console?
			M_Menu_Main_f ();
	}
	else
	{
		M_Menu_Main_f ();
	}
}

		
//=============================================================================
/* MAIN MENU */

int	m_main_cursor;
#define	MAIN_ITEMS	5


void M_Menu_Main_f (void)
{
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}
				

void M_Main_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mainmenu.lmp") );

	f = (int)(realtime * 10)%6;
	
	M_DrawTransPic (54, 32 + m_main_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}


void M_Main_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		key_dest = key_game;
		m_state = m_none;
		cls.demonum = m_save_demonum;
		if (cls.demonum != -1 && !cls.demoplayback && cls.state == ca_disconnected)
			CL_NextDemo ();
		break;
		
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_SinglePlayer_f ();
			break;

		case 1:
			M_Menu_MultiPlayer_f ();
			break;

		case 2:
			M_Menu_Options_f ();
			break;

		case 3:
			M_Menu_Help_f ();
			break;

		case 4:
			M_Menu_Quit_f ();
			break;
		}
	}
}


//=============================================================================
/* OPTIONS MENU */

#define	OPTIONS_ITEMS	17

#define	SLIDER_RANGE	10

int		options_cursor;

void M_Menu_Options_f (void)
{
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
}


void M_AdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor) {
		case 3:	// screen size
			Cvar_SetValue (scr_viewsize, bound (30, scr_viewsize->int_val + (dir * 10), 120));
			break;
		case 4:	// Brightness
			Cvar_SetValue (brightness, bound (1, brightness->value + (dir * 0.25), 5));
			break;
		case 5:	// Contrast
			Cvar_SetValue (contrast, bound (0.0, contrast->value + (dir * 0.05), 1));
			break;
		case 6:	// mouse speed
			Cvar_SetValue (sensitivity, bound (1, sensitivity->value + dir, 25));
			break;
		case 7:	// music volume
#ifdef _WIN32
			Cvar_SetValue (bgmvolume, bound (0, bgmvolume->value + dir, 1));
#else
			Cvar_SetValue (bgmvolume, bound (0, bgmvolume->value + (dir * 0.1), 1));
#endif
			break;
		case 8:	// sfx volume
			Cvar_SetValue (volume, bound (0, volume->value + (dir * 0.1), 1));
			break;

		case 9:	// allways run
			if (cl_forwardspeed->value > 200) {
				Cvar_SetValue (cl_forwardspeed, 200);
				Cvar_SetValue (cl_backspeed, 200);
			} else {
				Cvar_SetValue (cl_forwardspeed, 400);
				Cvar_SetValue (cl_backspeed, 400);
			}
			break;

		case 10:	// invert mouse
			Cvar_SetValue (m_pitch, -m_pitch->value);
			break;

		case 11:	// lookspring
			Cvar_SetValue (lookspring, !lookspring->int_val);
			break;

		case 12:	// lookstrafe
			Cvar_SetValue (lookstrafe, !lookstrafe->int_val);
			break;

		case 13:	// Use old-style sbar
			Cvar_SetValue (cl_sbar, !cl_sbar->int_val);
			break;
	
		case 14:	// HUD on left side
			Cvar_SetValue (cl_hudswap, !cl_hudswap->int_val);
			break;

		case 16:	// _windowed_mouse
			Cvar_SetValue (_windowed_mouse, !_windowed_mouse->int_val);
			break;
	}
}


void M_DrawSlider (int x, int y, float range)
{
	int	i;

	range = bound (0, range, 1);
	M_DrawCharacter (x-8, y, 128);
	for (i=0 ; i<SLIDER_RANGE ; i++)
		M_DrawCharacter (x + i*8, y, 129);
	M_DrawCharacter (x+i*8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox (int x, int y, int on)
{
#if 0
	if (on)
		M_DrawCharacter (x, y, 131);
	else
		M_DrawCharacter (x, y, 129);
#endif
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}

void M_Options_Draw (void)
{
	float		r;
	qpic_t	*p;
	
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	
	M_Print (16, 32, "    Customize controls");
	M_Print (16, 40, "         Go to console");
	M_Print (16, 48, "     Reset to defaults");

	M_Print (16, 56, "           Screen size");
	r = (scr_viewsize->int_val - 30) / (120 - 30);
	M_DrawSlider (220, 56, r);

	M_Print (16, 64, "            Brightness");
	r = (brightness->value - 1) / 4;
	M_DrawSlider (220, 64, r);

	M_Print (16, 72, "              Contrast");
	r = contrast->value;
	M_DrawSlider (220, 72, r);

	M_Print (16, 80, "           Mouse Speed");
	r = (sensitivity->value - 1) / 24;
	M_DrawSlider (220, 80, r);

	M_Print (16, 88, "       CD Music Volume");
	r = bgmvolume->value;
	M_DrawSlider (220, 88, r);

	M_Print (16, 96, "          Sound Volume");
	r = volume->value;
	M_DrawSlider (220, 96, r);

	M_Print (16, 104,  "            Always Run");
	M_DrawCheckbox (220, 104, cl_forwardspeed->value > 200);

	M_Print (16, 112, "          Invert Mouse");
	M_DrawCheckbox (220, 112, m_pitch->value < 0);

	M_Print (16, 120, "            Lookspring");
	M_DrawCheckbox (220, 120, lookspring->int_val);

	M_Print (16, 128, "            Lookstrafe");
	M_DrawCheckbox (220, 128, lookstrafe->int_val);

	M_Print (16, 136, "    Use old status bar");
	M_DrawCheckbox (220, 136, cl_sbar->int_val);

	M_Print (16, 144, "      HUD on left side");
	M_DrawCheckbox (220, 144, cl_hudswap->int_val);

	if (vid_menudrawfn)
		M_Print (16, 152, "         Video Options");

#ifdef _WIN32
	if (modestate == MS_WINDOWED) {
#endif
		if (_windowed_mouse) {
			M_Print (16, 160, "             Use Mouse");
			M_DrawCheckbox (220, 160, _windowed_mouse->int_val);
		}
#ifdef _WIN32
	}
#endif

// cursor
	M_DrawCharacter (200, 32 + options_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Options_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;
		
	case K_ENTER:
		m_entersound = true;
		switch (options_cursor)
		{
		case 0:
			M_Menu_Keys_f ();
			break;
		case 1:
			m_state = m_none;
			Con_ToggleConsole_f ();
			break;
		case 2:
			Cbuf_AddText ("exec default.cfg\n");
			break;
		case 14:
			if (vid_menudrawfn)
				M_Menu_Video_f ();
			break;
		default:
			M_AdjustSliders (1);
			break;
		}
		return;
	
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
#ifdef _WIN32
		if (options_cursor == 16 && (!(_windowed_mouse) || (modestate != MS_WINDOWED)))
			options_cursor--;
#endif
		if (options_cursor == 15 && !(vid_menudrawfn))
			options_cursor--;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor == 15 && !(vid_menudrawfn))
			options_cursor++;
#ifdef _WIN32
		if (options_cursor == 16 && (!(_windowed_mouse) || (modestate != MS_WINDOWED))) // ARGH!!!!!
			options_cursor++;
#endif
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;	

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		break;
	}
}


//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"impulse 10", 		"change weapon"},
{"+jump", 			"jump / swim up"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
{"+klook", 			"keyboard look"},
{"+moveup",			"swim up"},
{"+movedown",		"swim down"}
};

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

int		keys_cursor;
int		bind_grab;

void M_Menu_Keys_f (void)
{
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
}


void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, "");
	}
}


void M_Keys_Draw (void)
{
	int		i, l;
	int		keys[2];
	char	*name;
	int		x, y;
	qpic_t	*p;

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, backspace to clear");
		
// search for known bindings
	for (i=0 ; i<NUMCOMMANDS ; i++)
	{
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1]);

		l = strlen (bindnames[i][0]);
		
		M_FindKeysForCommand (bindnames[i][0], keys);
		
		if (keys[0] == -1)
		{
			M_Print (140, y, "???");
		}
		else
		{
			name = Key_KeynumToString (keys[0]);
			M_Print (140, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print (140 + x + 8, y, "or");
				M_Print (140 + x + 32, y, Key_KeynumToString (keys[1]));
			}
		}
	}
	
	if (bind_grab)
		M_DrawCharacter (130, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (130, 48 + keys_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Keys_Key (int k)
{
	char	cmd[80];
	int		keys[2];
	
	if (bind_grab)
	{	// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
		{
			bind_grab = false;
		}
		else if (k != '`')
		{
                        snprintf (cmd, sizeof(cmd), "bind %s \"%s\"\n", Key_KeynumToString (k), bindnames[keys_cursor][0]);            
			Cbuf_InsertText (cmd);
		}
		
		bind_grab = false;
		return;
	}
	
	switch (k)
	{
	case K_ESCAPE:
		M_Menu_Options_f ();
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = NUMCOMMANDS-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;

	case K_ENTER:		// go into bind mode
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}

//=============================================================================
/* VIDEO MENU */

void M_Menu_Video_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;
}


void M_Video_Draw (void)
{
	(*vid_menudrawfn) ();
}


void M_Video_Key (int key)
{
	(*vid_menukeyfn) (key);
}

//=============================================================================
/* HELP MENU */

int		help_page;
#define	NUM_HELP_PAGES	6


void M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}



void M_Help_Draw (void)
{
	M_DrawPic (0, 0, Draw_CachePic ( va("gfx/help%i.lmp", help_page)) );
}


void M_Help_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		M_Menu_Main_f ();
		break;
		
	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES-1;
		break;
	}

}

//=============================================================================
/* QUIT MENU */

int		msgNumber;
int		m_quit_prevstate;
qboolean	wasInMenus;

char *quitMessage [] = 
{
/* .........1.........2.... */
  "  Are you gonna quit    ",
  "  this game just like   ",
  "   everything else?     ",
  "                        ",
 
  " Milord, methinks that  ",
  "   thou art a lowly     ",
  " quitter. Is this true? ",
  "                        ",

  " Do I need to bust your ",
  "  face open for trying  ",
  "        to quit?        ",
  "                        ",

  " Man, I oughta smack you",
  "   for trying to quit!  ",
  "     Press Y to get     ",
  "      smacked out.      ",
 
  " Press Y to quit like a ",
  "   big loser in life.   ",
  "  Press N to stay proud ",
  "    and successful!     ",
 
  "   If you press Y to    ",
  "  quit, I will summon   ",
  "  Satan all over your   ",
  "      hard drive!       ",
 
  "  Um, Asmodeus dislikes ",
  " his children trying to ",
  " quit. Press Y to return",
  "   to your Tinkertoys.  ",
 
  "  If you quit now, I'll ",
  "  throw a blanket-party ",
  "   for you next time!   ",
  "                        "
};

void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = rand()&7;
}


void M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case 'Y':
	case 'y':
		key_dest = key_console;
		CL_Disconnect ();
		Sys_Quit ();
		break;

	default:
		break;
	}

}

void M_Menu_SinglePlayer_f (void) {
	m_state = m_singleplayer;
}

void M_SinglePlayer_Draw (void) {
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
//	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
//	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp") );

	M_DrawTextBox (60, 10*8, 23, 4);	
	M_PrintWhite (88, 12*8, "This client is for");
	M_PrintWhite (88, 13*8, "Internet play only");

}

void M_SinglePlayer_Key (key) {
	if (key == K_ESCAPE || key==K_ENTER)
		m_state = m_main;
}

#define MENU_X 50
#define MENU_Y 30
#define STAT_X 50
#define STAT_Y 122

int m_multip_cursor=0;
int m_multip_mins=0;
int m_multip_maxs=10;
int m_multip_horiz;

void M_Menu_MultiPlayer_f (void) {
	key_dest = key_menu;
	m_entersound = true;
	m_state = m_multiplayer;
	m_multip_horiz = 0;
}

void M_MultiPlayer_Draw (void) {
	int serv;
	int line = 1;
	server_entry_t *cp;
	qpic_t *p;
	//int f;

	M_DrawTransPic(16,4,Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320-p->width)/2,4,p);
	
	if (!slist) {
		M_DrawTextBox(60,80,23,4);
		M_PrintWhite(110,12*8,"No server list");
		M_PrintWhite(140,13*8,"found.");
		return;
	}
	M_DrawTextBox(STAT_X,STAT_Y,23,4);
	M_DrawTextBox(STAT_X,STAT_Y+38,23,3);
	M_DrawTextBox(MENU_X,MENU_Y,23,(m_multip_maxs - m_multip_mins)+1);
	for (serv = m_multip_mins; serv <= m_multip_maxs && serv < SL_Len(slist); serv++) {
		cp = SL_Get_By_Num(slist,serv);
		M_Print(MENU_X+18,line*8+MENU_Y,
			va("%1.21s",
			strlen(cp->desc) <= m_multip_horiz ? "" : cp->desc+m_multip_horiz));
		line++;
	}
	cp = SL_Get_By_Num(slist,m_multip_cursor);
	M_PrintWhite(STAT_X+18,STAT_Y+16,"IP/Hostname:");
	M_Print(STAT_X+18,STAT_Y+24,cp->server);
	M_DrawCharacter(MENU_X+8,(m_multip_cursor - m_multip_mins + 1) * 8+MENU_Y,
		12+((int)(realtime*4)&1));
	//f = (int)(realtime * 10) % 6;
	//M_PrintWhite(STAT_X+118,STAT_Y+58,"uakeforge!");
	//M_DrawTransPic(STAT_X+105,STAT_Y+48,Draw_CachePic(va("gfx/menudot%i.lmp",f+1)));
}

void M_MultiPlayer_Key (key) {
	server_entry_t *temp;
	if (!slist && key != K_ESCAPE && key != K_INS)
		return;
	switch(key) {
	case K_ESCAPE:
		M_Menu_Main_f();
		break;
	case KP_DOWNARROW:
	case K_DOWNARROW:
		S_LocalSound("misc/menu1.wav");
		m_multip_cursor++;
		break;
	case KP_UPARROW:
	case K_UPARROW:
		S_LocalSound("misc/menu1.wav");
		m_multip_cursor--;
		break;
	case K_PGUP:
		S_LocalSound("misc/menu1.wav");
		m_multip_cursor -= (m_multip_maxs - m_multip_mins);
		if (m_multip_cursor < 0)
			m_multip_cursor = 0;
		break;
	case K_PGDN:
		S_LocalSound("misc/menu1.wav");
		m_multip_cursor += (m_multip_maxs - m_multip_mins);
		if (SL_Len(slist) - 1  < m_multip_cursor )
			m_multip_cursor = SL_Len(slist) - 1;
		break;
	case K_RIGHTARROW:
		S_LocalSound("misc/menu1.wav");
		if (m_multip_horiz < 256)
			m_multip_horiz++;
		break;
	case K_LEFTARROW:
		S_LocalSound("misc/menu1.wav");
		if (m_multip_horiz > 0 )
			m_multip_horiz--;
		break;
	case K_ENTER:
		m_state = m_main;
		M_ToggleMenu_f();
		CL_Disconnect();
		strncpy(cls.servername,SL_Get_By_Num(slist,m_multip_cursor)->server,sizeof(cls.servername)-1);
		CL_BeginServerConnect();
		break;
	case 'e':
	case 'E':
		M_Menu_SEdit_f();
		break;
	case K_INS:
		S_LocalSound("misc/menu2.wav");
		if (!slist) {
			m_multip_cursor = 0;
			slist = SL_Add(slist, "127.0.0.1","<BLANK>");
		}
		else {
		temp = SL_Get_By_Num(slist,m_multip_cursor);
		slist = SL_InsB(slist, temp, "127.0.0.1","<BLANK>");
		}
		break;
	case K_DEL:
		S_LocalSound("misc/menu2.wav");
		if (SL_Len(slist) > 0) {
			slist = SL_Del(slist, SL_Get_By_Num(slist,m_multip_cursor));
			if (SL_Len(slist) == m_multip_cursor && slist)
				m_multip_cursor--;
		}
		break;
	case ']':
	case '}':
		S_LocalSound("misc/menu1.wav");
		if (m_multip_cursor != SL_Len(slist) - 1) {
			SL_Swap(SL_Get_By_Num(slist,m_multip_cursor),SL_Get_By_Num(slist,m_multip_cursor+1));
			m_multip_cursor++;
		}
		break;
	case '[':
	case '{':
		S_LocalSound("misc/menu1.wav");
		if (m_multip_cursor) {
			SL_Swap(SL_Get_By_Num(slist,m_multip_cursor),SL_Get_By_Num(slist,m_multip_cursor-1));
			m_multip_cursor--;
		}
		break;
	default:
		break;
	}
	if (m_multip_cursor < 0)
		m_multip_cursor = SL_Len(slist) - 1;
	if (m_multip_cursor >= SL_Len(slist))
		m_multip_cursor = 0;
	if (m_multip_cursor < m_multip_mins) {
		m_multip_maxs -= (m_multip_mins - m_multip_cursor);
		m_multip_mins = m_multip_cursor;
	}
	if (m_multip_cursor > m_multip_maxs) {
		m_multip_mins += (m_multip_cursor - m_multip_maxs);
		m_multip_maxs = m_multip_cursor;
	}
}
#define SERV_X 60
#define SERV_Y 64
#define DESC_X 60
#define DESC_Y 40
#define SERV_L 22
#define DESC_L 22

char serv[256];
char desc[256];
int serv_max;
int serv_min;
int desc_max;
int desc_min;
qboolean sedit_state;

void M_Menu_SEdit_f (void) {
	server_entry_t *c;
	key_dest = key_menu;
	m_entersound = true;
	m_state = m_sedit;
	sedit_state = false;
	c = SL_Get_By_Num(slist,m_multip_cursor);
	strncpy(serv,c->server,255);
	serv[strlen(c->server) + 1] = 0;
	strncpy(desc,c->desc,255);
	desc[strlen(c->desc) + 1] = 0;
	serv_max = strlen(serv) > SERV_L ? strlen(serv) : SERV_L;
	serv_min = serv_max - (SERV_L);
	desc_max = strlen(desc) > DESC_L ? strlen(desc) : DESC_L;
	desc_min = desc_max - (DESC_L);
}

void M_SEdit_Draw (void) {
	qpic_t *p;
	
	M_DrawTransPic(16,4,Draw_CachePic("gfx/qplaque.lmp"));
	p = Draw_CachePic("gfx/p_multi.lmp");
	M_DrawPic((320-p->width)/2,4,p);

	M_DrawTextBox(SERV_X,SERV_Y,23,1);
	M_DrawTextBox(DESC_X,DESC_Y,23,1);
	M_PrintWhite(SERV_X,SERV_Y-4,"Hostname/IP:");
	M_PrintWhite(DESC_X,DESC_Y-4,"Description:");
	M_Print(SERV_X+9,SERV_Y+8,va("%1.22s",serv+serv_min));
	M_Print(DESC_X+9,DESC_Y+8,va("%1.22s",desc+desc_min));
	if (sedit_state == 0)
		M_DrawCharacter(SERV_X+9+8*(strlen(serv)-serv_min),
			SERV_Y+8,10+((int)(realtime*4)&1));
	if (sedit_state == 1)
		M_DrawCharacter(DESC_X+9+8*(strlen(desc)-desc_min),
			DESC_Y+8,10+((int)(realtime*4)&1));
}


void M_SEdit_Key (int key) {
	int	l;
	server_entry_t *c;

	switch (key) {
		case K_ESCAPE:
			M_Menu_MultiPlayer_f ();
			break;
		case K_ENTER:
			c = SL_Get_By_Num(slist,m_multip_cursor);
			free(c->server);
			free(c->desc);
			c->server = malloc(strlen(serv) + 1);
			c->desc = malloc(strlen(desc) + 1);
			strcpy(c->server,serv);
			strcpy(c->desc,desc);
			M_Menu_MultiPlayer_f ();
			break;
		case K_UPARROW:
			S_LocalSound("misc/menu1.wav");
			sedit_state = !sedit_state; 
			break;
		case K_DOWNARROW:
			S_LocalSound("misc/menu1.wav");
			sedit_state = !sedit_state; 
			break;
		case K_BACKSPACE:
			if (sedit_state) {
				if ((l = strlen(desc)))
					desc[--l] = 0;
				if (strlen(desc)-6 < desc_min && desc_min) {
					desc_min--;
					desc_max--;
				}
			}
			else {
					if ((l = strlen(serv)))
						serv[--l] = 0;
					if (strlen(serv)-6 < serv_min && serv_min) {
						serv_min--;
						serv_max--;
					}
			}
			break;
		default:
			if (key < 32 || key > 127)
				break;
			if (sedit_state) {
				l = strlen(desc);
				if (l < 254) {
					desc[l+1] = 0;
					desc[l] = key;
				}
				if (strlen(desc) > desc_max) {
					desc_min++;
					desc_max++;
				}
			}
			else {	
				l = strlen(serv);
				if (l < 254) {
					serv[l+1] = 0;
					serv[l] = key;
				}
				if (strlen(serv) > serv_max) {
					serv_min++;
					serv_max++;
				}
			}
			break;
	}
}

void M_Quit_Draw (void)
{
#define VSTR(x) #x
#define VSTR2(x) VSTR(x)
//	char *cmsg[] = {
//    0123456789012345678901234567890123456789
//	"0            QuakeWorld",
//	"1    version " VSTR2(VERSION) " by id Software",
//	"0Programming",
//	"1 John Carmack    Michael Abrash",
//	"1 John Cash       Christian Antkow",
//	"0Additional Programming",
//	"1 Dave 'Zoid' Kirsch",
//	"1 Jack 'morbid' Mathews",
//	"0Id Software is not responsible for",
//	"0providing technical support for",
//	"0QUAKEWORLD(tm). (c)1996 Id Software,",
//	"0Inc.  All Rights Reserved.",
//	"0QUAKEWORLD(tm) is a trademark of Id",
//	"0Software, Inc.",
//	"1NOTICE: THE COPYRIGHT AND TRADEMARK",
//	"1NOTICES APPEARING  IN YOUR COPY OF",
//	"1QUAKE(r) ARE NOT MODIFIED BY THE USE",
//	"1OF QUAKEWORLD(tm) AND REMAIN IN FULL",
//	"1FORCE.",
//	"0NIN(r) is a registered trademark",
//	"0licensed to Nothing Interactive, Inc.",
//	"0All rights reserved. Press y to exit",
//	NULL };
//	char **p;
//	int y;

	if (wasInMenus)
	{
		m_state = m_quit_prevstate;
		m_recursiveDraw = true;
		M_Draw ();
		m_state = m_quit;
	}
#if 0
	M_DrawTextBox (0, 0, 38, 23);
	y = 12;
	for (p = cmsg; *p; p++, y += 8) {
		if (**p == '0')
			M_PrintWhite (16, y, *p + 1);
		else
			M_Print (16, y,	*p + 1);
	}
#else
	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84,  quitMessage[msgNumber*4+0]);
	M_Print (64, 92,  quitMessage[msgNumber*4+1]);
	M_Print (64, 100, quitMessage[msgNumber*4+2]);
	M_Print (64, 108, quitMessage[msgNumber*4+3]);
#endif
}



//=============================================================================
/* Menu Subsystem */


void M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("menu_help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}


void M_Draw (void)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw)
	{
		scr_copyeverything = 1;

		if (scr_con_current)
		{
			Draw_ConsoleBackground (vid.height);
			VID_UnlockBuffer ();
			S_ExtraUpdate ();
			VID_LockBuffer ();
		}
		else
			Draw_FadeScreen ();

		scr_fullupdate = 0;
	}
	else
	{
		m_recursiveDraw = false;
	}

	switch (m_state)
	{
	case m_none:
		break;

	case m_main:
		M_Main_Draw ();
		break;

	case m_singleplayer:
		M_SinglePlayer_Draw ();
		break;

	case m_load:
//		M_Load_Draw ();
		break;

	case m_save:
//		M_Save_Draw ();
		break;

	case m_multiplayer:
		M_MultiPlayer_Draw ();
		break;

	case m_setup:
//		M_Setup_Draw ();
		break;

	case m_net:
//		M_Net_Draw ();
		break;

	case m_options:
		M_Options_Draw ();
		break;

	case m_keys:
		M_Keys_Draw ();
		break;

	case m_video:
		M_Video_Draw ();
		break;

	case m_help:
		M_Help_Draw ();
		break;

	case m_quit:
		M_Quit_Draw ();
		break;

	case m_serialconfig:
//		M_SerialConfig_Draw ();
		break;

	case m_modemconfig:
//		M_ModemConfig_Draw ();
		break;

	case m_lanconfig:
//		M_LanConfig_Draw ();
		break;

	case m_gameoptions:
//		M_GameOptions_Draw ();
		break;

	case m_search:
//		M_Search_Draw ();
		break;

//	case m_slist:
//		M_ServerList_Draw ();
//		break;
	case m_sedit:
		M_SEdit_Draw ();
		break;
	}

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}


void M_Keydown (int key)
{
	switch (m_state)
	{
	case m_none:
		return;

	case m_main:
		M_Main_Key (key);
		return;

	case m_singleplayer:
		M_SinglePlayer_Key (key);
		return;

	case m_load:
//		M_Load_Key (key);
		return;

	case m_save:
//		M_Save_Key (key);
		return;

	case m_multiplayer:
		M_MultiPlayer_Key (key);
		return;

	case m_setup:
//		M_Setup_Key (key);
		return;

	case m_net:
//		M_Net_Key (key);
		return;

	case m_options:
		M_Options_Key (key);
		return;

	case m_keys:
		M_Keys_Key (key);
		return;

	case m_video:
		M_Video_Key (key);
		return;

	case m_help:
		M_Help_Key (key);
		return;

	case m_quit:
		M_Quit_Key (key);
		return;

	case m_serialconfig:
//		M_SerialConfig_Key (key);
		return;

	case m_modemconfig:
//		M_ModemConfig_Key (key);
		return;

	case m_lanconfig:
//		M_LanConfig_Key (key);
		return;

	case m_gameoptions:
//		M_GameOptions_Key (key);
		return;

	case m_search:
//		M_Search_Key (key);
		break;

//	case m_slist:
//		M_ServerList_Key (key);
//		return;
	case m_sedit:
		M_SEdit_Key (key);
		break;
	}
}


