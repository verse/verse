/*
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2010, Jiri Hnidek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END BSD LICENSE BLOCK *****
 *
 * authors: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

#if !defined V_SESSION_H
#define V_SESSION_H

#if defined WITH_PAM
#if defined  __APPLE__
/* MAC OS X */
#include <pam/pam_appl.h>
#include <pam/pam_misc.h>
#elif defined __linux__
/* Linux */
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#else
/* Other UNIX like operation systems */
#endif
#endif

#include "verse_types.h"

#include "v_connection.h"
#include "v_out_queue.h"
#include "v_in_queue.h"

#define COOKIE_SIZE		16

/* When negotiation is not used, then client and server consider
 * FPS to be 60 */
#define DEFAULT_FPS				60.0

/* Data Exchange Definition */
typedef struct VDED {
	char				*str;	/* String of negotiated DED */
} VDED;

typedef struct VCookie {
	char				*str;	/* Secret string */
	struct timeval		tv;		/* Cookie is valid since this time */
} VCookie;

typedef struct VSession {
	/* Thread stuffs */
	pthread_t				tcp_thread;		/* TCP/WebSocket thread for this session */
	pthread_attr_t			tcp_thread_attr;/* Attributes of the TCP/WebSocket thread */
	pthread_t				udp_thread;		/* UDP Thread for this session */
	pthread_attr_t			udp_thread_attr;/* Attributes of the UDP thread */
	/* Connection */
	char 					*peer_hostname;	/* Hostname of the peer */
	struct VNetworkAddress	peer_address;	/* Address of the peer and port number */
	char					*service;		/* Name or port number of the service */
	uint32					session_id;		/* The session id visible for client */
	struct VDgramConn		*dgram_conn;	/* Datagram (UDP) connection for real-time data exchange */
	struct VStreamConn		*stream_conn;	/* Stream (TCP) connection for authentication and new port negotiation */
	uint16					user_id;		/* Unique ID of the user at Verse server */
	uint32					avatar_id;		/* Unique ID of session of this verse client */
	struct VDED				ded;			/* Data Exchange Definition */
	uint16					flags;			/* Flags from verse_send_connect_request function */
	uint8					tmp;			/* Temporary value */
	int						usr_auth_att;	/* Number of user authentintication attempts */
#if defined WITH_PAM
	/* PAM authentication (verse server specific) */
	struct pam_conv			conv;
	pam_handle_t			*pamh;
#endif
	char					*username;		/* Username used for this connection. */
	char					*host_url;		/* UDP connection URL negotiated during authentication */
	struct VCookie			peer_cookie;	/* Cookie negotiated during authentication */
	struct VCookie			host_cookie;	/* Cookie negotiated during authentication */
	/* Queues */
	struct VOutQueue		*out_queue;		/* Queue of outgoing data (node commands) */
	struct VInQueue			*in_queue;		/* Queue of incoming data (fake and node commands) */
	/* FPS used by UDP and TCP data connection */
	float					fps_host;		/* FPS used by this host */
	float					fps_peer;		/* Negotiated FPS used by peer */
	unsigned char			tmp_flags;		/* Temporary flags (notification of received system commands) */
	/* Information about client program */
	char					*client_name;
	char					*client_version;
} VSession;

void v_init_session(struct VSession *vsession);
void v_destroy_session(struct VSession *vsession);

#endif
