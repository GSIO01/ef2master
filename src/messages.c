/*
	messages.c

	Message management for ef2master

	Copyright (C) 2004-2009  Mathieu Olivier
	Copyright (C) 2010		 Walter Julius Hennecke

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include "common.h"
#include "system.h"
#include "games.h"
#include "messages.h"
#include "servers.h"


// ---------- Constants ---------- //

// Timeout after a valid infoResponse (in secondes)
#define TIMEOUT_INFORESPONSE (15 * 60)

// Period of validity for a challenge string (in secondes)
#define TIMEOUT_CHALLENGE 2

// Gamename used for Q3A
#define GAMENAME_EF2 "STEF2"

// Maximum size of a reponse packet
#define MAX_PACKET_SIZE_OUT 1400


// Types of messages (with samples):

// Q3: "heartbeat QuakeArena-1\x0A"
// DP: "heartbeat DarkPlaces\x0A"
#define S2M_HEARTBEAT "heartbeat"

// Q3 & DP & QFusion: "getinfo A_Challenge"
#define M2S_GETINFO "getinfo"

// Q3 & DP & QFusion: "infoResponse\x0A\\pure\\1\\..."
#define S2M_INFORESPONSE "infoResponse\x0A"

// Q3: "getservers 67 ffa empty full"
// DP: "getservers DarkPlaces-Quake 3 empty full"
// DP: "getservers Transfusion 3 empty full"
// QFusion: "getservers qfusion 39 empty full"
#define C2M_GETSERVERS "getservers "

// DP: "getserversExt DarkPlaces-Quake 3 empty full ipv4 ipv6"
// IOQuake3: "getserversExt 68 empty ipv6"
#define C2M_GETSERVERSEXT "getserversExt "

// Q3 & DP & QFusion:
// "getserversResponse\\...(6 bytes)...\\...(6 bytes)...\\EOT\0\0\0"
#define M2C_GETSERVERSREPONSE "getserversResponse"

// DP & IOQuake3:
// "getserversExtResponse\\...(6 bytes)...//...(18 bytes)...\\EOT\0\0\0"
#define M2C_GETSERVERSEXTREPONSE "getserversExtResponse"



// ---------- Private functions ---------- //

/*
====================
SearchInfostring

Search an infostring for the value of a key
====================
*/
static const char* SearchInfostring (const char* infostring, const char* key)
{
	static char str_buffer [256];
	size_t buffer_ind;
	char c;

	if (*infostring++ != '\\')
		return NULL;

	for (;;)
	{
		buffer_ind = 0;

		// Get the key name
		for (;;)
		{
			c = *infostring++;

			if (c == '\\')
			{
				str_buffer[buffer_ind] = '\0';
				break;
			}

			// If it's the end of the infostring
			if (c == '\0')
				return NULL;

			// If the key name is too long, skip this key/value pair
			if (buffer_ind == sizeof (str_buffer) - 1)
			{
				// Skip the rest of the key name
				for (;;)
				{
					c = *infostring++;

					if (c == '\0')
						return NULL;
					if (c == '\\')
						break;
				}

				str_buffer[0] = '\0';
				break;
			}

			str_buffer[buffer_ind++] = c;
		}

		// If it's the key we are looking for, save it in "value"
		if (!strcmp (str_buffer, key))
		{
			buffer_ind = 0;

			for (;;)
			{
				c = *infostring++;

				if (c == '\0' || c == '\\')
				{
					str_buffer[buffer_ind] = '\0';
					return str_buffer;
				}

				// If the value name is too long, ignore it
				if (buffer_ind == sizeof (str_buffer) - 1)
					return NULL;

				str_buffer[buffer_ind++] = c;
			}
		}

		// Else, skip the value
		for (;;)
		{
			c = *infostring++;

			if (c == '\0')
				return NULL;
			if (c == '\\')
				break;
		}
	}
}


