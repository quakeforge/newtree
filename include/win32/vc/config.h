/*
	config.h

	Configuration for Visual C++. This file would have been
	autogenerated by configure, for any sane compiler.

	Copyright (C) 2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

	Author: Thad Ward
	Date: 18 May 2000

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

#ifndef _CONFIG_H
#define _CONFIG_H

#pragma warning( disable : 4761)

/* fix the retarded malloc.h problem */
#define alloca _alloca
									// disable data conversion warnings
# pragma warning(disable : 4051)	// ALPHA
# pragma warning(disable : 4136)	// X86
# pragma warning(disable : 4244)	// MIPS
# pragma warning(disable : 4305)	// thousands of double to float truncation warnings

/* Define to empty if the keyword does not work.  */
#undef const

/* Define if you don't have vprintf but do have _doprnt.  */
#undef HAVE_DOPRNT

/* Define if you have a working `mmap' system call.  */
#undef HAVE_MMAP

/* Define if you have the vprintf function.  */
#undef HAVE_VPRINTF

/* Define as __inline if that's what the C compiler calls it.  */
#define inline  __inline

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#undef TIME_WITH_SYS_TIME

/* Define if your <sys/time.h> declares struct tm.  */
#undef TM_IN_SYS_TIME

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#undef WORDS_BIGENDIAN

/* Define if the X Window System is missing or not being used.  */
#define X_DISPLAY_MISSING

/* Version string */
#include "..\version.h"

/* Define if you have the XFree86 DGA extension */
#undef HAVE_DGA

/* If your version of OpenGL uses APIENTRY, define GLAPIENTRY to be APIENTRY*/
#define GLAPIENTRY APIENTRY

/* Define this to something sane if you don't have stricmp */
#undef stricmp

/* Define this if you are using Mesa */
#undef XMESA

/* Define this if you have GL_COLOR_INDEX8_EXT in GL/gl.h */
#undef HAVE_GL_COLOR_INDEX8_EXT

/* Define this if C symbols are prefixed with an underscore */
#define HAVE_SYM_PREFIX_UNDERSCORE 1

/* Define this if you have a Linux-style CD-ROM API */
#undef USE_LINUX_CD

/* Define this if you have a BSD-style CD-ROM API */
#undef USE_BSD_CD

/* Define if you have the _ftime function.  */
#define HAVE__FTIME 1

/* Define if you have the _snprintf function.  */
#define HAVE__SNPRINTF 1

/* Define if you have the _vsnprintf function.  */
#define HAVE__VSNPRINTF 1

/* Define if you have the connect function.  */
#define HAVE_CONNECT 1

/* Define if you have the fcntl function.  */
#undef HAVE_FCNTL

/* Define if you have the ftime function.  */
#define HAVE_FTIME 1

/* Define if you have the gethostbyname function.  */
#define HAVE_GETHOSTBYNAME 1

/* Define if you have the gethostname function.  */
#define HAVE_GETHOSTNAME 1

/* Define if you have the getpagesize function.  */
#undef HAVE_GETPAGESIZE

/* Define if you have the gettimeofday function.  */
#undef HAVE_GETTIMEOFDAY

/* Define if you have the getwd function.  */
#undef HAVE_GETWD

/* Define if you have the mkdir function.  */
#define HAVE_MKDIR 1

/* Define if you have the putenv function.  */
#undef HAVE_PUTENV

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the snprintf function.  */
#undef HAVE_SNPRINTF

/* Define if you have the socket function.  */
#define HAVE_SOCKET 1

/* Define if you have the stat function.  */
#undef HAVE_STAT

/* Define if you have the strerror function.  */
#undef HAVE_STRERROR

/* Define if you have the strstr function.  */
#undef HAVE_STRSTR

/* Define if you have the vsnprintf function.  */
#undef HAVE_VSNPRINTF

/* Define if you have the <arpa/inet.h> header file.  */
#undef HAVE_ARPA_INET_H

/* Define if you have the <dsound.h> header file.  */
#define HAVE_DSOUND_H 1

/* Define if you have the <fcntl.h> header file.  */
#undef HAVE_FCNTL_H

