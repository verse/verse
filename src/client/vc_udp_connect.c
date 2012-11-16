/*
 * $Id: vc_udp_connect.c 1267 2012-07-23 19:10:28Z jiri $
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

#include <openssl/err.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "vc_main.h"
#include "vc_udp_connect.h"
#include "vc_tcp_connect.h"

#include "v_context.h"
#include "v_connection.h"
#include "v_network.h"
#include "v_common.h"
#include "v_fake_commands.h"
#include "v_session.h"
#include "v_resend_mechanism.h"

/************************************** REQUEST state **************************************/

/**
 * \brief This function is the callback function for received Change_L
 * commands in REQUEST state.
 */
static int vc_REQUEST_CHANGE_L_cb(struct vContext *C, struct Generic_Cmd *cmd)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct Negotiate_Cmd *change_l_cmd = (struct Negotiate_Cmd*)cmd;
	int i;

	if(change_l_cmd->feature == FTR_COOKIE) {
		/* This block of code checks, if received cookie is the same as the
		 * negotiated cookie. When cookie is not the same, then the client
		 * will not response to the received packet. */
		if( vsession->peer_cookie.str != NULL &&
				change_l_cmd->count > 0 &&
				change_l_cmd->value[0].string8.str != NULL )
		{
			v_print_log(VRS_PRINT_DEBUG_MSG, "Remote COOKIE: %s proposed\n", change_l_cmd->value[0].string8.str);
			return 1;
		} else {
			v_print_log(VRS_PRINT_WARNING, "Peer proposed wrong COOKIE\n");
			return 0;
		}
	}

	/* Server proposes it's own Flow Control */
	if(change_l_cmd->feature == FTR_FC_ID) {
		for(i=0; i<change_l_cmd->count; i++) {
			/* TODO: better implementation */
			if(change_l_cmd->value[i].uint8 == FC_NONE) { /* list of supported methods */
				/* It will try to use first found supported method, but ... */
				if(dgram_conn->fc_meth == FC_RESERVED) {
					/* Flow Control has not been proposed yet */
				} else {
					/* Server has to propose same FC methods for client
					 * and server. Client can't use different method then
					 * server and vice versa. */
					if(dgram_conn->fc_meth != change_l_cmd->value[i].uint8) {
						v_print_log(VRS_PRINT_WARNING, "Proposed FC local :%d is not same as proposed FC remote: %d\n",
								change_l_cmd->value[i].uint8, dgram_conn->fc_meth);
						return 0;
					}
				}
				dgram_conn->fc_meth = change_l_cmd->value[i].uint8;
				v_print_log(VRS_PRINT_DEBUG_MSG, "Local Flow Control ID: %d proposed\n", change_l_cmd->value[0].uint8);
				return 1;
			} else {
				v_print_log(VRS_PRINT_ERROR, "Unsupported Flow Control\n");
				return 0;
			}
		}
	}

	/* Server proposes it's own scale of Flow Control Window */
	if(change_l_cmd->feature == FTR_RWIN_SCALE) {
		if(change_l_cmd->count >= 1) {
			dgram_conn->rwin_peer_scale = change_l_cmd->value[0].uint8;
			v_print_log(VRS_PRINT_DEBUG_MSG, "Scale of peer RWIN: %d proposed\n", dgram_conn->rwin_peer_scale);
			return 1;
		} else {
			v_print_log(VRS_PRINT_ERROR, "At last on value of RWIN scale has to be proposed\n");
			return 0;
		}
	}

	return 1;
}

/**
 * \brief This function is the callback function for received Change_R
 * commands in REQUEST state.
 */
static int vc_REQUEST_CHANGE_R_cb(struct vContext *C, struct Generic_Cmd *cmd)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct Negotiate_Cmd *change_r_cmd = (struct Negotiate_Cmd*)cmd;
	int i;

	if(change_r_cmd->feature == FTR_FC_ID) {
		for(i=0; i<change_r_cmd->count; i++) {
			/* TODO: better implementation */
			if(change_r_cmd->value[i].uint8 == FC_NONE) { /* list of supported methods */
				/* It will try to use first found supported method, but ... */
				if(dgram_conn->fc_meth == FC_RESERVED) {
					/* Flow Control has not been proposed yet */
				} else {
					/* Server has to propose same FC methods for client
					 * and server. Client can't use different method then
					 * server and vice versa. */
					if(dgram_conn->fc_meth != change_r_cmd->value[i].uint8) {
						v_print_log(VRS_PRINT_WARNING, "Proposed FC remote :%d is not same as proposed FC local: %d\n",
								change_r_cmd->value[i].uint8, dgram_conn->fc_meth);
						return 0;
					}
				}
				dgram_conn->fc_meth = change_r_cmd->value[i].uint8;
				v_print_log(VRS_PRINT_DEBUG_MSG, "Remote Flow Control ID: %d proposed\n", change_r_cmd->value[0].uint8);
				return 1;
			} else {
				v_print_log(VRS_PRINT_ERROR, "Unsupported Flow Control\n");
				return 0;
			}
		}
	}

	return 1;
}

/**
 * \brief This function is the callback function for received Confirm_L
 * commands in REQUEST state.
 */
static int vc_REQUEST_CONFIRM_L_cb(struct vContext *C, struct Generic_Cmd *cmd)
{
	struct VC_CTX *vc_ctx = CTX_client_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct Negotiate_Cmd *confirm_l_cmd = (struct Negotiate_Cmd*)cmd;

	if(confirm_l_cmd->feature == FTR_COOKIE) {
		/* This block of code checks if the server confirmed send cookie. */
		if( vsession->host_cookie.str != NULL &&
				confirm_l_cmd->count > 0 &&
				confirm_l_cmd->value[0].string8.str != NULL &&
				strcmp((char*)confirm_l_cmd->value[0].string8.str, vsession->host_cookie.str)==0 )
		{
			v_print_log(VRS_PRINT_DEBUG_MSG, "Local COOKIE: %s confirmed\n", confirm_l_cmd->value[0].string8.str);
			return 1;
		} else {
			v_print_log(VRS_PRINT_WARNING, "COOCKIE: %s not confirmed\n", vsession->peer_cookie.str);
			return 0;
		}
	}

	/* Server should confirm client proposal of Congestion Control (local) */
	if(confirm_l_cmd->feature == FTR_CC_ID) {
		/* TODO: better implementation */
		if(confirm_l_cmd->count == 1 &&	/* Any confirm command has to include only one value */
				confirm_l_cmd->value[0].uint8 == CC_NONE) {	/* list of supported methods */
			v_print_log(VRS_PRINT_DEBUG_MSG, "Local Congestion Control ID: %d confirmed\n", confirm_l_cmd->value[0].uint8);
			dgram_conn->cc_meth = CC_NONE;
			return 1;
		} else {
			v_print_log(VRS_PRINT_ERROR, "Unsupported Congestion Control\n");
			return 0;
		}
	}

	/* Server should confirm client proposal of Flow Control Window scale (local) */
	if(confirm_l_cmd->feature == FTR_RWIN_SCALE) {
		if(confirm_l_cmd->count >= 1 ) {
			if(vc_ctx->rwin_scale == confirm_l_cmd->value[0].uint8) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Scale of host RWIN: %d confirmed\n", vc_ctx->rwin_scale);
				dgram_conn->rwin_host_scale = vc_ctx->rwin_scale;
				return 1;
			} else {
				v_print_log(VRS_PRINT_ERROR, "Scale of RWIN: %d != %d wasn't confirmed\n",
						vc_ctx->rwin_scale,
						confirm_l_cmd->value[0].uint8);
				dgram_conn->rwin_host_scale = 0;
				return 0;
			}
		} else {
			v_print_log(VRS_PRINT_ERROR, "One value of RWIN scale should to be confirmed\n");
			return 0;
		}
	}

	return 0;
}

