## Process this file with automake to produce Makefile.in
#
#	Makefile.am
#
#	Automake-using build system for QuakeForge
#
#	Copyright (C) 2000 Jeff Teunissen <deek@quakeforge.net>
#
#	This Makefile is free software; you can redistribute it and/or
#	modify it under the terms of the GNU General Public License
#	as published by the Free Software Foundation; either version 2
#	of the License, or (at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#	See the GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program; if not, write to:
#
#		Free Software Foundation, Inc.
#		59 Temple Place - Suite 330
#		Boston, MA  02111-1307, USA
#
#	$Id$
#
AUTOMAKE_OPTIONS=	foreign

#
# Stuff that is common to both client and server
#
INCLUDES=	-I$(top_srcdir)/include $(GGI_CFLAGS) $(MGL_CFLAGS) $(SVGA_CFLAGS) $(X_CFLAGS) $(GLX_CFLAGS) $(TDFXGL_CFLAGS)
AR=@AR@
bin_PROGRAMS =	@TARGETS@

EXTRA_PROGRAMS= qf-server \
		qf-client-3dfx qf-client-fbdev qf-client-ggi qf-client-glx \
		qf-client-mgl qf-client-sdl qf-client-sgl qf-client-svga \
		qf-client-wgl qf-client-x11

noinst_LIBRARIES= libqfcd.a libqfjs.a libqfnet.a libqfsnd.a libqfsys_cl.a libqfsys_sv.a

if PACKETLOG
packetlogger = net_packetlog.c
endif

if ASM_ARCH
math_ASM = math.S sys_x86.S
world_ASM=	worlda.S
endif
common_sources= buildnum.c checksum.c cmd.c com.c crc.c cvar.c hash.c \
	info.c link.c mathlib.c mdfour.c model.c model_brush.c \
	msg.c pmove.c pmovetst.c qargs.c qendian.c quakefs.c quakeio.c \
	sizebuf.c sys.c va.c zone.c $(math_ASM) $(packetlogger) $(world_ASM)

#
# ... Network type
#
if NETTYPE_IPV6
libqfnet_a_SOURCES=	net_chan.c net_com.c net_udp6.c
else
libqfnet_a_SOURCES=	net_chan.c net_com.c net_udp.c
endif
EXTRA_libqfnet_a_SOURCES= net_chan.c net_com.c net_udp.c net_udp6.c

#
# Server builds
#
# ... System type
#
if SYSTYPE_WIN32
libqfsys_sv_a_SOURCES=	fnmatch.c dirent.c sv_sys_win.c
else
libqfsys_sv_a_SOURCES=	sv_sys_unix.c
endif
EXTRA_libqfsys_sv_a_SOURCES= fnmatch.c dirent.c sv_sys_unix.c sv_sys_win.c sys.c sys.c

server_sources=	pr_edict.c pr_exec.c pr_offs.c sv_ccmds.c sv_cvar.c \
		sv_ents.c sv_init.c sv_main.c sv_misc.c sv_model.c \
		sv_move.c sv_nchan.c sv_phys.c sv_pr_cmds.c sv_progs.c sv_send.c \
		sv_user.c ver_check.c world.c

qf_server_SOURCES=	$(common_sources) $(server_sources)
qf_server_LDADD=	-L. -lqfnet -lqfsys_sv $(NET_LIBS) $(Z_LIBS) $(DL_LIBS)
qf_server_DEPENDENCIES= libqfnet.a libqfsys_sv.a

#
# Client builds
#
# ... System type
#
if SYSTYPE_WIN32
libqfsys_cl_a_SOURCES=	fnmatch.c dirent.c
else
libqfsys_cl_a_SOURCES=	
endif
EXTRA_libqfsys_cl_a_SOURCES= cl_sys_sdl.c cl_sys_unix.c cl_sys_win.c sys.c sys.c fnmatch.c dirent.c

