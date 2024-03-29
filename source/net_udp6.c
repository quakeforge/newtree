/*
	net_udp6.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  Nelson Rush.
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#ifdef _WIN32
# ifdef HAVE_IPV6
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <tpipv6.h>
#  define  _WINSOCKAPI_
#  define HAVE_SOCKLEN_T
# endif
#endif

#include <stdio.h>
#include <errno.h>

/* Sun's model_t in sys/model.h conflicts w/ Quake's model_t */
#define model_t quakeforgemodel_t

#ifdef _WIN32
# include <windows.h>
# undef EWOULDBLOCK
# define EWOULDBLOCK	WSAEWOULDBLOCK
#endif

#ifndef _WIN32
# include <unistd.h>
#endif

#include <sys/types.h>

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#undef model_t

#ifdef NeXT
# include <libc.h>
#endif

#include "console.h"
#include "net.h"
#include "qargs.h"
#include "qtypes.h"
#include "sys.h"

#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN	512
#endif

#ifdef __GLIBC__						// glibc macro
# define s6_addr32 in6_u.u6_addr32
# define ss_family __ss_family
#endif

netadr_t    net_local_adr;

netadr_t    net_from;
sizebuf_t   net_message;
int         net_socket;

#define	MAX_UDP_PACKET	(MAX_MSGLEN*2)
byte        net_message_buffer[MAX_UDP_PACKET];

#ifdef _WIN32
 WSADATA     winsockdata;
#endif

//=============================================================================

void
NetadrToSockadr (netadr_t *a, struct sockaddr_in6 *s)
{
	memset (s, 0, sizeof (*s));

	s->sin6_family = AF_INET6;
	memcpy (&s->sin6_addr, a->ip, sizeof (s->sin6_addr));
	s->sin6_port = a->port;
#ifdef HAVE_SIN6_LEN
	s->sin6_len = sizeof (struct sockaddr_in6);
#endif
}

void
SockadrToNetadr (struct sockaddr_in6 *s, netadr_t *a)
{
	memcpy (a->ip, &s->sin6_addr, sizeof (s->sin6_addr));
	a->port = s->sin6_port;
	a->family = s->sin6_family;
}

qboolean
NET_AdrIsLoopback (netadr_t a)
{
	if (IN6_IS_ADDR_LOOPBACK ((struct in6_addr *) &a.ip))
		return true;
	else if (IN6_IS_ADDR_V4MAPPED ((struct in6_addr *) &a.ip) &&
			 ((struct in_addr *) &a.ip[3])->s_addr == htonl (INADDR_LOOPBACK))
		return true;

	return false;
}

qboolean
NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2]
		&& a.ip[3] == b.ip[3])
		return true;
	return false;
}


qboolean
NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2]
		&& a.ip[3] == b.ip[3] && a.port == b.port)
		return true;
	return false;
}

char       *
NET_AdrToString (netadr_t a)
{
	static char s[64];
	char       *base;

	base = NET_BaseAdrToString (a);
	sprintf (s, "[%s]:%d", base, ntohs (a.port));
	return s;
}

char       *
NET_BaseAdrToString (netadr_t a)
{
	static char s[64];
	struct sockaddr_storage ss;

	// NetadrToSockadr(&a,&sa);

	memset (&ss, 0, sizeof (ss));
	if (IN6_IS_ADDR_V4MAPPED ((struct in6_addr *) a.ip)) {
#ifdef HAVE_SS_LEN
		ss.ss_len = sizeof (struct sockaddr_in);
#endif
		ss.ss_family = AF_INET;
		memcpy (&((struct sockaddr_in *) &ss)->sin_addr,
				&((struct in6_addr *) a.ip)->s6_addr[12],

				sizeof (struct in_addr));
	} else {
#ifdef HAVE_SS_LEN
		ss.ss_len = sizeof (struct sockaddr_in6);
#endif
		ss.ss_family = AF_INET6;
		memcpy (&((struct sockaddr_in6 *) &ss)->sin6_addr,

				a.ip, sizeof (struct in6_addr));
	}
#ifdef HAVE_SS_LEN
	if (getnameinfo ((struct sockaddr *) &ss, ss.ss_len, s, sizeof (s),
					 NULL, 0, NI_NUMERICHOST)) strcpy (s, "<invalid>");
#else
	// maybe needs switch for AF_INET6 or AF_INET?
	if (getnameinfo ((struct sockaddr *) &ss, sizeof (ss), s, sizeof (s),
					 NULL, 0, NI_NUMERICHOST))
		strcpy (s, "<invalid>");
#endif
	return s;
}