/**
 * \brief This function is the callback function for received Confirm_R
 * commands in REQUEST state.
 */
static int vc_REQUEST_CONFIRM_R_cb(struct vContext *C, struct Generic_Cmd *cmd)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct Negotiate_Cmd *confirm_r_cmd = (struct Negotiate_Cmd*)cmd;

	/* Server should confirm client proposal of Congestion Control (remote) */
	if(confirm_r_cmd->feature == FTR_CC_ID) {
		/* TODO: better implementation */
		if(confirm_r_cmd->count == 1 &&	/* Any confirm command has to include only one value */
				confirm_r_cmd->value[0].uint8 == CC_NONE) {	/* list of supported methods */
			v_print_log(VRS_PRINT_DEBUG_MSG, "Remote Congestion Control ID: %d confirmed\n", confirm_r_cmd->value[0].uint8);
			dgram_conn->cc_meth = CC_NONE;
			return 1;
		} else {
			v_print_log(VRS_PRINT_ERROR, "Unsupported Congestion Control\n");
			return 0;
		}
	}

	return 1;
}

void vc_REQUEST_init_send_packet(struct vContext *C)
{
	struct VC_CTX *vc_ctx = CTX_client_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VPacket *s_packet = CTX_s_packet(C);
	int i=0;

	/* Verse packet header */
	s_packet->header.version = 1;
	s_packet->header.flags = PAY_FLAG | SYN_FLAG;
	s_packet->header.window = dgram_conn->rwin_host >> dgram_conn->rwin_host_scale;
	s_packet->header.payload_id = dgram_conn->host_id;
	s_packet->header.ack_nak_id = 0;
	s_packet->header.ank_id = 0;

	/* System commands */
	if(vsession->host_cookie.str!=NULL) {
		/* Add negotiated cookie */
		s_packet->sys_cmd[i].change_l_cmd.id = CMD_CHANGE_L_ID;
		s_packet->sys_cmd[i].change_l_cmd.feature = FTR_COOKIE;
		s_packet->sys_cmd[i].change_l_cmd.count = 1;
		s_packet->sys_cmd[i].change_l_cmd.value[0].string8.length = strlen(vsession->host_cookie.str);
		strcpy((char*)s_packet->sys_cmd[0].change_l_cmd.value[0].string8.str, vsession->host_cookie.str);
		i++;
	}

	/* Add CC (local) proposal */
	s_packet->sys_cmd[i].change_l_cmd.id = CMD_CHANGE_L_ID;
	s_packet->sys_cmd[i].change_l_cmd.feature = FTR_CC_ID;
	s_packet->sys_cmd[i].change_l_cmd.count = 1;
	s_packet->sys_cmd[i].change_l_cmd.value[0].uint8 = CC_NONE;
	i++;

	/* Add CC (remote) proposal */
	s_packet->sys_cmd[i].change_r_cmd.id = CMD_CHANGE_R_ID;
	s_packet->sys_cmd[i].change_r_cmd.feature = FTR_CC_ID;
	s_packet->sys_cmd[i].change_r_cmd.count = 1;
	s_packet->sys_cmd[i].change_r_cmd.value[0].uint8 = CC_NONE;
	i++;

	/* Add Scale Window proposal */
	s_packet->sys_cmd[i].change_l_cmd.id = CMD_CHANGE_L_ID;
	s_packet->sys_cmd[i].change_l_cmd.feature = FTR_RWIN_SCALE;
	s_packet->sys_cmd[i].change_l_cmd.count = 1;
	s_packet->sys_cmd[i].change_l_cmd.value[0].uint8 = vc_ctx->rwin_scale;
	i++;

	/* Add command compression proposal */
	if(vsession->flags & VRS_NO_CMD_CMPR) {
		/* Client isn't able to send compressed commands (local proposal) */
		s_packet->sys_cmd[i].change_l_cmd.id = CMD_CHANGE_L_ID;
		s_packet->sys_cmd[i].change_l_cmd.feature = FTR_CMD_COMPRESS;
		s_packet->sys_cmd[i].change_l_cmd.count = 1;
		s_packet->sys_cmd[i].change_l_cmd.value[0].uint8 = CMPR_NONE;
		i++;
		/* Client isn't able to receive compressed commands (remote proposal) */
		s_packet->sys_cmd[i].change_l_cmd.id = CMD_CHANGE_R_ID;
		s_packet->sys_cmd[i].change_l_cmd.feature = FTR_CMD_COMPRESS;
		s_packet->sys_cmd[i].change_l_cmd.count = 1;
		s_packet->sys_cmd[i].change_l_cmd.value[0].uint8 = CMPR_NONE;
		i++;
	} else {
		/* Client wants to send compressed commands (local proposal) */
		s_packet->sys_cmd[i].change_l_cmd.id = CMD_CHANGE_L_ID;
		s_packet->sys_cmd[i].change_l_cmd.feature = FTR_CMD_COMPRESS;
		s_packet->sys_cmd[i].change_l_cmd.count = 1;
		s_packet->sys_cmd[i].change_l_cmd.value[0].uint8 = CMPR_ADDR_SHARE;
		i++;
		/* Client is able to receive compressed commands (remote proposal) */
		s_packet->sys_cmd[i].change_l_cmd.id = CMD_CHANGE_R_ID;
		s_packet->sys_cmd[i].change_l_cmd.feature = FTR_CMD_COMPRESS;
		s_packet->sys_cmd[i].change_l_cmd.count = 1;
		s_packet->sys_cmd[i].change_l_cmd.value[0].uint8 = CMPR_ADDR_SHARE;
		i++;
	}

	/* Add fake terminate command (this command will not be sent) */
	s_packet->sys_cmd[i].cmd.id = CMD_RESERVED_ID;
}

/**
 * \brief Send packet in REQUEST state
 */
int vc_REQUEST_send_packet(struct vContext *C)
{
	vc_REQUEST_init_send_packet(C);
	return vc_send_packet(C);
}

