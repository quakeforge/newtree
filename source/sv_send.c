/*
	sv_send.c

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
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "bothdefs.h"
#include "msg.h"
#include "server.h"
#include "sys.h"

#define CHAN_AUTO   0
#define CHAN_WEAPON 1
#define CHAN_VOICE  2
#define CHAN_ITEM   3
#define CHAN_BODY   4

/*
	Con_Printf redirection
*/

char        outputbuf[8000];

redirect_t  sv_redirected;
int         con_printf_no_log;

extern cvar_t *sv_phs;
extern cvar_t *sv_timestamps;
extern cvar_t *sv_timefmt;

/*
	SV_FlushRedirect
*/
void
SV_FlushRedirect (void)
{
	char        send[8000 + 6];

	if (sv_redirected == RD_PACKET) {
		send[0] = 0xff;
		send[1] = 0xff;
		send[2] = 0xff;
		send[3] = 0xff;
		send[4] = A2C_PRINT;
		memcpy (send + 5, outputbuf, strlen (outputbuf) + 1);

		NET_SendPacket (strlen (send) + 1, send, net_from);
	} else if (sv_redirected == RD_CLIENT) {
		ClientReliableWrite_Begin (host_client, svc_print,
								   strlen (outputbuf) + 3);
		ClientReliableWrite_Byte (host_client, PRINT_HIGH);
		ClientReliableWrite_String (host_client, outputbuf);
	}
	// clear it
	outputbuf[0] = 0;
}


/*
	SV_BeginRedirect

	Send Con_Printf data to the remote client
	instead of the console
*/
void
SV_BeginRedirect (redirect_t rd)
{
	sv_redirected = rd;
	outputbuf[0] = 0;
}

void
SV_EndRedirect (void)
{
	SV_FlushRedirect ();
	sv_redirected = RD_NONE;
}


/*
	Con_Printf

	Handles cursor positioning, line wrapping, etc
*/
#define	MAXPRINTMSG	4096

void
Con_Printf (char *fmt, ...)
{
	static int  pending = 0;			// partial line being printed
	va_list     argptr;
	char        msg[MAXPRINTMSG];
	char        msg2[MAXPRINTMSG];
	char        msg3[MAXPRINTMSG];

	time_t      mytime = 0;
	struct tm  *local = NULL;
	qboolean    timestamps = false;

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	if (sv_redirected) {				// Add to redirected message
		if (strlen (msg) + strlen (outputbuf) > sizeof (outputbuf) - 1)
			SV_FlushRedirect ();
		strncat (outputbuf, msg, sizeof (outputbuf) - strlen (outputbuf));
	}
	if (!con_printf_no_log) {
		// We want to output to console and maybe logfile
		if (sv_timestamps && sv_timefmt && sv_timefmt->string
			&& sv_timestamps->int_val && !pending)
			timestamps = true;

		if (timestamps) {
			mytime = time (NULL);
			local = localtime (&mytime);
			strftime (msg3, sizeof (msg3), sv_timefmt->string, local);

			snprintf (msg2, sizeof (msg2), "%s%s", msg3, msg);
		} else {
			snprintf (msg2, sizeof (msg2), "%s", msg);
		}
		if (msg2[strlen (msg2) - 1] != '\n') {
			pending = 1;
		} else {
			pending = 0;
		}

		Sys_Printf ("%s", msg2);		// also echo to debugging console
		if (sv_logfile)
			Qprintf (sv_logfile, "%s", msg2);
	}
}

/*
	Con_DPrintf

	A Con_Printf that only shows up if the "developer" cvar is set
*/
void
Con_DPrintf (char *fmt, ...)
{
	va_list     argptr;
	char        msg[MAXPRINTMSG];

	if (!developer->int_val)
		return;

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof (msg), fmt, argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
}

/*
	EVENT MESSAGES
*/

static void
SV_PrintToClient (client_t *cl, int level, char *string)
{
	ClientReliableWrite_Begin (cl, svc_print, strlen (string) + 3);
	ClientReliableWrite_Byte (cl, level);
	ClientReliableWrite_String (cl, string);
}


/*
	SV_ClientPrintf

	Sends text across to be displayed if the level passes
*/
void
SV_ClientPrintf (client_t *cl, int level, char *fmt, ...)
{
	va_list     argptr;
	char        string[1024];

	if (level < cl->messagelevel)
		return;

	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	SV_PrintToClient (cl, level, string);
}

