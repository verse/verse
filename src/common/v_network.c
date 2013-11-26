/*
 * $Id:
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

#include <openssl/ssl.h>
#include <openssl/err.h>



#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include "verse.h"

#ifdef WITH_KERBEROS
#include <krb5.h>
#endif

#include "verse_types.h"
#include "v_network.h"
#include "v_unpack.h"
#include "v_pack.h"
#include "v_common.h"
#include "v_context.h"
#include "v_connection.h"

static int v_compare_ipv4_addr(const struct in_addr *addr1, const struct in_addr *addr2);
static int v_compare_ipv6_addr(const struct in6_addr *addr1, const struct in6_addr *addr2);

/**
 * \brief Parse string of URL and store items in structure VURL
 * \param[in]	*str	The string of URL
 * \param[out]	*url	The structure for storing data from parsed URL
 * \return		This function returns 1, when it was correct URL and it
 * contained all needed items and it returns 0, when there were some problem
 * during parsing.
 */
int v_parse_url(const char *str, struct VURL *url)
{
	int str_pos=0;

	if(str==NULL) {
		return 0;
	}

	/* printf("\t%s\n", &str[str_pos]);*/

	/* ==== Scheme ==== */

	/* URL has to begin with string verse */
	if(strncmp(&str[str_pos], "verse", strlen("verse"))!=0) {
		return 0;
	} else {
		str_pos += strlen("verse");
		/* printf("\t%s\n", &str[str_pos]);*/
	}

	/* There has to be '-' after "verse" string */
	if(str[str_pos]!='-') {
		return 0;
	} else {
		str_pos += 1;
		/* printf("\t%s\n", &str[str_pos]);*/
	}

	/* There has to be "udp" protocol, because only UDP is supported now  */
	if(strncmp(&str[str_pos], "udp", strlen("udp"))==0) {
		url->dgram_protocol = VRS_DGRAM_TP_UDP;
		str_pos += strlen("udp");
		/*printf("\t%s\n", &vsession->host_url[str_pos]);*/
	} else {
		return 0;
	}

	/* There has to be '-' after "udp" string */
	if(str[str_pos]!='-') {
		return 0;
	} else {
		str_pos += 1;
		/* printf("\t%s\n", &str[str_pos]);*/
	}

	/* There could be "none" or "dtls" string */
	if(strncmp(&str[str_pos], "none", strlen("none"))==0) {
		url->security_protocol = VRS_DGRAM_SEC_NONE;
		str_pos += strlen("none");
		/*printf("\t%s\n", &str[str_pos]);*/
	} else if(strncmp(&str[str_pos], "dtls", strlen("dtls"))==0) {
		url->security_protocol = VRS_DGRAM_SEC_DTLS;
		str_pos += strlen("dtls");
		/*printf("\t%s\n", &str[str_pos]);*/
	} else {
		return 0;
	}

	/* There has to be separator between scheme and address  */
	if(strncmp(&str[str_pos], "://", strlen("://"))!=0) {
		return 0;
	} else {
		url->scheme = strndup(str, str_pos);
		str_pos += strlen("://");
		/*printf("\t%s\n", &str[str_pos]);*/
	}

	/* Get address from URL */
	if(str[str_pos]=='[') {
		size_t i;
		int j;
		for(i=str_pos, j=0; i<strlen(str); i++, j++) {
			if(str[i]==']') {
				url->node = strndup(&str[str_pos+1], j-1);
				url->ip_ver = IPV6;
				break;
			}
		}
		str_pos += j+2;
		/*printf("\t%s\n", &str[str_pos]);*/
	} else {
		size_t i;
		int j;
		for(i=str_pos, j=0; i<strlen(str); i++, j++) {
			if(str[i]==':') {
				url->node = strndup(&str[str_pos], j);
				url->ip_ver = 0;	/* it could be ipv4 or hostname */
				break;
			}
		}
		str_pos += j+1;
		/*printf("\t%s\n", &str[str_pos]);*/
	}

	url->service = strdup(&str[str_pos]);
	/*printf("service: %s\n", *service);*/

	return 1;
}

void v_print_url(const int level, struct VURL *url)
{
	v_print_log(level, "URL:\n");
	v_print_log_simple(level, "\tscheme: %s\n", url->scheme);
	if(url->dgram_protocol==VRS_DGRAM_TP_UDP) {
		v_print_log_simple(level, "\tdgram_protocol: UDP\n");
	} else {
		v_print_log_simple(level, "\tdgram_protocol: unknow\n");
	}
	if(url->security_protocol==VRS_DGRAM_SEC_NONE) {
		v_print_log_simple(level, "\tsecurity_protocol: NONE\n");
	} else if(url->security_protocol==VRS_DGRAM_SEC_DTLS) {
		v_print_log_simple(level, "\tsecurity_protocol: DTLS\n");
	} else {
		v_print_log_simple(level, "\tsecurity_protocol: unknown\n");
	}
	v_print_log_simple(level, "\tnode: %s\n", url->node);
	v_print_log_simple(level, "\tservice: %s\n", url->service);
}

