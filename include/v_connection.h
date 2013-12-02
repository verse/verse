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
 * Authors: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

#if !defined V_CONNECTION_H
#define V_CONNECTION_H

#include <openssl/ssl.h>

#include <sys/time.h>
#include <pthread.h>

#include "v_network.h"
#include "v_history.h"
#include "v_context.h"

#ifdef WITH_KERBEROS
/* Kerberos using */
#define NO_KERBEROS					0
#define USE_KERBEROS				1
#endif

/* Client states (UDP) */
#define UDP_CLIENT_STATE_RESERVED	0
#define	UDP_CLIENT_STATE_REQUEST	1	/* Has to be the first client state */
#define UDP_CLIENT_STATE_PARTOPEN	2
#define UDP_CLIENT_STATE_OPEN		3
#define UDP_CLIENT_STATE_CLOSING	4
#define UDP_CLIENT_STATE_CLOSED		5	/* Has to be the last client state */

/* Server states (UDP) */
#define UDP_SERVER_STATE_RESERVED	0
#define	UDP_SERVER_STATE_LISTEN		1	/* Has to be the first server state */
#define	UDP_SERVER_STATE_RESPOND	2
#define	UDP_SERVER_STATE_OPEN		3
#define	UDP_SERVER_STATE_CLOSEREQ	4
#define	UDP_SERVER_STATE_CLOSED		5	/* Has to be the last server state */

/* Client states (TCP) */
#define TCP_CLIENT_STATE_RESERVED				0
#define TCP_CLIENT_STATE_USRAUTH_NONE			1
#define TCP_CLIENT_STATE_USRAUTH_DATA			2
#define TCP_CLIENT_STATE_NEGOTIATE_COOKIE_DED	3
#define TCP_CLIENT_STATE_NEGOTIATE_NEWHOST		4
#define TCP_CLIENT_STATE_STREAM_OPEN			5
#define TCP_CLIENT_STATE_CLOSING				6
#define TCP_CLIENT_STATE_CLOSED					7
#ifdef WITH_KERBEROS
#define TCP_CLIENT_STATE_USRAUTH_KRB			8
#endif

/* Server states (TCP) */
#define TCP_SERVER_STATE_RESERVED				0
#define TCP_SERVER_STATE_LISTEN					1
#define TCP_SERVER_STATE_RESPOND_METHODS		2
#define TCP_SERVER_STATE_RESPOND_USRAUTH		3
#define TCP_SERVER_STATE_NEGOTIATE_COOKIE_DED	4
#define TCP_SERVER_STATE_NEGOTIATE_NEWHOST		5
#define TCP_SERVER_STATE_STREAM_OPEN			6
#define TCP_SERVER_STATE_CLOSING				7
#define TCP_SERVER_STATE_CLOSED					8
#ifdef WITH_KERBEROS
#define TCP_SERVER_STATE_RESPOND_KRB_AUTH		9
#endif

/* Maximal number of client or server state.  */
#define STATE_COUNT				(((UDP_CLIENT_STATE_CLOSED > UDP_SERVER_STATE_CLOSED) ? UDP_CLIENT_STATE_CLOSED : UDP_SERVER_STATE_CLOSED)+1)

/* Temporary flags that keeps information about receiving system commands
 * in OPEN and CLOSEREQ states */
#define SYS_CMD_NEGOTIATE_FPS					1

/* This structure stores "connection" attempts and pointers at callback
 * functions for specific state. Each state has different set of callback
 * functions. If pointer at function is NULL, then this calback function
 * will not be called. */
typedef struct VConnectionState {
	unsigned short			attempts;			/* Number of peer's send packet attempts in this state */
	struct timeval			tv_state_began;		/* Time [s], when current state began */
	/* Pointers at callback functions for system commands (each state has different set of callback function)*/
	int						(*CHANGE_L_cb)(struct vContext *C, struct Generic_Cmd *cmd);
	int						(*CHANGE_R_cb)(struct vContext *C, struct Generic_Cmd *cmd);
	int						(*CONFIRM_L_cb)(struct vContext *C, struct Generic_Cmd *cmd);
	int						(*CONFIRM_R_cb)(struct vContext *C, struct Generic_Cmd *cmd);
} VConnectionState;

/*
 * VDgramConn is structure storing information about datagram (UDP) connection.
 */
