/*
	vid_wgl.c

	Win32 WGL vid component

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

#include "winquake.h"

#include "cdaudio.h"
#include "cmd.h"
#include "console.h"
#include "draw.h"
#include "glquake.h"
#include "host.h"
#include "in_win.h"
#include "keys.h"
#include "qargs.h"
#include "qendian.h"
#include "resource.h"
#include "sbar.h"
#include "screen.h"
#include "sound.h"
#include "sys.h"
#include "va.h"

extern void (*vid_menudrawfn) (void);
extern void (*vid_menukeyfn) (int);

#define MAX_MODE_LIST	30
#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

#define MODE_WINDOWED			0
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

typedef struct {
	modestate_t type;
	int         width;
	int         height;
	int         modenum;
	int         dib;
	int         fullscreen;
	int         bpp;
	int         halfscreen;
	char        modedesc[17];
} vmode_t;

typedef struct {
	int         width;
	int         height;
} lmode_t;

lmode_t     lowresmodes[] = {
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

// ARB Multitexture
qboolean    gl_mtex_capable = false;
GLenum		gl_mtex_enum = GL_TEXTURE0_ARB;

// 8-bit and permedia support
QF_glColorTableEXT	qglColorTableEXT = NULL;
qboolean			is8bit = false;
qboolean			isPermedia = false;

// fixme: Only used by MGL ..
qboolean    DDActive;

// If you ever merge screen/gl_screen, don't forget this one
qboolean    scr_skipupdate;

static vmode_t modelist[MAX_MODE_LIST];
static int  nummodes;
static vmode_t badmode;

static DEVMODE gdevmode;
static qboolean vid_initialized = false;
static qboolean windowed, leavecurrentmode;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
static int  windowed_mouse;
static HICON hIcon;

RECT        WindowRect;
DWORD       WindowStyle, ExWindowStyle;

HWND        mainwindow;

int         vid_modenum = NO_MODE;
int         vid_realmode;
int         vid_default = MODE_WINDOWED;
static int  windowed_default;
unsigned char vid_curpal[256 * 3];
static qboolean fullsbardraw = true;

HDC         maindc;

glvert_t    glv;

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);
LONG	CDAudio_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

extern void CL_ClearStates ();

extern viddef_t vid;					// global video state

unsigned short d_8to16table[256];
unsigned int d_8to24table[256];
unsigned char d_15to8table[65536];

float       gldepthmin, gldepthmax;

modestate_t modestate = MS_UNINIT;

void        VID_MenuDraw (void);
void        VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void        AppActivate (BOOL fActive, BOOL minimize);
char       *VID_GetModeDescription (int mode);
void        ClearAllStates (void);
void        VID_UpdateWindowStatus (void);
void        GL_Init (void);

//====================================

cvar_t     *_windowed_mouse;
cvar_t     *vid_use8bit;


int         window_center_x, window_center_y, window_x, window_y, window_width,

	window_height;
RECT        window_rect;

// direct draw software compatability stuff

void
VID_ForceLockState (int lk)
{
}

void
VID_LockBuffer (void)
{
}

void
VID_UnlockBuffer (void)
{
}

int
VID_ForceUnlockedAndReturnState (void)
{
	return 0;
}

void
D_BeginDirectRect (int x, int y, byte * pbitmap, int width, int height)
{
}

void
D_EndDirectRect (int x, int y, int width, int height)
{
}


void
CenterWindow (HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
	int         CenterX, CenterY;

	CenterX = (GetSystemMetrics (SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics (SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY * 2)
		CenterX >>= 1;					// dual screens
	CenterX = (CenterX < 0) ? 0 : CenterX;
	CenterY = (CenterY < 0) ? 0 : CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

qboolean
VID_SetWindowedMode (int modenum)
{
	HDC         hdc;
	int         lastmodestate, width, height;
	RECT        rect;

	lastmodestate = modestate;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	window_width = modelist[modenum].width;
	window_height = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU |
		WS_MINIMIZEBOX | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	rect = WindowRect;
	AdjustWindowRect (&rect, WindowStyle, FALSE);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the window
	mainwindow = CreateWindow ("QuakeForge", "GLQuakeWorld",
		WindowStyle, rect.left, rect.top, width, height, NULL, NULL,
		global_hInstance, NULL);

	if (!mainwindow)
                Sys_Error ("Couldn't create DIB window (%lx)\r\n",GetLastError());

// Center and show the window
	CenterWindow (mainwindow, WindowRect.right - WindowRect.left,
	WindowRect.bottom - WindowRect.top, false);

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	modestate = MS_WINDOWED;

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
        hdc = GetDC (mainwindow);
	PatBlt (hdc, 0, 0, WindowRect.right, WindowRect.bottom, BLACKNESS);
        ReleaseDC (mainwindow, hdc);

	if (vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if (vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.numpages = 2;

	if(hIcon) {
		SendMessage (mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
		SendMessage (mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);
	}

	return true;
}


qboolean
VID_SetFullDIBMode (int modenum)
{
	HDC         hdc;
	int         lastmodestate, width, height;
	RECT        rect;

        memset(&gdevmode,0,sizeof(gdevmode));

	if (!leavecurrentmode) {
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		gdevmode.dmPelsWidth = modelist[modenum].width <<
			modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;
		gdevmode.dmSize = sizeof (gdevmode);

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) !=
			DISP_CHANGE_SUCCESSFUL)
                                Sys_Error ("Couldn't set fullscreen DIB mode (%lx)\r\n",GetLastError());
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	
	window_width = modelist[modenum].width;
	window_height = modelist[modenum].height;

// fixme: some drivers have broken FS popup window handling
// until I find way around it, or find some other cause for it
// this is way to avoid it

        if (COM_CheckParm ("-brokenpopup")) WindowStyle = 0;
        else WindowStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	rect = WindowRect;
	AdjustWindowRect (&rect, WindowStyle, FALSE);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	mainwindow = CreateWindow ("QuakeForge", "GLQuakeWorld",
		WindowStyle, rect.left, rect.top, width, height, NULL, NULL,
		global_hInstance, NULL);

	if (!mainwindow)
                Sys_Error ("Couldn't create DIB window (%lx)\r\n",GetLastError());

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC (mainwindow);
	PatBlt (hdc, 0, 0, WindowRect.right, WindowRect.bottom, BLACKNESS);
	ReleaseDC (mainwindow, hdc);

	if (vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if (vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.numpages = 2;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	if (hIcon) {
		SendMessage (mainwindow, WM_SETICON, (WPARAM) TRUE, (LPARAM) hIcon);
		SendMessage (mainwindow, WM_SETICON, (WPARAM) FALSE, (LPARAM) hIcon);
	}

	return true;
}


int
VID_SetMode (int modenum, unsigned char *palette)
{
	int         original_mode, temp;
	qboolean    stat = 0;
	MSG         msg;

	if ((windowed && (modenum != 0)) ||
		(!windowed && (modenum < 1)) || (!windowed && (modenum >= nummodes))) {
		Sys_Error ("Bad video mode\n");
	}
// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED) {
		if (_windowed_mouse->int_val && key_dest == key_game) {
			stat = VID_SetWindowedMode (modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		} else {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode (modenum);
		}
	} else if (modelist[modenum].type == MS_FULLDIB) {
		stat = VID_SetFullDIBMode (modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	} else {
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat) {
		Sys_Error ("Couldn't set video mode");
	}
// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	VID_SetPalette (palette);
	vid_modenum = modenum;

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	if (!msg_suppress_1)
		Con_Printf ("Video mode %s initialized.\n",
					VID_GetModeDescription (vid_modenum));

	VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	return true;
}


/*
	VID_UpdateWindowStatus
*/
void
VID_UpdateWindowStatus (void)
{

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}

