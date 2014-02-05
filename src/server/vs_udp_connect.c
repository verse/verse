/*
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

#ifdef WITH_OPENSSL
#include <openssl/rand.h>
#include <openssl/ssl.h>
#ifdef __APPLE__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>

#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include "vs_main.h"
#include "vs_udp_connect.h"

#include "v_context.h"
#include "v_network.h"
#include "v_connection.h"
#include "v_common.h"
#include "v_resend_mechanism.h"
#include "v_network.h"
#include "v_session.h"

#include "v_unpack.h"

static void vs_LISTEN_init(struct vContext *C);
static void vs_RESPOND_init(struct vContext *C);
static void vs_OPEN_init(struct vContext *C);
static void vs_CLOSEREQ_init(struct VDgramConn *dgram_conn);
static void vs_CLOSED_init_send_packet(struct vContext *C);
static void vs_CLOSED_init(struct vContext *C);

#if (defined WITH_OPENSSL) && OPENSSL_VERSION_NUMBER>=0x10000000
static int cookie_initialized = 0;
static unsigned char cookie_secret[COOKIE_SECRET_LENGTH];
#endif

/* Initialize VConnection at Verse server for potential clients */
void vs_init_dgram_conn(struct VDgramConn *dgram_conn)
{
	v_conn_dgram_init(dgram_conn);
	dgram_conn->host_state = UDP_SERVER_STATE_CLOSED;	/* Server is in listen state */
}

/* Clear VConnection of Verse server */
void vs_clear_vconn(struct VDgramConn *dgram_conn)
{
	v_packet_history_destroy(&dgram_conn->packet_history);
	v_ack_nak_history_clear(&dgram_conn->ack_nak);
	v_conn_dgram_clear(dgram_conn);
	dgram_conn->host_state = UDP_SERVER_STATE_CLOSED;	/* Server is in closed state */
}

/* This generic function is for handling packet received by server. It fills
 * C->r_packet structure with data in C->io_ctx->buf. C->r_packet is handled
 * in vs_STATE_handle_packet() function, which is specific for each state. */
int vs_handle_packet(struct vContext *C, int vs_STATE_handle_packet(struct vContext*C))
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VDgramConn *vconn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	int buffer_pos = 0;

	/* Extract Verse header from packet */
	v_unpack_packet_header(io_ctx->buf, io_ctx->buf_size, r_packet);

	/* Compute RWIN */
	vconn->rwin_peer = r_packet->header.window << vconn->rwin_peer_scale;

	/* Extract all system commands from the received packet */
	buffer_pos += v_unpack_packet_system_commands(io_ctx->buf, io_ctx->buf_size, r_packet);

	/* Are there some node commands in the buffer? */
	if(vconn->io_ctx.buf_size > buffer_pos) {
		/* Set pointer to not proceeded buffer */
		r_packet->data = (uint8*)&io_ctx->buf[buffer_pos];
		/* Set size of not proceeded buffer */
		r_packet->data_size = vconn->io_ctx.buf_size - buffer_pos;
	} else {
		r_packet->data = NULL;
		r_packet->data_size = 0;
	}

	v_print_receive_packet(C);

	if(r_packet->header.version==VRS_VERSION) {
		int ret;

		/* Do state specific things here */
		ret = vs_STATE_handle_packet(C);

		if(ret == RECEIVE_PACKET_CORRUPTED)
			if(is_log_level(VRS_PRINT_DEBUG_MSG))
				v_print_log(VRS_PRINT_DEBUG_MSG, "Bad or missing header flags or system commands\n");

		if(ret == RECEIVE_PACKET_SUCCESS) {
			/* Update information about last received packet */
			if(r_packet->header.flags & PAY_FLAG) vconn->last_r_pay = r_packet->header.payload_id;
			if(r_packet->header.flags & ACK_FLAG) vconn->last_r_ack = r_packet->header.ack_nak_id;
		}

		return ret;

	} else {
		if(is_log_level(VRS_PRINT_DEBUG_MSG))
			v_print_log(VRS_PRINT_DEBUG_MSG, "Unsupported protocol version: %d\n", r_packet->header.version);
		return RECEIVE_PACKET_CORRUPTED;
	}

	return RECEIVE_PACKET_ERROR;
}

/* This is generic function for sending packet to the client. C->s_packet has
 * to be filled with data (header, system commands and node commands). */
int vs_send_packet(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VDgramConn *vconn = CTX_current_dgram_conn(C);
	struct VPacket *s_packet = CTX_s_packet(C);
	unsigned short buffer_pos = 0;
	struct timeval tv;
	int ret, error_num;

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
		vconn->tv_pay_send.tv_sec = tv.tv_sec;
		vconn->tv_pay_send.tv_usec = tv.tv_usec;

		/* Update counter of sent packets */
		switch(vconn->host_state) {
		case UDP_SERVER_STATE_LISTEN:
			vconn->count_s_pay = 1;
			vconn->count_s_ack = 0;
			break;
		case UDP_SERVER_STATE_RESPOND:
			vconn->count_s_pay = 2;
			vconn->count_s_ack = 1;
			break;
		case UDP_SERVER_STATE_OPEN:
			vconn->count_s_pay++;
			vconn->count_s_ack++;
			break;
		case UDP_SERVER_STATE_CLOSEREQ:
			vconn->count_s_pay++;
			vconn->count_s_ack++;
			break;
		case UDP_SERVER_STATE_CLOSED:
			/* TODO: add useful code here */
			break;
		}
	}

	return ret;
}

/************************************** LISTEN state **************************************/

/**
 * \brief This function checks received Change_L system commands, when system
 * command is wrong, then this function returns 0 value; otherwise it returns 1.
 */
static int vs_LISTEN_CHANGE_L_cb(struct vContext *C, struct Generic_Cmd *cmd)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct Negotiate_Cmd *change_l_cmd = (struct Negotiate_Cmd*)cmd;
	int value_rank, tmp = 0;

	/* This block of code checks if received cookie is the same as the
	 * negotiated cookie. When cookie is not the same, then the server
	 * will not response to the received packet.*/
	if(change_l_cmd->feature == FTR_COOKIE) {
		if( vsession->peer_cookie.str != NULL &&
				change_l_cmd->count > 0 &&
				change_l_cmd->value[0].string8.str != NULL &&
				strcmp((char*)change_l_cmd->value[0].string8.str, vsession->peer_cookie.str)==0 )
		{
			return 1;
		} else {
			return 0;
		}
	}

	/* Check if at least one of proposed Congestion Control methods are
	 * supported */
	if(change_l_cmd->feature == FTR_CC_ID) {
		for(value_rank=0; value_rank<change_l_cmd->count; value_rank++) {
			/* This could not be never send */
			if(change_l_cmd->value[value_rank].uint8 == CC_RESERVED) {
				break;
			/* TODO: better implementation: server could have list of supported CC methods */
			} else if(change_l_cmd->value[value_rank].uint8 == vs_ctx->cc_meth) {
				/* It will try to use first found supported method, but ... */
				if(dgram_conn->cc_meth == CC_RESERVED) {
					/* Congestion Control has not been proposed yet */
				} else {
					/* Client has to propose same CC methods for client
					 * and server. Server can't use different method then
					 * client and vice versa. */
					if(dgram_conn->cc_meth != change_l_cmd->value[value_rank].uint8) {
						v_print_log(VRS_PRINT_WARNING,
								"Proposed CC local :%d is not same as proposed CC remote: %d\n",
								change_l_cmd->value[value_rank].uint8, dgram_conn->cc_meth);
						break;
					}
				}
				tmp = 1;
				dgram_conn->cc_meth = change_l_cmd->value[value_rank].uint8;
				break;
			}
		}

		if(tmp==1) {
			return 1;
		} else {
			return 0;
		}
	}

	/* Client wants to use scaling of Flow Control Window */
	if(change_l_cmd->feature == FTR_RWIN_SCALE) {
		if(change_l_cmd->count >= 1) {
			dgram_conn->rwin_peer_scale = change_l_cmd->value[0].uint8;
			return 1;
		} else {
			return 0;
		}
	}

	/* Client wants to negotiate type of command compression */
	if(change_l_cmd->feature == FTR_CMD_COMPRESS) {
		for(value_rank=0; value_rank<change_l_cmd->count; value_rank++) {
			if(change_l_cmd->value[value_rank].uint8 == CMPR_NONE ||
					change_l_cmd->value[value_rank].uint8 == CMPR_ADDR_SHARE)
			{
				dgram_conn->peer_cmd_cmpr = change_l_cmd->value[value_rank].uint8;
				tmp = 1;
				break;
			}
		}

		/* Does client sent at least one supported compress type? */
		if(tmp==1) {
			return 1;
		} else {
			return 0;
		}
	}

	/* Received command is unknown. Ignore it. */
	return 1;
}