/*
	NET_StringToAdr

	idnewt
	idnewt:28000
	192.246.40.70
	192.246.40.70:28000
*/
qboolean
NET_StringToAdr (char *s, netadr_t *a)
{

	struct addrinfo hints;
	struct addrinfo *resultp;
	char       *space;
	char       *ports = NULL;
	char        copy[128];
	char       *addrs;
	int         err;
	struct sockaddr_storage ss;
	struct sockaddr_in6 *ss6;
	struct sockaddr_in *ss4;

	memset (&hints, 0, sizeof (hints));
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_family = PF_UNSPEC;

	strcpy (copy, s);
	addrs = space = copy;
	if (*addrs == '[') {
		addrs++;
		for (; *space && *space != ']'; space++);
		if (!*space) {
			Con_Printf ("NET_StringToAdr: invalid IPv6 address %s\n", s);
			return 0;
		}
		*space++ = '\0';
	}

	for (; *space; space++) {
		if (*space == ':') {
			*space = '\0';
			ports = space + 1;
		}
	}

	// Con_Printf ("NET_StringToAdr: addrs %s ports %s\n",addrs, ports);

	if ((err = getaddrinfo (addrs, ports, &hints, &resultp))) {
		// Error
		Con_Printf ("NET_StringToAdr: string %s:\n%s\n", s, gai_strerror (err));
		return 0;
	}

	switch (resultp->ai_family) {
		case AF_INET:
			// convert to ipv6 addr
			memset (&ss, 0, sizeof (ss));
			ss6 = (struct sockaddr_in6 *) &ss;
			ss4 = (struct sockaddr_in *) resultp->ai_addr;
			ss6->sin6_family = AF_INET6;

			memset (&ss6->sin6_addr, 0, sizeof (ss6->sin6_addr));
			ss6->sin6_addr.s6_addr[10] = ss6->sin6_addr.s6_addr[11] = 0xff;
			memcpy (&ss6->sin6_addr.s6_addr[12], &ss4->sin_addr,
					sizeof (ss4->sin_addr));

			ss6->sin6_port = ss4->sin_port;
			break;
		case AF_INET6:
			memcpy (&ss, resultp->ai_addr, sizeof (struct sockaddr_in6));

			break;
		default:
			Con_Printf
				("NET_StringToAdr: string %s:\nprotocol family %d not supported\n",
				 s, resultp->ai_family);
		return 0;
	}
	SockadrToNetadr ((struct sockaddr_in6 *) &ss, a);

	return true;
}

// Returns true if we can't bind the address locally--in other words,
// the IP is NOT one of our interfaces.
qboolean
NET_IsClientLegal (netadr_t *adr)
{
#if 0
	struct sockaddr_in sadr;
	int         newsocket;

	if (adr->ip[0] == 127)
		return false;					// no local connections period

	NetadrToSockadr (adr, &sadr);

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		Sys_Error ("NET_IsClientLegal: socket: %s", strerror (errno));

	sadr.sin_port = 0;

	if (bind (newsocket, (void *) &sadr, sizeof (sadr)) == -1) {
		// It is not a local address
		close (newsocket);
		return true;
	}
	close (newsocket);
	return false;
#else
	return true;
#endif
}


//=============================================================================

qboolean
NET_GetPacket (void)
{
	int         ret;
	struct sockaddr_in6 from;
	unsigned int fromlen;

	fromlen = sizeof (from);
	ret =
		recvfrom (net_socket, (void *) net_message_buffer,
				  sizeof (net_message_buffer), 0, (struct sockaddr *) &from,
				  &fromlen);
	SockadrToNetadr (&from, &net_from);

	if (ret == -1) {
#ifdef _WIN32
		int         err = WSAGetLastError ();

		if (err == WSAEMSGSIZE) {
			Con_Printf ("Warning:  Oversize packet from %s\n",
						NET_AdrToString (net_from));
			return false;
		}
#else /* _WIN32 */
		int         err = errno;

		if (err == ECONNREFUSED)
			return false;
#endif /* _WIN32 */
		if (err == EWOULDBLOCK)
			return false;
		Sys_Printf ("NET_GetPacket: %s\n", strerror (err));
		return false;
	}

	net_message.cursize = ret;
	if (ret == sizeof (net_message_buffer)) {
		Con_Printf ("Oversize packet from %s\n", NET_AdrToString (net_from));
		return false;
	}

	return ret;
}

//=============================================================================