int vc_REQUEST_handle_packet(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);

	/* Received packet has to have PAY flag (this packet have to be acknowledged),
	 * ACK flag (acknowledgment of request packet sent to the server) and
	 * SYN flag (it is handshake packet) */
	if(r_packet->header.flags == (PAY_FLAG|ACK_FLAG|SYN_FLAG) &&
			/* Does this packet acknowledge right ID? (ACK command has to be first command!) */
			r_packet->sys_cmd[0].cmd.id==CMD_ACK_ID &&
			r_packet->sys_cmd[0].ack_cmd.pay_id==dgram_conn->host_id)
	{
		/* Client state */
		dgram_conn->host_state = UDP_CLIENT_STATE_PARTOPEN;

		/* Save ID of received payload packet */
		dgram_conn->peer_id = r_packet->header.payload_id;

		/* Call callback functions for system commands */
		if(v_conn_dgram_handle_sys_cmds(C, UDP_CLIENT_STATE_REQUEST)!=1) {
			v_print_log(VRS_PRINT_WARNING, "Received packet with wrong system command(s)\n");
			return RECEIVE_PACKET_CORRUPTED;
		}

		/* Compute RWIN */
		dgram_conn->rwin_peer = r_packet->header.window << dgram_conn->rwin_peer_scale;
	}
	else {
		v_print_log(VRS_PRINT_WARNING, "Packet with corrupted header was dropped.\n");
		return RECEIVE_PACKET_CORRUPTED;
	}

	return RECEIVE_PACKET_SUCCESS;
}

int vc_REQUEST_loop(struct vContext *C)
{
	struct VC_CTX *vc_ctx = CTX_client_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	int ret;

	/* REQUEST STATE */
	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client state: REQUEST\n");
		printf("%c[%dm", 27, 0);
	}

	/* State of client */
	dgram_conn->host_state = UDP_CLIENT_STATE_REQUEST;

	/* Initialize callback functions for this state */
	dgram_conn->state[UDP_CLIENT_STATE_REQUEST].CHANGE_L_cb = vc_REQUEST_CHANGE_L_cb;
	dgram_conn->state[UDP_CLIENT_STATE_REQUEST].CHANGE_R_cb = vc_REQUEST_CHANGE_R_cb;
	dgram_conn->state[UDP_CLIENT_STATE_REQUEST].CONFIRM_L_cb = vc_REQUEST_CONFIRM_L_cb;
	dgram_conn->state[UDP_CLIENT_STATE_REQUEST].CONFIRM_R_cb = vc_REQUEST_CONFIRM_R_cb;

	dgram_conn->state[UDP_CLIENT_STATE_REQUEST].attempts = 0;

	while(dgram_conn->state[UDP_CLIENT_STATE_REQUEST].attempts < vc_ctx->max_connection_attempts) {
		/* Try to send a packet with connect request */
		if( vc_REQUEST_send_packet(C) == SEND_PACKET_ERROR) {
			return STATE_EXIT_ERROR;
		}
		/* Try to receive packet and handle received packet (update client state) */
		ret = vc_receive_and_handle_packet(C, vc_REQUEST_handle_packet);
		if(ret == RECEIVE_PACKET_SUCCESS) {
			break;			/* Break loop and receive to the next state */
		} else if(ret == RECEIVE_PACKET_TIMEOUT) {
			dgram_conn->state[UDP_CLIENT_STATE_REQUEST].attempts++;		/* No packet receive ... try it again */
		} else if(ret == RECEIVE_PACKET_CORRUPTED) {
			dgram_conn->state[UDP_CLIENT_STATE_REQUEST].attempts++;		/* Corrupted packet received ... try it again */
		} else if(ret == RECEIVE_PACKET_FAKED) {
			continue;		/* Packet wasn't received from the server (should not happen, because connect()) */
		} else if(ret == RECEIVE_PACKET_ERROR) {
			return STATE_EXIT_ERROR;
		}

		if(dgram_conn->io_ctx.flags & SOCKET_SECURED) {
			/* Did server close DTLS connection? */
			if((SSL_get_shutdown(dgram_conn->io_ctx.ssl) & SSL_RECEIVED_SHUTDOWN)) {
				return STATE_EXIT_ERROR;
			}
		}
	}

	/* Maximal count of connection attempts reached and the client is
	 * still in REQUEST state -> end handshake. */
	if(dgram_conn->host_state == UDP_CLIENT_STATE_REQUEST) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR,
				"Maximum count of connection attempts reached %d.\n", vc_ctx->max_connection_attempts);
		return STATE_EXIT_ERROR;
	}

	return STATE_EXIT_SUCCESS;
}

/************************************** PARTOPEN state **************************************/

void vc_PARTOPEN_init_send_packet(struct vContext *C)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *s_packet = CTX_s_packet(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	int i=0;

	/* Verse packet header */
	s_packet->header.version = 1;
	s_packet->header.flags = PAY_FLAG | ACK_FLAG ;
	s_packet->header.window = dgram_conn->rwin_host >> dgram_conn->rwin_host_scale;
	s_packet->header.payload_id = dgram_conn->host_id + 1;
	s_packet->header.ack_nak_id = 0;
	if(r_packet->sys_cmd[0].cmd.id == CMD_ACK_ID) {
		s_packet->header.flags |= ANK_FLAG;
		s_packet->header.ank_id = r_packet->sys_cmd[0].ack_cmd.pay_id;
		dgram_conn->ank_id = r_packet->sys_cmd[0].ack_cmd.pay_id;
	} else {
		s_packet->header.ank_id = 0;
	}

	/* System commands */
	s_packet->sys_cmd[i].ack_cmd.id = CMD_ACK_ID;
	s_packet->sys_cmd[i].ack_cmd.pay_id = dgram_conn->peer_id;
	i++;

	/* Confirm proposed COOKIE */
	if(vsession->peer_cookie.str!=NULL) {
		s_packet->sys_cmd[i].confirm_l_cmd.id = CMD_CONFIRM_L_ID;
		s_packet->sys_cmd[i].confirm_l_cmd.feature = FTR_COOKIE;
		s_packet->sys_cmd[i].confirm_l_cmd.count = 1;
		s_packet->sys_cmd[i].confirm_l_cmd.value[0].string8.length = strlen(vsession->peer_cookie.str);
		strcpy((char*)s_packet->sys_cmd[1].confirm_l_cmd.value[0].string8.str, vsession->peer_cookie.str);
		i++;
	}

	/* Confirm proposed Flow Control ID */
	if(dgram_conn->fc_meth != FC_RESERVED) {
		s_packet->sys_cmd[i].confirm_l_cmd.id = CMD_CONFIRM_L_ID;
		s_packet->sys_cmd[i].confirm_l_cmd.feature = FTR_FC_ID;
		s_packet->sys_cmd[i].confirm_l_cmd.count = 1;
		s_packet->sys_cmd[i].confirm_l_cmd.value[0].uint8 = dgram_conn->fc_meth;
		i++;

		s_packet->sys_cmd[i].confirm_l_cmd.id = CMD_CONFIRM_R_ID;
		s_packet->sys_cmd[i].confirm_l_cmd.feature = FTR_FC_ID;
		s_packet->sys_cmd[i].confirm_l_cmd.count = 1;
		s_packet->sys_cmd[i].confirm_l_cmd.value[0].uint8 = dgram_conn->fc_meth;
		i++;
	}

	/* Confirm proposed shifting of Flow Control Window */
	if(dgram_conn->rwin_peer_scale != 0) {
		s_packet->sys_cmd[i].confirm_l_cmd.id = CMD_CONFIRM_L_ID;
		s_packet->sys_cmd[i].confirm_l_cmd.feature = FTR_RWIN_SCALE;
		s_packet->sys_cmd[i].confirm_l_cmd.count = 1;
		s_packet->sys_cmd[i].confirm_l_cmd.value[0].uint8 = dgram_conn->rwin_peer_scale;
		i++;
	}

	s_packet->sys_cmd[i].cmd.id = CMD_RESERVED_ID;
}