/**
 * \brief This function checks received Change_R system commands, when system
 * command is wrong, then this function returns 0 value; otherwise it returns 1.
 */
static int	vs_LISTEN_CHANGE_R_cb(struct vContext *C, struct Generic_Cmd *cmd)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct Negotiate_Cmd *change_r_cmd = (struct Negotiate_Cmd*)cmd;
	int value_rank, tmp = 0;

	/* Check if at least one of proposed Congestion Control methods are
	 * supported */
	if(change_r_cmd->feature == FTR_CC_ID) {
		for(value_rank=0; value_rank<change_r_cmd->count; value_rank++) {
			/* This could not be never send */
			if(change_r_cmd->value[value_rank].uint8 == CC_RESERVED) {
				break;
			} else if(change_r_cmd->value[value_rank].uint8 == vs_ctx->cc_meth) { /* TODO: better implementation */
				/* It will try to use first found supported method, but ... */
				if(dgram_conn->cc_meth == CC_RESERVED) {
					/* Congestion Control has not been proposed yet */
				} else {
					/* Client has to propose same CC methods for client
					 * and server. Server can't use different method then
					 * client and vice versa. */
					if(dgram_conn->cc_meth != change_r_cmd->value[value_rank].uint8) {
						v_print_log(VRS_PRINT_WARNING,
								"Proposed CC remote :%d is not same as proposed CC local: %d\n",
								change_r_cmd->value[value_rank].uint8, dgram_conn->cc_meth);
						break;
					}
				}
				tmp = 1;
				dgram_conn->cc_meth = change_r_cmd->value[value_rank].uint8;
				break;
			}
		}

		if(tmp==1) {
			return 1;
		} else {
			return 0;
		}
	}

	/* Client wants to negotiate type of command compression */
	if(change_r_cmd->feature == FTR_CMD_COMPRESS) {
		for(value_rank=0; value_rank<change_r_cmd->count; value_rank++) {
			if(change_r_cmd->value[value_rank].uint8 == CMPR_NONE ||
					change_r_cmd->value[value_rank].uint8 == CMPR_ADDR_SHARE)
			{
				dgram_conn->host_cmd_cmpr = change_r_cmd->value[value_rank].uint8;
				tmp = 1;
				break;
			}
		}

		/* Does client sent at least one supported compress type? */
		if(tmp==1) {
			return 1;
		} else {
			return 0;
		}
	}

	return 1;
}

/**
 * \brief This function init datagrame connection for LISTEN state
 * \param[in]	*C	The verse context
 */
static void vs_LISTEN_init(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct timeval tv;

	/* Set up time, when this state beagn*/
	gettimeofday(&tv, NULL);
	dgram_conn->state[UDP_SERVER_STATE_LISTEN].tv_state_began.tv_sec = tv.tv_sec;
	dgram_conn->state[UDP_SERVER_STATE_LISTEN].tv_state_began.tv_usec = tv.tv_usec;

	dgram_conn->host_id = (unsigned int)rand();

	/* Set up callback function for system commands */
	dgram_conn->state[UDP_SERVER_STATE_LISTEN].CHANGE_L_cb = vs_LISTEN_CHANGE_L_cb;
	dgram_conn->state[UDP_SERVER_STATE_LISTEN].CHANGE_R_cb = vs_LISTEN_CHANGE_R_cb;

	dgram_conn->host_state = UDP_SERVER_STATE_LISTEN;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Connection %d state: LISTEN\n", dgram_conn->host_id);
		printf("%c[%dm", 27, 0);
	}
}

/**
 * \brief This function initialize packet, that is send during handshake
 * in LISTEN and RESPOND state
 * \param[in]	*C	The verse context
 */
static void vs_LISTEN_init_send_packet(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	struct VPacket *s_packet = CTX_s_packet(C);
	int cmd_rank = 0;

	/* Verse packet header */
	s_packet->header.version = 1;
	s_packet->header.flags = PAY_FLAG | SYN_FLAG | ACK_FLAG;
	s_packet->header.window = dgram_conn->rwin_host >> dgram_conn->rwin_host_scale;
	s_packet->header.payload_id = dgram_conn->host_id;	/* Random number */
	s_packet->header.ack_nak_id = 0;
	s_packet->header.ank_id = 0;

	/* System commands */
	s_packet->sys_cmd[cmd_rank].ack_cmd.id = CMD_ACK_ID;
	s_packet->sys_cmd[cmd_rank].ack_cmd.pay_id = r_packet->header.payload_id;
	cmd_rank++;

	/* Send confirmation about cookie */
	if(vsession->peer_cookie.str!=NULL) {
		cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
				CMD_CONFIRM_L_ID, FTR_COOKIE, vsession->peer_cookie.str, NULL);
	}

	/* Send own cookie */
	if(vsession->host_cookie.str!=NULL) {
		cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
				CMD_CHANGE_L_ID, FTR_COOKIE, vsession->host_cookie.str, NULL);
	}

	/* Send confirmation about Congestion Control method if client proposed
	 * supported Congestion Control method */
	if(dgram_conn->cc_meth != CC_RESERVED) {
		cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
				CMD_CONFIRM_L_ID, FTR_CC_ID, &dgram_conn->cc_meth, NULL);
		cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
				CMD_CONFIRM_R_ID, FTR_CC_ID, &dgram_conn->cc_meth, NULL);
	} else {
		/* TODO: when client didn't send some proposal, then send server preference
		 * as proposal */
	}

	/* Send proposal of Flow Control (server send this proposal first) */
	if(vs_ctx->fc_meth != FC_RESERVED) {
		/* Flow control used at link between server and client */
		cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
				CMD_CHANGE_L_ID, FTR_FC_ID, &vs_ctx->fc_meth, NULL);
		/* Flow control used at link between client and server */
		cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
				CMD_CHANGE_R_ID, FTR_FC_ID, &vs_ctx->fc_meth, NULL);
	}

	/* Send proposal of host (local) Flow Control Window scaling */
	if(vs_ctx->rwin_scale != 0) {
		cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
				CMD_CHANGE_L_ID, FTR_RWIN_SCALE, &vs_ctx->rwin_scale, NULL);
	}

	/* Send confirmation of peer Flow Control Window scaling */
	if(dgram_conn->rwin_peer_scale != 0) {
		cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
				CMD_CONFIRM_L_ID, FTR_RWIN_SCALE, &dgram_conn->rwin_peer_scale, NULL);
	}

	/* Send confirmation of peer command compression */
	if(dgram_conn->host_cmd_cmpr != CMPR_RESERVED) {
		cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
				CMD_CONFIRM_R_ID, FTR_CMD_COMPRESS, &dgram_conn->host_cmd_cmpr, NULL);
	} else {
		/* When client didn't propose any command compression, then propose compression
		 * form server settings */
		uint8 cmpr_addr_share = CMPR_ADDR_SHARE;
		uint8 cmpr_none = CMPR_NONE;
		if(vs_ctx->cmd_cmpr == CMPR_ADDR_SHARE) {
			cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
					CMD_CHANGE_L_ID, FTR_CMD_COMPRESS, &cmpr_addr_share, &cmpr_none, NULL);
		} else {
			cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
					CMD_CHANGE_L_ID, FTR_CMD_COMPRESS, &cmpr_none, &cmpr_addr_share, NULL);
		}
	}

	/* Send confirmation of peer command compression */
	if(dgram_conn->peer_cmd_cmpr != CMPR_RESERVED) {
		cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
				CMD_CONFIRM_L_ID, FTR_CMD_COMPRESS, &dgram_conn->peer_cmd_cmpr, NULL);
	} else {
		/* When client didn't propose any command compression, then propose compression
		 * form server settings */
		uint8 cmpr_addr_share = CMPR_ADDR_SHARE;
		uint8 cmpr_none = CMPR_NONE;
		if(vs_ctx->cmd_cmpr == CMPR_ADDR_SHARE) {
			cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
					CMD_CHANGE_R_ID, FTR_CMD_COMPRESS, &cmpr_addr_share, &cmpr_none, NULL);
		} else {
			cmd_rank += v_add_negotiate_cmd(s_packet->sys_cmd, cmd_rank,
					CMD_CHANGE_R_ID, FTR_CMD_COMPRESS, &cmpr_none, &cmpr_addr_share, NULL);
		}
	}

}