void
NET_SendPacket (int length, void *data, netadr_t to)
{
	int         ret;
	struct sockaddr_in6 addr;

	NetadrToSockadr (&to, &addr);

	ret =
		sendto (net_socket, data, length, 0, (struct sockaddr *) &addr,
				sizeof (addr));
	if (ret == -1) {
#ifdef _WIN32
		int         err = WSAGetLastError ();

#ifndef SERVERONLY
		if (err == WSAEADDRNOTAVAIL)
			Con_DPrintf ("NET_SendPacket Warning: %i\n", err);
#endif
#else /* _WIN32 */
		int         err = errno;

		if (err == ECONNREFUSED)
			return;
#endif /* _WIN32 */
		if (err == EWOULDBLOCK)
			return;

		Sys_Printf ("NET_SendPacket: %s\n", strerror (err));
	}
}

//=============================================================================

int
UDP_OpenSocket (int port)
{
	int         newsocket;
	struct sockaddr_in6 address;
	struct addrinfo hints, *res;
	int         Error;
	char        Buf[BUFSIZ], *Host, *Service;

#ifdef IPV6_BINDV6ONLY
	int         dummy;
#endif
#ifdef _WIN32
#define ioctl ioctlsocket
	unsigned long _true = true;
#else
	int         _true = 1;
#endif
	int         i;

	if ((newsocket = socket (PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		Sys_Error ("UDP_OpenSocket: socket: %s", strerror (errno));
	if (ioctl (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror (errno));
	memset (&address, 0, sizeof (address));
	address.sin6_family = AF_INET6;
//ZOID -- check for interface binding option

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;

	if ((i = COM_CheckParm ("-ip")) != 0 && i < com_argc) {
		Host = com_argv[i + 1];
	} else {
		Host = "0::0";
	}
	Con_Printf ("Binding to IP Interface Address of %s\n", Host);

	if (port == PORT_ANY)
		Service = NULL;
	else {
		sprintf (Buf, "%5d", port);
		Service = Buf;
	}

	if ((Error = getaddrinfo (Host, Service, &hints, &res)))
		Sys_Error ("UDP_OpenSocket: getaddrinfo: %s", gai_strerror (Error));

	if (
		(newsocket =
		 socket (res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
		Sys_Error ("UDP_OpenSocket: socket: %s", strerror (errno));
	// FIONBIO sets non-blocking IO for this socket
#ifdef _WIN32
	if (ioctl (newsocket, FIONBIO, &_true) == -1)
#else /* _WIN32 */
	if (ioctl (newsocket, FIONBIO, (char *) &_true) == -1)
#endif /* _WIN32 */
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror (errno));

#ifdef IPV6_BINDV6ONLY
	if (setsockopt (newsocket, IPPROTO_IPV6, IPV6_BINDV6ONLY, &dummy,
					sizeof (dummy)) < 0) {
		/* I don't care */
	}
#endif

	if (bind (newsocket, res->ai_addr, res->ai_addrlen) < 0)
		Sys_Error ("UDP_OpenSocket: bind: %s", strerror (errno));

	freeaddrinfo (res);

	return newsocket;
}

void
NET_GetLocalAddress (void)
{
	char        buff[MAXHOSTNAMELEN];
	struct sockaddr_in6 address;
	unsigned int namelen;

	if (gethostname (buff, MAXHOSTNAMELEN) == -1)
		Sys_Error ("Net_GetLocalAddress: gethostname: %s", strerror (errno));
	buff[MAXHOSTNAMELEN - 1] = 0;

	NET_StringToAdr (buff, &net_local_adr);

	namelen = sizeof (address);
	if (getsockname (net_socket, (struct sockaddr *) &address, &namelen) == -1)
		Sys_Error ("NET_GetLocalAddress: getsockname: %s", strerror (errno));
	net_local_adr.port = address.sin6_port;

	Con_Printf ("IP address %s\n", NET_AdrToString (net_local_adr));
}

/*
	NET_Init
*/
void
NET_Init (int port)
{
#ifdef _WIN32
	WORD        wVersionRequested;
	int         r;

	wVersionRequested = MAKEWORD (1, 1);

	r = WSAStartup (MAKEWORD (1, 1), &winsockdata);
	if (r)
		Sys_Error ("Winsock initialization failed.");
#endif /* _WIN32 */

	// 
	// open the single socket to be used for all communications
	// 
	net_socket = UDP_OpenSocket (port);

	// 
	// init the message buffer
	// 
	net_message.maxsize = sizeof (net_message_buffer);
	net_message.data = net_message_buffer;

	// 
	// determine my name & address
	// 
	NET_GetLocalAddress ();

	Con_Printf ("UDP Initialized\n");
}

/*
	NET_Shutdown
*/
void
NET_Shutdown (void)
{
#ifdef _WIN32
	closesocket (net_socket);
	WSACleanup ();
#else
	close (net_socket);
#endif
}