#
# ... Digital audio
#
if SNDTYPE_ALSA_0_5
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_5.c
else
if SNDTYPE_ALSA_0_9
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_9.c
else
if SNDTYPE_MME
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_mme.c
else
if SNDTYPE_OSS
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_oss.c
else
if SNDTYPE_SDL
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_sdl.c
libqfsnd_a_INCLUDES= $(SDL_CFLAGS)
else
if SNDTYPE_SGI
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_sgi.c
else
if SNDTYPE_SUN
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_sun.c
else
if SNDTYPE_WIN32
libqfsnd_a_SOURCES=	snd_dma.c snd_mem.c snd_mix.c snd_win.c
else
libqfsnd_a_SOURCES=	snd_null.c
endif
endif
endif
endif
endif
endif
endif
endif
EXTRA_libqfsnd_a_SOURCES= snd_dma.c snd_mem.c snd_mix.c snd_alsa_0_5.c \
			snd_alsa_0_9.c snd_oss.c snd_sdl.c snd_sgi.c \
			snd_sun.c snd_win.c snd_null.c

#
# ... CD audio
#
if CDTYPE_LINUX
libqfcd_a_SOURCES=	cd_linux.c
else
if CDTYPE_SDL
libqfcd_a_SOURCES=	cd_sdl.c
libqfcd_a_INCLUDES=$(SDL_CFLAGS)
else
if CDTYPE_SGI
libqfcd_a_SOURCES=	cd_sgi.c
else
if CDTYPE_WIN32
libqfcd_a_SOURCES=	cd_win.c
else
libqfcd_a_SOURCES=	cd_null.c
endif
endif
endif
endif
EXTRA_libqfcd_a_SOURCES= cd_dos.c cd_linux.c cd_sdl.c cd_sgi.c cd_win.c cd_null.c

#
# ... Joystick
#
if JOYTYPE_LINUX
libqfjs_a_SOURCES=	joy_linux.c
else
if JOYTYPE_WIN32
libqfjs_a_SOURCES=	joy_win.c
else
libqfjs_a_SOURCES=	joy_null.c
endif
endif
libqfjs_a_CFLAGS=	$(JOY_CFLAGS)
EXTRA_libqfjs_a_SOURCES= joy_linux.c joy_win.c joy_null.c

CLIENT_LIBS=	-L. -lqfnet -lqfsys_cl -lqfsnd -lqfcd -lqfjs $(SOUND_LIBS) $(NET_LIBS) $(CD_LIBS) $(JOY_LIBS) $(Z_LIBS)

if ASM_ARCH
client_ASM=	snd_mixa.S cl_math.S
endif

client_sources= cl_cam.c cl_cmd.c cl_cvar.c cl_demo.c cl_ents.c cl_input.c \
	cl_main.c cl_misc.c cl_parse.c cl_pred.c cl_slist.c cl_tent.c \
	console.c keys.c locs.c menu.c model_alias.c model_sprite.c nonintel.c \
	pcx.c r_view.c sbar.c skin.c teamplay.c tga.c wad.c vid.c $(client_ASM)

#
# Software-rendering clients
#
# ... Common stuff
#
if ASM_ARCH
soft_ASM=	d_draw.S d_draw16.S d_parta.S d_polysa.S d_scana.S d_spr8.S \
		d_varsa.S r_aclipa.S r_aliasa.S r_drawa.S r_edgea.S r_varsa.S \
		surf16.S surf8.S
endif

soft_sources=	d_edge.c d_fill.c d_init.c d_modech.c \
		d_part.c d_polyse.c d_scan.c d_sky.c d_sprite.c d_surf.c \
		d_vars.c d_zpoint.c draw.c r_aclip.c r_alias.c r_bsp.c \
		r_draw.c r_edge.c r_efrag.c r_light.c r_main.c r_misc.c \
		r_part.c r_sky.c r_sprite.c r_surf.c r_vars.c sw_skin.c sw_view.c \
		screen.c $(soft_ASM) sw_model_alias.c sw_model_brush.c \
		sw_model_sprite.c

#
# ... Linux FBDev
#
YFLAGS = -d
fbdev_sources=		fbset.c fbset_modes_y.y fbset_modes_l.l vid_fbdev.c in_fbdev.c cl_sys_unix.c
YACCLEX_CLEANFILES=	fbset_modes_y.c fbset_modes_y.h fbset_modes_y.tab.h fbset_modes_l.c
EXTRA_qf_client_fbdev_SOURCES=fbset_modes_y.h