int vc_PARTOPEN_send_packet(struct vContext *C)
{
	vc_PARTOPEN_init_send_packet(C);
	return vc_send_packet(C);
}

int vc_PARTOPEN_handle_packet(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);

	/* Verse packet header has to include some flags and verse packet has to contain ack command */
	if(r_packet->header.flags == (PAY_FLAG|ACK_FLAG|ANK_FLAG) &&
			/* Payload ID has to be incremented */
			r_packet->header.payload_id == dgram_conn->peer_id+1 &&
			/* Does this packet contain ANK ID of last acknowledged Payload ID? */
			r_packet->header.ank_id == dgram_conn->peer_id &&
			/* Does this packet acknowledge right ID? (ACK command has to be first command!) */
			r_packet->sys_cmd[0].cmd.id == CMD_ACK_ID &&
			r_packet->sys_cmd[0].ack_cmd.pay_id == dgram_conn->host_id+1 )
	{
		/* Client state */
		dgram_conn->host_state = UDP_CLIENT_STATE_OPEN;

		/* Call callback functions for system commands */
		if(v_conn_dgram_handle_sys_cmds(C, UDP_CLIENT_STATE_PARTOPEN) != 1) {
			v_print_log(VRS_PRINT_WARNING, "Received packet with wrong system command(s)\n");
			return RECEIVE_PACKET_CORRUPTED;
		}

		/* Compute RWIN */
		dgram_conn->rwin_peer = r_packet->header.window << dgram_conn->rwin_peer_scale;
	} else {
		if(is_log_level(VRS_PRINT_WARNING)) v_print_log(VRS_PRINT_WARNING, "Packet with bad header was dropped.\n");
		return RECEIVE_PACKET_CORRUPTED;
	}

	return RECEIVE_PACKET_SUCCESS;
}

int vc_PARTOPEN_loop(struct vContext *C)
{
	struct VC_CTX *vc_ctx = CTX_client_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	int ret;

	/* PARTOPEN STATE */
	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client state: PARTOPEN.\n");
		printf("%c[%dm", 27, 0);
	}

	dgram_conn->state[UDP_CLIENT_STATE_PARTOPEN].attempts = 0;

	while(dgram_conn->state[UDP_CLIENT_STATE_PARTOPEN].attempts < vc_ctx->max_connection_attempts) {
		/* Try to send a confirmation packet */
		if( vc_PARTOPEN_send_packet(C) == SEND_PACKET_ERROR ) {
			return STATE_EXIT_ERROR;
		}
		/* Try to receive packet and handle received packet (update client state) */
		ret = vc_receive_and_handle_packet(C, vc_PARTOPEN_handle_packet);
		if(ret == RECEIVE_PACKET_SUCCESS) {
			break;			/* Break loop and receive to the next state */
		} else if(ret == RECEIVE_PACKET_TIMEOUT) {
			dgram_conn->state[UDP_CLIENT_STATE_PARTOPEN].attempts++;		/* No packet receive ... try it again */
		} else if(ret == RECEIVE_PACKET_CORRUPTED) {
			dgram_conn->state[UDP_CLIENT_STATE_PARTOPEN].attempts++;		/* Corrupted packet received ... try it again */
		} else if(ret == RECEIVE_PACKET_FAKED) {
			continue;	/* Packet wasn't received from the server (should not happen, because connect()) */
		} else if(ret == RECEIVE_PACKET_ERROR) {
			return STATE_EXIT_ERROR;
		}

		if(dgram_conn->io_ctx.flags & SOCKET_SECURED) {
			/* Did server close DTLS connection? */
			if((SSL_get_shutdown(dgram_conn->io_ctx.ssl) & SSL_RECEIVED_SHUTDOWN)) {
				return STATE_EXIT_ERROR;
			}
		}
	}

	if(dgram_conn->host_state == UDP_CLIENT_STATE_PARTOPEN) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR,
				"Maximum count of connection attempts reached %d.\n", vc_ctx->max_connection_attempts);
		return STATE_EXIT_ERROR;
	}

	return STATE_EXIT_SUCCESS;
}

/************************************** OPEN state **************************************/

int vc_OPEN_send_packet(struct vContext *C)
{
	int ret;

	/* Send as much packets as needed or possible */
	do {
		ret = send_packet_in_OPEN_CLOSEREQ_state(C);
	} while (!(ret == SEND_PACKET_CANCELED || ret == SEND_PACKET_SUCCESS));
	return ret;
}

int vc_OPEN_handle_packet(struct vContext *C)
{
	/* Use generic function for handling packet, because OPEN state is the same for the client and server */
	return handle_packet_in_OPEN_state(C);
}

int vc_OPEN_loop(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	struct Ack_Nak_Cmd ack_cmd;
	int ret;

	/* OPEN state */
	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client state: OPEN\n");
		printf("%c[%dm", 27, 0);
	}

	/* Add ACK command to the list of ACK NAK commands to be send to the peer */
	ack_cmd.id = CMD_ACK_ID;
	ack_cmd.pay_id = r_packet->header.payload_id;
	v_ack_nak_history_add_cmd(&dgram_conn->ack_nak, &ack_cmd);

	/* Receiving of last packet in previous packet had to be successful, because
	 * client is in this state now :-). */
	ret = RECEIVE_PACKET_SUCCESS;

	while(dgram_conn->host_state == UDP_CLIENT_STATE_OPEN) {

		dgram_conn->state[UDP_CLIENT_STATE_OPEN].attempts++;

		/* Send payload data or keep-alive packet */
		if( vc_OPEN_send_packet(C) == SEND_PACKET_ERROR ) {
			return STATE_EXIT_ERROR;
		}

		/* Try to receive packet and handle received packet */
		ret = vc_receive_and_handle_packet(C, vc_OPEN_handle_packet);
		if(ret == RECEIVE_PACKET_SUCCESS) {
			continue;
		} else if(ret == RECEIVE_PACKET_TIMEOUT) {
			struct timeval tv;
			gettimeofday(&tv, NULL);
			/* When no valid packet received from server for defined time, then consider this
			 * connection as dead and return error */
			if((tv.tv_sec - dgram_conn->tv_pay_recv.tv_sec) >= VRS_TIMEOUT) {
				return STATE_EXIT_ERROR;
			} else {
				continue;
			}
		} else if(ret == RECEIVE_PACKET_CORRUPTED) {
			continue;
		} else if(ret == RECEIVE_PACKET_FAKED) {
			return STATE_EXIT_ERROR;
		} else if(ret == RECEIVE_PACKET_ERROR) {
			return STATE_EXIT_ERROR;
		}

		if(dgram_conn->io_ctx.flags & SOCKET_SECURED) {
			/* Did server close DTLS connection? */
			if((SSL_get_shutdown(dgram_conn->io_ctx.ssl) & SSL_RECEIVED_SHUTDOWN)) {
				return STATE_EXIT_ERROR;
			}
		}
	}

	return STATE_EXIT_SUCCESS;
}