/* Handle received packet, when server is in LISTEN state */
static int vs_LISTEN_handle_packet(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);

	if(dgram_conn->host_state != UDP_SERVER_STATE_LISTEN) {
		v_print_log(VRS_PRINT_WARNING, "Bad call of function: %s()\n", __FUNCTION__);
		return RECEIVE_PACKET_ERROR;
	}

	/* Is this packet from client in REQUEST state? */
	if( r_packet->header.flags==(PAY_FLAG|SYN_FLAG) ) {
		/* Set up counter of connection attempts (this is the first attempt) */
		dgram_conn->state[UDP_SERVER_STATE_LISTEN].attempts = 1;

		/* Call callback functions for system commands. */
		if(v_conn_dgram_handle_sys_cmds(C, UDP_SERVER_STATE_LISTEN)!=1) {
			return RECEIVE_PACKET_CORRUPTED;
		}

		/* Initialize header of verse packet and add ack system command
		 * to the packet */
		vs_LISTEN_init_send_packet(C);

		/* Send reply packet to the client */
		vs_send_packet(C);

		/* Initialize and switch to the RESPOND state */
		vs_RESPOND_init(C);
	} else {
		return RECEIVE_PACKET_CORRUPTED;
	}

	return RECEIVE_PACKET_SUCCESS;
}

/************************************** RESPOND state **************************************/

/**
 * \brief This function check if the client confirmed Cookie or right Flow Control.
 */
static int vs_RESPOND_CONFIRM_R_cb(struct vContext *C, struct Generic_Cmd *cmd)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct Negotiate_Cmd *confirm_r_cmd = (struct Negotiate_Cmd*)cmd;
	int value_rank, tmp = 0;

	/* Check if at least one of proposed Flow Control methods are
	 * supported */
	if(confirm_r_cmd->feature == FTR_FC_ID) {
		for(value_rank=0; value_rank<confirm_r_cmd->count; value_rank++) {
			/* This could not be never send */
			if(confirm_r_cmd->value[value_rank].uint8 == FC_RESERVED) {
				break;
			/* TODO: better implementation: server could have list of supported FC methods */
			} else if(confirm_r_cmd->value[value_rank].uint8 == vs_ctx->fc_meth) {
				/* It will try to use first found supported method, but ... */
				if(dgram_conn->fc_meth == FC_RESERVED) {
					/* Congestion Control has not been proposed yet */
				} else {
					/* Client has to propose same FC methods for client
					 * and server. Server can't use different method then
					 * client and vice versa. */
					if(dgram_conn->fc_meth != confirm_r_cmd->value[value_rank].uint8) {
						v_print_log(VRS_PRINT_WARNING, "Proposed FC local :%d is not the same as proposed FC remote: %d\n",
								confirm_r_cmd->value[value_rank].uint8, dgram_conn->fc_meth);
						break;
					}
				}
				tmp = 1;
				dgram_conn->fc_meth = confirm_r_cmd->value[value_rank].uint8;
				break;
			}
		}

		if(tmp==1) {
			return 1;
		} else {
			return 0;
		}
	}

	return 1;
}

/**
 * \brief This function check if the client confirmed Cookie or right Flow Control.
 */
static int vs_RESPOND_CONFIRM_L_cb(struct vContext *C, struct Generic_Cmd *cmd)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct Negotiate_Cmd *confirm_l_cmd = (struct Negotiate_Cmd*)cmd;
	int value_rank, tmp = 0;

	if(confirm_l_cmd->feature == FTR_COOKIE) {
		if( vsession->host_cookie.str != NULL &&
				confirm_l_cmd->count > 0 &&
				confirm_l_cmd->value[0].string8.str != NULL &&
				strcmp((char*)confirm_l_cmd->value[0].string8.str, vsession->host_cookie.str)==0 )
		{
			return 1;
		} else {
			return 0;
		}
	}

	/* Check if at least one of proposed Flow Control methods are
	 * supported */
	if(confirm_l_cmd->feature == FTR_FC_ID) {
		for(value_rank=0; value_rank<confirm_l_cmd->count; value_rank++) {
			/* This could not be never send */
			if(confirm_l_cmd->value[value_rank].uint8 == FC_RESERVED) {
				break;
			/* TODO: better implementation: server could have list of supported FC methods */
			} else if(confirm_l_cmd->value[value_rank].uint8 == vs_ctx->fc_meth) {
				/* It will try to use first found supported method, but ... */
				if(dgram_conn->fc_meth == FC_RESERVED) {
					/* Congestion Control has not been proposed yet */
				} else {
					/* Client has to propose same FC methods for client
					 * and server. Server can't use different method then
					 * client and vice versa. */
					if(dgram_conn->fc_meth != confirm_l_cmd->value[value_rank].uint8) {
						v_print_log(VRS_PRINT_WARNING, "Proposed FC local :%d is not the same as proposed FC remote: %d\n",
								confirm_l_cmd->value[value_rank].uint8, dgram_conn->fc_meth);
						break;
					}
				}
				tmp = 1;
				dgram_conn->fc_meth = confirm_l_cmd->value[value_rank].uint8;
				break;
			}
		}

		if(tmp==1) {
			return 1;
		} else {
			return 0;
		}
	}

	/* Client should confirm server proposal of Flow Control Window scale (local) */
	if(confirm_l_cmd->feature == FTR_RWIN_SCALE) {
		if(confirm_l_cmd->count >= 1 ) {
			if(vs_ctx->rwin_scale == confirm_l_cmd->value[0].uint8) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Scale of host RWIN: %d confirmed\n", vs_ctx->rwin_scale);
				dgram_conn->rwin_host_scale = vs_ctx->rwin_scale;
				return 1;
			} else {
				dgram_conn->rwin_host_scale = 0;
				return 0;
			}
		} else {
			dgram_conn->rwin_host_scale = 0;
			return 0;
		}
	}

	return 1;
}

