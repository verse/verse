/*
 * $Id: v_network.h 903 2011-08-08 12:38:35Z jiri $
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

#if !defined V_NETWORK_H
#define V_NETWORK_H

#ifdef WITH_OPENSSL
#include <openssl/ssl.h>
#endif

#include <netinet/in.h>
#include <limits.h>

#include "v_commands.h"
#include "v_sys_commands.h"
#include "v_node_commands.h"
#include "v_taggroup_commands.h"
#include "v_context.h"

#define PAY_FLAG	1 << 7
#define ACK_FLAG	1 << 6
#define ANK_FLAG	1 << 5
#define SYN_FLAG	1 << 4
#define FIN_FLAG	1 << 3
/* Reservation of flags */
/* #define _FLAG	1 << 2 */
/* #define _FLAG	1 << 1 */
/* #define _FLAG	1 */

#define IPV4		4
#define IPV6		6

#define UDP			IPPROTO_UDP
#define TCP			IPPROTO_TCP

/* Maximal size of one UDP packet: 2^16 */
#define MAX_PACKET_SIZE 			USHRT_MAX
/* Maximal number of system commands in packet */
#define MAX_SYSTEM_COMMAND_COUNT	64
/* Theoretical maximal number of node commands in packet: PMTU: 2^16 and all
 * node commands are 2 bytes long (command id + command length) */
#define MAX_NODE_COMMAND_COUNT		(MAX_PACKET_SIZE / (1+1))
/* Default Ethernet MTU */
#define DEFAULT_MTU					(1500 - 40 - 8)
/* Define alpha value for computing SRTT */
#define RTT_ALPHA					0.9

/* It was not possible to send packet, because error of*/
#define SEND_PACKET_ERROR			0
/* All data from queues were send in packet successfully */
#define SEND_PACKET_SUCCESS			1
/* Packet was send full and there are data pending in queues */
#define SEND_PACKET_FULL			2
/* Sending of packet was canceled, because there was nothing to send
 * or there was congestion or flow control did not allow it, etc. */
#define SEND_PACKET_CANCELED		3

#define RECEIVE_PACKET_RESERVED			0
#define	RECEIVE_PACKET_ERROR			1
#define RECEIVE_PACKET_SUCCESS			2
#define RECEIVE_PACKET_TIMEOUT			3
#define RECEIVE_PACKET_CORRUPTED		4
#define RECEIVE_PACKET_FAKED			5
#define RECEIVE_PACKET_ATTEMPTS_EXCEED	6
#define RECEIVE_PACKET_UNORDERED		7

#define SOCKET_CONNECTED			1
#define SOCKET_SECURED				2

/* How long should client or server wait for packet in select() function */
#define TIMEOUT			1

/* Initial value of timeout */
#define INIT_TIMEOUT	1
/* Maximal back off of truncated binary exponential backing off  */
#define MAX_BACK_OFF	32

/**
 * Verse packet structure:
 *
 *    0             0     1         1         2     2             3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-------+-------+-+-+-+-+-+-----+---------------+---------------+
 *   |       |       |P|A|A|S|F|     |                               |
 * 0 |Version|  RES  |A|C|N|Y|I| RES |            Window Size        |
 *   |       |       |Y|K|K|N|N|     |                               |
 *   +-------+-------+-+-+-+-+-+-----+---------------+---------------+
 * 1 |                           Payload ID                          |
 *   +---------------+---------------+---------------+---------------+
 * 2 |                           ACK_NAK ID                          |
 *   +---------------+---------------+---------------+---------------+
 * 3 |                             ANK ID                            |
 *   +---------------+---------------+---------------+---------------+
 * 4 |                        ACK & NAK Commands                     |
 *   .                                                               .
 *   .                                                               .
 *   +---------------+---------------+---------------+---------------+
 *   |                       Next System Commands                    |
 *   .                                                               .
 *   .                                                               .
 *   |                                                               |
 *   +---------------+---------------+---------------+---------------+
 *   |                         Node Commands                         |
 *   .                                                               .
 *   .                                                               .
 *   |                                                               |
 *   +---------------+---------------+---------------+---------------+
 */

#define VERSE_PACKET_HEADER_SIZE	16	/* Size of Verse packet header in bytes */

/* Verse packet header */
typedef struct VPacketHeader {
	unsigned char	version;
	unsigned char	flags;
	unsigned short	window;
	unsigned int	payload_id;
	unsigned int	ack_nak_id;
	unsigned int	ank_id;
} VPacketHeader;