/**
 * \brief Return value for exponential backoff. Maximal value could be MAX_BACK_OFF
 * \param[in]	steps	The number of exponential backoff steps
 * \return	$ 2^{steps} $
 */
int v_exponential_backoff(const int steps)
{
	int i, ret=2;

	for(i=0; i<steps; i++) {
		ret *= 2;
	}

	return (MAX_BACK_OFF>(ret - 1)) ? (ret - 1) : MAX_BACK_OFF;
}

/**
 * \brief Get header of verse packet from received buffer
 * \param[in]	*buffer		The received buffer
 * \param[in]	buffer_len	The size of received buffer
 * \param[out]	*vpacket	The structure of packet, that will be filled with
 * information from the buffer.
 * \return This function returns relative buffer position of buffer proceeding.
 * When corrupted header detected, then it returns -1.
 */
int v_unpack_packet_header(const char *buffer,
		const unsigned short buffer_len,
		struct VPacket *vpacket)
{
	unsigned short buffer_pos=0;
	unsigned char ver;

	if(buffer_len < VERSE_PACKET_HEADER_SIZE) return -1;

	/* Length of packet */
	vpacket->len = buffer_len;
	/* Verse version */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &ver);
	vpacket->header.version = (ver >> 4) & 0x0F;
	/* Flags */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &vpacket->header.flags);
	/* Flow control window */
	buffer_pos += vnp_raw_unpack_uint16(&buffer[buffer_pos], &vpacket->header.window);
	/* Payload ID */
	buffer_pos += vnp_raw_unpack_uint32(&buffer[buffer_pos], &vpacket->header.payload_id);
	/* ACK NAK ID */
	buffer_pos += vnp_raw_unpack_uint32(&buffer[buffer_pos], &vpacket->header.ack_nak_id);
	/* ANK ID */
	buffer_pos += vnp_raw_unpack_uint32(&buffer[buffer_pos], &vpacket->header.ank_id);

	return buffer_pos;
}

/**
 * \brief Get header of verse message from received buffer
 * \param[in]	*buffer		The received buffer
 * \param[in]	buffer_len	The size of received buffer
 * \param[out]	*vmessage	The structure of message, that will be filled with
 * information from the buffer.
 * \return This function returns relative buffer position of buffer proceeding.
 * When corrupted header detected, then it returns -1.
 */
int v_unpack_message_header(const char *buffer,
		const unsigned short buffer_len,
		struct VMessage *vmessage)
{
	unsigned short buffer_pos=0;
	unsigned char tmp;

	if(buffer_len < VERSE_MESSAGE_HEADER_SIZE) return -1;

	/* Unpack version of protocol */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &tmp);
	vmessage->header.version = (tmp >> 4) & 0x0F;
	/* Unpack zero byte for reservation */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &tmp);
	/* Unpack message length of message  */
	buffer_pos += vnp_raw_unpack_uint16(&buffer[buffer_pos], &vmessage->header.len);

	return buffer_pos;
}

/**
 * \brief Pack header of VPacket to the buffer
 * \return This function return of bytes packed to the buffer.
 */
int v_pack_packet_header(const VPacket *vpacket, char *buffer)
{
	unsigned short buffer_pos=0;
	unsigned char ver;

	/* Pack version */
	ver = (vpacket->header.version << 4) & 0xF0;
	buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], ver);
	/* Flags */
	buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], vpacket->header.flags);
	/* Flow control window */
	buffer_pos += vnp_raw_pack_uint16(&buffer[buffer_pos], vpacket->header.window);
	/* Payload ID */
	buffer_pos += vnp_raw_pack_uint32(&buffer[buffer_pos], vpacket->header.payload_id);
	/* ACK NAK ID */
	buffer_pos += vnp_raw_pack_uint32(&buffer[buffer_pos], vpacket->header.ack_nak_id);
	/* ANK ID */
	buffer_pos += vnp_raw_pack_uint32(&buffer[buffer_pos], vpacket->header.ank_id);

	return buffer_pos;
}

/**
 * \brief Pack header of VMessage to the buffer
 * \param[in]	vmessage	The structure with data for message
 * \param[out]	buffer		The buffer, that will be sent to the peer.
 * \return This function return of bytes packed to the buffer.
 */