fbset_modes_y.o: fbset_modes_y.c
	$(CC) $(INCLUDES) $(CFLAGS) -Wno-error -c fbset_modes_y.c
fbset_modes_l.o: fbset_modes_l.c
	$(CC) $(INCLUDES) $(CFLAGS) -Wno-error -c fbset_modes_l.c

qf_client_fbdev_SOURCES= $(common_sources) $(client_sources) $(soft_sources) $(fbdev_sources)
qf_client_fbdev_LDADD= $(CLIENT_LIBS)
qf_client_fbdev_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... GGI
#
ggi_sources=		vid_ggi.c in_ggi.c cl_sys_unix.c

qf_client_ggi_SOURCES=	$(common_sources) $(client_sources) $(soft_sources) $(ggi_sources)
qf_client_ggi_LDADD=	$(GGI_LIBS) $(CLIENT_LIBS)
qf_client_ggi_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... SciTech MGL
#
mgl_sources=		vid_mgl.c in_win.c cl_sys_win.c

qf_client_mgl_SOURCES=	$(common_sources) $(client_sources) $(soft_sources) $(mgl_sources)
qf_client_mgl_LDADD=	$(MGL_LIBS) $(CLIENT_LIBS)
qf_client_mgl_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... Sam Lantinga's Simple DirectMedia Layer, version 1.0 and higher
#
sdl_sources=		vid_sdl.c in_sdl.c cl_sys_sdl.c

qf_client_sdl_SOURCES=	$(common_sources) $(client_sources) $(soft_sources) $(sdl_sources)
qf_client_sdl_LDADD=	$(SDL_LIBS) $(CLIENT_LIBS)
qf_client_sdl_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... Linux SVGAlib
#
svga_sources=		d_copy.S vid_svgalib.c in_svgalib.c cl_sys_unix.c

qf_client_svga_SOURCES= $(common_sources) $(client_sources) $(soft_sources) $(svga_sources)
qf_client_svga_LDADD=	$(SVGA_LIBS) $(CLIENT_LIBS)
qf_client_svga_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... X11
#
x11_sources=		vid_x11.c in_x11.c context_x11.c dga_check.c cl_sys_unix.c