int         texture_extension_number = 1;

void
CheckMultiTextureExtensions (void)
{
	Con_Printf ("Checking for multitexture... ");
	if (COM_CheckParm ("-nomtex")) {
		Con_Printf ("disabled.\n");
		return;
	}

	if (QFGL_ExtensionPresent ("GL_ARB_multitexture")) {

		int max_texture_units = 0;

		glGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &max_texture_units);
		if (max_texture_units >= 2) {
			Con_Printf ("enabled, %d TMUs.\n", max_texture_units);
			qglMultiTexCoord2f = QFGL_ExtensionAddress ("glMultiTexCoord2fARB");
			qglActiveTexture = QFGL_ExtensionAddress ("glActiveTextureARB");
			gl_mtex_enum = GL_TEXTURE0_ARB;
			gl_mtex_capable = true;
		} else {
			Con_Printf ("disabled, not enough TMUs.\n");
		}
	} else {
		Con_Printf ("not found.\n");
	}
}

/*
	GL_Init
*/
void
GL_Init (void)
{
	gl_vendor = glGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = glGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glGetString (GL_EXTENSIONS);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

//  Con_Printf ("%s %s\n", gl_renderer, gl_version);

	if (strnicmp (gl_renderer, "PowerVR", 7) == 0)
		fullsbardraw = true;

	if (strnicmp (gl_renderer, "Permedia", 8) == 0)
		isPermedia = true;

	CheckMultiTextureExtensions ();

	glClearColor (0, 0, 0, 0);
	glCullFace (GL_FRONT);
	glEnable (GL_TEXTURE_2D);

	glEnable (GL_ALPHA_TEST);
	glAlphaFunc (GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glEnable (GL_BLEND);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

}

/*
	GL_BeginRendering
*/
void
GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;
}