/*
====================
BuildChallenge

Build a challenge string for a "getinfo" message
====================
*/
static const char* BuildChallenge (void)
{
	static char challenge [CHALLENGE_MAX_LENGTH];
	size_t ind;
	size_t length = CHALLENGE_MIN_LENGTH - 1;  // We start at the minimum size

	// ... then we add a random number of characters
	length += rand () % (CHALLENGE_MAX_LENGTH - CHALLENGE_MIN_LENGTH + 1);

	for (ind = 0; ind < length; ind++)
	{
		char c;
		do
		{
			c = 33 + rand () % (126 - 33 + 1);  // -> c = 33..126
		} while (c == '\\' || c == ';' || c == '"' || c == '%' || c == '/');

		challenge[ind] = c;
	}

	challenge[length] = '\0';
	return challenge;
}


/*
====================
SendGetInfo

Send a "getinfo" message to a server
====================
*/
static void SendGetInfo (server_t* server, socket_t recv_socket)
{
	char msg [64] = "\xFF\xFF\xFF\xFF" M2S_GETINFO " ";
	size_t msglen;

	if (!server->challenge_timeout || server->challenge_timeout < crt_time)
	{
		const char* challenge;

		challenge = BuildChallenge ();
		strncpy (server->challenge, challenge, sizeof (server->challenge) - 1);
		server->challenge_timeout = crt_time + TIMEOUT_CHALLENGE;
	}

	msglen = strlen (msg);
	strncpy (msg + msglen, server->challenge, sizeof (msg) - msglen - 1);
	msg[sizeof (msg) - 1] = '\0';
	if (sendto (recv_socket, msg, strlen (msg), 0,
				(const struct sockaddr*)&server->address,
				server->addrlen) < 0)
		Com_Printf (MSG_WARNING, "> WARNING: can't send getinfo (%s)\n",
					Sys_GetLastNetErrorString ());
	else
		Com_Printf (MSG_NORMAL, "> %s <--- getinfo with challenge \"%s\"\n",
					peer_address, server->challenge);
}