static void vs_RESPOND_init(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	int ret = 0;
	struct timeval tv;

	/* Switch to RESPOND state */
	dgram_conn->host_state = UDP_SERVER_STATE_RESPOND;

	/* Set up time, when this state beagn*/
	gettimeofday(&tv, NULL);
	dgram_conn->state[UDP_SERVER_STATE_RESPOND].tv_state_began.tv_sec = tv.tv_sec;
	dgram_conn->state[UDP_SERVER_STATE_RESPOND].tv_state_began.tv_usec = tv.tv_usec;

	/* Set up callback function for system commands */
	dgram_conn->state[UDP_SERVER_STATE_RESPOND].CONFIRM_R_cb = vs_RESPOND_CONFIRM_R_cb;
	dgram_conn->state[UDP_SERVER_STATE_RESPOND].CONFIRM_L_cb = vs_RESPOND_CONFIRM_L_cb;

	/* Store address of peer to VConnection, when we are sure, that packet
	 * received from peer is probably verse packet */
	memcpy(&(dgram_conn->peer_address), &io_ctx->peer_addr, sizeof(VNetworkAddress));

	/* Set up peer ID */
	dgram_conn->peer_id = r_packet->header.payload_id;

	/* When unsecured connection is used, then "connect" server to the client
	 * after first phase of handshake */
	if(!(dgram_conn->flags & SOCKET_SECURED)) {
		/* When handshake is finished, then do connect */
		if(io_ctx->peer_addr.ip_ver == IPV4) {
			ret = connect(io_ctx->sockfd, (struct sockaddr*)&io_ctx->peer_addr.addr.ipv4, sizeof(io_ctx->peer_addr.addr.ipv4));
		} else if(io_ctx->peer_addr.ip_ver == IPV6) {
			ret = connect(io_ctx->sockfd, (struct sockaddr*)&io_ctx->peer_addr.addr.ipv6, sizeof(io_ctx->peer_addr.addr.ipv6));
		}

		if(ret == -1) {
			v_print_log(VRS_PRINT_ERROR, "socket(): %s\n", strerror(errno));
		}

		dgram_conn->io_ctx.flags |= SOCKET_CONNECTED;
	}

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Connection %d state: RESPOND\n", dgram_conn->host_id);
		printf("%c[%dm", 27, 0);
	}
}

/* This packet is send during handshake in RESPOND and PARTOPEN state */
static void vs_RESPOND_init_send_packet(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	struct VPacket *s_packet = CTX_s_packet(C);

	/* Verse packet header */
	s_packet->header.version = 1;
	s_packet->header.flags = PAY_FLAG | ACK_FLAG;
	s_packet->header.window = dgram_conn->rwin_host >> dgram_conn->rwin_host_scale;
	s_packet->header.payload_id = dgram_conn->host_id+1;
	s_packet->header.ack_nak_id = 1;
	if(r_packet->sys_cmd[0].cmd.id == CMD_ACK_ID) {
		s_packet->header.flags |= ANK_FLAG;
		s_packet->header.ank_id = r_packet->sys_cmd[0].ack_cmd.pay_id;
		dgram_conn->ank_id = r_packet->sys_cmd[0].ack_cmd.pay_id;
	} else {
		s_packet->header.ank_id = 0;
	}

	/* System commands */
	s_packet->sys_cmd[0].ack_cmd.id = CMD_ACK_ID;
	s_packet->sys_cmd[0].ack_cmd.pay_id = r_packet->header.payload_id;

	s_packet->sys_cmd[1].cmd.id = CMD_RESERVED_ID;
}

/* Handle received packet, when server is in the RESPOND state */
static int vs_RESPOND_handle_packet(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);

	if(dgram_conn->host_state != UDP_SERVER_STATE_RESPOND) {
		v_print_log(VRS_PRINT_WARNING, "Bad call of function: %s()\n", __FUNCTION__);
		return RECEIVE_PACKET_ERROR;
	}

	/* Still handle packets from client which sent packet in REQUEST state */
	if( r_packet->header.flags==(PAY_FLAG|SYN_FLAG) &&
			(r_packet->header.payload_id == dgram_conn->peer_id)) {
		if(dgram_conn->state[UDP_SERVER_STATE_LISTEN].attempts < vs_ctx->max_connection_attempts) {
			/* Increment counter of connection attempts */
			dgram_conn->state[UDP_SERVER_STATE_LISTEN].attempts++;

			/* Call callback functions for system commands (fill C->s_packet
			 * according C->r_packet) */
			if(v_conn_dgram_handle_sys_cmds(C, UDP_SERVER_STATE_LISTEN)!=1) {
				return RECEIVE_PACKET_CORRUPTED;
			}

			/* Still response on the client requests to create connection,
			 * because previous PAY SYN packet could be lost or delayed. */
			vs_LISTEN_init_send_packet(C);

			/* Send reply packet to the client */
			vs_send_packet(C);
		} else {
			if(is_log_level(VRS_PRINT_DEBUG_MSG))
				v_print_log(VRS_PRINT_DEBUG_MSG, "Too much connection attempts: %d\n",
					dgram_conn->state[UDP_SERVER_STATE_LISTEN].attempts);
			return RECEIVE_PACKET_ATTEMPTS_EXCEED;
		}

	} else if( (r_packet->header.flags==(PAY_FLAG|ACK_FLAG|ANK_FLAG)) &&
			(r_packet->header.payload_id==dgram_conn->peer_id+1) &&
			(r_packet->header.ank_id == dgram_conn->peer_id) &&
			(r_packet->sys_cmd[0].cmd.id == CMD_ACK_ID) &&
			(r_packet->sys_cmd[0].ack_cmd.pay_id==dgram_conn->host_id) ) {

		/* It is first valid packet received from client in RESPOND state */
		dgram_conn->state[UDP_SERVER_STATE_RESPOND].attempts = 1;

		/* Initialize header of verse packet, add ack system command
		 * to the packet and add other system commands according verse
		 * server configuration */
		vs_RESPOND_init_send_packet(C);

		/* Call callback functions for system commands */
		if(v_conn_dgram_handle_sys_cmds(C, UDP_SERVER_STATE_RESPOND) != 1) {
			return RECEIVE_PACKET_CORRUPTED;
		}

		/* Send reply packet to the client */
		vs_send_packet(C);

		/* Init OPEN state */
		vs_OPEN_init(C);
	} else {
		return RECEIVE_PACKET_CORRUPTED;
	}

	return RECEIVE_PACKET_SUCCESS;
}

/************************************** OPEN state **************************************/

static void vs_OPEN_init(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	struct Ack_Nak_Cmd ack_cmd;
	struct timeval tv;
	int ret;

	/* Server is in OPEN state now */
	dgram_conn->host_state = UDP_SERVER_STATE_OPEN;

	/* Set up time, when this state beagn*/
	gettimeofday(&tv, NULL);
	dgram_conn->state[UDP_SERVER_STATE_OPEN].tv_state_began.tv_sec = tv.tv_sec;
	dgram_conn->state[UDP_SERVER_STATE_OPEN].tv_state_began.tv_usec = tv.tv_usec;

#ifdef WITH_OPENSSL
	/* Try to get MTU from the bio */
	ret = BIO_ctrl(dgram_conn->io_ctx.bio, BIO_CTRL_DGRAM_QUERY_MTU, 0, NULL);
	if(ret > 0) {
		dgram_conn->io_ctx.mtu = ret;
		v_print_log(VRS_PRINT_DEBUG_MSG, "PMTU: %d\n", dgram_conn->io_ctx.mtu);
	} else {
		dgram_conn->io_ctx.mtu = DEFAULT_MTU;
		v_print_log(VRS_PRINT_DEBUG_MSG, "Default MTU: %d\n", dgram_conn->io_ctx.mtu);
	}
#else
	(void)ret;
	dgram_conn->io_ctx.mtu = DEFAULT_MTU;
#endif

	/* Add ACK command to the list of ACK NAK commands to be send to the peer */
	ack_cmd.id = CMD_ACK_ID;
	ack_cmd.pay_id = r_packet->header.payload_id;
	v_ack_nak_history_add_cmd(&dgram_conn->ack_nak, &ack_cmd);

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Connection %d state: OPEN\n", dgram_conn->host_id);
		printf("%c[%dm", 27, 0);
	}
}

static int vs_OPEN_CLOSEREQ_send_packet(struct vContext *C)
{
	int ret;

	/* Send as much packets as needed or possible */
	do {
		ret = send_packet_in_OPEN_CLOSEREQ_state(C);
	} while (!(ret == SEND_PACKET_CANCELED || ret == SEND_PACKET_SUCCESS));
	return ret;
}

