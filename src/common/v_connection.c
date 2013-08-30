/*
 * $Id: v_connection.c 1267 2012-07-23 19:10:28Z jiri $
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2011, Jiri Hnidek
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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include "v_connection.h"
#include "v_out_queue.h"
#include "v_common.h"
#include "v_context.h"

#include "vs_main.h"
#include "vc_main.h"

/**
 * \brief Call callback functions for all received system commands. Each state
 * could have different callback function that is stored in VConnection
 * structure.
 * \param[in]	*C			The Verse context;
 * \param[in]	state_id	The id of the current state
 * \return		The function returns 1, when all system commands were processes
 * without problem and it returns 0, when some error occurs.
 */
int v_conn_dgram_handle_sys_cmds(struct vContext *C, const unsigned short state_id)
{
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	int cmd_rank, r, ret = 1;

	/* Parse system commands (not ACK and NAK messages ... these are proccessed separately) */
	for(cmd_rank=0;
			r_packet->sys_cmd[cmd_rank].cmd.id != CMD_RESERVED_ID &&
					cmd_rank < MAX_SYSTEM_COMMAND_COUNT;
			cmd_rank++)
	{
		switch (r_packet->sys_cmd[cmd_rank].cmd.id) {
		case CMD_ACK_ID:
			break;
		case CMD_NAK_ID:
			break;
		case CMD_CHANGE_L_ID:
			if(dgram_conn->state[state_id].CHANGE_L_cb) {
				r = dgram_conn->state[state_id].CHANGE_L_cb(C, &r_packet->sys_cmd[cmd_rank].cmd);
				ret = (ret==0)?0:r;
			}
			break;
		case CMD_CHANGE_R_ID:
			if(dgram_conn->state[state_id].CHANGE_R_cb) {
				r = dgram_conn->state[state_id].CHANGE_R_cb(C, &r_packet->sys_cmd[cmd_rank].cmd);
				ret = (ret==0)?0:r;
			}
			break;
		case CMD_CONFIRM_L_ID:
			if(dgram_conn->state[state_id].CONFIRM_L_cb) {
				r = dgram_conn->state[state_id].CONFIRM_L_cb(C, &r_packet->sys_cmd[cmd_rank].cmd);
				ret = (ret==0)?0:r;
			}
			break;
		case CMD_CONFIRM_R_ID:
			if(dgram_conn->state[state_id].CONFIRM_R_cb) {
				r = dgram_conn->state[state_id].CONFIRM_R_cb(C, &r_packet->sys_cmd[cmd_rank].cmd);
				ret = (ret==0)?0:r;
			}
			break;
		default:
			/* Unknown command ID */
			if(is_log_level(VRS_PRINT_WARNING)) {
				v_print_log(VRS_PRINT_WARNING, "Unknown system command ID: %d\n", r_packet->sys_cmd[cmd_rank].cmd.id);
			}
			break;
		}
	}

	return ret;
}

/**
 * \brief This function lock connection, compare state of connection with
 * the state, unlock it.
 * \return This function returns 1, when state of connection is equal to the
 * state, otherwise it returns 0.
 */
int v_conn_dgram_cmp_state(struct VDgramConn *stream_conn, unsigned short state)
{
	int ret = 0;

	pthread_mutex_lock(&stream_conn->mutex);
	if(stream_conn->host_state == state)
		ret = 1;
	pthread_mutex_unlock(&stream_conn->mutex);

	return ret;
}

/**
 * \brief This function lock connection, change state and unlock it.
 */
void v_conn_dgram_set_state(struct VDgramConn *stream_conn, unsigned short state)
{
	pthread_mutex_lock(&stream_conn->mutex);
	stream_conn->host_state = state;
	pthread_mutex_unlock(&stream_conn->mutex);
}

/**
 * \brief Initialize datagram connection (code shared with server and client)
 */