int v_pack_message_header(const VMessage *vmessage, char *buffer)
{
	unsigned short buffer_pos=0;
	unsigned char ver;

	/* Pack version */
	ver = (vmessage->header.version << 4) & 0xF0;
	buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], ver);
	/* Pack zeros for reservation space */
	buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], 0);
	/* Pack length of command */
	buffer_pos += vnp_raw_pack_uint16(&buffer[buffer_pos], vmessage->header.len);

	return buffer_pos;
}

/**
 * \brief Print verse message header to the log file
 */
void v_print_message_header(const unsigned char level, const VMessage *vmessage)
{
	v_print_log_simple(level, "Ver: %d, Len: %d", vmessage->header.version, vmessage->header.len);
}

/**
 * \brief Print verse packet header to the log file
 */
void v_print_packet_header(const unsigned char level, const VPacket *vpacket)
{
	int i;

	v_print_log_simple(level, "Ver: %d, Flags: ", vpacket->header.version);
	for(i=7; i>=0; i--) {
		if((1<<i) & vpacket->header.flags) {
			switch (1<<i) {
				case PAY_FLAG:
					v_print_log_simple(level, "PAY,");
					break;
				case ACK_FLAG:
					v_print_log_simple(level, "ACK,");
					break;
				case ANK_FLAG:
					v_print_log_simple(level, "ANK,");
					break;
				case SYN_FLAG:
					v_print_log_simple(level, "SYN,");
					break;
				case FIN_FLAG:
					v_print_log_simple(level, "FIN,");
					break;
			}
		}
	}
	v_print_log_simple(level, " Window:%d, PayID:%d, AckNakID:%d, AnkID:%d\n",
			vpacket->header.window, vpacket->header.payload_id,
			vpacket->header.ack_nak_id, vpacket->header.ank_id);
}

void v_print_send_message(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VMessage *s_message = CTX_s_message(C);

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 32);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Send message: ");
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "Socket: %d, ", io_ctx->sockfd);
		v_print_addr_port(VRS_PRINT_DEBUG_MSG, &io_ctx->peer_addr);
		v_print_message_header(VRS_PRINT_DEBUG_MSG, s_message);
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "\n");
		v_print_message_sys_cmds(VRS_PRINT_DEBUG_MSG, s_message);
		printf("%c[%dm", 27, 0);
	}
}

void v_print_receive_message(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VMessage *r_message = CTX_r_message(C);

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 34);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Receive message: ");
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "Socket: %d, ", io_ctx->sockfd);
		v_print_addr_port(VRS_PRINT_DEBUG_MSG, &io_ctx->peer_addr);
		v_print_message_header(VRS_PRINT_DEBUG_MSG, r_message);
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "\n");
		v_print_message_sys_cmds(VRS_PRINT_DEBUG_MSG, r_message);
		printf("%c[%dm", 27, 0);
	}
}

void v_print_send_packet(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *s_packet = CTX_s_packet(C);

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 32);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Send packet: ");
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "Socket: %d, ", io_ctx->sockfd);
		v_print_addr_port(VRS_PRINT_DEBUG_MSG, &io_ctx->peer_addr);
		v_print_packet_header(VRS_PRINT_DEBUG_MSG, s_packet);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Shift: %d -> Window: %d\n", dgram_conn->rwin_host_scale, (unsigned int)s_packet->header.window << dgram_conn->rwin_host_scale);
		v_print_packet_sys_cmds(VRS_PRINT_DEBUG_MSG, s_packet);
		printf("%c[%dm", 27, 0);
	}
}

void v_print_receive_packet(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 34);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Receive packet: ");
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "Socket: %d, ", io_ctx->sockfd);
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "bufsize: %d, ", io_ctx->buf_size);
		v_print_addr_port(VRS_PRINT_DEBUG_MSG, &io_ctx->peer_addr);
		v_print_packet_header(VRS_PRINT_DEBUG_MSG, r_packet);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Shift: %d -> Window: %d\n", dgram_conn->rwin_host_scale, (unsigned int)r_packet->header.window << dgram_conn->rwin_host_scale);
		v_print_packet_sys_cmds(VRS_PRINT_DEBUG_MSG, r_packet);
		printf("%c[%dm", 27, 0);
	}
}

static int v_compare_ipv4_addr(const struct in_addr *addr1, const struct in_addr *addr2)
{
	if(addr1->s_addr == addr2->s_addr)
		return 1;
	else
		return 0;
}

static int v_compare_ipv6_addr(const struct in6_addr *addr1, const struct in6_addr *addr2)
{
	if((addr1->s6_addr[0]==addr2->s6_addr[0]) &&
			(addr1->s6_addr[1]==addr2->s6_addr[1]) &&
			(addr1->s6_addr[2]==addr2->s6_addr[2]) &&
			(addr1->s6_addr[3]==addr2->s6_addr[3]))
		return 1;
	else
		return 0;
}