/* Handle received packet, when server is in the OPEN or CLOSEREQ state */
static int vs_OPEN_CLOSEREQ_handle_packet(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);

	if(!(dgram_conn->host_state == UDP_SERVER_STATE_OPEN || dgram_conn->host_state == UDP_SERVER_STATE_CLOSEREQ)) {
		v_print_log(VRS_PRINT_WARNING, "Bad call of function: %s()\n", __FUNCTION__);
		return 0;
	}

	/* Still handle packet from client which sent packet in RESPOND state */
	if( (r_packet->header.flags==(PAY_FLAG|ACK_FLAG|ANK_FLAG)) &&
			(r_packet->header.payload_id == dgram_conn->peer_id+1) &&
			(r_packet->header.ank_id == dgram_conn->peer_id) &&
			(r_packet->sys_cmd[0].cmd.id == CMD_ACK_ID) &&
			(r_packet->sys_cmd[0].ack_cmd.pay_id==dgram_conn->host_id) )
	{
		if(dgram_conn->state[UDP_SERVER_STATE_RESPOND].attempts < vs_ctx->max_connection_attempts) {

			dgram_conn->state[UDP_SERVER_STATE_RESPOND].attempts++;

			/* Initialize header of verse packet, add ack system command
			 * to the packet and add other system commands according verse
			 * server configuration */
			vs_RESPOND_init_send_packet(C);

			/* Call callback functions for system commands */
			if(v_conn_dgram_handle_sys_cmds(C, UDP_SERVER_STATE_RESPOND) != 1) {
				return RECEIVE_PACKET_CORRUPTED;
			}

			/* Send reply packet to the client */
			vs_send_packet(C);
		} else {
			if(is_log_level(VRS_PRINT_DEBUG_MSG))
				v_print_log(VRS_PRINT_DEBUG_MSG, "Too much connection attempts: %d\n",
					dgram_conn->state[UDP_SERVER_STATE_RESPOND].attempts);
			return RECEIVE_PACKET_ATTEMPTS_EXCEED;
		}
	/* Is this packet from the client in the CLOSING state? */
	} else if( (r_packet->header.flags & PAY_FLAG) &&
			(r_packet->header.flags & FIN_FLAG) ) {

		/* Go to the CLOSED state first (it is different from handshake) */
		vs_CLOSED_init(C);

		/* This is the first client attempt of teardown */
		dgram_conn->state[UDP_SERVER_STATE_CLOSED].attempts = 1;

		/* Init packet header and some system commands */
		vs_CLOSED_init_send_packet(C);

		/* Call callback functions for system commands */
		if(v_conn_dgram_handle_sys_cmds(C, UDP_SERVER_STATE_CLOSED) != 1) {
			return RECEIVE_PACKET_CORRUPTED;
		}

		/* Send reply packet to the client */
		vs_send_packet(C);
	} else {
		/* Use generic function for handling packet, because OPEN state is the same for the client and server */
		handle_packet_in_OPEN_state(C);

		/* When some payload data were received, then poke data thread */
		if( (vs_ctx != NULL) && (r_packet->data != NULL)) {
			sem_post(vs_ctx->data.sem);
		}

		/* Call callback functions for system commands */
		v_conn_dgram_handle_sys_cmds(C, UDP_SERVER_STATE_OPEN);

		/* Send reply to the client or send keep alive packet */
		vs_OPEN_CLOSEREQ_send_packet(C);
	}

	return RECEIVE_PACKET_SUCCESS;
}

/************************************** CLOSEREQ state **************************************/

static void vs_CLOSEREQ_init(struct VDgramConn *dgram_conn)
{
	struct timeval tv;

	dgram_conn->host_state = UDP_SERVER_STATE_CLOSEREQ;

	/* Set up time, when this state began */
	gettimeofday(&tv, NULL);
	dgram_conn->state[UDP_SERVER_STATE_CLOSEREQ].tv_state_began.tv_sec = tv.tv_sec;
	dgram_conn->state[UDP_SERVER_STATE_CLOSEREQ].tv_state_began.tv_usec = tv.tv_usec;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Connection %d state: CLOSEREQ\n", dgram_conn->host_id);
		printf("%c[%dm", 27, 0);
	}
}

/************************************** CLOSED state **************************************/

static void vs_CLOSED_init(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct timeval tv;

	/* Switch to CLOSED state */
	dgram_conn->host_state = UDP_SERVER_STATE_CLOSED;

	/* Set up time, when this state beagn*/
	gettimeofday(&tv, NULL);
	dgram_conn->state[UDP_SERVER_STATE_CLOSED].tv_state_began.tv_sec = tv.tv_sec;
	dgram_conn->state[UDP_SERVER_STATE_CLOSED].tv_state_began.tv_usec = tv.tv_usec;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Connection %d state: CLOSED\n", dgram_conn->host_id);
		printf("%c[%dm", 27, 0);
	}
}

static void vs_CLOSED_init_send_packet(struct vContext *C)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	struct VPacket *s_packet = CTX_s_packet(C);

	/* Verse packet header */
	s_packet->header.version = 1;
	s_packet->header.flags = PAY_FLAG | FIN_FLAG | ACK_FLAG;
	s_packet->header.window = dgram_conn->rwin_host >> dgram_conn->rwin_host_scale;
	s_packet->header.payload_id = dgram_conn->host_id + dgram_conn->count_s_pay;
	s_packet->header.ack_nak_id = dgram_conn->count_s_ack;
	s_packet->header.ank_id = 0;

	/* System commands */
	s_packet->sys_cmd[0].ack_cmd.id = CMD_ACK_ID;
	s_packet->sys_cmd[0].ack_cmd.pay_id = r_packet->header.payload_id;
	s_packet->sys_cmd[1].cmd.id = CMD_RESERVED_ID;
}

/* Handle received packet, when server is in the CLOSED state */
static int vs_CLOSED_handle_packet(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);

	if(dgram_conn->host_state != UDP_SERVER_STATE_CLOSED) {
		v_print_log(VRS_PRINT_WARNING, "Bad call of function: %s()\n", __FUNCTION__);
		return 0;
	}

	if( r_packet->header.flags & (PAY_FLAG|FIN_FLAG) ) {
		if(dgram_conn->state[UDP_SERVER_STATE_CLOSED].attempts < vs_ctx->max_connection_attempts) {

			/* Increase number of client attempts of teardown */
			dgram_conn->state[UDP_SERVER_STATE_CLOSED].attempts++;

			/* Init packet header and some system commands */
			vs_CLOSED_init_send_packet(C);

			/* Call callback functions for system commands (fill C->s_packet
			 * according C->r_packet) */
			v_conn_dgram_handle_sys_cmds(C, UDP_SERVER_STATE_CLOSED);

			/* Send reply packet to the client */
			vs_send_packet(C);
		} else {
			if(is_log_level(VRS_PRINT_DEBUG_MSG))
				v_print_log(VRS_PRINT_DEBUG_MSG, "Too much connection attempts: %d\n",
					dgram_conn->state[UDP_SERVER_STATE_CLOSED].attempts);
			return RECEIVE_PACKET_ATTEMPTS_EXCEED;
		}
	} else {
		return 0;
	}

	return 1;
}

#if (defined WITH_OPENSSL) && OPENSSL_VERSION_NUMBER>=0x10000000

