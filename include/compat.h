/*

compat.h - Misc compability stuff

Copyright (C) 1996-1997	Id Software, Inc.
Copyright (C) 2000		Marcus Sundberg <mackan@stacken.kth.se>

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

#ifndef __QF_COMPAT_H
#define __QF_COMPAT_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

/* This fixes warnings when compiling with -pedantic */
#if defined(__GNUC__) && !defined(inline)
# define inline __inline__
#endif

/* These may be underscored... */
#if !defined(HAVE_SNPRINTF) && defined(HAVE__SNPRINTF)
# define snprintf _snprintf
#endif
#if !defined(HAVE_VSNPRINTF) && defined(HAVE__VSNPRINTF)
# define vsnprintf _vsnprintf
#endif
#if defined(_WIN32) && !defined(__BORLANDC__)
# define kbhit _kbhit
#endif

/* If we don't have them in the C-library we declare them to avoid warnings */
#if ! (defined(HAVE_SNPRINTF) || defined(HAVE__SNPRINTF))
extern int snprintf(char * s, size_t maxlen, const char *format, ...);
#endif
#if ! (defined(HAVE_VSNPRINTF) || defined(HAVE__VSNPRINTF))
extern int vsnprintf(char *s, size_t maxlen, const char *format, va_list arg);
#endif

#endif /* __QF_COMPAT_H */