/*
====================
HandleGetServers

Parse getservers requests and send the appropriate response
====================
*/
static void HandleGetServers (const char* msg, const struct sockaddr_storage* addr, socklen_t addrlen, socket_t recv_socket, qboolean extended_request)
{
	const char* packetheader;
	size_t headersize;
	char* end_ptr;
	const char* msg_ptr;
	char gamename [GAMENAME_LENGTH] = "";
	qbyte packet [MAX_PACKET_SIZE_OUT];
	size_t packetind;
	server_t* sv;
	int protocol;
	char gametype [GAMETYPE_LENGTH] = "0";
	qboolean use_dp_protocol;
	qboolean opt_empty = false;
	qboolean opt_full = false;
	qboolean opt_ipv4 = (! extended_request);
	qboolean opt_ipv6 = false;
	qboolean opt_gametype = false;
	char filter_options [MAX_PACKET_SIZE_IN];
	char* option_ptr;
	unsigned int nb_servers;
	const char* request_name;
	char buffer[8];
	int port1, port2;

	if (extended_request)
	{
		request_name = "getserversExt";
		use_dp_protocol = true;
	}
	else
	{
		request_name = "getservers";

		// Check if there's a name before the protocol number
		// In this case, the message comes from a DarkPlaces-compatible client
		protocol = (int)strtol (msg, &end_ptr, 0);
		use_dp_protocol = (end_ptr == msg || (*end_ptr != ' ' && *end_ptr != '\0'));
	}

	if (use_dp_protocol)
	{
		char *space;

		// Skip leading spaces
		msg_ptr = msg;
		while (*msg_ptr == ' ')
			msg_ptr++;

		if (*msg_ptr == '\0')
		{
			Com_Printf (MSG_WARNING,
						"> WARNING: Rejecting %s from %s (missing game name and protocol number)\n",
						request_name, peer_address);
			return;
		}

		// Read the game name
		strncpy (gamename, msg_ptr, sizeof (gamename) - 1);
		gamename[sizeof (gamename) - 1] = '\0';
		space = strchr (gamename, ' ');
		if (space)
			*space = '\0';
		msg_ptr = msg_ptr + strlen (gamename);

		// Read the protocol number
		protocol = (int)strtol (msg_ptr, &end_ptr, 0);
		if (end_ptr == msg_ptr || (*end_ptr != ' ' && *end_ptr != '\0'))
		{
			Com_Printf (MSG_WARNING,
						"> WARNING: Rejecting %s from %s (missing or invalid protocol number)\n",
						request_name, peer_address);
			return;
		}
	}
	// Else, it comes from a Quake III Arena client
	else
	{
		strncpy (gamename, GAMENAME_EF2, sizeof (gamename) - 1);
		gamename[sizeof (gamename) - 1] = '\0';
		msg_ptr = end_ptr;
	}

	Com_Printf (MSG_NORMAL, "> %s ---> %s (%s)\n", peer_address, request_name,
				gamename);

	if (! Game_IsAccepted (gamename))
	{
		Com_Printf (MSG_WARNING,
					"> WARNING: Rejecting %s from %s (game \"%s\" is not accepted)\n",
					request_name, peer_address, gamename);
		return;
	}

	// Parse the filtering options
	strncpy (filter_options, msg_ptr, sizeof (filter_options) - 1);
	filter_options[sizeof (filter_options) - 1] = '\0';
	option_ptr = strtok (filter_options, " ");
	while (option_ptr != NULL)
	{
		if (strcmp (option_ptr, "empty") == 0)
			opt_empty = true;
		else if (strcmp (option_ptr, "full") == 0)
			opt_full = true;
		else if (strcmp (option_ptr, "ffa") == 0)
		{
			gametype[0] = '0';
			gametype[1] = '\0';
			opt_gametype = true;
		}
		else if (strcmp (option_ptr, "tourney") == 0)
		{
			gametype[0] = '1';
			gametype[1] = '\0';
			opt_gametype = true;
		}
		else if (strcmp (option_ptr, "team") == 0)
		{
			gametype[0] = '3';
			gametype[1] = '\0';
			opt_gametype = true;
		}
		else if (strcmp (option_ptr, "ctf") == 0)
		{
			gametype[0] = '4';
			gametype[1] = '\0';
			opt_gametype = true;
		}
		else if (strncmp (option_ptr, "gametype=", 9) == 0)
		{
			const char* gametype_string = option_ptr + 9;

			strncpy(gametype, gametype_string, sizeof(gametype) - 1);
			gametype[sizeof(gametype) - 1] = '\0';
			opt_gametype = true;
		}
		else if (extended_request)
		{
			if (strcmp (option_ptr, "ipv4") == 0)
				opt_ipv4 = true;
			else if (strcmp (option_ptr, "ipv6") == 0)
				opt_ipv6 = true;
		}
		option_ptr = strtok (NULL, " ");
	}

	// If no IP version was given for the filtering, accept any version
	if (! opt_ipv4 && ! opt_ipv6)
	{
		opt_ipv4 = true;
		opt_ipv6 = true;
	}

	// Initialize the packet contents with the header
	if (extended_request)
		packetheader = "\xFF\xFF\xFF\xFF" M2C_GETSERVERSEXTREPONSE;
	else
		packetheader = "\xFF\xFF\xFF\xFF" M2C_GETSERVERSREPONSE "\0";
	headersize = strlen (packetheader);
	packetind = headersize;
	memcpy(packet, packetheader, headersize);

	// Add every relevant server
	nb_servers = 0;
	for (sv = Sv_GetFirst (); sv != NULL;  sv = Sv_GetNext ())
	{
		size_t next_sv_size;

		assert (sv->state != sv_state_unused_slot);

		// Extra debugging info
		if (max_msg_level >= MSG_DEBUG)
		{
			const char * addrstr = Sys_SockaddrToString (&sv->address, sv->addrlen);
			Com_Printf (MSG_DEBUG,
						"  - Comparing server: IP:\"%s\", p:%d, g:\"%s\"\n",
						addrstr, sv->protocol, sv->gamename);

			if (sv->state <= sv_state_uninitialized)
				Com_Printf (MSG_DEBUG,
							"    Reject: server is not initialized\n");
			if (sv->protocol != protocol)
				Com_Printf (MSG_DEBUG,
							"    Reject: protocol %d != requested %d\n",
							sv->protocol, protocol);
			if (! opt_empty && sv->state == sv_state_empty)
				Com_Printf (MSG_DEBUG, "    Reject: no empty server allowed\n");
			if (! opt_full && sv->state == sv_state_full)
				Com_Printf (MSG_DEBUG, "    Reject: no full server allowed\n");
			if (! opt_ipv4 && sv->address.ss_family == AF_INET)
				Com_Printf (MSG_DEBUG, "    Reject: no IPv4 servers allowed\n");
			if (! opt_ipv6 && sv->address.ss_family == AF_INET6)
				Com_Printf (MSG_DEBUG, "    Reject: no IPv6 servers allowed\n");
			if (opt_gametype && strcmp (gametype, sv->gametype) != 0)
				Com_Printf (MSG_DEBUG,
							"    Reject: gametype \"%s\" != requested \"%s\"\n",
							sv->gametype, gametype);
			if (strcmp (gamename, sv->gamename) != 0)
				Com_Printf (MSG_DEBUG,
							"    Reject: gamename \"%s\" != requested \"%s\"\n",
							sv->gamename, gamename);
		}

		// Check protocols, options, and gamename
		if (sv->state <= sv_state_uninitialized ||
			sv->protocol != protocol ||
			(! opt_empty && sv->state == sv_state_empty) ||
			(! opt_full && sv->state == sv_state_full) ||
			(! opt_ipv4 && sv->address.ss_family == AF_INET) ||
			(! opt_ipv6 && sv->address.ss_family == AF_INET6) ||
			(opt_gametype && strcmp (gametype, sv->gametype) != 0) ||
			strcmp (gamename, sv->gamename) != 0)
		{
			// Skip it
			continue;
		}

		// If the packet doesn't have enough free space for this server
		next_sv_size = (sv->address.ss_family == AF_INET ? 4 : 16) + 3;
		if (packetind +  13 > sizeof (packet))
		{
			// Send the packet to the client
			if (sendto (recv_socket, (void*)packet, packetind, 0,
					(const struct sockaddr*)addr, addrlen) < 0)
				Com_Printf (MSG_WARNING, "> WARNING: can't send %s (%s)\n",
							request_name, Sys_GetLastNetErrorString ());
			else
				Com_Printf (MSG_NORMAL, "> %s <--- %sResponse (%u servers)\n",
							peer_address, request_name, nb_servers);
			
			// Reset the packet index (no need to change the header)
			packetind = headersize;
			nb_servers = 0;
		}

		if (sv->address.ss_family == AF_INET)
		{
			const struct sockaddr_in* sv_sockaddr;
			unsigned int sv_addr;
			unsigned short sv_port;

			sv_sockaddr = (const struct sockaddr_in *)&sv->address;
			sv_addr = ntohl (sv_sockaddr->sin_addr.s_addr);
			sv_port = ntohs (sv_sockaddr->sin_port);

			// Use the address mapping associated with the server, if any
			if (sv->addrmap != NULL)
			{
				const addrmap_t* addrmap = sv->addrmap;

				sv_addr = ntohl (addrmap->to.sin_addr.s_addr);
				if (addrmap->to.sin_port != 0)
					sv_port = ntohs (addrmap->to.sin_port);

				Com_Printf (MSG_DEBUG,
							"  - Using mapped address %u.%u.%u.%u:%hu\n",
							sv_addr >> 24, (sv_addr >> 16) & 0xFF,
							(sv_addr >>  8) & 0xFF, sv_addr & 0xFF,
							sv_port);
			}

			// Heading '\'
			packet[packetind    ] = '\\';

			// IP address
			/*packet[packetind + 1] = (sv_addr >> 24);
			packet[packetind + 2] = ((sv_addr >> 16) & 0xFF);
			packet[packetind + 3] = ((sv_addr >>  8) & 0xFF);
			packet[packetind + 4] =  (sv_addr        & 0xFF);*/

			sprintf(buffer, "%x", (sv_addr >> 24));
			if(!buffer[1] || buffer[1] == '\0') {
				buffer[1] = buffer[0];
				buffer[0] = '0';
			}
			packet[packetind + 1] = buffer[0];
			packet[packetind + 2] = buffer[1];

			sprintf(buffer, "%x", ((sv_addr >> 16) & 0xFF));
			if(!buffer[1] || buffer[1] == '\0') {
				buffer[1] = buffer[0];
				buffer[0] = '0';
			}
			packet[packetind + 3] = buffer[0];
			packet[packetind + 4] = buffer[1];

			sprintf(buffer, "%x", ((sv_addr >> 8) & 0xFF));
			if(!buffer[1] || buffer[1] == '\0') {
				buffer[1] = buffer[0];
				buffer[0] = '0';
			}
			packet[packetind + 5] = buffer[0];
			packet[packetind + 6] = buffer[1];

			sprintf(buffer, "%x", (sv_addr & 0xFF));
			if(!buffer[1] || buffer[1] == '\0') {
				buffer[1] = buffer[0];
				buffer[0] = '0';
			}
			packet[packetind + 7] = buffer[0];
			packet[packetind + 8] = buffer[1];

			// Port
			//packet[packetind + 5] = (sv_port >> 8);
			//packet[packetind + 6] = (sv_port & 0xFF);
			/*packet[packetind + 5] = (sv_port - 16) & 0xFF;
			packet[packetind + 6] = (sv_port >> 8) & 0xFF;
			packet[packetind + 7] = (sv_port >> 12) & 0xFF;*/
			
			port1 = sv_port;
			port2 = port1 - (((int)(port1 / 256)) * 256);
			port1 = ((int)(port1 / 256));

			sprintf(buffer, "%x", port1);
			if(!buffer[1] || buffer[1] == '\0') {
				buffer[1] = buffer[0];
				buffer[0] = '0';
			}
			packet[packetind + 9] = buffer[0];
			packet[packetind + 10] = buffer[1];

			sprintf(buffer, "%x", port2);
			if(!buffer[1] || buffer[1] == '\0') {
				buffer[1] = buffer[0];
				buffer[0] = '0';
			}
			packet[packetind + 11] = buffer[0];
			packet[packetind + 12] = buffer[1];

			/*Com_Printf (MSG_DEBUG, "  - Sending server %u.%u.%u.%u:%hu\n",
						packet[packetind + 1], packet[packetind + 2],
						packet[packetind + 3], packet[packetind + 4],
						sv_port);*/

			Com_Printf(MSG_DEBUG, "\npacket = %s\n", packet);

			//packetind += 7;
			packetind += 13;
		}
		else
		{
			const struct sockaddr_in6* sv_sockaddr6;
			unsigned short sv_port;

			sv_sockaddr6 = (const struct sockaddr_in6 *)&sv->address;

			// Heading '/'
			packet[packetind] = '/';
			packetind += 1;

			// IP address
			memcpy (&packet[packetind], &sv_sockaddr6->sin6_addr.s6_addr,
					sizeof(sv_sockaddr6->sin6_addr.s6_addr));
			packetind += sizeof(sv_sockaddr6->sin6_addr.s6_addr);

			// Port
			sv_port = ntohs (sv_sockaddr6->sin6_port);
			packet[packetind    ] = sv_port >> 8;
			packet[packetind + 1] = sv_port & 0xFF;
			packetind += 2;
		}

		nb_servers++;
	}

	// If the packet doesn't have enough free space for the EOT mark
	if (packetind + 13 > sizeof (packet))
	{
		// Send the packet to the client
		if (sendto (recv_socket, (void*)packet, packetind, 0,
				(const struct sockaddr*)addr, addrlen) < 0)
			Com_Printf (MSG_WARNING, "> WARNING: can't send %s (%s)\n",
						request_name, Sys_GetLastNetErrorString ());
		else
			Com_Printf (MSG_NORMAL, "> %s <--- %sResponse (%u servers)\n",
						peer_address, request_name, nb_servers);
		
		// Reset the packet index (no need to change the header)
		packetind = headersize;
		nb_servers = 0;
	}

	// End Of Transmission
	packet[packetind    ] = '\\';
	packet[packetind + 1] = 'E';
	packet[packetind + 2] = 'O';
	packet[packetind + 3] = 'T';
	packet[packetind + 4] = '\0';
	packet[packetind + 5] = '\0';
	packet[packetind + 6] = '\0';
	packetind += 7;

	// Send the packet to the client
	if (sendto (recv_socket, (void*)packet, packetind, 0,
			(const struct sockaddr*)addr, addrlen) < 0)
		Com_Printf (MSG_WARNING, "> WARNING: can't send %s (%s)\n",
					request_name, Sys_GetLastNetErrorString ());
	else
		Com_Printf (MSG_NORMAL, "> %s <--- %sResponse (%u servers)\n",
					peer_address, request_name, nb_servers);
}