int vs_dtls_generate_cookie(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len)
{
	unsigned char *buffer, result[EVP_MAX_MD_SIZE];
	unsigned int length = 0, resultlength, ret;
	union {
		struct sockaddr_storage storage;
		struct sockaddr_in		ip4;
		struct sockaddr_in6 	ip6;
	} peer;

	v_print_log(VRS_PRINT_DEBUG_MSG, "Generating cookie\n");

	/* Initialize a random secret */
	if(cookie_initialized == 0) {
		if(!RAND_bytes(cookie_secret, COOKIE_SECRET_LENGTH)) {
			v_print_log(VRS_PRINT_ERROR, "Setting random cookie secret\n");
			return 0;
		}
		cookie_initialized = 1;
	}

	/* Read peer information */
	ret = BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

	if(ret<1) {
		v_print_log(VRS_PRINT_ERROR, "Could not get peer from SSL bio: %d\n", ret);
		return 0;
	}

	/* Create buffer with peer's address and port */
	length = 0;
	if(peer.storage.ss_family == AF_INET6) {
		length += sizeof(struct in6_addr);
	} else if(peer.storage.ss_family == AF_INET) {
		length += sizeof(struct in_addr);
	}
	length += sizeof(in_port_t);
	buffer = (unsigned char*) OPENSSL_malloc(length);

	if(buffer == NULL) {
		v_print_log(VRS_PRINT_ERROR, "Not enough memory for cookie\n");
		return 0;
	}

	if(peer.storage.ss_family == AF_INET6) {
		memcpy(buffer, &peer.ip6.sin6_port, sizeof(in_port_t));
		memcpy(buffer + sizeof(in_port_t),
				&peer.ip6.sin6_addr,
				sizeof(struct in6_addr));
	} else if(peer.storage.ss_family == AF_INET) {
		memcpy(buffer, &peer.ip4.sin_port, sizeof(in_port_t));
		memcpy(buffer + sizeof(in_port_t),
				&peer.ip4.sin_addr,
				sizeof(struct in_addr));
	}

	/* Calculate HMAC of buffer using the secret */
	HMAC(EVP_sha1(), (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
	     (const unsigned char*) buffer, length, result, &resultlength);
	OPENSSL_free(buffer);

	memcpy(cookie, result, resultlength);
	*cookie_len = resultlength;

	return 1;
}

int vs_dtls_verify_cookie(SSL *ssl, unsigned char *cookie, unsigned int cookie_len)
{
	unsigned char		*buffer, result[EVP_MAX_MD_SIZE];
	unsigned int		length = 0, resultlength, ret;
	union {
		struct sockaddr_storage storage;
		struct sockaddr_in		ip4;
		struct sockaddr_in6 	ip6;
	} peer;

	v_print_log(VRS_PRINT_DEBUG_MSG, "Verifying cookie\n");

	/* If secret isn't initialized yet, the cookie can't be valid */
	if (!cookie_initialized)
		return 0;

	/* Read peer information */
	ret = BIO_dgram_get_peer(SSL_get_rbio(ssl), &peer);

	if(ret<1) {
		v_print_log(VRS_PRINT_ERROR, "Could not get peer from SSL bio: %d\n", ret);
		return 0;
	}

	/* Create buffer with peer's address and port */
	length = 0;
	if(peer.storage.ss_family == AF_INET6) { /* IPv6 */
		length += sizeof(struct in6_addr);
	} else if(peer.storage.ss_family == AF_INET) { /* IPv4 */
		length += sizeof(struct in_addr);
	}
	length += sizeof(in_port_t);
	buffer = (unsigned char*) OPENSSL_malloc(length);

	if(buffer == NULL) {
		v_print_log(VRS_PRINT_ERROR, "Not enough memory for cookie\n");
		return 0;
	}

	if(peer.storage.ss_family == AF_INET6) { /* IPv6 */
		memcpy(buffer, &peer.ip6.sin6_port, sizeof(in_port_t));
		memcpy(buffer + sizeof(in_port_t),
				&peer.ip6.sin6_addr,
				sizeof(struct in6_addr));
	} else if(peer.storage.ss_family == AF_INET) { /* IPv4 */
		memcpy(buffer, &peer.ip4.sin_port, sizeof(in_port_t));
		memcpy(buffer + sizeof(in_port_t),
				&peer.ip4.sin_addr,
				sizeof(struct in_addr));
	}

	/* Calculate HMAC of buffer using the secret */
	HMAC(EVP_sha1(), (const void*) cookie_secret, COOKIE_SECRET_LENGTH,
	     (const unsigned char*) buffer, length, result, &resultlength);
	OPENSSL_free(buffer);

	if(cookie_len == resultlength && memcmp(result, cookie, resultlength) == 0)
		return 1;

	return 0;
}

static void vs_destroy_dtls_connection(struct vContext *C)
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

static int vs_init_dtls_connection(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct timeval timeout;

	/* Set and activate timeouts */
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	BIO_ctrl(io_ctx->bio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
	BIO_ctrl(io_ctx->bio, BIO_CTRL_DGRAM_SET_SEND_TIMEOUT, 0, &timeout);

	/* Create new SSL*/
	if( (io_ctx->ssl = SSL_new(vs_ctx->dtls_ctx)) == NULL) {
		v_print_log(VRS_PRINT_ERROR, "SSL_new\n");
		return 0;
	}


	SSL_set_bio(io_ctx->ssl, io_ctx->bio, io_ctx->bio);

	SSL_set_options(io_ctx->ssl, SSL_OP_COOKIE_EXCHANGE);

	return 1;
}

#endif

/* Initialize verse server context */
static int vs_init_dgram_ctx(struct vContext *C)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	int flag;

	/* "Address" of server */
	if(dgram_conn->io_ctx.host_addr.ip_ver == IPV4) {		/* IPv4 */

		/* Update type of transport protocol */
		dgram_conn->io_ctx.host_addr.protocol = UDP;

		/* Create socket which server uses for listening for new connections */
		if ( (dgram_conn->io_ctx.sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 ) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "socket(): %s\n", strerror(errno));
			return -1;
		}

		/* Set socket non-blocking */
		flag = fcntl(dgram_conn->io_ctx.sockfd, F_GETFL, 0);
		if( (fcntl(dgram_conn->io_ctx.sockfd, F_SETFL, flag | O_NONBLOCK)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
			return -1;
		}

		/* Set socket to reuse addresses */
		flag = 1;
		setsockopt(dgram_conn->io_ctx.sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*) &flag, (socklen_t) sizeof(flag));

		/* Set up server address and port */
		memset(&dgram_conn->io_ctx.host_addr.addr.ipv4, 0, sizeof(struct sockaddr_in));
		dgram_conn->io_ctx.host_addr.addr.ipv4.sin_family = AF_INET;
		/* Server accept connection from any address */
		dgram_conn->io_ctx.host_addr.addr.ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
		/* Use negotiated free port */
		dgram_conn->io_ctx.host_addr.addr.ipv4.sin_port = htons(vsession->dgram_conn->io_ctx.host_addr.port);

		/* Bind address and socket */
		if( bind(dgram_conn->io_ctx.sockfd, (struct sockaddr*)&(dgram_conn->io_ctx.host_addr.addr.ipv4), sizeof(dgram_conn->io_ctx.host_addr.addr.ipv4)) == -1) {
			v_print_log(VRS_PRINT_ERROR, "bind(): %s\n", strerror(errno));
			return -1;
		}
	}
	else if(dgram_conn->io_ctx.host_addr.ip_ver == IPV6) {	/* IPv6 */

		/* Update type of transport protocol */
		dgram_conn->io_ctx.host_addr.protocol = UDP;

		/* Create socket which server uses for listening for new connections */
		if ( (dgram_conn->io_ctx.sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1 ) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "socket(): %s\n", strerror(errno));
			return -1;
		}

		/* Set socket non-blocking */
		flag = fcntl(dgram_conn->io_ctx.sockfd, F_GETFL, 0);
		if( (fcntl(dgram_conn->io_ctx.sockfd, F_SETFL, flag | O_NONBLOCK)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
			return -1;
		}

		/* Set socket to reuse addresses */
		flag = 1;
		setsockopt(dgram_conn->io_ctx.sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*) &flag, (socklen_t) sizeof(flag));

		/* Set up server address and port */
		memset(&dgram_conn->io_ctx.host_addr.addr.ipv6, 0, sizeof(struct sockaddr_in6));
		dgram_conn->io_ctx.host_addr.addr.ipv6.sin6_family = AF_INET6;
		/* Server accept connection from any address */
		dgram_conn->io_ctx.host_addr.addr.ipv6.sin6_addr = in6addr_any;
		/* Use negotiated free port */
		dgram_conn->io_ctx.host_addr.addr.ipv6.sin6_port = htons(vsession->dgram_conn->io_ctx.host_addr.port);

		/* Bind address and socket */
		if( bind(dgram_conn->io_ctx.sockfd, (struct sockaddr*)&(dgram_conn->io_ctx.host_addr.addr.ipv6), sizeof(dgram_conn->io_ctx.host_addr.addr.ipv6)) == -1) {
			v_print_log(VRS_PRINT_ERROR, "bind(): %d\n", strerror(errno));
			return -1;
		}
	}

	/* Set up flag for IO_CTX of server */
	dgram_conn->io_ctx.flags = 0;

	/* Set all bytes of buffer for incoming packet to zero */
	memset(dgram_conn->io_ctx.buf, 0, MAX_PACKET_SIZE);