void
GL_EndRendering (void)
{
	if (!scr_skipupdate || block_drawing)
		SwapBuffers (maindc);

// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED) {
		if (!_windowed_mouse->int_val) {
			if (windowed_mouse) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		} else {
			windowed_mouse = true;
			if (key_dest == key_game && !mouseactive && ActiveApp) {
				IN_ActivateMouse ();
				IN_HideMouse ();
			} else if (mouseactive && key_dest != key_game) {
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
		}
	}
	if (fullsbardraw)
		Sbar_Changed ();
}

void
VID_SetPalette (unsigned char *palette)
{
	byte       *pal;
	unsigned int r, g, b;
	unsigned int v;
	int         r1, g1, b1;
	int         k;
	unsigned short i;
	unsigned int *table;
	QFile      *f;
	char        s[255];
	float       dist, bestdist;
	static qboolean palflag = false;

//
// 8 8 8 encoding
//
//      Con_Printf("Converting 8to24\n");

	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 256; i++) {
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

//      v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
//      v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		*table++ = v;
	}
	d_8to24table[255] &= 0;				// 255 is transparent

	// JACK: 3D distance calcs - k is last closest, l is the distance.

	if (palflag)
		return;
	palflag = true;

	COM_FOpenFile ("glquake/15to8.pal", &f);
	if (f) {
		Qread (f, d_15to8table, 1 << 15);
		Qclose (f);
	} else {
		for (i = 0; i < (1 << 15); i++) {
			/* Maps 000000000000000 000000000011111 = Red  = 0x1F
			   000001111100000 = Blue = 0x03E0 111110000000000 = Grn  =
			   0x7C00 */
			r = ((i & 0x1F) << 3) + 4;
			g = ((i & 0x03E0) >> 2) + 4;
			b = ((i & 0x7C00) >> 7) + 4;
			pal = (unsigned char *) d_8to24table;
			for (v = 0, k = 0, bestdist = 10000.0; v < 256; v++, pal += 4) {
				r1 = (int) r - (int) pal[0];
				g1 = (int) g - (int) pal[1];
				b1 = (int) b - (int) pal[2];
				dist = sqrt (((r1 * r1) + (g1 * g1) + (b1 * b1)));
				if (dist < bestdist) {
					k = v;
					bestdist = dist;
				}
			}
			d_15to8table[i] = k;
		}
		snprintf (s, sizeof (s), "%s/glquake/15to8.pal", com_gamedir);
		COM_CreatePath (s);
		if ((f = Qopen (s, "wb")) != NULL) {
			Qwrite (f, d_15to8table, 1 << 15);
			Qclose (f);
		}
	}
}

void
VID_ShiftPalette (unsigned char *palette)
{
}

void
VID_SetDefaultMode (void)
{
	IN_DeactivateMouse ();
}