/* Compare two addresses */
int v_compare_addr(const struct VNetworkAddress *addr1, const struct VNetworkAddress *addr2)
{
	if(addr1->ip_ver==IPV4 && addr2->ip_ver==IPV4) {
		return v_compare_ipv4_addr(&addr1->addr.ipv4.sin_addr, &addr2->addr.ipv4.sin_addr);
	}
	else if(addr1->ip_ver==IPV6 && addr2->ip_ver==IPV6) {
		return v_compare_ipv6_addr(&addr1->addr.ipv6.sin6_addr, &addr2->addr.ipv6.sin6_addr);
	}
	else {
		return -1;
	}
}

/* Compare two addresses and ports */
int v_compare_addr_and_port(const struct VNetworkAddress *addr1, const struct VNetworkAddress *addr2)
{
	if(addr1->ip_ver==IPV4 && addr2->ip_ver==IPV4) {
		if(v_compare_ipv4_addr(&addr1->addr.ipv4.sin_addr, &addr2->addr.ipv4.sin_addr) &&
		  addr1->addr.ipv4.sin_port == addr2->addr.ipv4.sin_port)
			return 1;
		else
			return 0;
	}
	else if(addr1->ip_ver==IPV6 && addr2->ip_ver==IPV6) {
		if(v_compare_ipv6_addr(&addr1->addr.ipv6.sin6_addr, &addr2->addr.ipv6.sin6_addr) &&
		  addr1->addr.ipv6.sin6_port == addr2->addr.ipv6.sin6_port)
			return 1;
		else
			return 0;
	}
	else {
		return -1;
	}
}

/* Print network address and port */
void v_print_addr_port(const unsigned char level, const struct VNetworkAddress *addr)
{
	unsigned short port;

	if(addr->ip_ver==IPV4) {
		char str_addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(addr->addr.ipv4.sin_addr), str_addr, sizeof(str_addr));
		port = ntohs(addr->addr.ipv4.sin_port);
		v_print_log_simple(level, "%s:%d ", str_addr, port);
	}
	else if(addr->ip_ver==IPV6) {
		char str_addr[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &(addr->addr.ipv6.sin6_addr), str_addr, sizeof(str_addr));
		port = ntohs(addr->addr.ipv6.sin6_port);
		v_print_log_simple(level, "[%s]:%d ", str_addr, port);
	}
}

/* Print network address */
void v_print_addr(const unsigned char level, const struct VNetworkAddress *addr)
{
	if(addr->ip_ver==IPV4) {
		char str_addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(addr->addr.ipv4.sin_addr), str_addr, sizeof(str_addr));
		v_print_log_simple(level, "%s ", str_addr);
	}
	else if(addr->ip_ver==IPV6) {
		char str_addr[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &(addr->addr.ipv6.sin6_addr), str_addr, sizeof(str_addr));
		v_print_log_simple(level, "%s ", str_addr);
	}
}
#ifdef WITH_KERBEROS
int v_krb5_set_addrs_ipv4(krb5_context ctx, krb5_auth_context auth_ctx,
		struct sockaddr_in *local, struct sockaddr_in *remote, krb5_error_code *err)
{
	krb5_address *krb5_local, *krb5_remote;

	if (local == NULL){
		krb5_local = NULL;
	} else {
		krb5_local = malloc(sizeof (krb5_address));
		krb5_local->addrtype = ADDRTYPE_INET;
		krb5_local->length = sizeof(local->sin_addr);
		krb5_local->contents = (krb5_octet *)&local->sin_addr;
	}
	if (remote == NULL) {
		krb5_remote = NULL;
	} else {
		krb5_remote = malloc(sizeof (krb5_address));
		krb5_remote->addrtype = ADDRTYPE_INET;
		krb5_remote->length = sizeof(remote->sin_addr);
		krb5_remote->contents = (krb5_octet *) &remote->sin_addr;
	}
	if ((*err = krb5_auth_con_setaddrs(ctx, auth_ctx, krb5_local, krb5_remote))) {
		v_print_log(VRS_PRINT_ERROR, "krb5_auth_con_setaddrs %d: %s\n",
				(int) *err, krb5_get_error_message(ctx, *err));
		return 0;
	}
	if (local != NULL){
		krb5_local->addrtype = ADDRTYPE_IPPORT;
		krb5_local->length = sizeof(local->sin_port);
		krb5_local->contents = (krb5_octet *) &local->sin_port;
	}
	if (remote != NULL){
		krb5_remote->addrtype = ADDRTYPE_IPPORT;
		krb5_remote->length = sizeof(remote->sin_port);
		krb5_remote->contents = (krb5_octet *) &remote->sin_port;
	}
	if ((*err = krb5_auth_con_setports(ctx, auth_ctx, krb5_local, krb5_remote))) {
		v_print_log(VRS_PRINT_ERROR, "krb5_auth_con_setports %d: %s\n",
				(int) *err, krb5_get_error_message(ctx, *err));
		return 0;
	}
	free(krb5_local);
	free(krb5_remote);
	return 1;
}