#ifdef WITH_OPENSSL
	/* Create BIO */
	if((dgram_conn->io_ctx.bio = BIO_new_dgram(dgram_conn->io_ctx.sockfd, BIO_NOCLOSE)) == NULL) {
		v_print_log(VRS_PRINT_ERROR, "BIO_new_dgram\n");
		return 0;
	}

	/* Try to do MTU discover */
	if( BIO_ctrl(dgram_conn->io_ctx.bio, BIO_CTRL_DGRAM_MTU_DISCOVER, 0, NULL) < 0) {
		v_print_log(VRS_PRINT_ERROR, "BIO_ctrl()\n");
		return 0;
	}
#endif

	/*
	v_print_addr(VRS_PRINT_DEBUG_MSG, &dgram_conn->io_ctx.host_addr);
	dgram_conn->io_ctx.mtu = BIO_ctrl(dgram_conn->io_ctx.bio, BIO_CTRL_DGRAM_GET_MTU, 0, NULL);
	v_print_log(VRS_PRINT_DEBUG_MSG, "PMTU: %d\n", dgram_conn->io_ctx.mtu);
	*/

	return 1;
}

/************************************** MAIN loop **************************************/

/**
 * \brief Main UDP thread. This thread waits for connection from client. It
 * expects connection from certain IP address and client has to send negotiated
 * cookie in its REQUEST state.
 * \param[in]	*arg	The void pointer is pointer at copy of Verse context.
 * \return		This thread does not return any usefull information now.
 */