void
VID_Shutdown (void)
{
	HGLRC       hRC;
	HDC         hDC;
	int         i, temp[8192];

#ifdef SPLASH_SCREEN
        if(hwnd_dialog)
                DestroyWindow (hwnd_dialog);
#endif

	if (vid_initialized) {
		vid_canalttab = false;
		hRC = wglGetCurrentContext ();
		hDC = wglGetCurrentDC ();

		wglMakeCurrent (NULL, NULL);

		// LordHavoc: free textures before closing (may help NVIDIA)
		for (i = 0; i < 8192; i++)
                        temp[i] = i + 1;
		glDeleteTextures (8192, temp);

		if (hRC)
			wglDeleteContext (hRC);

                if (hDC && mainwindow)
                        ReleaseDC (mainwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

                if (maindc && mainwindow)
                        ReleaseDC (mainwindow, maindc);

		AppActivate (false, false);
	}
}


//==========================================================================

BOOL
bSetupPixelFormat (HDC hDC)
{
	PIXELFORMATDESCRIPTOR pfd ;
	int         pixelformat;

	memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR); 
	pfd.nVersion   = 1;
	pfd.dwFlags    = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
	pfd.iPixelType = PFD_TYPE_RGBA;

	if (!modelist[vid_default].bpp) pfd.cColorBits = 16;
	else pfd.cColorBits = modelist[vid_default].bpp;

	pfd.cDepthBits = 32;
	pfd.iLayerType = PFD_MAIN_PLANE;

	if ((pixelformat = ChoosePixelFormat (hDC, &pfd)) == 0) {
		MessageBox (NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (SetPixelFormat (hDC, pixelformat, &pfd) == FALSE) {
		MessageBox (NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}
	return TRUE;
}


//==========================================================================

byte        scantokey[128] = {
//  0       1        2       3       4       5       6       7
//  8       9        A       B       C       D       E       F
	0, 27, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', K_BACKSPACE, 9,	// 0
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', 13, K_CTRL, 'a', 's',	// 1
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', K_SHIFT, '\\', 'z', 'x', 'c', 'v',	// 2
	'b', 'n', 'm', ',', '.', '/', K_SHIFT, KP_MULTIPLY,
	K_ALT, ' ', K_CAPSLOCK, K_F1, K_F2, K_F3, K_F4, K_F5,	// 3
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE, K_SCRLCK, KP_HOME,
	KP_UPARROW, KP_PGUP, KP_MINUS, KP_LEFTARROW, KP_5, KP_RIGHTARROW, KP_PLUS, KP_END,	// 4
	KP_DOWNARROW, KP_PGDN, KP_INS, KP_DEL, 0, 0, 0, K_F11,
	K_F12, 0, 0, 0, 0, 0, 0, 0,			// 5
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

byte        extscantokey[128] = {
//  0       1        2       3       4       5       6       7
//  8       9        A       B       C       D       E       F
	0, 27, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', K_BACKSPACE, 9,	// 0
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', KP_ENTER, K_CTRL, 'a', 's',	// 1
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	'\'', '`', K_SHIFT, '\\', 'z', 'x', 'c', 'v',	// 2
	'b', 'n', 'm', ',', '.', KP_DIVIDE, K_SHIFT, '*',
	K_ALT, ' ', K_CAPSLOCK, K_F1, K_F2, K_F3, K_F4, K_F5,	// 3
	K_F6, K_F7, K_F8, K_F9, K_F10, KP_NUMLCK, 0, K_HOME,
	K_UPARROW, K_PGUP, '-', K_LEFTARROW, '5', K_RIGHTARROW, '+', K_END,	// 4
	K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0, 0, 0, K_F11,
	K_F12, 0, 0, 0, 0, 0, 0, 0,			// 5
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};



/*
	MapKey

	Map from windows to quake keynums
*/
int
MapKey (int key)
{
	int         extended;

	extended = (key >> 24) & 1;

	key = (key >> 16) & 255;
	if (key > 127)
		return 0;

	if (extended)
		return extscantokey[key];
	else
		return scantokey[key];
}


/*
	MAIN WINDOW
*/


/*
	ClearAllStates
*/
void
ClearAllStates (void)
{
	CL_ClearStates ();
	Key_ClearStates ();
	IN_ClearStates ();
}

void
AppActivate (BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
	static BOOL sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active) {
		S_BlockSound ();
		sound_active = false;
	} else if (ActiveApp && !sound_active) {
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive) {
		if (modestate == MS_FULLDIB) {
			IN_ActivateMouse ();
			IN_HideMouse ();
			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = false;

				if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) !=
					DISP_CHANGE_SUCCESSFUL) {
					IN_ShowMouse ();
                                        Sys_Error ("Couldn't set fullscreen DIB mode\n(try upgrading your video drivers)\r\n (%lx)",GetLastError());
				}
				ShowWindow (mainwindow, SW_SHOWNORMAL);

				// Fix for alt-tab bug in NVidia drivers
				MoveWindow(mainwindow,0,0,gdevmode.dmPelsWidth,gdevmode.dmPelsHeight,false);
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse->int_val
			&& key_dest == key_game) {
			IN_ActivateMouse ();
			IN_HideMouse ();
		}

	} else {
		if (modestate == MS_FULLDIB) {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			if (vid_canalttab) {
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		} else if ((modestate == MS_WINDOWED) && _windowed_mouse->int_val) {
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}
	}
}


/* main window procedure */
LONG WINAPI
MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG        lRet = 1;
	int         fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;

	if (uMsg == uiWheelMessage)
		uMsg = WM_MOUSEWHEEL;

	switch (uMsg) {
			case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow (mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			window_x = (int) LOWORD (lParam);
			window_y = (int) HIWORD (lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			Key_Event (MapKey (lParam), -1, true);
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			Key_Event (MapKey (lParam), -1, false);
			break;

		case WM_SYSCHAR:
			// keep Alt-Space from happening
			break;

			// this is complicated because Win32 seems to pack multiple mouse 
			// events into
			// one update sometimes, so we always check all states and look
			// for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			IN_MouseEvent (temp);

			break;

			// JACK: This is the mouse wheel with the Intellimouse
			// Its delta is either positive or neg, and we generate the
			// proper
			// Event.
		case WM_MOUSEWHEEL:
			if ((short) HIWORD (wParam) > 0) {
				Key_Event (K_MWHEELUP, -1, true);
				Key_Event (K_MWHEELUP, -1, false);
			} else {
				Key_Event (K_MWHEELDOWN, -1, true);
				Key_Event (K_MWHEELDOWN, -1, false);
			}
			break;

		case WM_SIZE:
			break;

		case WM_CLOSE:
			if (MessageBox
				(mainwindow, "Are you sure you want to quit?", "Confirm Exit",
				 MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES) {
				Sys_Quit ();
			}

			break;

		case WM_ACTIVATE:
			fActive = LOWORD (wParam);
			fMinimized = (BOOL) HIWORD (wParam);
			AppActivate (!(fActive == WA_INACTIVE), fMinimized);
			// fix the leftover Alt from any Alt-Tab or the like that
			// switched us away
			ClearAllStates ();

			break;

		case WM_DESTROY:
			if (mainwindow)
				DestroyWindow (mainwindow);
			PostQuitMessage (0);
			break;

		case MM_MCINOTIFY:
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

		default:
			/* pass all unhandled messages to DefWindowProc */
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}


/*
	VID_NumModes
*/
int
VID_NumModes (void)
{
	return nummodes;
}


/*
	VID_GetModePtr
*/
vmode_t    *
VID_GetModePtr (int modenum)
{

	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
	VID_GetModeDescription
*/
char       *
VID_GetModeDescription (int mode)
{
	char       *pinfo;
	vmode_t    *pv;
	static char temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode) {
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	} else {
		snprintf (temp, sizeof (temp), "Desktop resolution (%dx%d)",
				  modelist[MODE_FULLSCREEN_DEFAULT].width,
				  modelist[MODE_FULLSCREEN_DEFAULT].height);
		pinfo = temp;
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

char       *
VID_GetExtModeDescription (int mode)
{
	static char pinfo[40];
	vmode_t    *pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB) {
		if (!leavecurrentmode) {
			snprintf (pinfo, sizeof (pinfo), "%s fullscreen", pv->modedesc);
		} else {
			snprintf (pinfo, sizeof (pinfo), "Desktop resolution (%dx%d)",
					  modelist[MODE_FULLSCREEN_DEFAULT].width,
					  modelist[MODE_FULLSCREEN_DEFAULT].height);
		}
	} else {
		if (modestate == MS_WINDOWED)
			snprintf (pinfo, sizeof (pinfo), "%s windowed", pv->modedesc);
		else
			snprintf (pinfo, sizeof (pinfo), "windowed");
	}

	return pinfo;
}


/*
	VID_DescribeCurrentMode_f
*/
void
VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
	VID_NumModes_f
*/
void
VID_NumModes_f (void)
{

	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", nummodes);
	else
		Con_Printf ("%d video modes are available\n", nummodes);
}


/*
	VID_DescribeMode_f
*/
void
VID_DescribeMode_f (void)
{
	int         t, modenum;

	modenum = atoi (Cmd_Argv (1));

	t = leavecurrentmode;
	leavecurrentmode = 0;

	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));

	leavecurrentmode = t;
}


/*
	VID_DescribeModes_f
*/
void
VID_DescribeModes_f (void)
{
	int         i, lnummodes, t;
	char       *pinfo;
	vmode_t    *pv;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i = 1; i < lnummodes; i++) {
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);
		Con_Printf ("%2d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}


void
VID_InitDIB (HINSTANCE hInstance)
{
	WNDCLASS    wc;

	/* Register the frame class */
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC) MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = 0;
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = 0;
	wc.lpszClassName = "QuakeForge";

	if (!RegisterClass (&wc))
                Sys_Error ("Couldn't register window class (%lx)\r\n",GetLastError());

	modelist[0].type = MS_WINDOWED;

	if (COM_CheckParm ("-width"))
		modelist[0].width = atoi (com_argv[COM_CheckParm ("-width") + 1]);
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if (COM_CheckParm ("-height"))
		modelist[0].height = atoi (com_argv[COM_CheckParm ("-height") + 1]);
	else
		modelist[0].height = modelist[0].width * 240 / 320;

	if (modelist[0].height < 240)
		modelist[0].height = 240;

	snprintf (modelist[0].modedesc, sizeof (modelist[0].modedesc), "%dx%d",
			  modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 0;

	nummodes = 1;
}


/*
	VID_InitFullDIB
*/
void
VID_InitFullDIB (HINSTANCE hInstance)
{
	DEVMODE     devmode;
	int         i, modenum, originalnummodes, existingmode, numlowresmodes;
	int         j, bpp, done;
	BOOL        stat;

// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do {
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmBitsPerPel >= 15) &&
			(devmode.dmPelsWidth <= MAXWIDTH) &&
			(devmode.dmPelsHeight <= MAXHEIGHT) && (nummodes < MAX_MODE_LIST)) {
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
				DISP_CHANGE_SUCCESSFUL) {
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				snprintf (modelist[nummodes].modedesc,
						  sizeof (modelist[nummodes].modedesc), "%dx%dx%d",
						  devmode.dmPelsWidth, devmode.dmPelsHeight,
						  devmode.dmBitsPerPel);

				// if the width is more than twice the height, reduce it by
				// half because this
				// is probably a dual-screen monitor
				if (!COM_CheckParm ("-noadjustaspect")) {
					if (modelist[nummodes].width >
						(modelist[nummodes].height << 1)) {
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						snprintf (modelist[nummodes].modedesc,
								  sizeof (modelist[nummodes].modedesc),
								  "%dx%dx%d", modelist[nummodes].width,
								  modelist[nummodes].height,
								  modelist[nummodes].bpp);
					}
				}

				for (i = originalnummodes, existingmode = 0; i < nummodes; i++) {
					if ((modelist[nummodes].width == modelist[i].width) &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp)) {
						existingmode = 1;
						break;
					}
				}

				if (!existingmode) {
					nummodes++;
				}
			}
		}

		modenum++;
	} while (stat);

// see if there are any low-res modes that aren't being reported
	numlowresmodes = sizeof (lowresmodes) / sizeof (lowresmodes[0]);
	bpp = 16;
	done = 0;

	do {
		for (j = 0; (j < numlowresmodes) && (nummodes < MAX_MODE_LIST); j++) {
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
				DISP_CHANGE_SUCCESSFUL) {
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				snprintf (modelist[nummodes].modedesc,
						  sizeof (modelist[nummodes].modedesc), "%dx%dx%d",
						  devmode.dmPelsWidth, devmode.dmPelsHeight,
						  devmode.dmBitsPerPel);

				for (i = originalnummodes, existingmode = 0; i < nummodes; i++) {
					if ((modelist[nummodes].width == modelist[i].width) &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp)) {
						existingmode = 1;
						break;
					}
				}

				if (!existingmode) {
					nummodes++;
				}
			}
		}
		switch (bpp) {
			case 16:
				bpp = 32;
				break;

			case 32:
				bpp = 24;
				break;

			case 24:
				done = 1;
				break;
		}
	} while (!done);

	if (nummodes == originalnummodes)
		Con_Printf ("No fullscreen DIB modes found\n");
}