/* Verse packet */
typedef struct VPacket {
	/* Length of packet (from UDP header) */
	unsigned short				len;
	/* Verse packet header */
	struct VPacketHeader		header;
	/* Flag controlling if this packed has been already acked? */
	char						acked;
	/* Array of unions of system commands */
	VSystemCommands sys_cmd[MAX_SYSTEM_COMMAND_COUNT];
	/* Number of system commands in packet */
	unsigned short				sys_cmd_count;
	/* Pointer to the received buffer, where are stored node commands */
	uint8						*data;
	uint16						data_size;

} VPacket;

/**
 * Verse packet structure:
 *    0             0 0   1         1         2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-------+-------+---------------+---------------+---------------+
 * 0 |Version|      Reservation      |          Message Size         |
 *   +-------+-------+---------------+---------------+---------------+
 * 1 |                            Commands                           |
 *   .                                                               .
 *
 *   .                                                               .
 *   |                                                               |
 *   +---------------------------------------------------------------+
 *
 */

#define VERSE_MESSAGE_HEADER_SIZE	4	/* Size of Verse message header in bytes */

/**
 * Verse message header
 */
typedef struct VMessageHeader {
	unsigned char	version;
	unsigned short	len;
} VMessageHeader;

/**
 * Verse message used for TCP communication (user authentication only)
 */
typedef struct VMessage {
	/* Verse message header */
	struct VMessageHeader		header;
	/* Array of unions of system commands */
	VSystemCommands sys_cmd[MAX_SYSTEM_COMMAND_COUNT];
} VMessage;

/* Network address of the peer */
typedef struct VNetworkAddress {
	unsigned short			ip_ver;		/* Version of IP protocol */
	union {
		struct sockaddr_in	ipv4;		/* IPv4 */
		struct sockaddr_in6	ipv6;		/* IPv6 */
	} addr;
	int						protocol;	/* Type of protocol at transport layer */
	unsigned short			port;		/* Port number */
} VNetworkAddress;

/* Context for sending and receiving packets */
typedef struct IO_CTX {
	struct VNetworkAddress	host_addr;		/* Address of application running this code */
	struct VNetworkAddress	peer_addr;		/* Address of peer (other side of communication) */
	char 					*buf;			/* Buffer for incoming/outgoing packet/message */
	ssize_t					buf_size;		/* Size of received/sent packet/message */
	int						sockfd;			/* UDP/TCP/WebSocket socket */
	unsigned char			flags;			/* Flags for sending and receiving context */
	unsigned short			mtu;			/* MTU of connection discovered with PMTU */
#ifdef WITH_OPENSSL
	/* Security */
	SSL						*ssl;
	BIO						*bio;
#endif
} IO_CTX;

/* Structure for storing data from parsed URL */
typedef struct VURL {
	char		*scheme;				/* Scheme of URL */
	char		transport_protocol;		/* Transport protocol used for data exchange */
	char		security_protocol;		/* Security protocol */
	char		ip_ver;					/* Version of IP */
	char		*node;					/* Hostname or IP address */
	char		*service;				/* Port number or name of service */
} VURL;

int v_parse_url(const char *str, struct VURL *url);
void v_print_url(const int level, struct VURL *url);
void v_clear_url(struct VURL *url);

int v_exponential_backoff(const int steps);

int v_tcp_read(struct IO_CTX *io_ctx, int *error_num);
int v_tcp_write(struct IO_CTX *io_ctx, int *error_num);

int v_receive_packet(struct IO_CTX *io_ctx, int *error_num);
int v_send_packet(struct IO_CTX *io_ctx, int *error_num);

int v_unpack_packet_header(const char *buffer, const unsigned short buffer_len, struct VPacket *vpacket);
int v_pack_packet_header(const VPacket *vpacket, char *buffer);

int v_unpack_message_header(const char *buffer, const unsigned short buffer_len, struct VMessage *vmessage);
int v_pack_message_header(const VMessage *vmessage, char *buffer);

void v_print_packet_header(const unsigned char level, const VPacket *vpacket);

void v_print_send_message(struct vContext *C);
void v_print_receive_message(struct vContext *C);

void v_print_send_packet(struct vContext *C);
void v_print_receive_packet(struct vContext *C);

void v_print_addr_port(const unsigned char level, const struct VNetworkAddress *addr);
void v_print_addr(const unsigned char level, const struct VNetworkAddress *addr);

int v_compare_addr(const struct VNetworkAddress *addr1, const struct VNetworkAddress *addr2);
int v_compare_addr_and_port(const struct VNetworkAddress *addr1, const struct VNetworkAddress *addr2);

#endif