/************************************** CLOSING state **************************************/

void vc_CLOSING_init_send_packet(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *s_packet = CTX_s_packet(C);

	/* Verse packet header */
	s_packet->header.version = 1;
	s_packet->header.flags = PAY_FLAG | FIN_FLAG;
	s_packet->header.window = dgram_conn->rwin_host >> dgram_conn->rwin_host_scale;
	s_packet->header.payload_id = dgram_conn->host_id + dgram_conn->count_s_pay;
	s_packet->header.ack_nak_id = dgram_conn->count_s_ack;
	s_packet->header.ank_id = 0;

	/* System commands */
	/* TODO: send ack and nak commands */
	s_packet->sys_cmd[0].cmd.id = CMD_RESERVED_ID;
}

int vc_CLOSING_send_packet(struct vContext *C)
{
	vc_CLOSING_init_send_packet(C);
	return vc_send_packet(C);
}

int vc_CLOSING_handle_packet(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);

	/* Received packet has to have PAY flag (this packet have to be acknowledged),
	 * ACK flag (it has to acknowledge close request) and FIN flag (it is
	 * teardown packet) */
	if(r_packet->header.flags == (PAY_FLAG|ACK_FLAG|FIN_FLAG) &&
		/* Does this packet acknowledge right ID? (ACK command has to be first command!) */
			r_packet->sys_cmd[0].cmd.id==CMD_ACK_ID &&
			r_packet->sys_cmd[0].ack_cmd.pay_id==dgram_conn->host_id + dgram_conn->count_s_pay)
	{
		/* Client state */
		dgram_conn->host_state = UDP_CLIENT_STATE_CLOSED;

		/* Call callback functions for system commands */
		v_conn_dgram_handle_sys_cmds(C, UDP_CLIENT_STATE_CLOSING);

		/* Compute RWIN */
		dgram_conn->rwin_peer = r_packet->header.window << dgram_conn->rwin_peer_scale;
	}
	else {
		if(is_log_level(VRS_PRINT_WARNING)) v_print_log(VRS_PRINT_WARNING, "Packet with corrupted header was dropped.\n");
		return RECEIVE_PACKET_CORRUPTED;
	}

	return RECEIVE_PACKET_SUCCESS;
}

int vc_CLOSING_loop(struct vContext *C)
{
	struct VC_CTX *vc_ctx = CTX_client_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	int ret;

	/* CLOSING state */
	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client state: CLOSING\n");
		printf("%c[%dm", 27, 0);
	}

	dgram_conn->state[UDP_CLIENT_STATE_CLOSING].attempts = 0;

	while(dgram_conn->state[UDP_CLIENT_STATE_CLOSING].attempts < vc_ctx->max_connection_attempts) {
		/* Send closing packet */
		if( vc_CLOSING_send_packet(C) == SEND_PACKET_ERROR) {
			return STATE_EXIT_ERROR;
		}
		/* Try to receive packet and handle received packet */
		ret = vc_receive_and_handle_packet(C, vc_CLOSING_handle_packet);
		if(ret == RECEIVE_PACKET_SUCCESS) {
			break;			/* Break loop and receive to the next state */
		} else if(ret == RECEIVE_PACKET_TIMEOUT) {
			dgram_conn->state[UDP_CLIENT_STATE_CLOSING].attempts++;		/* No packet receive ... try it again */
		} else if(ret == RECEIVE_PACKET_CORRUPTED) {
			dgram_conn->state[UDP_CLIENT_STATE_CLOSING].attempts++;		/* Corrupted packet received ... try it again */
		} else if(ret == RECEIVE_PACKET_FAKED) {
			continue;		/* Packet wasn't received from the server (should not happen, because connect()) */
		} else if(ret == RECEIVE_PACKET_ERROR) {
			return STATE_EXIT_ERROR;
		}

		if(dgram_conn->io_ctx.flags & SOCKET_SECURED) {
			/* Did server close DTLS connection? */
			if((SSL_get_shutdown(dgram_conn->io_ctx.ssl) & SSL_RECEIVED_SHUTDOWN)) {
				return STATE_EXIT_ERROR;
			}
		}
	}

	if(dgram_conn->host_state == UDP_CLIENT_STATE_CLOSING) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR,
				"Maximum count of teardown attempts reached %d.\n", vc_ctx->max_connection_attempts);
		return STATE_EXIT_ERROR;
	}

	return STATE_EXIT_SUCCESS;
}