void *vs_main_dgram_loop(void *arg)
{
	struct vContext *C = (struct vContext*)arg;
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *dgram_conn=vsession->dgram_conn;
	struct VPacket *r_packet=NULL, *s_packet=NULL;
	struct timeval tv;
	fd_set set;
	int i, j, ret, error_num;

	/* Set up datagram connection */
	CTX_current_dgram_conn_set(C, dgram_conn);

	/* Copy version of IP from Verse server context */
	dgram_conn->io_ctx.host_addr.ip_ver = vs_ctx->tcp_io_ctx.host_addr.ip_ver;
	if (vs_init_dgram_ctx(C) != 1) {
		goto end;
	}

	/* Set up IO CTX */
	CTX_io_ctx_set(C, &dgram_conn->io_ctx);

#if (defined WITH_OPENSSL) && OPENSSL_VERSION_NUMBER>=0x10000000
	if(vsession->flags & VRS_SEC_DATA_TLS) {
		if( vs_init_dtls_connection(C) == 0) {
			goto end;
		}
		/*dgram_conn->io_ctx.mtu -= 100;*/
		dgram_conn->flags |= SOCKET_SECURED;
		dgram_conn->io_ctx.flags |= SOCKET_SECURED;
	} else {
		dgram_conn->flags &= ~SOCKET_SECURED;
		dgram_conn->io_ctx.flags &= ~SOCKET_SECURED;
	}
#else
	dgram_conn->flags &= ~SOCKET_SECURED;
	dgram_conn->io_ctx.flags &= ~SOCKET_SECURED;
#endif

	/* Packet structure for receiving */
	r_packet = (struct VPacket*)calloc(1, sizeof(struct VPacket));
	CTX_r_packet_set(C, r_packet);

	/* Packet structure for sending */
	s_packet = (struct VPacket*)calloc(1, sizeof(struct VPacket));
	CTX_s_packet_set(C, s_packet);

	vs_LISTEN_init(C);

hello:
#if (defined WITH_OPENSSL) && OPENSSL_VERSION_NUMBER>=0x10000000
	/* Wait for DTLS Hello Command from client */
	if(vsession->flags & VRS_SEC_DATA_TLS) {

		v_print_log(VRS_PRINT_DEBUG_MSG, "Wait for DTLS Client Hello message\n");
		/* "Never ending" listen loop (wait for first packet to do DTLS handshake) */
		while(1) {

			gettimeofday(&tv, NULL);

			/* Check if cookie is still fresh */
			if((tv.tv_sec - vsession->peer_cookie.tv.tv_sec) > VRS_TIMEOUT) {
				v_print_log(VRS_PRINT_ERROR, "Cookie timed out\n");
				goto end;
			}

			/* Initialize set */
			FD_ZERO(&set);
			FD_SET(dgram_conn->io_ctx.sockfd, &set);

			/* Initialize time value */
			tv.tv_sec = 1;	/* Seconds */
			tv.tv_usec = 0;	/* Microseconds */

			/* Wait for event on socket sockfd */
			if( (ret = select(dgram_conn->io_ctx.sockfd+1, &set, NULL, NULL, &tv)) == -1 ) {
				if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n",  __FILE__, __FUNCTION__,  __LINE__, strerror(errno));
				break;
			}
			/* Check if the event occurred on sockfd */
			else if(ret>0 && FD_ISSET(dgram_conn->io_ctx.sockfd, &set)) {
				union {
					struct sockaddr_storage storage;
					struct sockaddr_in		ip4;
					struct sockaddr_in6 	ip6;
				} client_addr;
				unsigned int addr_len = sizeof(struct sockaddr_in6);

				ret = DTLSv1_listen(dgram_conn->io_ctx.ssl, &client_addr);

				if(ret > 0) {
					v_print_log(VRS_PRINT_DEBUG_MSG, "Received Client Hello\n");

					/* Check if the packet is from the authenticated client */
					if(client_addr.storage.ss_family == AF_INET) {
						dgram_conn->io_ctx.peer_addr.ip_ver = IPV4;
						dgram_conn->io_ctx.peer_addr.port = client_addr.ip4.sin_port;
						memcpy(&dgram_conn->io_ctx.peer_addr.addr.ipv4, &client_addr.ip4, sizeof(struct sockaddr_in));
					} else if(client_addr.storage.ss_family == AF_INET6) {
						dgram_conn->io_ctx.peer_addr.ip_ver = IPV6;
						dgram_conn->io_ctx.peer_addr.port = client_addr.ip6.sin6_port;
						memcpy(&dgram_conn->io_ctx.peer_addr.addr.ipv6, &client_addr.ip6, sizeof(struct sockaddr_in6));
					}

					if(v_compare_addr(&vsession->peer_address, &dgram_conn->io_ctx.peer_addr)==1) {
						break;
					} else {
						v_print_log(VRS_PRINT_WARNING, "Secured UDP connection negotiated for address: \n\t");
						v_print_addr(VRS_PRINT_DEBUG_MSG, &dgram_conn->io_ctx.peer_addr);
						v_print_log_simple(VRS_PRINT_DEBUG_MSG, "\n,but Client Hello was received from address: \n\t");
						v_print_addr(VRS_PRINT_DEBUG_MSG, &vsession->peer_address);
						v_print_log_simple(VRS_PRINT_DEBUG_MSG, "\n ... ignoring\n");
					}
				} else {
					/* ERR_VRS_PRINT_ERRORs_fp(stderr); */
					dgram_conn->io_ctx.buf_size = recvfrom(dgram_conn->io_ctx.sockfd, dgram_conn->io_ctx.buf, MAX_PACKET_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
					v_print_log(VRS_PRINT_WARNING, "Received corrupted packet size: %d\n", dgram_conn->io_ctx.buf_size);
				}
			}
		}

		/* UDP stack will communicate only with this address and port on this
		 * socket */
		if( dgram_conn->io_ctx.peer_addr.ip_ver == IPV4 ) {
			connect(dgram_conn->io_ctx.sockfd, (struct sockaddr*)&dgram_conn->io_ctx.peer_addr.addr.ipv4, sizeof(struct sockaddr_in));
		} else if( dgram_conn->io_ctx.host_addr.ip_ver == IPV6 ) {
			connect(dgram_conn->io_ctx.sockfd, (struct sockaddr*)&dgram_conn->io_ctx.peer_addr.addr.ipv6, sizeof(struct sockaddr_in6));
		}

		dgram_conn->io_ctx.flags |= SOCKET_CONNECTED;

again:
		/* Finish handshake */
		do {
			ret = SSL_accept(dgram_conn->io_ctx.ssl);
		} while (ret == 0);
		if(ret < 0) {
			int err = SSL_get_error(dgram_conn->io_ctx.ssl, ret);
			if(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
				goto again;
			}
			/* TODO: use v_print_log() ... ERR_VRS_PRINT_ERRORs_fp(stderr); */
			v_print_log(VRS_PRINT_DEBUG_MSG, "SSL_accept() failed: %d -> %d\n", ret, err);
			goto end;
		}

		v_print_log(VRS_PRINT_DEBUG_MSG, "DTLS handshake finished.\n");

		v_print_log(VRS_PRINT_DEBUG_MSG, "Current cipher: %s\n",
				SSL_CIPHER_get_name(SSL_get_current_cipher(dgram_conn->io_ctx.ssl)));
	}
#endif

	/* "Never ending" listen loop */
	while(1) {
		gettimeofday(&tv, NULL);

		/* Check if cookie is still fresh */
		if(dgram_conn->host_state == UDP_SERVER_STATE_LISTEN &&
				(tv.tv_sec - vsession->peer_cookie.tv.tv_sec) > VRS_TIMEOUT) {
			v_print_log(VRS_PRINT_ERROR, "Cookie timed out\n");
			goto end;
		}

		/* Initialize set */
		FD_ZERO(&set);
		FD_SET(dgram_conn->io_ctx.sockfd, &set);

		/* Compute timeout for select from negotiated FPS */
		tv.tv_sec = 1/vsession->fps_host;			/* Seconds */
		tv.tv_usec = 1000000/vsession->fps_host;	/* Microseconds */

		/* Wait for event on socket sockfd */
		if( (ret = select(dgram_conn->io_ctx.sockfd+1, &set, NULL, NULL, &tv)) == -1 ) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n",  __FILE__, __FUNCTION__,  __LINE__, strerror(errno));
			break;
		}
		/* Check if the event occurred on sockfd */
		else if(ret>0 && FD_ISSET(dgram_conn->io_ctx.sockfd, &set)) {

			/* Try to receive packet */
			ret = v_receive_packet(&dgram_conn->io_ctx, &error_num);

			/* If receiving of packet was successful, then process the packet */
			if((ret==1) && (dgram_conn->io_ctx.buf_size > 0)) {

				/* Check if the packet is from the authenticated client */
				if((dgram_conn->io_ctx.flags & SOCKET_CONNECTED) ||
						v_compare_addr(&vsession->peer_address, &dgram_conn->io_ctx.peer_addr)==1) {

					/* Get time of receiving packet */
					gettimeofday(&tv, NULL);

					/* Handle packet according state of connection */
					switch(dgram_conn->host_state) {
					case UDP_SERVER_STATE_LISTEN:
						ret = vs_handle_packet(C, vs_LISTEN_handle_packet);
						break;
					case UDP_SERVER_STATE_RESPOND:
						ret = vs_handle_packet(C, vs_RESPOND_handle_packet);
						break;
					case UDP_SERVER_STATE_OPEN:
						ret = vs_handle_packet(C, vs_OPEN_CLOSEREQ_handle_packet);
						break;
					case UDP_SERVER_STATE_CLOSEREQ:
						ret = vs_handle_packet(C, vs_OPEN_CLOSEREQ_handle_packet);
						break;
					case UDP_SERVER_STATE_CLOSED:
						ret = vs_handle_packet(C, vs_CLOSED_handle_packet);
						break;
					default:
						break;
					}

					/* Update time of last receiving payload packet for
					 * current connection */
					if(r_packet->header.flags & PAY_FLAG) {
						dgram_conn->tv_pay_recv.tv_sec = tv.tv_sec;
						dgram_conn->tv_pay_recv.tv_usec = tv.tv_usec;
					}

					/* Handle returned values */
					switch(ret) {
					case RECEIVE_PACKET_ATTEMPTS_EXCEED:
						goto hello;
						break;
					case RECEIVE_PACKET_CORRUPTED:
						if(dgram_conn->host_state == UDP_SERVER_STATE_LISTEN)
							vs_LISTEN_init(C);
						break;
					default:
						break;
					}
				} else {
					if(is_log_level(VRS_PRINT_WARNING)) {
						v_print_log(VRS_PRINT_WARNING, "Client sent packet from wrong address: ");
						v_print_addr(VRS_PRINT_WARNING, &dgram_conn->io_ctx.peer_addr);
						v_print_log_simple(VRS_PRINT_WARNING, " != ");
						v_print_addr(VRS_PRINT_WARNING, &vsession->peer_address);
						v_print_log_simple(VRS_PRINT_WARNING, "\n");
					}
				}
			} else {
				if(error_num == ECONNREFUSED) {
					v_print_log(VRS_PRINT_WARNING, "Closing connection ...\n");
					break;
				}
			}
		}

#if (defined WITH_OPENSSL) && OPENSSL_VERSION_NUMBER>=0x10000000
		/* Did client close DTLS connection? */
		if(vsession->flags & VRS_SEC_DATA_TLS) {
			if((SSL_get_shutdown(dgram_conn->io_ctx.ssl) & SSL_RECEIVED_SHUTDOWN)) {
				vs_destroy_dtls_connection(C);
				break;
			}
		}
#endif

		gettimeofday(&tv, NULL);
		if(dgram_conn->host_state != UDP_SERVER_STATE_LISTEN) {

			/* Verse server has to try to send payload packets with negotiated FPSs */
			if(dgram_conn->host_state == UDP_SERVER_STATE_OPEN) {
				vs_OPEN_CLOSEREQ_send_packet(C);
			} else {
				/* When client is too long in the handshake or teardown
				 * state, then terminate connection. */
				if((tv.tv_sec - dgram_conn->state[dgram_conn->host_state].tv_state_began.tv_sec) >= VRS_TIMEOUT) {
					v_print_log(VRS_PRINT_DEBUG_MSG, "Connection timed out\n");
					break;
				}
			}

			/* When no valid packet received from client for defined time, then consider this
			 * connection as dead and free it */
			if((tv.tv_sec - dgram_conn->tv_pay_recv.tv_sec) >= VRS_TIMEOUT) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Connection timed out\n");
				break;
			}

		}
	}

end:

	/* Free port used by datagram connection */
	for(i=vs_ctx->port_low, j=0; i<vs_ctx->port_high; i++, j++) {
		if(dgram_conn->io_ctx.host_addr.port == vs_ctx->port_list[j].port_number) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "Free port: %d\n", vs_ctx->port_list[j].port_number);
			vs_ctx->port_list[j].flag = 0;
			break;
		}
	}

	/* Clear this datagram connection for other session */
	vs_clear_vconn(dgram_conn);

	free(r_packet);
	free(s_packet);

	free(C);

	pthread_exit(NULL);

	return NULL;
}

/**
 * brief This function will switch datagram connection to CLOSEREG state
 */
void vs_close_dgram_conn(struct VDgramConn *dgram_conn)
{
	vs_CLOSEREQ_init(dgram_conn);
}