/*
	SV_BroadcastPrintf

	Sends text to all active clients
*/
void
SV_BroadcastPrintf (int level, char *fmt, ...)
{
	va_list     argptr;
	char        string[1024];
	client_t   *cl;
	int         i;

	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	Con_Printf ("%s", string);			// print to the console

	for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (level < cl->messagelevel)
			continue;
		if (!cl->state)
			continue;

		SV_PrintToClient (cl, level, string);
	}
}

/*
	SV_BroadcastCommand

	Sends text to all active clients
*/
void
SV_BroadcastCommand (char *fmt, ...)
{
	va_list     argptr;
	char        string[1024];

	if (!sv.state)
		return;
	va_start (argptr, fmt);
	vsnprintf (string, sizeof (string), fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&sv.reliable_datagram, svc_stufftext);
	MSG_WriteString (&sv.reliable_datagram, string);
}


/*
	SV_Multicast

	Sends the contents of sv.multicast to a subset of the clients,
	then clears sv.multicast.

	MULTICAST_ALL	same as broadcast
	MULTICAST_PVS	send to clients potentially visible from org
	MULTICAST_PHS	send to clients potentially hearable from org
*/
void
SV_Multicast (vec3_t origin, int to)
{
	client_t   *client;
	byte       *mask;
	mleaf_t    *leaf;
	int         leafnum;
	int         j;
	qboolean    reliable;

	leaf = Mod_PointInLeaf (origin, sv.worldmodel);
	if (!leaf)
		leafnum = 0;
	else
		leafnum = leaf - sv.worldmodel->leafs;

	reliable = false;

	switch (to) {
		case MULTICAST_ALL_R:
			reliable = true;			// intentional fallthrough
		case MULTICAST_ALL:
			mask = sv.pvs;				// leaf 0 is everything;
			break;

		case MULTICAST_PHS_R:
			reliable = true;			// intentional fallthrough
		case MULTICAST_PHS:
			mask = sv.phs + leafnum * 4 * ((sv.worldmodel->numleafs + 31) >> 5);
			break;

		case MULTICAST_PVS_R:
			reliable = true;			// intentional fallthrough
		case MULTICAST_PVS:
			mask = sv.pvs + leafnum * 4 * ((sv.worldmodel->numleafs + 31) >> 5);
			break;

		default:
			mask = NULL;
			SV_Error ("SV_Multicast: bad to:%i", to);
	}

	// send the data to all relevent clients
	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state != cs_spawned)
			continue;

		if (to == MULTICAST_PHS_R || to == MULTICAST_PHS) {
			vec3_t      delta;

			VectorSubtract (origin, client->edict->v.v.origin, delta);
			if (Length (delta) <= 1024)
				goto inrange;
		}

		leaf = Mod_PointInLeaf (client->edict->v.v.origin, sv.worldmodel);
		if (leaf) {
			// -1 is because pvs rows are 1 based, not 0 based like leafs
			leafnum = leaf - sv.worldmodel->leafs - 1;
			if (!(mask[leafnum >> 3] & (1 << (leafnum & 7)))) {
//              Con_Printf ("supressed multicast\n");
				continue;
			}
		}

	  inrange:
		if (reliable) {
			ClientReliableCheckBlock (client, sv.multicast.cursize);
			ClientReliableWrite_SZ (client, sv.multicast.data,
									sv.multicast.cursize);
		} else
			SZ_Write (&client->datagram, sv.multicast.data,
					  sv.multicast.cursize);
	}

	SZ_Clear (&sv.multicast);
}


/*  
	SV_StartSound

	Each entity can have eight independant sound sources, like voice,
	weapon, feet, etc.

	Channel 0 is an auto-allocate channel, the others override anything
	allready running on that entity/channel pair.

	An attenuation of 0 will play full volume everywhere in the level.
	Larger attenuations will drop off.  (max 4 attenuation)
*/
void
SV_StartSound (edict_t *entity, int channel, char *sample, int volume,
			   float attenuation)
{
	int         sound_num;
	int         field_mask;
	int         i;
	int         ent;
	vec3_t      origin;
	qboolean    use_phs;
	qboolean    reliable = false;

	if (volume < 0 || volume > 255)
		SV_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		SV_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 15)
		SV_Error ("SV_StartSound: channel = %i", channel);