int v_krb5_set_addrs_ipv6(krb5_context ctx, krb5_auth_context auth_ctx,
		struct sockaddr_in6 *local, struct sockaddr_in6 *remote,
		krb5_error_code *err)
{
	krb5_address *krb5_local, *krb5_remote;

	if (local == NULL) {
		krb5_local = NULL;
	} else {
		krb5_local = malloc(sizeof(krb5_address));
		krb5_local->addrtype = ADDRTYPE_INET;
		krb5_local->length = sizeof(local->sin6_addr);
		krb5_local->contents = (krb5_octet *) &local->sin6_addr;
	}
	if (remote == NULL) {
		krb5_remote = NULL;
	} else {
		krb5_remote = malloc(sizeof(krb5_address));
		krb5_remote->addrtype = ADDRTYPE_INET;
		krb5_remote->length = sizeof(remote->sin6_addr);
		krb5_remote->contents = (krb5_octet *) &remote->sin6_addr;
	}
	if ((*err = krb5_auth_con_setaddrs(ctx, auth_ctx, krb5_local, krb5_remote))) {
		v_print_log(VRS_PRINT_ERROR, "krb5_auth_con_setaddrs %d: %s\n",
				(int) *err, krb5_get_error_message(ctx, *err));
		return 0;
	}
	if (local != NULL) {
		krb5_local->addrtype = ADDRTYPE_IPPORT;
		krb5_local->length = sizeof(local->sin6_port);
		krb5_local->contents = (krb5_octet *) &local->sin6_port;
	}
	if (remote != NULL) {
		krb5_remote->addrtype = ADDRTYPE_IPPORT;
		krb5_remote->length = sizeof(remote->sin6_port);
		krb5_remote->contents = (krb5_octet *) &remote->sin6_port;
	}
	if ((*err = krb5_auth_con_setports(ctx, auth_ctx, krb5_local, krb5_remote))) {
		v_print_log(VRS_PRINT_ERROR, "krb5_auth_con_setports %d: %s\n",
				(int) *err, krb5_get_error_message(ctx, *err));
		return 0;
	}
	free(krb5_local);
	free(krb5_remote);

	return 1;
}

/* Read data form kerberos connection to the IO_CTX. Return number of bytes read
 * from the kerberos connection. When some error occurs, then error code is stored
 * in error_num */
int v_krb5_read(struct IO_CTX *io_ctx, krb5_context krb5_ctx,
		krb5_error_code *error_num)
{
	int ret;
	unsigned char pkt_buf[MAX_PACKET_SIZE];
	krb5_data packet, message;

	if(io_ctx->host_addr.ip_ver == IPV4) {
		struct sockaddr_in addr;
		socklen_t len;
		/*char str[INET_ADDRSTRLEN];*/


		ret = recvfrom(io_ctx->sockfd, pkt_buf, MAX_PACKET_SIZE, 0,
				(struct sockaddr *) &addr, &len);
		addr.sin_addr.s_addr = 0;
		addr.sin_port = 0;
		/*inet_ntop(AF_INET, (struct sin_addr *)&(addr.sin_addr), str, INET_ADDRSTRLEN);
		printf("Addr %s port %d\n", str, addr.sin_port);*/
		if (ret < 0) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "Error receiving data\n");
			return io_ctx->buf_size = 0;
		}
		if (v_krb5_set_addrs_ipv4(krb5_ctx, io_ctx->krb5_auth_ctx, NULL, &addr,
				error_num) != 1) {
			return io_ctx->buf_size = 0;
		}
	} else if (io_ctx->host_addr.ip_ver == IPV6) {
		struct sockaddr_in6 addr;
		socklen_t len = INET6_ADDRSTRLEN;
		/*char str[INET6_ADDRSTRLEN];*/

		ret = recvfrom(io_ctx->sockfd, pkt_buf, MAX_PACKET_SIZE, 0,
				(struct sockaddr *) &addr, &len);
		addr.sin6_addr = in6addr_any;
		addr.sin6_port = 0;
		/*inet_ntop(AF_INET6, (struct sin_addr *)&(addr.sin6_addr), str, INET6_ADDRSTRLEN);
		printf("Addr %s port %d\n", str, addr.sin6_port);*/
		if (ret < 0) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "Error receiving data\n");
			return io_ctx->buf_size = 0;
		}
		if (v_krb5_set_addrs_ipv6(krb5_ctx, io_ctx->krb5_auth_ctx, NULL, &addr,
				error_num) != 1) {
			return io_ctx->buf_size = 0;
		}
	} else {
		return io_ctx->buf_size = 0;
	}


	packet.data = (krb5_pointer) pkt_buf;
	io_ctx->buf_size = packet.length = ret;
	*error_num = krb5_rd_priv(krb5_ctx, io_ctx->krb5_auth_ctx, &packet, &message, NULL);
	if (*error_num) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "krb5_rd_priv %d: %s\n", *error_num,
				krb5_get_error_message(krb5_ctx, *error_num));
		return io_ctx->buf_size = 0;
	}
	io_ctx->buf = message.data;

	return io_ctx->buf_size;
}