qboolean
VID_Is8bit ()
{
	return is8bit;
}

void
VID_Init8bitPalette (void)
{
	int 		i;
	GLubyte 	thePalette[256 * 3];
	GLubyte		*oldPalette, *newPalette;

	vid_use8bit =
		Cvar_Get ("vid_use8bit", "0", CVAR_ROM, NULL, "Use 8-bit shared palettes.");

        if (!vid_use8bit->int_val) return;

        if (is8bit) {           // already done...
		return;
	}

	if (QFGL_ExtensionPresent ("GL_EXT_shared_texture_palette")) {
		if (!(qglColorTableEXT = QFGL_ExtensionAddress ("glColorTableEXT"))) {
			return;
		}

		Con_Printf ("8-bit GL extension enabled.\n");

		glEnable (GL_SHARED_TEXTURE_PALETTE_EXT);
		oldPalette = (GLubyte *) d_8to24table;	// d_8to24table3dfx;
		newPalette = thePalette;
		for (i = 0; i < 256; i++) {
			*newPalette++ = *oldPalette++;
			*newPalette++ = *oldPalette++;
			*newPalette++ = *oldPalette++;
			oldPalette++;
		}
		qglColorTableEXT (GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256, GL_RGB,
							GL_UNSIGNED_BYTE, (GLvoid *) thePalette);
		is8bit = true;
	}
}