// find precache number for sound
	for (sound_num = 1; sound_num < MAX_SOUNDS
		 && sv.sound_precache[sound_num]; sound_num++)
		if (!strcmp (sample, sv.sound_precache[sound_num]))
			break;

	if (sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num]) {
		Con_Printf ("SV_StartSound: %s not precacheed\n", sample);
		return;
	}

	ent = NUM_FOR_EDICT (&sv_pr_state, entity);

	if ((channel & 8) || !sv_phs->int_val)	// no PHS flag
	{
		if (channel & 8)
			reliable = true;			// sounds that break the phs are
										// reliable
		use_phs = false;
		channel &= 7;
	} else
		use_phs = true;

//  if (channel == CHAN_BODY || channel == CHAN_VOICE)
//      reliable = true;

	channel = (ent << 3) | channel;

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		channel |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		channel |= SND_ATTENUATION;

	// use the entity origin unless it is a bmodel
	if (entity->v.v.solid == SOLID_BSP) {
		for (i = 0; i < 3; i++)
			origin[i] =
				entity->v.v.origin[i] + 0.5 * (entity->v.v.mins[i] +
											 entity->v.v.maxs[i]);
	} else {
		VectorCopy (entity->v.v.origin, origin);
	}

	MSG_WriteByte (&sv.multicast, svc_sound);
	MSG_WriteShort (&sv.multicast, channel);
	if (channel & SND_VOLUME)
		MSG_WriteByte (&sv.multicast, volume);
	if (channel & SND_ATTENUATION)
		MSG_WriteByte (&sv.multicast, attenuation * 64);
	MSG_WriteByte (&sv.multicast, sound_num);
	for (i = 0; i < 3; i++)
		MSG_WriteCoord (&sv.multicast, origin[i]);

	if (use_phs)
		SV_Multicast (origin, reliable ? MULTICAST_PHS_R : MULTICAST_PHS);
	else
		SV_Multicast (origin, reliable ? MULTICAST_ALL_R : MULTICAST_ALL);
}


/*
	FRAME UPDATES
*/

int         sv_nailmodel, sv_supernailmodel, sv_playermodel;

void
SV_FindModelNumbers (void)
{
	int         i;

	sv_nailmodel = -1;
	sv_supernailmodel = -1;
	sv_playermodel = -1;

	for (i = 0; i < MAX_MODELS; i++) {
		if (!sv.model_precache[i])
			break;
		if (!strcmp (sv.model_precache[i], "progs/spike.mdl"))
			sv_nailmodel = i;
		if (!strcmp (sv.model_precache[i], "progs/s_spike.mdl"))
			sv_supernailmodel = i;
		if (!strcmp (sv.model_precache[i], "progs/player.mdl"))
			sv_playermodel = i;
	}
}


/*
	SV_WriteClientdataToMessage
*/
void
SV_WriteClientdataToMessage (client_t *client, sizebuf_t *msg)
{
	int         i;
	edict_t    *other;
	edict_t    *ent;

	ent = client->edict;

	// send the chokecount for r_netgraph
	if (client->chokecount) {
		MSG_WriteByte (msg, svc_chokecount);
		MSG_WriteByte (msg, client->chokecount);
		client->chokecount = 0;
	}
	// send a damage message if the player got hit this frame
	if (ent->v.v.dmg_take || ent->v.v.dmg_save) {
		other = PROG_TO_EDICT (&sv_pr_state, ent->v.v.dmg_inflictor);
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, ent->v.v.dmg_save);
		MSG_WriteByte (msg, ent->v.v.dmg_take);
		for (i = 0; i < 3; i++)
			MSG_WriteCoord (msg,
							other->v.v.origin[i] + 0.5 * (other->v.v.mins[i] +
														other->v.v.maxs[i]));

		ent->v.v.dmg_take = 0;
		ent->v.v.dmg_save = 0;
	}
	// a fixangle might get lost in a dropped packet.  Oh well.
	if (ent->v.v.fixangle) {
		MSG_WriteByte (msg, svc_setangle);
		for (i = 0; i < 3; i++)
			MSG_WriteAngle (msg, ent->v.v.angles[i]);
		ent->v.v.fixangle = 0;
	}
}