/* Define if you have the <fnmatch.h> header file.  */
#define HAVE_FNMATCH_H 1

/* Define if you have the <initguid.h> header file.  */
#undef HAVE_INITGUID_H

/* Define if you have the <linux/soundcard.h> header file.  */
#undef HAVE_LINUX_SOUNDCARD_H

/* Define if you have the <machine/soundcard.h> header file.  */
#undef HAVE_MACHINE_SOUNDCARD_H

/* Define if you have the <mme/mme_public.h> header file.  */
#undef HAVE_MME_MME_PUBLIC_H

/* Define if you have the <mme/mmsystem.h> header file.  */
#undef HAVE_MME_MMSYSTEM_H

/* Define if you have the <mmsystem.h> header file.  */
#define HAVE_MMSYSTEM_H 1

/* Define if you have the <netdb.h> header file.  */
#undef HAVE_NETDB_H

/* Define if you have the <netinet/in.h> header file.  */
#undef HAVE_NETINET_IN_H

/* Define if you have the <sys/asoundlib.h> header file.  */
#undef HAVE_SYS_ASOUNDLIB_H

/* Define if you have the <sys/audioio.h> header file.  */
#undef HAVE_SYS_AUDIOIO_H

/* Define if you have the <sys/filio.h> header file.  */
#undef HAVE_SYS_FILIO_H

/* Define if you have the <sys/ioctl.h> header file.  */
#undef HAVE_SYS_IOCTL_H

/* Define if you have the <sys/mman.h> header file.  */
#undef HAVE_SYS_MMAN_H

/* Define if you have the <sys/socket.h> header file.  */
#undef HAVE_SYS_SOCKET_H

/* Define if you have the <sys/soundcard.h> header file.  */
#undef HAVE_SYS_SOUNDCARD_H

/* Define if you have the <sys/stat.h> header file.  */
#undef HAVE_SYS_STAT_H

/* Define if you have the <sys/time.h> header file.  */
#undef HAVE_SYS_TIME_H

/* Define if you have the <sys/timeb.h> header file.  */
#define HAVE_SYS_TIMEB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H

/* Define if you have the <windows.h> header file.  */
#define HAVE_WINDOWS_H 1

/* Define if you have the Xxf86dga library (-lXxf86dga).  */
#undef HAVE_LIBXXF86DGA

/* Define if you have the Xxf86vm library (-lXxf86vm).  */
#undef HAVE_LIBXXF86VM

/* Define if you have the db library (-ldb).  */
#undef HAVE_LIBDB

/* Define if you have the m library (-lm).  */
#undef HAVE_LIBM

/* Define if you have the zlib library (-lz).  */
#define HAVE_ZLIB

/* Define if you have a sane fnmatch.h   */
#define HAVE_FNMATCH_PROTO

/* Define if you have winsock.h in your includes path */
#define HAVE_WINSOCK_H

/* Define this to something appropriate for declaring 0 length arrays */
#define ZERO_LENGTH_ARRAY 0

/* Posix, needed for limits.h and Unix stuffs to work right */
#define _POSIX_

/* Define if we've scitech MGL library and mgraph.h */
#define HAVE_MGRAPH_H 1

/* Dir used for shared game data */
#define FS_SHAREPATH "."

/* Dir used for unshared (user) game data */
#define FS_USERPATH "."

/* Location of QuakeForge's global config file */
#define FS_GLOBALCFG "~/quakeforge.conf"

/* Location of QuakeForge's user config file */
#define FS_USERCFG ""

#define strcasecmp(s1, s2) stricmp((s1), (s2))
#define strncasecmp(s1, s2, n) strnicmp((s1), (s2), (n))

#ifdef HAVE_3DNOW_ASM
# define atan  _atan
# define atan2 _atan2
# define acos _acos
# define asin _asin
# define log _log
# define log10 _log10
# define pow _pow
# define exp _exp
# define sqrt _sqrt
# define fabs _fabs
# define ceil _ceil
# define floor  _floor
# define frexp _frexp
# define ldexp _ldexp
# define modf _modf
# define fmod _fmod
# define sincos _sincos
# define sin _sin
# define cos _cos
# define tan _tan
#endif

#endif	// _CONFIG_H