/* Generic receiving and handling of packet */
int vc_receive_and_handle_packet(struct vContext *C, int handle_packet(struct vContext *C))
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	int ret, error_num;
	long int sec, usec;
	fd_set set;
	struct timeval tv;

	/* Initialize set */
	FD_ZERO(&set);
	FD_SET(dgram_conn->io_ctx.sockfd, &set);

	switch(dgram_conn->host_state) {
		case UDP_CLIENT_STATE_REQUEST:
		case UDP_CLIENT_STATE_PARTOPEN:
		case UDP_CLIENT_STATE_CLOSING:
			sec = INIT_TIMEOUT + (int) (v_exponential_backoff(dgram_conn->state[dgram_conn->host_state].attempts) * (rand() / (RAND_MAX + 1.0)));
			usec = 0;
			break;
		case UDP_CLIENT_STATE_OPEN:
			sec = 0;
			usec = 10000;
			break;
	}

	/* Initialize time value */
	tv.tv_sec = sec;
	tv.tv_usec = usec;

	/* Wait on response from server */
	if( (ret = select(io_ctx->sockfd+1, &set, NULL, NULL, &tv)) == -1 ) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n", __FILE__, __FUNCTION__,  __LINE__, strerror(errno));
		return RECEIVE_PACKET_ERROR;
	}
	/* Check if the event occurred on sockfd */
	else if(ret>0 && FD_ISSET(io_ctx->sockfd, &set)) {
		/* Try to receive packet from server */
		if(v_receive_packet(io_ctx, &error_num)==-1) {
			switch(error_num) {
				case ECONNREFUSED:	/* A remote host refused this connection */
					return RECEIVE_PACKET_ERROR;
				case EAGAIN:
				case EBADF:
				case EFAULT:
				case EINTR:
				case EINVAL:
				case ENOMEM:
				case ENOTCONN:
				case ENOTSOCK:
					break;
			}
		/* Address of received packet has to be same as address of server, when
		 * connection is not connected */
		} else if((io_ctx->flags & SOCKET_CONNECTED) ||
				v_compare_addr_and_port(&dgram_conn->peer_address, &dgram_conn->io_ctx.peer_addr))
		{
			/* The size of buffer must be bigger then zero */
			if(dgram_conn->io_ctx.buf_size>0) {
				int buffer_pos = 0;

				/* Get time of receiving packet */
				gettimeofday(&tv, NULL);

				/* Extract verse header from packet */
				ret = v_unpack_packet_header(io_ctx->buf,
						io_ctx->buf_size,
						r_packet);
				if(ret != VERSE_PACKET_HEADER_SIZE)
					return RECEIVE_PACKET_CORRUPTED;

				/* Check for right packet header version number */
				if(r_packet->header.version != VRS_VERSION) {
					if(is_log_level(VRS_PRINT_WARNING)) v_print_log(VRS_PRINT_WARNING, "Packet with unsupported version: %d was dropped.\n",
							r_packet->header.version);
					return RECEIVE_PACKET_CORRUPTED;
				}

				/* Compute RWIN */
				dgram_conn->rwin_peer = r_packet->header.window << dgram_conn->rwin_peer_scale;

				/* Extract system commands from packet */
				buffer_pos += v_unpack_packet_system_commands(io_ctx->buf,
						io_ctx->buf_size,
						r_packet);

				/* Are there some node commands in the buffer? */
				if(io_ctx->buf_size > buffer_pos) {
					/* Set pointer to not proceeded buffer */
					r_packet->data = (uint8*)&io_ctx->buf[buffer_pos];
					/* Set size of not proceeded buffer */
					r_packet->data_size = io_ctx->buf_size - buffer_pos;
				} else {
					r_packet->data = NULL;
					r_packet->data_size = 0;
				}

				/* When important things are done, then print content of the
				 * command (in debug mode) */
				v_print_receive_packet(C);

				/* Handle received packet and its data */
				ret = handle_packet(C);

				if(ret == RECEIVE_PACKET_SUCCESS) {
					/* Update information about last received packet payload
					 * packet */
					if(r_packet->header.flags & PAY_FLAG) {
						dgram_conn->last_r_pay = r_packet->header.payload_id;
						/* Update time of last received payload packet */
						dgram_conn->tv_pay_recv.tv_sec = tv.tv_sec;
						dgram_conn->tv_pay_recv.tv_usec = tv.tv_usec;
					}
					/* Update information about last received packet acknowledgment
					 * packet */
					if(r_packet->header.flags & ACK_FLAG) {
						dgram_conn->last_r_ack = r_packet->header.ack_nak_id;
					}
				}

				return ret;

			} else {
				if(is_log_level(VRS_PRINT_WARNING)) v_print_log(VRS_PRINT_WARNING, "Packet with zero size was dropped.\n");
				return RECEIVE_PACKET_CORRUPTED;
			}
		} else {
			/* When some 'bad' packet is received on this port, then do not increase counter
			 * of connection attempts (packet was received from address, that does not belong
			 * to the server) */
			if(is_log_level(VRS_PRINT_WARNING))
				v_print_log(VRS_PRINT_WARNING, "Packet from unknown address was dropped.\n");

			return RECEIVE_PACKET_FAKED;	/* This should not happen, because connect() function is used */
		}
	}

	return RECEIVE_PACKET_TIMEOUT;
}

int vc_send_packet(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VPacket *s_packet = CTX_s_packet(C);
	int error_num;
	struct timeval tv;
	unsigned short buffer_pos = 0;
	int ret;

	v_print_send_packet(C);

	/* Fill buffer */
	buffer_pos += v_pack_packet_header(s_packet, &io_ctx->buf[buffer_pos]);
	buffer_pos += v_pack_dgram_system_commands(s_packet, &io_ctx->buf[buffer_pos]);
	io_ctx->buf_size = buffer_pos;

	/* Send buffer */
	ret = v_send_packet(io_ctx, &error_num);

	if(ret==SEND_PACKET_SUCCESS) {

		/* Update time of sending last packet */
		gettimeofday(&tv, NULL);
		dgram_conn->tv_pay_send.tv_sec = tv.tv_sec;
		dgram_conn->tv_pay_send.tv_usec = tv.tv_usec;

		/* Update counter of sent packets */
		switch(dgram_conn->host_state) {
		case UDP_CLIENT_STATE_REQUEST:
			dgram_conn->count_s_pay = 1;
			dgram_conn->count_s_ack = 0;
			break;
		case UDP_CLIENT_STATE_PARTOPEN:
			dgram_conn->count_s_pay = 2;
			dgram_conn->count_s_ack = 1;
			break;
		case UDP_CLIENT_STATE_OPEN:
			dgram_conn->count_s_pay++;
			dgram_conn->count_s_ack++;
			break;
		case UDP_CLIENT_STATE_CLOSING:
			break;
		case UDP_CLIENT_STATE_CLOSED:
			break;
		}
	}

	return ret;
}

#if OPENSSL_VERSION_NUMBER>=0x10000000

void vc_destroy_dtls_connection(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	int ret;

	v_print_log(VRS_PRINT_DEBUG_MSG, "Try to shut down DTLS connection.\n");

again:
	ret = SSL_shutdown(dgram_conn->io_ctx.ssl);
	if(ret!=1) {
		int error = SSL_get_error(dgram_conn->io_ctx.ssl, ret);
		switch(error) {
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				goto again;
			default:
				v_print_log(VRS_PRINT_DEBUG_MSG, "DTLS connection was not able to shut down: %d\n", error);
				break;
		}
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "DTLS connection was shut down.\n");
	}

	SSL_free(dgram_conn->io_ctx.ssl);
}