/*
	SV_UpdateClientStats

	Performs a delta update of the stats array.  This should only be performed
	when a reliable message can be delivered this frame.
*/
void
SV_UpdateClientStats (client_t *client)
{
	edict_t    *ent;
	int         stats[MAX_CL_STATS];
	int         i;

	ent = client->edict;
	memset (stats, 0, sizeof (stats));

	// if we are a spectator and we are tracking a player, we get his stats
	// so our status bar reflects his
	if (client->spectator && client->spec_track > 0)
		ent = svs.clients[client->spec_track - 1].edict;

	stats[STAT_HEALTH] = ent->v.v.health;
	stats[STAT_WEAPON] = SV_ModelIndex (PR_GetString (&sv_pr_state, ent->v.v.weaponmodel));
	stats[STAT_AMMO] = ent->v.v.currentammo;
	stats[STAT_ARMOR] = ent->v.v.armorvalue;
	stats[STAT_SHELLS] = ent->v.v.ammo_shells;
	stats[STAT_NAILS] = ent->v.v.ammo_nails;
	stats[STAT_ROCKETS] = ent->v.v.ammo_rockets;
	stats[STAT_CELLS] = ent->v.v.ammo_cells;
	if (!client->spectator)
		stats[STAT_ACTIVEWEAPON] = ent->v.v.weapon;
	// stuff the sigil bits into the high bits of items for sbar
	stats[STAT_ITEMS] =
		(int) ent->v.v.items | ((int) sv_pr_state.pr_global_struct->serverflags << 28);

	// Extensions to the QW 2.40 protocol for Mega2k  --KB
	stats[STAT_VIEWHEIGHT] = (int) ent->v.v.view_ofs[2];

	// FIXME: this should become a * key!  --KB
	if (ent->v.v.movetype == MOVETYPE_FLY && !atoi (Info_ValueForKey
												  (svs.info, "playerfly")))
		ent->v.v.movetype = MOVETYPE_WALK;

	stats[STAT_FLYMODE] = (ent->v.v.movetype == MOVETYPE_FLY);


	for (i = 0; i < MAX_CL_STATS; i++)
		if (stats[i] != client->stats[i]) {
			client->stats[i] = stats[i];
			if (stats[i] >= 0 && stats[i] <= 255) {
				ClientReliableWrite_Begin (client, svc_updatestat, 3);
				ClientReliableWrite_Byte (client, i);
				ClientReliableWrite_Byte (client, stats[i]);
			} else {
				ClientReliableWrite_Begin (client, svc_updatestatlong, 6);
				ClientReliableWrite_Byte (client, i);
				ClientReliableWrite_Long (client, stats[i]);
			}
		}
}

/*
	SV_SendClientDatagram
*/
qboolean
SV_SendClientDatagram (client_t *client)
{
	byte        buf[MAX_DATAGRAM];
	sizebuf_t   msg;

	msg.data = buf;
	msg.maxsize = sizeof (buf);
	msg.cursize = 0;
	msg.allowoverflow = true;
	msg.overflowed = false;

	// add the client specific data to the datagram
	SV_WriteClientdataToMessage (client, &msg);

	// send over all the objects that are in the PVS
	// this will include clients, a packetentities, and
	// possibly a nails update
	SV_WriteEntitiesToClient (client, &msg);

	// copy the accumulated multicast datagram
	// for this client out to the message
	if (client->datagram.overflowed)
		Con_Printf ("WARNING: datagram overflowed for %s\n", client->name);
	else
		SZ_Write (&msg, client->datagram.data, client->datagram.cursize);
	SZ_Clear (&client->datagram);

	// send deltas over reliable stream
	if (Netchan_CanReliable (&client->netchan))
		SV_UpdateClientStats (client);

	if (msg.overflowed) {
		Con_Printf ("WARNING: msg overflowed for %s\n", client->name);
		SZ_Clear (&msg);
	}
	// send the datagram
	Netchan_Transmit (&client->netchan, msg.cursize, buf);

	return true;
}

