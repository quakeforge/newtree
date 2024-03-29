/*
	net_udp.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]

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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <sys/types.h>

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
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

#ifdef NeXT
# include <libc.h>
#endif

#include "console.h"
#include "net.h"
#include "sys.h"
#include "qargs.h"

#ifdef _WIN32
# include <windows.h>
# undef EWOULDBLOCK
# define EWOULDBLOCK    WSAEWOULDBLOCK
#endif

#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN	512
#endif

#ifndef HAVE_SOCKLEN_T
# ifdef HAVE_SIZE
   typedef size_t socklen_t;
# else
   typedef unsigned int socklen_t;
# endif
#endif

netadr_t    net_local_adr;

netadr_t    net_from;
sizebuf_t   net_message;
int         net_socket;

extern qboolean is_server;

#define	MAX_UDP_PACKET	(MAX_MSGLEN*2)
byte        net_message_buffer[MAX_UDP_PACKET];

#ifdef _WIN32
 WSADATA     winsockdata;
#endif

//=============================================================================

void
NetadrToSockadr (netadr_t *a, struct sockaddr_in *s)
{
	memset (s, 0, sizeof (*s));
	s->sin_family = AF_INET;

	*(int *) &s->sin_addr = *(int *) &a->ip;
	s->sin_port = a->port;
}

void
SockadrToNetadr (struct sockaddr_in *s, netadr_t *a)
{
	*(int *) &a->ip = *(int *) &s->sin_addr;
	a->port = s->sin_port;
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

	snprintf (s, sizeof (s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2],
			  a.ip[3], ntohs (a.port));

	return s;
}

char       *
NET_BaseAdrToString (netadr_t a)
{
	static char s[64];

	snprintf (s, sizeof (s), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);

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
	struct hostent *h;
	struct sockaddr_in sadr;
	char       *colon;
	char        copy[128];


	memset (&sadr, 0, sizeof (sadr));
	sadr.sin_family = AF_INET;

	sadr.sin_port = 0;

	strcpy (copy, s);
	// strip off a trailing :port if present
	for (colon = copy; *colon; colon++)
		if (*colon == ':') {
			*colon = 0;
			sadr.sin_port = htons ((unsigned short) atoi (colon + 1));
		}

	if (copy[0] >= '0' && copy[0] <= '9') {
		*(int *) &sadr.sin_addr = inet_addr (copy);
	} else {
		if (!(h = gethostbyname (copy)))
			return 0;
		*(int *) &sadr.sin_addr = *(int *) h->h_addr_list[0];
	}

	SockadrToNetadr (&sadr, a);

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
		Sys_Error ("NET_IsClientLegal: socket:", strerror (errno));

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
	struct sockaddr_in from;
	socklen_t   fromlen;

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
		if (err == 10054) {
			Con_DPrintf ("error 10054 from %s\n", NET_AdrToString (net_from));
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

// Check for malformed packets

	if (is_server && ntohs(net_from.port)<1024) {
		Con_Printf ("Warning: Packet from %s dropped: Bad port\n",
				NET_AdrToString (net_from));
		return false;
	}

	if (from.sin_addr.s_addr==INADDR_ANY || from.sin_addr.s_addr==INADDR_BROADCAST) {
		Con_Printf ("Warning: Packet dropped - bad address\n");
		return false;
	}

	net_message.cursize = ret;
	if (ret == sizeof (net_message_buffer)) {
		Con_Printf ("Oversize packet from %s\n", NET_AdrToString (net_from));
		return false;
	}

#ifdef PACKET_LOGGING
        Log_Incoming_Packet(net_message_buffer,net_message.cursize);
#endif
	return ret;
}

//=============================================================================

void
NET_SendPacket (int length, void *data, netadr_t to)
{
	int         ret;
	struct sockaddr_in addr;

	NetadrToSockadr (&to, &addr);

#ifdef PACKET_LOGGING
        Log_Outgoing_Packet(data,length);
#endif

	ret =
		sendto (net_socket, data, length, 0, (struct sockaddr *) &addr,
				sizeof (addr));
	if (ret == -1) {
#ifdef _WIN32
		int         err = WSAGetLastError ();

		if (err == WSAEADDRNOTAVAIL) {
			if (is_server)
				Con_DPrintf ("NET_SendPacket Warning: %i\n", err);
			else
				Con_Printf ("NET_SendPacket ERROR: %i\n", err);
		}
#else /* _WIN32 */
		int         err = errno;

		if (err == ECONNREFUSED)
			return;
#endif /* _WIN32 */
		if (err == EWOULDBLOCK)
			return;

		Sys_Printf ("NET_SendPacket: %s\n", strerror (errno));
	}
}

//=============================================================================

int
UDP_OpenSocket (int port)
{
	int         newsocket;
	struct sockaddr_in address;

#ifdef _WIN32
#define ioctl ioctlsocket
	unsigned long _true = true;
#else
	int         _true = 1;
#endif
	int         i;

	memset (&address, 0, sizeof(address));

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		Sys_Error ("UDP_OpenSocket: socket:%s", strerror (errno));
	if (ioctl (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO:%s", strerror (errno));
	address.sin_family = AF_INET;
//ZOID -- check for interface binding option
	if ((i = COM_CheckParm ("-ip")) != 0 && i < com_argc) {
		address.sin_addr.s_addr = inet_addr (com_argv[i + 1]);
		Con_Printf ("Binding to IP Interface Address of %s\n",
					inet_ntoa (address.sin_addr));
	} else
		address.sin_addr.s_addr = INADDR_ANY;
	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons ((short) port);
	if (bind (newsocket, (void *) &address, sizeof (address)) == -1)
		Sys_Error ("UDP_OpenSocket: bind: %s", strerror (errno));

	return newsocket;
}

void
NET_GetLocalAddress (void)
{
	char        buff[MAXHOSTNAMELEN];
	struct sockaddr_in address;
	socklen_t   namelen;

	gethostname (buff, MAXHOSTNAMELEN);
	buff[MAXHOSTNAMELEN - 1] = 0;

	NET_StringToAdr (buff, &net_local_adr);

	namelen = sizeof (address);
	if (getsockname (net_socket, (struct sockaddr *) &address, &namelen) == -1)
		Sys_Error ("NET_Init: getsockname: %s", strerror (errno));
	net_local_adr.port = address.sin_port;

	Con_Printf ("IP address %s\n", NET_AdrToString (net_local_adr));
}

/*
	NET_Init
*/
void
NET_Init (int port)
{
#ifdef _WIN32
	int         r;

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