/*
	VID_Init
*/
void
VID_Init (unsigned char *palette)
{
	int         i, existingmode;
	int         basenummodes, width, height=480, bpp, findbpp, done;
	char        gldir[MAX_OSPATH];
	HDC         hdc;
	DEVMODE     devmode;
	HGLRC       baseRC;
	DWORD lasterror;

	memset (&devmode, 0, sizeof (devmode));

	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f, "Reports the total number of video modes available");
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f, "Report current video mode.");
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f, "Report information on specified video mode, default is current.\n"
		"(vid_describemode (mode))");
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f, "Report information on all video modes.");

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON1));

// fixme: If you put these back, remember commctrl.h
//        InitCommonControls ();

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes = 1;

	VID_InitFullDIB (global_hInstance);

	if (COM_CheckParm ("-window")) {
		hdc = GetDC (NULL);

		if (GetDeviceCaps (hdc, RASTERCAPS) & RC_PALETTE) {
			Sys_Error ("Can't run in non-RGB mode");
		}

		ReleaseDC (NULL, hdc);

		windowed = true;

		vid_default = MODE_WINDOWED;
	} else {
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if (COM_CheckParm ("-mode")) {
			vid_default = atoi (com_argv[COM_CheckParm ("-mode") + 1]);
		} else {
			if (COM_CheckParm ("-current")) {
				modelist[MODE_FULLSCREEN_DEFAULT].width =
					GetSystemMetrics (SM_CXSCREEN);
				modelist[MODE_FULLSCREEN_DEFAULT].height =
					GetSystemMetrics (SM_CYSCREEN);
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			} else {
				if (COM_CheckParm ("-width")) {
					width = atoi (com_argv[COM_CheckParm ("-width") + 1]);
				} else {
					width = 640;
				}

				if (COM_CheckParm ("-bpp")) {
					bpp = atoi (com_argv[COM_CheckParm ("-bpp") + 1]);
					findbpp = 0;
				} else {
					bpp = 15;
					findbpp = 1;
				}

				if (COM_CheckParm ("-height"))
					height = atoi (com_argv[COM_CheckParm ("-height") + 1]);

				// if they want to force it, add the specified mode to the
				// list
				if (COM_CheckParm ("-force") && (nummodes < MAX_MODE_LIST)) {
					modelist[nummodes].type = MS_FULLDIB;
					modelist[nummodes].width = width;
					modelist[nummodes].height = height;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = bpp;
					snprintf (modelist[nummodes].modedesc,
							  sizeof (modelist[nummodes].modedesc), "%dx%dx%d",
							  devmode.dmPelsWidth, devmode.dmPelsHeight,
							  devmode.dmBitsPerPel);

					for (i = nummodes, existingmode = 0; i < nummodes; i++) {
						if ((modelist[nummodes].width == modelist[i].width) &&
							(modelist[nummodes].height == modelist[i].height) &&
							(modelist[nummodes].bpp == modelist[i].bpp)) {
							existingmode = 1;
							break;
						}
					}

					if (!existingmode) {
						nummodes++;
					}
				}

				done = 0;

				do {
					if (COM_CheckParm ("-height")) {
						height = atoi (com_argv[COM_CheckParm ("-height") + 1]);

						for (i = 1, vid_default = 0; i < nummodes; i++) {
							if ((modelist[i].width == width) &&
								(modelist[i].height == height) &&
								(modelist[i].bpp == bpp)) {
								vid_default = i;
								done = 1;
								break;
							}
						}
					} else {
						for (i = 1, vid_default = 0; i < nummodes; i++) {
							if ((modelist[i].width == width)
								&& (modelist[i].bpp == bpp)) {
								vid_default = i;
								done = 1;
								break;
							}
						}
					}

					if (!done) {
						if (findbpp) {
							switch (bpp) {
								case 15:
									bpp = 16;
									break;
								case 16:
									bpp = 32;
									break;
								case 32:
									bpp = 24;
									break;
								case 24:
									done = 1;
									break;
							}
						} else {
							done = 1;
						}
					}
				} while (!done);

				if (!vid_default) {
					Sys_Error ("Specified video mode not available");
				}
			}
		}
	}

	vid_initialized = true;

	if ((i = COM_CheckParm ("-conwidth")) != 0)
		vid.conwidth = atoi (com_argv[i + 1]);
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8;				// make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	if ((i = COM_CheckParm ("-conheight")) != 0)
		vid.conheight = atoi (com_argv[i + 1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap + 2048));