int vc_create_dtls_connection(struct vContext *C)
{
	struct VC_CTX *vc_ctx = CTX_client_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct timeval timeout;
	int ret;

	v_print_log(VRS_PRINT_DEBUG_MSG, "Try to do DTLS handshake at UDP socket: %d\n",
			dgram_conn->io_ctx.sockfd);

	/* Create ssl for new connection */
	if( (dgram_conn->io_ctx.ssl = SSL_new(vc_ctx->dtls_ctx)) == NULL) {
		v_print_log(VRS_PRINT_ERROR, "SSL_new(%p)\n", (void*)vc_ctx->dtls_ctx);
		return 0;
	}

	/* Set state of bio as connected */
	if(dgram_conn->io_ctx.peer_addr.ip_ver == IPV4) {
		ret = BIO_ctrl(dgram_conn->io_ctx.bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &dgram_conn->io_ctx.peer_addr.addr.ipv6);
	} else if(dgram_conn->io_ctx.peer_addr.ip_ver == IPV6) {
		ret = BIO_ctrl(dgram_conn->io_ctx.bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &dgram_conn->io_ctx.peer_addr.addr.ipv4);
	}

	/* When BIO_ctrl was called with bad arguments, then it returns 0 */
	if(ret==0) {
		v_print_log(VRS_PRINT_ERROR, "BIO_ctrl()\n");
		SSL_free(dgram_conn->io_ctx.ssl);
		return 0;
	}

	/* Set and activate timeouts */
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	BIO_ctrl(dgram_conn->io_ctx.bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
	BIO_ctrl(dgram_conn->io_ctx.bio, BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &timeout);

	/* Bind ssl and bio */
	SSL_set_bio(dgram_conn->io_ctx.ssl, dgram_conn->io_ctx.bio, dgram_conn->io_ctx.bio);

	/* Try to do DTLS handshake */
again:
	if ((ret = SSL_connect(dgram_conn->io_ctx.ssl)) <= 0) {
		int err = SSL_get_error(dgram_conn->io_ctx.ssl, ret);
		if(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
			gettimeofday(&timeout, NULL);
			if((timeout.tv_sec - vsession->peer_cookie.tv.tv_sec) > VRS_TIMEOUT) {
				v_print_log(VRS_PRINT_ERROR, "Cookie timed out\n");
				return 0;
			}
			usleep(1000);
			goto again;
		}
		ERR_print_errors_fp(stderr);
		v_print_log(VRS_PRINT_ERROR, "SSL_connect() failed: %d -> %d\n", ret, err);
		SSL_free(dgram_conn->io_ctx.ssl);
		dgram_conn->io_ctx.ssl = NULL;
		dgram_conn->io_ctx.bio = NULL;
		return 0;
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "DTLS handshake finished\n");

		v_print_log(VRS_PRINT_DEBUG_MSG, "Current cipher: %s\n",
				SSL_CIPHER_get_name(SSL_get_current_cipher(dgram_conn->io_ctx.ssl)));
	}

	return 1;
}

#endif

/* Create new UDP connection to the server */
struct VDgramConn *vc_create_client_dgram_conn(struct vContext *C)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn = NULL;
	struct addrinfo hints, *result, *rp;
	int sockfd;
	int flag, ret;
	struct VURL url;

	/* Seed random number generator,  */
#ifdef __APPLE__
	sranddev();
	/* Other BSD based systems probably support this or similar function too. */
#else
	/* Other systems have to use this evil trick */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_sec - tv.tv_usec);
#endif

	if (v_parse_url(vsession->host_url, &url) != 1) {
		return NULL;
	} else {
		/* v_print_url(VRS_PRINT_DEBUG_MSG, &url); */
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;		/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM;		/* Allow datagram protocol */
	hints.ai_flags = 0;					/* No flags required */
	hints.ai_protocol = IPPROTO_UDP;	/* Allow UDP protocol only */

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		if(url.ip_ver==IPV6) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "Try to connect to: [%s]:%s\n", url.node, url.service);
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "Try to connect to: %s:%s\n", url.node, url.service);
		}
	}

	if( (ret = getaddrinfo(url.node, url.service, &hints, &result)) !=0 ) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "getaddrinfo(): %s\n", gai_strerror(ret));
		return NULL;
	}

	/* Try to use addrinfo from getaddrinfo() */
	for(rp=result; rp!=NULL; rp=rp->ai_next) {
		if( (sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "socket(): %s\n", strerror(errno));
			continue;
		}
		else {
			/* Try to "connect" to this address ... the client will be able to send and
			 * receive packets only from this address. */
			if(connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
				break;
			close(sockfd);
		}
	}

	if(rp==NULL) {
		if(is_log_level(VRS_PRINT_ERROR)) {
			if(url.ip_ver==IPV6) {
				v_print_log(VRS_PRINT_ERROR, "Could not connect to the [%s]:%s\n", url.node, url.service);
			} else {
				v_print_log(VRS_PRINT_ERROR, "Could not connect to the %s:%s\n", url.node, url.service);
			}
		}
		freeaddrinfo(result);
		return NULL;
	}

	if( (dgram_conn = (struct VDgramConn*)calloc(1, sizeof(struct VDgramConn))) == NULL) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "malloc(): %s\n", strerror(errno));
		freeaddrinfo(result);
		return NULL;
	}

	/* Initialize datagram connection */
	v_conn_dgram_init(dgram_conn);

	/* Use first successfully assigned socket */
	dgram_conn->io_ctx.sockfd = sockfd;

	/* Set socket non-blocking */
	flag = fcntl(dgram_conn->io_ctx.sockfd, F_GETFL, 0);
	if( (fcntl(dgram_conn->io_ctx.sockfd, F_SETFL, flag | O_NONBLOCK)) == -1) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
		free(dgram_conn);
		freeaddrinfo(result);
		return NULL;
	}

	/* Set socket to reuse address */
	flag = 1;
	if( setsockopt(dgram_conn->io_ctx.sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "setsockopt(): %s\n", strerror(errno));
		free(dgram_conn);
		return NULL;
	}

	/* Set address of peer and host */
	if(rp->ai_family==AF_INET) {
		/* Address type of client */
		dgram_conn->io_ctx.host_addr.ip_ver = IPV4;
		dgram_conn->io_ctx.host_addr.protocol = UDP;
		/* Address of peer */
		dgram_conn->io_ctx.peer_addr.ip_ver = IPV4;
		dgram_conn->io_ctx.peer_addr.protocol = UDP;
		dgram_conn->io_ctx.peer_addr.port = ntohs(((struct sockaddr_in*)rp->ai_addr)->sin_port);
		memcpy(&dgram_conn->io_ctx.peer_addr.addr.ipv4, rp->ai_addr, rp->ai_addrlen);
		/* Address of peer (reference in connection) */
		dgram_conn->peer_address.ip_ver = IPV4;
		dgram_conn->peer_address.protocol = UDP;
		dgram_conn->peer_address.port = ntohs(((struct sockaddr_in*)rp->ai_addr)->sin_port);
		memcpy(&dgram_conn->peer_address.addr.ipv4, rp->ai_addr, rp->ai_addrlen);
	}
	else if(rp->ai_family==AF_INET6) {
		/* Address type of client */
		dgram_conn->io_ctx.host_addr.ip_ver = IPV6;
		dgram_conn->io_ctx.host_addr.protocol = UDP;
		/* Address of peer */
		dgram_conn->io_ctx.peer_addr.ip_ver = IPV6;
		dgram_conn->io_ctx.peer_addr.protocol = UDP;
		dgram_conn->io_ctx.peer_addr.port = ntohs(((struct sockaddr_in6*)rp->ai_addr)->sin6_port);
		memcpy(&dgram_conn->io_ctx.peer_addr.addr.ipv6, rp->ai_addr, rp->ai_addrlen);
		/* Address of peer (reference in connection) */
		dgram_conn->peer_address.ip_ver = IPV6;
		dgram_conn->peer_address.protocol = UDP;
		dgram_conn->peer_address.port = ntohs(((struct sockaddr_in6*)rp->ai_addr)->sin6_port);
		memcpy(&dgram_conn->peer_address.addr.ipv6, rp->ai_addr, rp->ai_addrlen);
	}

	freeaddrinfo(result);

	/* When DTLS was negotiated, then set flag */
	if(url.security_protocol == VRS_DGRAM_SEC_DTLS) {
#if OPENSSL_VERSION_NUMBER>=0x10000000
		dgram_conn->io_ctx.flags |= SOCKET_SECURED;
#else
		v_print_log(VRS_PRINT_ERROR, "Server tries to force client to use secured connection, but it is not supported\n");
		return NULL;
#endif
	}


	/* Create BIO, connect and set to already connected */
	if( (dgram_conn->io_ctx.bio = BIO_new_dgram(dgram_conn->io_ctx.sockfd, BIO_CLOSE)) == NULL) {
		v_print_log(VRS_PRINT_ERROR, "BIO_new_dgram()\n");
		return NULL;
	}

	/* Try to do PMTU discovery */
	if( BIO_ctrl(dgram_conn->io_ctx.bio, BIO_CTRL_DGRAM_MTU_DISCOVER, 0, NULL) < 0) {
		v_print_log(VRS_PRINT_ERROR, "BIO_ctrl()\n");
		return NULL;
	}

	/* Try to get MTU from the bio */
	ret = BIO_ctrl(dgram_conn->io_ctx.bio, BIO_CTRL_DGRAM_QUERY_MTU, 0, NULL);
	if(ret > 0) {
		dgram_conn->io_ctx.mtu = ret;
		v_print_log(VRS_PRINT_DEBUG_MSG, "PMTU: %d\n", dgram_conn->io_ctx.mtu);
	} else {
		dgram_conn->io_ctx.mtu = DEFAULT_MTU;
		v_print_log(VRS_PRINT_DEBUG_MSG, "Default MTU: %d\n", dgram_conn->io_ctx.mtu);
	}

	/* Set up necessary flag for V_CTX (client will be able to send and receive packets only to/from server) */
	dgram_conn->io_ctx.flags |= SOCKET_CONNECTED;

	dgram_conn->host_id = (unsigned int)rand();

	return dgram_conn;
}