qf_client_x11_SOURCES=	$(common_sources) $(client_sources) $(soft_sources) $(x11_sources)
qf_client_x11_LDADD=	$(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(X_SHM_LIB) $(CLIENT_LIBS)
qf_client_x11_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a


#
# OpenGL-using clients
#
# ... Common stuff
#
ogl_sources=	fractalnoise.c gl_draw.c gl_dyn_fires.c gl_dyn_part.c \
		gl_dyn_textures.c gl_mesh.c gl_ngraph.c r_efrag.c \
		gl_rlight.c gl_rmain.c gl_rmisc.c gl_rsurf.c gl_screen.c \
		gl_skin.c gl_sky.c gl_sky_clip.c gl_view.c gl_warp.c \
		gl_model_alias.c gl_model_brush.c gl_model_fullbright.c \
		gl_model_sprite.c qfgl_ext.c

#
# ... 3Dfx Voodoo 1 and 2 SVGAlib-based console GL
#
tdfx_sources=	vid_3dfxsvga.c vid_common_gl.c in_svgalib.c cl_sys_unix.c

qf_client_3dfx_SOURCES=	$(common_sources) $(client_sources) $(ogl_sources) $(tdfx_sources)
qf_client_3dfx_LDADD=	$(TDFXGL_LIBS) $(SVGA_LIBS) $(CLIENT_LIBS) $(DL_LIBS)
qf_client_3dfx_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... OpenGL in X Window
#
glx_sources=	vid_glx.c vid_common_gl.c in_x11.c context_x11.c dga_check.c cl_sys_unix.c

qf_client_glx_SOURCES=	$(common_sources) $(client_sources) $(ogl_sources) $(glx_sources)
qf_client_glx_LDADD=	$(GLX_LIBS) $(X_PRE_LIBS) $(VIDMODE_LIBS) $(DGA_LIBS) $(X_LIBS) -lX11 $(X_EXTRA_LIBS) $(CLIENT_LIBS) $(DL_LIBS)
qf_client_glx_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# ... Sam Lantinga's Simple DirectMedia Layer, version 1.1 and higher, in GL mode
#
sgl_sources=	vid_sgl.c vid_common_gl.c in_sdl.c cl_sys_sdl.c

qf_client_sgl_SOURCES=	$(common_sources) $(client_sources) $(ogl_sources) $(sgl_sources)
qf_client_sgl_LDADD=	$(SDL_LIBS) $(X_LIBS) $(GLX_LIBS) $(CLIENT_LIBS) $(DL_LIBS)
qf_client_sgl_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# SGI/Microsoft WGL (Windows OpenGL)
#
wgl_sources=	vid_wgl.c vid_common_gl.c in_win.c cl_sys_win.c

qf_client_wgl_SOURCES=	$(common_sources) $(client_sources) $(ogl_sources) $(wgl_sources)
qf_client_wgl_LDADD=	$(CLIENT_LIBS) $(GLX_LIBS) -lgdi32 -lwinmm
qf_client_wgl_DEPENDENCIES=libqfnet.a libqfsys_cl.a libqfsnd.a libqfcd.a libqfjs.a

#
# Stuff that doesn't get linked into an executable NEEDS to be mentioned here,
# or it won't be distributed with 'make dist'
#
EXTRA_DIST=	makefile.mgw makefile.win \
		qf-server.mak qw_server.dsp \
		qf-client-sdl.mak qf-client-win.mak qw_client.dsp \
		qf-client-sgl.mak qf-client-wgl.mak qf-client-win.mak

# Kill the temp files, hopefully.
CLEANFILES	= *.i *.s $(YACCLEX_CLEANFILES)

cl_sys_sdl.o: cl_sys_sdl.c
	@echo '$(COMPILE) $(SDL_CFLAGS) -c $<'; \
	$(COMPILE) $(SDL_CFLAGS) -Wp,-MD,.deps/$(*F).pp -c $<
	@-cp .deps/$(*F).pp .deps/$(*F).P; \
	tr ' ' '\012' < .deps/$(*F).pp \
	  | sed -e 's/^\\$$//' -e '/^$$/ d' -e '/:$$/ d' -e 's/$$/ :/' \
	    >> .deps/$(*F).P; \
	rm .deps/$(*F).pp

in_sdl.o: in_sdl.c
	@echo '$(COMPILE) $(SDL_CFLAGS) -c $<'; \
	$(COMPILE) $(SDL_CFLAGS) -Wp,-MD,.deps/$(*F).pp -c $<
	@-cp .deps/$(*F).pp .deps/$(*F).P; \
	tr ' ' '\012' < .deps/$(*F).pp \
	  | sed -e 's/^\\$$//' -e '/^$$/ d' -e '/:$$/ d' -e 's/$$/ :/' \
	    >> .deps/$(*F).P; \
	rm .deps/$(*F).pp

vid_sdl.o: vid_sdl.c
	@echo '$(COMPILE) $(SDL_CFLAGS) -c $<'; \
	$(COMPILE) $(SDL_CFLAGS) -Wp,-MD,.deps/$(*F).pp -c $<
	@-cp .deps/$(*F).pp .deps/$(*F).P; \
	tr ' ' '\012' < .deps/$(*F).pp \
	  | sed -e 's/^\\$$//' -e '/^$$/ d' -e '/:$$/ d' -e 's/$$/ :/' \
	    >> .deps/$(*F).P; \
	rm .deps/$(*F).pp

vid_sgl.o: vid_sgl.c
	@echo '$(COMPILE) $(SDL_CFLAGS) -c $<'; \
	$(COMPILE) $(SDL_CFLAGS) -Wp,-MD,.deps/$(*F).pp -c $<
	@-cp .deps/$(*F).pp .deps/$(*F).P; \
	tr ' ' '\012' < .deps/$(*F).pp \
	  | sed -e 's/^\\$$//' -e '/^$$/ d' -e '/:$$/ d' -e 's/$$/ :/' \
	    >> .deps/$(*F).P; \
	rm .deps/$(*F).pp