#ifdef SPLASH_SCREEN
        if(hwnd_dialog)
                DestroyWindow (hwnd_dialog);
#endif

	VID_InitGamma (palette);
	VID_SetPalette (palette);

	VID_SetMode (vid_default, palette);

	maindc = GetDC (mainwindow);
	bSetupPixelFormat (maindc);

	baseRC = wglCreateContext (maindc);
	if (!baseRC) {
		lasterror=GetLastError();
		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);
		Sys_Error("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window. \nError code: (%lx)\r\n",lasterror);
	}

	if (!wglMakeCurrent (maindc, baseRC)) {
		lasterror=GetLastError();
		if (baseRC)
			wglDeleteContext (baseRC);
                if (maindc && mainwindow)
       	                ReleaseDC (mainwindow, maindc);
		Sys_Error ("wglMakeCurrent failed (%lx)\r\n",lasterror);
	}

	GL_Init ();

	snprintf (gldir, sizeof (gldir), "%s/glquake", com_gamedir);
	Sys_mkdir (gldir);

	vid_realmode = vid_modenum;

	// Check for 3DFX Extensions and initialize them.
	VID_Init8bitPalette ();

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode");
	vid_canalttab = true;

	if (COM_CheckParm ("-nofullsbar"))
		fullsbardraw = false;
}