/* Write data form IO_CTX into kerberos connection. Return number of bytes written
 * to the kerberos connection. When some error occurs, then error code is stored
 * in error_num */
int v_krb5_write(struct IO_CTX *io_ctx, krb5_context krb5_ctx,
		krb5_error_code *error_num)
{
	krb5_data inbuf, packet;
	int ret;

	if (io_ctx->peer_addr.ip_ver == IPV4) {
		/*socklen_t len = 0;*/
		struct sockaddr_in addr;
		/*char str[INET_ADDRSTRLEN];*/

		/*if ((getsockname(io_ctx->sockfd, (struct sockaddr *) &addr,
				&len)) < 0) {
			v_print_log(VRS_PRINT_ERROR, "getsockname\n");
			return 0;
		}*/
		addr.sin_addr.s_addr = 0;
		addr.sin_port = 0;
		/*inet_ntop(AF_INET, (struct sin_addr *) &(addr.sin_addr), str,
				INET_ADDRSTRLEN);
		printf("Addr %s port %d\n", str, addr.sin_port);*/
		if (v_krb5_set_addrs_ipv4(krb5_ctx, io_ctx->krb5_auth_ctx, &addr, NULL,
				error_num) != 1) {
			return io_ctx->buf_size = 0;
		}

	} else if (io_ctx->peer_addr.ip_ver == IPV6) {
		/*socklen_t len = 0;*/
		struct sockaddr_in6 addr;
		/*char str[INET6_ADDRSTRLEN];*/

		addr.sin6_addr = in6addr_any;
		addr.sin6_port = 0;

		/*if ((getsockname(io_ctx->sockfd, (struct sockaddr *) &addr, &len))
				< 0) {
			v_print_log(VRS_PRINT_ERROR, "getsockname\n");
			return 0;
		}*/
		/*inet_ntop(AF_INET6, (struct sin_addr *) &(addr.sin6_addr), str,
		INET6_ADDRSTRLEN);
		printf("Addr %s port %d\n", str, addr.sin6_port);*/
		if (v_krb5_set_addrs_ipv6(krb5_ctx, io_ctx->krb5_auth_ctx, &addr, NULL,
				error_num) != 1) {
			return io_ctx->buf_size = 0;
		}
	} else {
		return 0;
	}

	inbuf.length = io_ctx->buf_size;
	inbuf.data = (krb5_pointer) io_ctx->buf;

	*error_num = krb5_mk_priv(krb5_ctx, io_ctx->krb5_auth_ctx, &inbuf, &packet, NULL);
	if(*error_num){
		v_print_log(VRS_PRINT_DEBUG_MSG, "krb5_mk_priv %d: %s\n", *error_num,
				krb5_get_error_message(krb5_ctx, *error_num));
		return /*io_ctx->buf_size =*/ 0;
	}
	ret = send(io_ctx->sockfd, (char *)packet.data, (unsigned) packet.length, 0);
	if (ret < 0) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Error sending data\n");
		return /*io_ctx->buf_size =*/ 0;
	}
	krb5_free_data_contents(krb5_ctx, &packet);

	return ret;
}
#endif
/* Read data form SSL connection to the IO_CTX. Return number of bytes read
 * from the SSL connection. When some error occurs, then error code is stored
 * in error_num */