/*
====================
HandleInfoResponse

Parse infoResponse messages
====================
*/
static void HandleInfoResponse (server_t* server, const char* msg)
{
	const char* value;
	int new_protocol;
	char new_gametype [GAMETYPE_LENGTH];
	char* end_ptr;
	unsigned int new_maxclients, new_clients;

	// Check the challenge
	if (!server->challenge_timeout || server->challenge_timeout < crt_time)
	{
		Com_Printf (MSG_WARNING,
					"> WARNING: infoResponse with obsolete challenge from %s\n",
					peer_address);
		return;
	}
	value = SearchInfostring (msg, "challenge");
	if (!value || strcmp (value, server->challenge))
	{
		Com_Printf (MSG_WARNING, "> WARNING: invalid challenge from %s (%s)\n",
					peer_address, value);
		return;
	}

	// Check the value of "protocol"
 	value = SearchInfostring (msg, "protocol");
	if (value == NULL)
	{
		Com_Printf (MSG_WARNING,
					"> WARNING: invalid infoResponse from %s (no protocol value)\n",
					peer_address);
		return;
	}
	new_protocol = (int)strtol (value, &end_ptr, 0);
	if (end_ptr == value || *end_ptr != '\0')
	{
		Com_Printf (MSG_WARNING,
					"> WARNING: invalid infoResponse from %s (invalid protocol value: %s)\n",
					peer_address, value);
		return;
	}

	// Check the value of "gametype"
 	value = SearchInfostring (msg, "gametype");
	if (value != NULL)
	{
		if (strchr (value, ' ') != NULL)
		{
			Com_Printf (MSG_WARNING,
						"> WARNING: invalid infoResponse from %s (game type contains whitespaces)\n",
						peer_address);
			return;
		}
	}
	// Default to gametype = "0" if the server hasn't sent this information
	else
		value = "0";

	strncpy (new_gametype, value, sizeof (new_gametype) - 1);
	new_gametype[sizeof (new_gametype) - 1] = '\0';


	// Check the value of "maxclients"
	value = SearchInfostring (msg, "sv_maxclients");
	new_maxclients = ((value != NULL) ? atoi (value) : 0);
	if (new_maxclients == 0)
	{
		Com_Printf (MSG_WARNING,
					"> WARNING: invalid infoResponse from %s (sv_maxclients = %d)\n",
					peer_address, new_maxclients);
		return;
	}

	// Check the presence of "clients"
	value = SearchInfostring (msg, "clients");
	if (value == NULL)
	{
		Com_Printf (MSG_WARNING,
					"> WARNING: invalid infoResponse from %s (no \"clients\" value)\n",
					peer_address);
		return;
	}
	new_clients = ((value != NULL) ? atoi (value) : 0);

	// Q3A doesn't send a gamename, so we add it manually
	value = SearchInfostring (msg, "gamename");
	if (value == NULL)
		value = GAMENAME_EF2;
	else if (value[0] == '\0')
	{
		Com_Printf (MSG_WARNING,
					"> WARNING: invalid infoResponse from %s (game name is void)\n",
					peer_address);
		return;
	}
	else if (strchr (value, ' ') != NULL)
	{
		Com_Printf (MSG_WARNING,
					"> WARNING: invalid infoResponse from %s (game name contains whitespaces)\n",
					peer_address);
		return;
	}
	
	if (! Game_IsAccepted (value))
	{
		Com_Printf (MSG_WARNING,
					"> WARNING: Rejecting infoResponse from %s (game \"%s\" is not accepted)\n",
					peer_address, value);
		return;
	}

	// Save some useful informations in the server entry
	strncpy (server->gamename, value, sizeof (server->gamename) - 1);
	server->protocol = new_protocol;
	strncpy (server->gametype, new_gametype, sizeof (server->gametype) - 1);
	if (new_clients == 0)
		server->state = sv_state_empty;
	else if (new_clients == new_maxclients)
		server->state = sv_state_full;
	else
		server->state = sv_state_occupied;

	// Set a new timeout
	server->timeout = crt_time + TIMEOUT_INFORESPONSE;
}


