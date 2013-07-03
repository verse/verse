/*
 * $Id: vs_main.h 1348 2012-09-19 20:08:18Z jiri $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Contributor(s): Jiri Hnidek <jiri.hnidek@tul.cz>.
 *
 */

#ifndef VS_MAIN_H_
#define VS_MAIN_H_

#include <openssl/ssl.h>

#include <stdio.h>

#include <pthread.h>
#include <semaphore.h>

#include "verse_types.h"

#include "vs_user.h"
#include "vs_node.h"

#include "v_connection.h"
#include "v_network.h"
#include "v_context.h"
#include "v_list.h"

/* Default configuration file of verse server */
#define DEFAULT_SERVER_CONFIG_FILE			"/etc/verse/server.ini"

/* Flags vs_ctx->flags */
#define SERVER_DEBUG_MODE					1		/* Server will not detach from terminal, etc. */

#define SERVER_PORT_USED					1		/* Server port is in use */

#define MAX_USER_AUTH_ATTEMPTS				1		/* Maximal attempts of user authentication */

#define LINE_LEN							65536	/* Maximal length of the length of the CSV file */

#define AUTH_METHOD_CSV_FILE				1
#define AUTH_METHOD_PAM						2
#define AUTH_METHOD_LDAP					3

/**
 * States of Verse server
 */
typedef enum ServerState{
	SERVER_STATE_RESERVED =	0,
	SERVER_STATE_CONF =		1,
	SERVER_STATE_READY =	2,
	SERVER_STATE_CLOSING =	3,
	SERVER_STATE_CLOSED =	4
} ServerState;

typedef struct VS_Port {
	unsigned short		port_number;
	char				flag;
} VS_Port;

typedef struct VSData {
	struct VHashArrayBase	nodes;					/* Hashed linked list of Verse Nodes */

	/* Node IDs in range 1000 - (2^16 - 1) are nodes intended as representation of users */
	unsigned int		last_common_node_id;		/* Last assigned common node_id (in range: 2^16 - 2^32) */

	/* Fast access to special nodes */
	struct VSNode		*root_node;					/* Pointer at root node (node_id=0) */
	struct VSNode		*avatar_node;				/* Pointer at parent of all avatar nodes (node_id=1) */
	struct VSNode		*user_node;					/* Pointer at parent of all user nodes (node_id=2) */
	struct VSNode		*scene_node;				/* Pointer at parent of all scene nodes (node_id=3) */
	/* Thread staff */
	pthread_mutex_t		mutex;						/* Connection threads needs create avatar nodes occasionally */
	sem_t				sem;						/* Semaphore used for notification data thread (some data were added to the queue) */
} VSData;

/* Verse Server Context */
typedef struct VS_CTX {
	/* Configuration of connections */
	unsigned short 		max_connection_attempts;	/* Maximum number of connection attempts from one address:port */
	unsigned short 		max_sessions;				/* Maximum number of connected clients */
	unsigned short		max_sockets;				/* Maximum number of sockets server can create */
	unsigned short 		tcp_port;					/* TCP port number which server is listening on */
	unsigned short		ws_port;					/* WebSocket TCP port number */
	int					stream_protocol;			/* Transport protocol for listening (Only TCP is supported now) */
	char				dgram_protocol;				/* Flag with datagrame protocol supported for data exchange (Only UDP now) */
	char				security_protocol;			/* Flag with security protocol supported for data exchange (Only None and DTLS now)*/
	unsigned int		flag;						/* Miscellaneous flags */
	enum ServerState	state;						/* States of server */
	/* Debug print */
	unsigned char		print_log_level;			/* Amount of information printed to log file */
	FILE				*log_file;					/* File used for logs */
	/* Data for connections */
	unsigned short 		connected_clients;			/* Number of connected clients */
	struct VSession		**vsessions;				/* List of sessions and session with connection attempts */
	unsigned int		in_queue_max_size;			/* Default value of max size of incoming queue */
	unsigned int		out_queue_max_size;			/* Default value of max size of outgoing queue */
	/* Ports for connections */
	unsigned short		port_low;					/* The lowest port number in port range */
	unsigned short		port_high;					/* The highest port number in port range */
	struct VS_Port		*port_list;					/* List of free ports used for communication with clients */
	/* Data for packet receiving */
	struct IO_CTX 		tcp_io_ctx;					/* Verse context for TCP connection attempts */
	struct IO_CTX		ws_io_ctx;					/* Verse context for WebSocket connection attempts */
	/* SSL context */
	SSL_CTX				*tls_ctx;					/* SSL context for main secured TCP TLS socket */
	SSL_CTX				*dtls_ctx;					/* SSL context for secured UDP DTLS connections (shared with all connections) */
	/* Path to files with certificates */
	char				*public_cert_file;			/* Path to the certificate file with public key */
	char				*ca_cert_file;				/* Path to the certificate file with CA certificate */
	char				*private_cert_file;			/* Path to the certificate file with private key */
	/* Host used for UDP connection */
	char				*hostname;					/* Hostname  used for UDP connection and
													   negotiated during authentication of users */
	char				*ded;						/* String of Data Exchange Definition (Version, URL, etc.) */
	unsigned char		cc_meth;					/* Allowed methods of Congestion Control */
	unsigned char		fc_meth;					/* Allowed methods of Flow Control */
	unsigned char		rwin_scale;					/* Scale of Flow Control Window */
	unsigned char		cmd_cmpr;					/* Prefered command compression */
	/* User authentication */
	char				auth_type;					/* Type of user authentication */
	char				*csv_user_file;				/* CSV file with definition of user account */
	struct VListBase	users;						/* Linked list of users */
	struct VSUser		*other_users;				/* The pointer at fake user other_users */
	unsigned char		default_perm;				/* Default permissions for other users */
	/* Data shared at verse server */
	struct VSData		data;
	pthread_t			data_thread;				/* Data thread */
	pthread_attr_t		data_thread_attr;			/* The attribute of data thread */
	/* WebSocket thread */
	pthread_t			websocket_thread;			/* WebSocket thread */
	pthread_attr_t		websocket_thread_attr;		/* The attribute of WebSocket thread*/
} VS_CTX;

#endif