/*
	SV_UpdateToReliableMessages
*/
void
SV_UpdateToReliableMessages (void)
{
	int         i, j;
	client_t   *client;
	eval_t     *val;
	edict_t    *ent;

// check for changes to be sent over the reliable streams to all clients
	for (i = 0, host_client = svs.clients; i < MAX_CLIENTS; i++, host_client++) {
		if (host_client->state != cs_spawned)
			continue;
		if (host_client->sendinfo) {
			host_client->sendinfo = false;
			SV_FullClientUpdate (host_client, &sv.reliable_datagram);
		}
		if (host_client->old_frags != (int) host_client->edict->v.v.frags) {
			for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
				if (client->state < cs_connected)
					continue;
				ClientReliableWrite_Begin (client, svc_updatefrags, 4);
				ClientReliableWrite_Byte (client, i);
				ClientReliableWrite_Short (client, host_client->edict->v.v.frags);
			}

			host_client->old_frags = host_client->edict->v.v.frags;
		}
		// maxspeed/entgravity changes
		ent = host_client->edict;

		val = GetEdictFieldValue (&sv_pr_state, ent, "gravity");
		if (val && host_client->entgravity != val->_float) {
			host_client->entgravity = val->_float;
			ClientReliableWrite_Begin (host_client, svc_entgravity, 5);
			ClientReliableWrite_Float (host_client, host_client->entgravity);
		}
		val = GetEdictFieldValue (&sv_pr_state, ent, "maxspeed");
		if (val && host_client->maxspeed != val->_float) {
			host_client->maxspeed = val->_float;
			ClientReliableWrite_Begin (host_client, svc_maxspeed, 5);
			ClientReliableWrite_Float (host_client, host_client->maxspeed);
		}

	}

	if (sv.datagram.overflowed)
		SZ_Clear (&sv.datagram);

	// append the broadcast messages to each client messages
	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state < cs_connected)
			continue;					// reliables go to all connected or
										// spawned

		ClientReliableCheckBlock (client, sv.reliable_datagram.cursize);
		ClientReliableWrite_SZ (client, sv.reliable_datagram.data,
								sv.reliable_datagram.cursize);

		if (client->state != cs_spawned)
			continue;					// datagrams only go to spawned
		SZ_Write (&client->datagram, sv.datagram.data, sv.datagram.cursize);
	}

	SZ_Clear (&sv.reliable_datagram);
	SZ_Clear (&sv.datagram);
}

#if defined(_WIN32) && !defined(__GNUC__)
#pragma optimize( "", off )
#endif



/*
	SV_SendClientMessages
*/
void
SV_SendClientMessages (void)
{
	int         i, j;
	client_t   *c;

// update frags, names, etc
	SV_UpdateToReliableMessages ();

// build individual updates
	for (i = 0, c = svs.clients; i < MAX_CLIENTS; i++, c++) {
		if (!c->state)
			continue;

		if (c->drop) {
			SV_DropClient (c);
			c->drop = false;
			continue;
		}
		// check to see if we have a backbuf to stick in the reliable
		if (c->num_backbuf) {
			// will it fit?
			if (c->netchan.message.cursize + c->backbuf_size[0] <
				c->netchan.message.maxsize) {

				Con_DPrintf ("%s: backbuf %d bytes\n",
							 c->name, c->backbuf_size[0]);

				// it'll fit
				SZ_Write (&c->netchan.message, c->backbuf_data[0],
						  c->backbuf_size[0]);

				// move along, move along
				for (j = 1; j < c->num_backbuf; j++) {
					memcpy (c->backbuf_data[j - 1], c->backbuf_data[j],
							c->backbuf_size[j]);
					c->backbuf_size[j - 1] = c->backbuf_size[j];
				}

				c->num_backbuf--;
				if (c->num_backbuf) {
					memset (&c->backbuf, 0, sizeof (c->backbuf));
					c->backbuf.data = c->backbuf_data[c->num_backbuf - 1];
					c->backbuf.cursize = c->backbuf_size[c->num_backbuf - 1];
					c->backbuf.maxsize =
						sizeof (c->backbuf_data[c->num_backbuf - 1]);
				}
			}
		}
		// if the reliable message overflowed,
		// drop the client
		if (c->netchan.message.overflowed) {
			SZ_Clear (&c->netchan.message);
			SZ_Clear (&c->datagram);
			SV_BroadcastPrintf (PRINT_HIGH, "%s overflowed\n", c->name);
			Con_Printf ("WARNING: reliable overflow for %s\n", c->name);
			SV_DropClient (c);
			c->send_message = true;
			c->netchan.cleartime = 0;	// don't choke this message
		}
		// only send messages if the client has sent one
		// and the bandwidth is not choked
		if (!c->send_message)
			continue;
		c->send_message = false;		// try putting this after choke?
		if (!sv.paused && !Netchan_CanPacket (&c->netchan)) {
			c->chokecount++;
			continue;					// bandwidth choke
		}

		if (c->state == cs_spawned)
			SV_SendClientDatagram (c);
		else
			Netchan_Transmit (&c->netchan, 0, NULL);	// just update
														// reliable

	}
}

#if defined(_WIN32) && !defined(__GNUC__)
#pragma optimize( "", on )
#endif



/*
	SV_SendMessagesToAll

	FIXME: does this sequence right?
*/
void
SV_SendMessagesToAll (void)
{
	int         i;
	client_t   *c;

	for (i = 0, c = svs.clients; i < MAX_CLIENTS; i++, c++)
		if (c->state)					// FIXME: should this only send to
										// active?
			c->send_message = true;

	SV_SendClientMessages ();
}