// ---------- Public functions ---------- //

extern server_t *servers;

/*
====================
HandleMessage

Parse a packet to figure out what to do with it
====================
*/
void HandleMessage (const char* msg, size_t length,
					const struct sockaddr_storage* address,
					socklen_t addrlen,
					socket_t recv_socket)
{
	server_t* server;

	// If it's an heartbeat
	if (!strncmp (S2M_HEARTBEAT, msg, strlen (S2M_HEARTBEAT)))
	{
		char gameId [64];

		// Extract the game id
		sscanf (msg + strlen (S2M_HEARTBEAT) + 1, "%63s", gameId);

		// Check if this server goes down
		if(!strncmp(gameId, "TikiServer-Flatline", 19)) {
			server = Sv_GetByAddr(address, addrlen, false);
			Sv_IsActive((unsigned int)(server - servers));
			return;
		}


		Com_Printf (MSG_NORMAL, "> %s ---> heartbeat (%s)\n",
					peer_address, gameId);

		// Get the server in the list (add it to the list if necessary)
		server = Sv_GetByAddr (address, addrlen, true);
		if (server == NULL)
			return;

		assert (server->state != sv_state_unused_slot);

		// Ask for some infos
		SendGetInfo (server, recv_socket);
	}

	// If it's an infoResponse message
	else if (!strncmp (S2M_INFORESPONSE, msg, strlen (S2M_INFORESPONSE)))
	{
		Com_Printf (MSG_NORMAL, "> %s ---> infoResponse\n", peer_address);
	
		server = Sv_GetByAddr (address, addrlen, false);
		if (server == NULL)
		{
			Com_Printf (MSG_WARNING,
						"> WARNING: infoResponse from unknown server %s\n",
						peer_address);
			return;
		}

		HandleInfoResponse (server, msg + strlen (S2M_INFORESPONSE));
	}

	// If it's a getservers request
	else if (!strncmp (C2M_GETSERVERS, msg, strlen (C2M_GETSERVERS)))
	{
		HandleGetServers (msg + strlen (C2M_GETSERVERS), address, addrlen,
						  recv_socket, false);
	}

	// If it's a getserversExt request
	else if (!strncmp (C2M_GETSERVERSEXT, msg, strlen (C2M_GETSERVERSEXT)))
	{
		HandleGetServers (msg + strlen (C2M_GETSERVERSEXT), address, addrlen,
						  recv_socket, true);
	}
}