typedef struct VDgramConn {
	/* IO */
	struct IO_CTX			io_ctx;				/* Context for sending and receiving data */
	/* States */
	unsigned short			host_state;			/* State of host (current state of current machine) */
	/* Addresses */
	struct VNetworkAddress	peer_address;		/* Address of peer and port number */
	struct VNetworkAddress	host_address;		/* Address of host and port number */
	/* Resend mechanism */
	struct timeval			tv_pay_recv;		/* Time, when last payload packet was received from the address:port */
	struct timeval			tv_pay_send;		/* Time, when last payload packet was sent to the address:port */
	struct timeval			tv_ack_recv;		/* Time, when last acknowledgment packet was received  the address:port */
	struct timeval			tv_ack_send;		/* Time, when last acknowledgment packet was sent to the address:port */
	unsigned char			flags;				/* Flags for this connection */
	unsigned int			peer_id;			/* ID of first received payload packet */
	unsigned int			host_id;			/* Random number (used for initial value of PayloadID) */
	unsigned int			last_r_pay;			/* ID of last received payload packets */
	unsigned int			last_r_ack;			/* ID of last received acknowledgment packets (ACK and NAK commands) */
	unsigned int			ank_id;				/* ID of last acknowledged payload packet */
	unsigned int			count_s_pay;		/* Counter of sent payload packets */
	unsigned int			count_s_ack;		/* Counter of sent acknowledgment packets (ACK and NAK commands) */
	unsigned int			last_acked_pay;		/* ID of last acked payload packet */
	/* Congestion and flow control */
	unsigned char			fc_meth;			/* Negotiated Flow Control method */
	unsigned char			cc_meth;			/* Negotiated Congestion Control method */
	unsigned int			srtt;				/* Smoothed Round-Trip Time */
	unsigned int			cwin;				/* Congestion Control Window */
	unsigned int			rwin_host;			/* Flow Control Window of host (my) */
	unsigned int			rwin_peer;			/* Flow Control Window of peer */
	unsigned int			sent_size;			/* Size of data that were sent and were not acknowledged */
	unsigned char			rwin_host_scale;	/* Scaling of host Flow Control Window (my) */
	unsigned char			rwin_peer_scale;	/* Scaling of perr Flow Control Window */
	unsigned char			host_cmd_cmpr;		/* Command compression used by host for sedning commands */
	unsigned char			peer_cmd_cmpr;		/* Command compression used by peer for sending commands */
	/* States */
	struct VConnectionState	state[STATE_COUNT];	/* Array of structure storing state specific things (callbacks, counters, etc.) */
	/* Histories */
	struct AckNakHistory	ack_nak;			/* Compressed array of ACK and NAK commands */
	struct VPacket_History	packet_history;		/* History of sent and not acknowledged packets */
	/* Multi-threading */
	pthread_mutex_t			mutex;				/* Mutex used, when state of connection is changing */
} VDgramConn;

/* VStreamConn is structure storing information about stream (TCP) connection. */
typedef struct VStreamConn {
	/* IO */
	struct IO_CTX			io_ctx;				/* Context for sending and receiving data */
	/* States */
	unsigned short			host_state;			/* State of host (current state of current machine) */
	/* Addresses */
	struct VNetworkAddress	peer_address;		/* Address of peer and port number */
	struct VNetworkAddress	host_address;		/* Address of host and port number */
	/* Security */
#ifdef WITH_KERBEROS
	krb5_context			krb5_ctx;			/* Kerberos library context */
#endif
	/* Flow control */
	int						socket_buffer_size;	/* Size of socket buffer */
	int						sent_data_offset;	/* Offset of data poped from TCP stack */
	int						pushed_data_offset;	/* Offset of data pushed to TCP stack*/
	/* Multi-threading */
	pthread_mutex_t			mutex;				/* Mutex used, when state of connection is changing */
} VStreamConn;

int v_conn_dgram_handle_sys_cmds(struct vContext *C,
		const unsigned short state_id);
int v_conn_dgram_cmp_state(struct VDgramConn *stream_conn,
		unsigned short state);
void v_conn_dgram_set_state(struct VDgramConn *stream_conn,
		unsigned short state);
void v_conn_dgram_init(struct VDgramConn *dgram_conn);
void v_conn_dgram_destroy(struct VDgramConn *dgram_conn);
void v_conn_dgram_clear(struct VDgramConn *dgram_conn);

int v_conn_stream_cmp_state(struct VStreamConn *stream_conn,
		unsigned short state);
void v_conn_stream_set_state(struct VStreamConn *stream_conn,
		unsigned short state);
void v_conn_stream_init(struct VStreamConn *stream_conn);
void v_conn_stream_destroy(struct VStreamConn *stream_conn);

#endif