void
VID_Init_Cvars ()
{
	_windowed_mouse = Cvar_Get ("_windowed_mouse", "0", CVAR_ARCHIVE, NULL, "Grab the mouse from X while playing quake");
}


//========================================================
// Video menu stuff
//========================================================

extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);

static int  vid_wmodes;

typedef struct {
	int         modenum;
	char       *desc;
	int         iscur;
} modedesc_t;

#define MAX_COLUMN_SIZE		9
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 2)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE*3)

static modedesc_t modedescs[MAX_MODEDESCS];

/*
	VID_MenuDraw
*/
void
VID_MenuDraw (void)
{
	qpic_t     *p;
	char       *ptr;
	int         lnummodes, i, k, column, row;
	vmode_t    *pv;

	p = Draw_CachePic ("gfx/vidmodes.lmp", true);
	M_DrawPic ((320 - p->width) / 2, 4, p);

	vid_wmodes = 0;
	lnummodes = VID_NumModes ();

	for (i = 1; (i < lnummodes) && (vid_wmodes < MAX_MODEDESCS); i++) {
		ptr = VID_GetModeDescription (i);
		pv = VID_GetModePtr (i);

		k = vid_wmodes;

		modedescs[k].modenum = i;
		modedescs[k].desc = ptr;
		modedescs[k].iscur = 0;

		if (i == vid_modenum)
			modedescs[k].iscur = 1;

		vid_wmodes++;

	}

	if (vid_wmodes > 0) {
		M_Print (2 * 8, 36 + 0 * 8, "Fullscreen Modes (WIDTHxHEIGHTxBPP)");

		column = 8;
		row = 36 + 2 * 8;

		for (i = 0; i < vid_wmodes; i++) {
			if (modedescs[i].iscur)
				M_PrintWhite (column, row, modedescs[i].desc);
			else
				M_Print (column, row, modedescs[i].desc);

			column += 13 * 8;

			if ((i % VID_ROW_SIZE) == (VID_ROW_SIZE - 1)) {
				column = 8;
				row += 8;
			}
		}
	}

	M_Print (3 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 2,
			 "Video modes must be set from the");
	M_Print (3 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 3,
			 "command line with -width <width>");
	M_Print (3 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 4,
			 "and -bpp <bits-per-pixel>");
	M_Print (3 * 8, 36 + MODE_AREA_HEIGHT * 8 + 8 * 6,
			 "Select windowed mode with -window");
}


/*
	VID_MenuKey
*/
void
VID_MenuKey (int key)
{
	switch (key) {
		case K_ESCAPE:
			S_LocalSound ("misc/menu1.wav");
			M_Menu_Options_f ();
			break;

		default:
			break;
	}
}

void
VID_SetCaption (char *text)
{
	if (text && *text) {
		char *temp = strdup (text);

		SetWindowText (mainwindow,
			(LPSTR) va ("%s %s: %s", PROGRAM, VERSION, temp));
		free (temp);
	} else {
		SetWindowText (mainwindow, (LPSTR) va ("%s %s", PROGRAM, VERSION));
	}
}

qboolean
VID_SetGamma (double gamma)
{
	    return false; //FIXME
}