int v_SSL_read(struct IO_CTX *io_ctx, int *error_num)
{
	io_ctx->buf_size = SSL_read(io_ctx->ssl, io_ctx->buf, MAX_PACKET_SIZE);

	/**
	 * TODO: setting real address
	 */
	if( io_ctx->buf_size > 0 ) {
		*error_num = 0;
		return io_ctx->buf_size;
	} else if(io_ctx->buf_size == 0) {
		/* Get error code from SSL */
		int error = SSL_get_error(io_ctx->ssl, io_ctx->buf_size);
		*error_num = error;
		/* When error is SSL_ERROR_ZERO_RETURN, then SSL connection was closed by peer */
		if(error==SSL_ERROR_ZERO_RETURN) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "SSL connection was closed\n");
			return io_ctx->buf_size=0;
		}
		/* Print debug */
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s:%d SSL_read() buf_size: %d, error: %d\n",
				__FUNCTION__, __LINE__, (int)io_ctx->buf_size, error);
		return io_ctx->buf_size=0;
	} else if(io_ctx->buf_size < 0) {
		/* Get error code from SSL */
		int error = SSL_get_error(io_ctx->ssl, io_ctx->buf_size);
		/* Print debug */
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s:%d SSL_read() buf_size: %d, error: %d\n",
				__FUNCTION__, __LINE__, (int)io_ctx->buf_size, error);
		*error_num = error;
		return io_ctx->buf_size=0;
	}

	return 0;
}

/* Write data form IO_CTX into SSL connection. Return number of bytes written
 * to the SSL connection. When some error occurs, then error code is stored
 * in error_num */
int v_SSL_write(struct IO_CTX *io_ctx, int *error_num)
{
	int ret;

	/**
	 * TODO: Setting real address
	 */

	ret = SSL_write(io_ctx->ssl, io_ctx->buf, io_ctx->buf_size);
	if(ret > 0) {
		if(ret != io_ctx->buf_size) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "%s:%d SSL_write() buf_size: %d != ret: %d\n",
					__FUNCTION__, __LINE__, io_ctx->buf_size, ret);
		}
		*error_num = 0;
		return ret;
	} else if(ret == 0) {
		int error = SSL_get_error(io_ctx->ssl, io_ctx->buf_size);
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s:%d SSL_write() buf_size: %d, error: %d\n",
				__FUNCTION__, __LINE__, (int)io_ctx->buf_size, error);
		*error_num = error;
		if(error==SSL_ERROR_ZERO_RETURN) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "SSL connection was closed.\n");
			return 0;
		}
	} else if(ret < 0) {
		int error = SSL_get_error(io_ctx->ssl, io_ctx->buf_size);
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s:%d SSL_read() buf_size: %d, error: %d\n",
				__FUNCTION__, __LINE__, (int)io_ctx->buf_size, error);
		*error_num = error;
		return 0;
	}

	return 0;
}

/* Receive Verse packet through unsecured UDP socket. */
int v_receive_packet(struct IO_CTX *io_ctx, int *error_num)
{
	unsigned int addr_len;

	*error_num = 0;

	if(io_ctx->flags & SOCKET_CONNECTED) {
		if(io_ctx->flags & SOCKET_SECURED) {
again:
			io_ctx->buf_size = SSL_read(io_ctx->ssl, io_ctx->buf, MAX_PACKET_SIZE);
			if( io_ctx->buf_size > 0 ) {
				*error_num = 0;
				return 1;
			} else if(io_ctx->buf_size <= 0) {
				/* Get error code from SSL */
				int error = SSL_get_error(io_ctx->ssl, io_ctx->buf_size);
				if(error == SSL_ERROR_NONE) {
					return 1;
				} else if(error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
					goto again;
				} else if(error==SSL_ERROR_ZERO_RETURN) {
					/* When error is SSL_ERROR_ZERO_RETURN, then DTLS sent some
					 * packet, that didn't contain application data */
					return 1;
				}
				ERR_print_errors_fp(stderr);
				*error_num = error;
				/* Print debug */
				v_print_log(VRS_PRINT_DEBUG_MSG, "%s:%d SSL_read() buf_size: %d, error: %d\n",
						__FUNCTION__, __LINE__, (int)io_ctx->buf_size, error);
				return 0;
			}

			return 0;
		} else {
			/* Try to receive packet from connected socket */
			if( (io_ctx->buf_size=recv(io_ctx->sockfd, io_ctx->buf, MAX_PACKET_SIZE, 0)) == -1) {
				*error_num = errno;
				v_print_log(VRS_PRINT_ERROR, "recv(): %s\n", strerror(*error_num));
				return -1;
			}
		}
		return 1;
	} else {
		/* Try to receive packet from unconnected socket */
		if( io_ctx->host_addr.ip_ver == IPV4 ) {
			addr_len = sizeof(io_ctx->peer_addr.addr.ipv4);
			io_ctx->peer_addr.ip_ver = IPV4;
			if( (io_ctx->buf_size=recvfrom(io_ctx->sockfd, io_ctx->buf, MAX_PACKET_SIZE, 0, (struct sockaddr*)&(io_ctx->peer_addr.addr.ipv4), &addr_len)) == -1) {
				*error_num = errno;
				v_print_log(VRS_PRINT_ERROR, "recvfrom(): %s\n", strerror(*error_num));
				return -1;
			}
			return 1;
		}
		else if( io_ctx->host_addr.ip_ver == IPV6 ) {
			addr_len = sizeof(io_ctx->peer_addr.addr.ipv6);
			io_ctx->peer_addr.ip_ver = IPV6;
			if( (io_ctx->buf_size=recvfrom(io_ctx->sockfd, io_ctx->buf, MAX_PACKET_SIZE, 0, (struct sockaddr*)&(io_ctx->peer_addr.addr.ipv6), &addr_len)) == -1) {
				*error_num = errno;
				v_print_log(VRS_PRINT_ERROR, "recvfrom(): %s\n", strerror(*error_num));
				return -1;
			}
			return 1;
		}
		else {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Unsupported address type %d\n", io_ctx->host_addr.ip_ver);
			io_ctx->buf_size = 0;
			return -1;
		}
	}
	return -1;

}