void v_conn_dgram_init(struct VDgramConn *dgram_conn)
{
	int i;

	/* Allocate buffer for incoming packets */
	if(dgram_conn->io_ctx.buf==NULL)
		dgram_conn->io_ctx.buf = (char*)calloc(MAX_PACKET_SIZE, sizeof(char));
	else
		memset(dgram_conn->io_ctx.buf, 0, MAX_PACKET_SIZE);
	/* Reserved undefined state. State could not be set now, because this
	 * code is called at client and server too. */
	dgram_conn->host_state = 0;
	/* Negotiated Congestion Control and Flow Control methods */
	dgram_conn->fc_meth = FC_RESERVED;
	dgram_conn->cc_meth= CC_RESERVED;
	/* No flags */
	dgram_conn->flags = 0;
	/* Addresses */
	memset(&(dgram_conn->host_address), 0, sizeof(VNetworkAddress));
	memset(&(dgram_conn->peer_address), 0, sizeof(VNetworkAddress));
	/* No packet was received yet (seconds) */
	dgram_conn->tv_pay_recv.tv_sec = 0;
	dgram_conn->tv_pay_recv.tv_usec = 0;
	/* No packet was send yet (seconds) */
	dgram_conn->tv_pay_send.tv_sec = 0;
	dgram_conn->tv_pay_send.tv_usec = 0;
	/* No packet was received yet (seconds) */
	dgram_conn->tv_ack_recv.tv_sec = 0;
	dgram_conn->tv_ack_recv.tv_usec = 0;
	/* No packet was send yet (seconds) */
	dgram_conn->tv_ack_send.tv_sec = 0;
	dgram_conn->tv_ack_send.tv_usec = 0;
	/* Reset counters of sent/received packets */
	dgram_conn->last_r_pay = 0;
	dgram_conn->last_r_ack = 0;
	dgram_conn->ank_id = 0;
	dgram_conn->count_s_pay = 0;
	dgram_conn->count_s_ack = 0;
	/* Ack Ration staff */
	dgram_conn->last_acked_pay = 0;
	/* Default Smoothed RTT */
	dgram_conn->srtt = 0;
	dgram_conn->rwin_host = 0xFFFFFFFF;	/* Default value */
	dgram_conn->rwin_peer = 0xFFFFFFFF;	/* Default value */
	dgram_conn->sent_size = 0;
	dgram_conn->rwin_host_scale = 0;	/* rwin_host is >> by this value for outgoing packet */
	dgram_conn->rwin_peer_scale = 0;	/* rwin_host is << by this value for incomming packet */
	dgram_conn->cwin = 0xFFFFFFFF;		/* TODO: Congestion Control */
	/* Command compression */
	dgram_conn->host_cmd_cmpr = CMPR_RESERVED;
	dgram_conn->peer_cmd_cmpr = CMPR_RESERVED;
	/* Initialize array of ACK and NAK commands, that are sent to the peer */
	v_ack_nak_history_init(&dgram_conn->ack_nak);
	/* Initialize history of sent packets */
	v_packet_history_init(&dgram_conn->packet_history);
	/* Initialize connection mutex */
	pthread_mutex_init(&dgram_conn->mutex, NULL);
	/* Set callback function for system commands to NULL */
	for(i=0; i<STATE_COUNT; i++) {
		dgram_conn->state[i].attempts = 0;
		/* Default callback functions are NULL */
		dgram_conn->state[i].CHANGE_L_cb = NULL;
		dgram_conn->state[i].CHANGE_R_cb = NULL;
		dgram_conn->state[i].CONFIRM_L_cb = NULL;
		dgram_conn->state[i].CONFIRM_R_cb = NULL;
	}
}

/* Destroy datagram connection */
void v_conn_dgram_destroy(struct VDgramConn *dgram_conn)
{
	/* Debug print */
	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Free connection ");
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "%d ", dgram_conn->host_id);
		v_print_addr_port(VRS_PRINT_DEBUG_MSG, &dgram_conn->peer_address);
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "\n");
	}

	if(dgram_conn->io_ctx.buf) {
		free(dgram_conn->io_ctx.buf);
		dgram_conn->io_ctx.buf = NULL;
	}

	v_packet_history_destroy(&dgram_conn->packet_history);

	v_ack_nak_history_clear(&dgram_conn->ack_nak);

	close(dgram_conn->io_ctx.sockfd);
}

/* Clear datagram connection */
void v_conn_dgram_clear(struct VDgramConn *dgram_conn)
{
	/* Debug print */
	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Free connection ");
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "%d ", dgram_conn->host_id);
		v_print_addr_port(VRS_PRINT_DEBUG_MSG, &dgram_conn->peer_address);
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "\n");
	}

	BIO_vfree(dgram_conn->io_ctx.bio);
	dgram_conn->io_ctx.bio = NULL;

	close(dgram_conn->io_ctx.sockfd);

	v_conn_dgram_init(dgram_conn);
}

/**
 * \brief This function lock connection, change state and unlock it.
 */
void v_conn_stream_set_state(struct VStreamConn *stream_conn, unsigned short state)
{
	pthread_mutex_lock(&stream_conn->mutex);
	stream_conn->host_state = state;
	pthread_mutex_unlock(&stream_conn->mutex);
}

void v_conn_stream_init(struct VStreamConn *stream_conn)
{
	/* Allocate buffer for incoming packets */
	stream_conn->io_ctx.buf = (char*)calloc(MAX_PACKET_SIZE, sizeof(char));
	/* Reserved undefined state. State could not be set now, because this
	 * code is called at client and server too. */
	stream_conn->host_state = 0;
	/* Addresses */
	memset(&(stream_conn->host_address), 0, sizeof(VNetworkAddress));
	memset(&(stream_conn->peer_address), 0, sizeof(VNetworkAddress));
	/* Initialize connection mutex */
	pthread_mutex_init(&stream_conn->mutex, NULL);
	/* Initialize offsets */
	stream_conn->sent_data_offset = 0;
	stream_conn->pushed_data_offset = 0;
}

void v_conn_stream_destroy(struct VStreamConn *stream_conn)
{
	free(stream_conn->io_ctx.buf);
	stream_conn->io_ctx.buf = NULL;
}