/************************************** MAIN loop **************************************/

const uint8 vrs_conn_term_error = VRS_CONN_TERM_ERROR;
const uint8 vrs_conn_term_server = VRS_CONN_TERM_SERVER;

void* vc_main_dgram_loop(void *arg)
{
	struct vContext *C = (struct vContext*)arg;
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn = vsession->dgram_conn;
	struct VStreamConn *stream_conn = CTX_current_stream_conn(C);
	struct VPacket *r_packet, *s_packet;
	char error=0;
	const uint8 *ret_val;

	if(dgram_conn==NULL) {
		/* Create new datagrame connection */
		if((dgram_conn = vc_create_client_dgram_conn(C))==NULL) {
			goto finish;
		}
		vsession->dgram_conn = dgram_conn;
	}

	CTX_current_dgram_conn_set(C, dgram_conn);
	CTX_io_ctx_set(C, &dgram_conn->io_ctx);

#if OPENSSL_VERSION_NUMBER>=0x10000000
	/* If negotiated security is DTLS, then try to do DTLS handshake */
	if(dgram_conn->io_ctx.flags & SOCKET_SECURED) {
		if(vc_create_dtls_connection(C) == 0) {
			CTX_current_dgram_conn_set(C, NULL);
			CTX_io_ctx_set(C, NULL);
			free(dgram_conn);
			ret_val = &vrs_conn_term_error;
			goto finish;
		}
	}
#endif

	/* Packet structure for receiving */
	r_packet = (struct VPacket*)malloc(sizeof(struct VPacket));
	CTX_r_packet_set(C, r_packet);

	/* Packet structure for sending */
	s_packet = (struct VPacket*)malloc(sizeof(struct VPacket));
	CTX_s_packet_set(C, s_packet);

	/* Run loop of the first phase of the handshake */
	if((error = vc_REQUEST_loop(C)) == STATE_EXIT_ERROR) {
		/* Do not confirm proposed URL, when client was not able to connect
		 * to the server */
		CTX_io_ctx_set(C, &stream_conn->io_ctx);
		vc_NEGOTIATE_newhost(C, NULL);
		CTX_io_ctx_set(C, &dgram_conn->io_ctx);
		ret_val = &vrs_conn_term_error;
		goto end;
	}

	/* When server responded in the first phase of the handshake, then run loop of the second
	 * phase of the handshake */
	if((error = vc_PARTOPEN_loop(C)) ==  STATE_EXIT_ERROR) {
		/* Do not confirm proposed URL, when client was not able to connect
		 * to the server */
		CTX_io_ctx_set(C, &stream_conn->io_ctx);
		vc_NEGOTIATE_newhost(C, NULL);
		CTX_io_ctx_set(C, &dgram_conn->io_ctx);
		ret_val = &vrs_conn_term_error;
		goto end;
	} else {
		struct Connect_Accept_Cmd *conn_accept;

		/* Put connect accept command to queue -> call callback function */
		conn_accept = v_Connect_Accept_create(vsession->avatar_id, vsession->user_id);
		v_in_queue_push(vsession->in_queue, (struct Generic_Cmd*)conn_accept);

		/* Send confirmation of the URL to the server */
		CTX_io_ctx_set(C, &stream_conn->io_ctx);
		vc_NEGOTIATE_newhost(C, vsession->host_url);
		CTX_io_ctx_set(C, &dgram_conn->io_ctx);

		/* TCP connection could be closed now */
		vsession->stream_conn->host_state = TCP_CLIENT_STATE_CLOSING;
	}

	/* Main loop for data exchange */
	if((error = vc_OPEN_loop(C)) == STATE_EXIT_ERROR) {
		ret_val = &vrs_conn_term_error;
		goto end;
	}

	/* Closing loop */
	if((error = vc_CLOSING_loop(C)) == STATE_EXIT_ERROR) {
		ret_val = &vrs_conn_term_error;
		goto end;
	}

	/* TODO: distinguish between terminating connection by server and client */
	ret_val = &vrs_conn_term_server;

end:

	/* CLOSED state */
	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client state: CLOSED\n");
		printf("%c[%dm", 27, 0);
	}

	free(r_packet);
	free(s_packet);

#if OPENSSL_VERSION_NUMBER>=0x10000000
	if(dgram_conn->io_ctx.flags & SOCKET_SECURED) {
		vc_destroy_dtls_connection(C);
	}
#endif

	v_conn_dgram_destroy(dgram_conn);

	CTX_current_dgram_conn_set(C, NULL);

finish:
	pthread_exit((void*)ret_val);
	return NULL;
}