/* Send Verse packet through unsecured UDP socket. */
int v_send_packet(struct IO_CTX *io_ctx, int *error_num)
{
	if(error_num!=NULL) *error_num = 0;

	if(io_ctx->flags & SOCKET_CONNECTED) {
		int ret;
		if(io_ctx->flags & SOCKET_SECURED) {
again:
			ret = SSL_write(io_ctx->ssl, io_ctx->buf, io_ctx->buf_size);
			if(ret > 0) {
				if(ret != io_ctx->buf_size) {
					v_print_log(VRS_PRINT_DEBUG_MSG, "%s:%d SSL_write() buf_size: %d != ret: %d\n",
							__FUNCTION__, __LINE__, io_ctx->buf_size, ret);
				} else {
					v_print_log(VRS_PRINT_DEBUG_MSG, "SSL_write() %d bytes\n", ret);
				}
				if(error_num!=NULL) *error_num = 0;
				return SEND_PACKET_SUCCESS;
			} else if(ret <= 0) {
				int error = SSL_get_error(io_ctx->ssl, io_ctx->buf_size);
				if(error == SSL_ERROR_NONE) {
					return SEND_PACKET_SUCCESS;
				} else if(error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
					goto again;
				}
				ERR_print_errors_fp(stderr);
				if(error_num!=NULL) *error_num = error;
				v_print_log(VRS_PRINT_ERROR, "%s:%d SSL_write(%p, %p, %d): %d\n",
						__FUNCTION__, __LINE__,
						(void*)io_ctx->ssl, (void*)io_ctx->buf, io_ctx->buf_size,
						error);
				return SEND_PACKET_ERROR;
			}
		}
		if((ret = send(io_ctx->sockfd, io_ctx->buf, io_ctx->buf_size, 0)) == -1) {
			if(error_num!=NULL) *error_num = errno;
			v_print_log(VRS_PRINT_ERROR, "send(%d, %p, %d, 0): %s\n",
					io_ctx->sockfd, (void*)io_ctx->buf, io_ctx->buf_size, strerror(errno));
			return SEND_PACKET_ERROR;
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "send() %d bytes\n", ret);
		}
		return SEND_PACKET_SUCCESS;
	} else {
		if(io_ctx->host_addr.ip_ver==IPV4) {
			if(sendto(io_ctx->sockfd, io_ctx->buf, io_ctx->buf_size, 0, (struct sockaddr*)&(io_ctx->peer_addr.addr.ipv4), sizeof(io_ctx->peer_addr.addr.ipv4)) == -1) {
				if(error_num!=NULL) *error_num = errno;
				v_print_log(VRS_PRINT_ERROR, "sendto(): %s\n", strerror(errno));
				return SEND_PACKET_ERROR;
			}
			return SEND_PACKET_SUCCESS;
		} else if(io_ctx->host_addr.ip_ver==IPV6) {
			if(sendto(io_ctx->sockfd, io_ctx->buf, io_ctx->buf_size, 0, (struct sockaddr*)&(io_ctx->peer_addr.addr.ipv6), sizeof(io_ctx->peer_addr.addr.ipv6)) == -1) {
				if(error_num!=NULL) *error_num = errno;
				v_print_log(VRS_PRINT_ERROR, "sendto(): %s\n", strerror(errno));
				return SEND_PACKET_ERROR;
			}
			return SEND_PACKET_SUCCESS;
		}

		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Unsupported address type %d\n", io_ctx->host_addr.ip_ver);
		return SEND_PACKET_ERROR;
	}

	return SEND_PACKET_SUCCESS;
}

