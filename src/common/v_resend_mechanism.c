/*
 * $Id: v_resend_mechanism.c 1324 2012-09-11 18:27:14Z jiri $
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS.
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

#include <string.h>
#include <assert.h>

#include "v_network.h"
#include "v_connection.h"
#include "v_common.h"
#include "v_session.h"
#include "v_fake_commands.h"
#include "v_out_queue.h"
#include "v_history.h"
#include "v_cmd_queue.h"

#include "v_resend_mechanism.h"

/**
 * \brief This function check if it is necessary to send payload packet
 */
static int check_pay_flag(struct vContext *C)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *vconn = CTX_current_dgram_conn(C);
	struct timeval tv;
	int ret = 0;

	/* When there are no payload data to send, then there is possibly need
	 * for sending keep alive packet. */
	if(v_out_queue_get_count(vsession->out_queue) > 0) {
		ret = 1;
	} else {
		long int d_timeout, d_sec_timeout, d_usec_timeout;

		/* Get current time */
		gettimeofday(&tv, NULL);

		/* When was sent last payload packet? */
		d_sec_timeout = tv.tv_sec - vconn->tv_pay_send.tv_sec;
		d_usec_timeout = tv.tv_usec - vconn->tv_pay_send.tv_usec;
		d_timeout = 1000000*d_sec_timeout + d_usec_timeout;

		/* Is it necessary to send keep alive packet? */
		if(d_timeout > RESEND_TIMEOUT) {
			ret = 2;
		}
	}

	return ret;
}

/**
 * \brief Check if it is necessary to send acknowledgment packet?
 */
static int check_ack_nak_flag(struct vContext *C)
{
	struct VDgramConn *vconn = CTX_current_dgram_conn(C);
	struct VPacket *s_packet = CTX_s_packet(C);
	struct VPacket *last_r_packet = CTX_r_packet(C);
	int ret = 0;

	/* Is it necessary to acknowledge payload packet? Several conditions
	 * has to be accomplished */

	/* There has to be some ACK and NAK commands for sending */
	if( vconn->ack_nak.count > 0 ) {
		if(
			/* Piggy packing of AckNak packet to the payload packet */
			(s_packet->header.flags & PAY_FLAG)
				||
			/* Last received packet was payload packet (not only AckNak packet) ... */
			(
					(last_r_packet->header.flags & PAY_FLAG) &&
					(last_r_packet->acked == 0)
			)
		)
		{
			last_r_packet->acked = 1;
			ret = 1;
		}
	}

	return ret;
}

/**
 * \brief This function set size of congestion window (congestion control)
 */
static void set_host_cwin(struct VDgramConn *dgram_conn)
{
	switch(dgram_conn->cc_meth) {
	case CC_NONE:
		dgram_conn->cwin = 0xFFFF;
		break;
	case CC_TCP_LIKE:
		/* TODO: implement real TCP like congestion control */
		dgram_conn->cwin = 0xFFFF;
		break;
	case CC_RESERVED:
		v_print_log(VRS_PRINT_WARNING, "CC_RESERVED should never be used\n");
		dgram_conn->cwin = 0xFFFF;
		break;
	}
}

/**
 * \brief This function set size of receive window (flow control)
 */
static void set_host_rwin(struct VDgramConn *dgram_conn)
{
	switch(dgram_conn->fc_meth) {
	case FC_NONE:
		dgram_conn->rwin_host = 0xFFFFFFFF;
		break;
	case FC_TCP_LIKE:
		/* TODO: implement real TCP like flow control */
		dgram_conn->rwin_host = 0xFFFFFFFF;
		break;
	case FC_RESERVED:
		v_print_log(VRS_PRINT_WARNING, "FC_RESERVED should never be used\n");
		dgram_conn->rwin_host = 0xFFFFFFFF;
		break;
	}
}

/**
 * \brief This function packs and compress command to the packet from one
 * priority queue.
 */
static int pack_prio_queue(struct vContext *C,
		struct VSent_Packet *sent_packet,
		int buffer_pos,
		uint8 prio,
		uint16 prio_win)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *vconn = CTX_current_dgram_conn(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct Generic_Cmd *cmd;
	int ret, last_cmd_count = 0;
	uint16 cmd_count, cmd_len, sum_len=0;
	int8 cmd_share;
	uint8  last_cmd_id = CMD_RESERVED_ID;

	while( (v_out_queue_get_count_prio(vsession->out_queue, prio) > 0) &&
			(sum_len < prio_win) &&
			(buffer_pos < vconn->io_ctx.mtu))
	{
		cmd_count = 0;
		cmd_share = 0;

		/* Compute how many commands could be compressed to the packet
		 * and compute right length of compressed commands. */
		cmd_len = ((prio_win - sum_len)<(vconn->io_ctx.mtu - buffer_pos)) ?
				(prio_win - sum_len) :
				(vconn->io_ctx.mtu - buffer_pos);

		/* Remove command from queue */
		cmd = v_out_queue_pop(vsession->out_queue, prio, &cmd_count, &cmd_share, &cmd_len);

		/* When it is not possible to pop more commands from queue, then break
		 * while loop */
		if(cmd == NULL) {
			break;
		}

		/* Is this command fake command? */
		if(cmd->id < MIN_CMD_ID) {
			if(cmd->id == FAKE_CMD_CONNECT_TERMINATE) {
				struct VS_CTX *vs_ctx = CTX_server_ctx(C);
				if(vs_ctx != NULL) {
					vconn->host_state = UDP_SERVER_STATE_CLOSEREQ;
				} else {
					vconn->host_state = UDP_CLIENT_STATE_CLOSING;
				}
			} else if(cmd->id == FAKE_CMD_FPS) {
				struct Fps_Cmd *fps_cmd = (struct Fps_Cmd*)cmd;
				/* Change value of FPS. It will be sent in negotiate command
				 * until it is confirmed be the peer (server) */
				vconn->fps_host = fps_cmd->fps;
			}
			v_cmd_destroy(&cmd);
		} else {
			if(!(buffer_pos < (vconn->io_ctx.mtu - v_cmd_size(cmd)))) {
				/* When there is not enough space for other command,
				 * then push command back to the beginning of queue. */
				v_out_queue_push_head(vsession->out_queue, prio, cmd);
				break;
			} else {

				/* When compression is not allowed, then add this command as is */
				if( vconn->host_cmd_cmpr == CMPR_NONE) {
					cmd_count = 0;
					cmd_len = v_cmd_size(cmd);
					/* Debug print */
					v_print_log(VRS_PRINT_DEBUG_MSG, "Cmd: %d, count: %d, length: %d\n",
							cmd->id, cmd_count, cmd_len);
					/* Add command to the buffer */
					buffer_pos += v_cmd_pack(&io_ctx->buf[buffer_pos], cmd, cmd_len, 0);
				} else {
					/* When command compression is allowed and was ID of command changed? */
					if( (cmd->id != last_cmd_id) || (last_cmd_count <= 0) )	{
						/* When this command is alone, then use default command size */
						if(cmd_count == 0) {
							cmd_len = v_cmd_size(cmd);
						} else {
							/* FIXME: do not recompute command length here, but do it right,
							 * when command is added to the queue */
							cmd_len = v_cmds_len(cmd, cmd_count, cmd_share, cmd_len);
						}
						/* Debug print */
						v_print_log(VRS_PRINT_DEBUG_MSG, "Cmd: %d, count: %d, length: %d\n",
								cmd->id, cmd_count, cmd_len);
						/* Add command to the buffer */
						buffer_pos += v_cmd_pack(&io_ctx->buf[buffer_pos], cmd, cmd_len, cmd_share);
						/* Set up current count of commands in the line */
						last_cmd_count = cmd_count;
						/* Update summary of commands length */
						sum_len += cmd_len;
					} else {
						buffer_pos += v_cmd_pack(&io_ctx->buf[buffer_pos], cmd, 0, cmd_share);
					}
				}

				/* Print command */
				v_cmd_print(VRS_PRINT_DEBUG_MSG, cmd);

				/* TODO: remove command alias here (layer value set/unset) */

				/* Add command to the packet history */
				ret = v_packet_history_add_cmd(&vconn->packet_history, sent_packet, cmd, prio);
				assert(ret==1);

				/* Update last command id */
				last_cmd_id = cmd->id;
				/* Decrement counter of commands in queue */
				last_cmd_count--;
			}
		}
	}

	return buffer_pos;
}

/**
 * \brief This function send packets in OPEN and CLOSEREQ state.
 */
int send_packet_in_OPEN_CLOSEREQ_state(struct vContext *C)
{
	struct VDgramConn *vconn = CTX_current_dgram_conn(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VPacket *s_packet = CTX_s_packet(C);
	struct VSent_Packet *sent_packet = NULL;
	unsigned short buffer_pos = 0;
	struct timeval tv;
	int ret, keep_alive_packet = -1, full_packet = 0;
	int error_num;
	uint16 swin, prio_win;
	int cmd_rank = 0;

	/* Verse packet header */
	s_packet->header.version = 1;

	/* Clear header flags */
	s_packet->header.flags = 0;

	/* Check if it is necessary to send payload packet */
	ret = check_pay_flag(C);
	if(ret!=0) {
		s_packet->header.flags |= PAY_FLAG;
		if(ret==2) {
			keep_alive_packet = 1;
		}
	}

	/* When server is in CLOSEREQ state, then FIN flag should be set up */
	if(vconn->host_state == UDP_SERVER_STATE_CLOSEREQ) {
		s_packet->header.flags |= FIN_FLAG;
	}

	/* Check if it is necessary to send acknowledgment of received payload
	 * packet */
	ret = check_ack_nak_flag(C);
	if(ret==1) {
		s_packet->header.flags |= ACK_FLAG;

		/* Update last acknowledged Payload packet */
		vconn->last_acked_pay = vconn->last_r_pay;

		/* Add ACK and NAK commands from the list of ACK and NAK commands to the
		 * packet (only max count of ACK and NAK commands could be added to
		 * the packet) */
		for(cmd_rank = 0; cmd_rank<vconn->ack_nak.count && cmd_rank<MAX_SYSTEM_COMMAND_COUNT; cmd_rank++) {
			s_packet->sys_cmd[cmd_rank].ack_cmd.id = vconn->ack_nak.cmds[cmd_rank].id;
			s_packet->sys_cmd[cmd_rank].ack_cmd.pay_id = vconn->ack_nak.cmds[cmd_rank].pay_id;
		}
		s_packet->sys_cmd[cmd_rank].cmd.id = CMD_RESERVED_ID;

	}

	/* If there is no need to send Payload or AckNak packet, then cancel
	 * sending of packet */
	if(! ((s_packet->header.flags & PAY_FLAG) ||
			(s_packet->header.flags & ACK_FLAG)) ) return SEND_PACKET_CANCELED;

	s_packet->header.flags |= ANK_FLAG;
	s_packet->header.ank_id = vconn->ank_id;

	set_host_rwin(vconn);
	set_host_cwin(vconn);

	s_packet->header.window = (unsigned short)(vconn->rwin_host >> vconn->rwin_host_scale);

	swin = (vconn->cwin < vconn->rwin_peer) ? vconn->cwin : vconn->rwin_peer;

	/* Set up Payload ID, when there is need to send payload packet */
	if(s_packet->header.flags & PAY_FLAG)
		s_packet->header.payload_id = vconn->host_id + vconn->count_s_pay;
	else
		s_packet->header.payload_id  = 0;

	/* Set up AckNak ID, when there are some ACK or NAK command in the packet */
	if(s_packet->header.flags & ACK_FLAG)
		s_packet->header.ack_nak_id = vconn->count_s_ack;
	else
		s_packet->header.ack_nak_id = 0;

	/* When negotiated and used FPS is different, then pack negotiate command
	 * for FPS */
	if(vconn->fps_host != vconn->fps_peer) {
		cmd_rank += v_add_negotiate_cmd(s_packet, cmd_rank,
				CMD_CHANGE_L_ID, FTR_FPS, &vconn->fps_host, NULL);
	} else {
		if(vconn->tmp_flags & SYS_CMD_NEGOTIATE_FPS) {
			cmd_rank += v_add_negotiate_cmd(s_packet, cmd_rank,
					CMD_CONFIRM_L_ID, FTR_FPS, &vconn->fps_peer, NULL);
			/* Send confirmation only once for received system command */
			vconn->tmp_flags &= ~SYS_CMD_NEGOTIATE_FPS;
		}
	}

	v_print_send_packet(C);

	/* Fill buffer */
	buffer_pos += v_pack_packet_header(s_packet, &io_ctx->buf[buffer_pos]);
	buffer_pos += v_pack_dgram_system_commands(s_packet, &io_ctx->buf[buffer_pos]);

	/* When this is not pure keep alive packet */
	if(s_packet->header.flags & PAY_FLAG) {

		sent_packet = v_packet_history_add_packet(&vconn->packet_history, s_packet->header.payload_id);

		assert(sent_packet!=NULL);

		if(keep_alive_packet != 1) {
			real32 prio_sum_high, prio_sum_low, r_prio;
			uint32 prio_count;
			int16 prio, max_prio, min_prio;

			/* Print outgoing command with green color */
			if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
				printf("%c[%d;%dm", 27, 1, 32);
			}

			max_prio = v_out_queue_get_max_prio(vsession->out_queue);
			min_prio = v_out_queue_get_min_prio(vsession->out_queue);

			prio_sum_high = v_out_queue_get_prio_sum_high(vsession->out_queue);
			prio_sum_low = v_out_queue_get_prio_sum_low(vsession->out_queue);

			v_print_log(VRS_PRINT_DEBUG_MSG, "Packing prio queues, cmd count: %d\n", v_out_queue_get_count(vsession->out_queue));

			/* Go through high priorities and pick commands from priority queues */
			for(prio = max_prio; prio >= VRS_DEFAULT_PRIORITY; prio--)
			{
				prio_count = v_out_queue_get_count_prio(vsession->out_queue, prio);

				if(prio_count > 0) {
					r_prio = v_out_queue_get_prio(vsession->out_queue, prio);

					/* Compute size of buffer that could be occupied by
					 * commands from this queue */
					prio_win = ((swin - buffer_pos)*r_prio)/prio_sum_high;

					/* Debug print */
					v_print_log(VRS_PRINT_DEBUG_MSG, "Queue: %d, count: %d, r_prio: %6.3f, prio_win: %d\n",
							prio, prio_count, r_prio, prio_win);

					/* Pack commands from queues with high priority to the buffer */
					buffer_pos = pack_prio_queue(C, sent_packet, buffer_pos, prio, prio_win);
				}
			}

			/* If packet is not full yet, then it's possible to add commands from
			 * queues with low priorities */
			if(buffer_pos < vconn->io_ctx.mtu) {
				/* Go through low priorities and pick commands from priority queues */
				for(prio = VRS_DEFAULT_PRIORITY - 1; prio >= min_prio; prio--)
				{
					prio_count = v_out_queue_get_count_prio(vsession->out_queue,prio);

					if(prio_count > 0) {
						r_prio = v_out_queue_get_prio(vsession->out_queue, prio);

						/* Compute size of buffer that could be occupied by
						 * commands from this queue */
						prio_win = ((swin - buffer_pos)*r_prio)/prio_sum_low;

						/* Debug print */
						v_print_log(VRS_PRINT_DEBUG_MSG, "Queue: %d, count: %d, r_prio: %6.3f, prio_win: %d\n",
								prio, prio_count, r_prio, prio_win);

						/* Pack commands from queues with low priority to the buffer */
						buffer_pos = pack_prio_queue(C, sent_packet, buffer_pos, prio, prio_win);
					}
				}
			}

			/* Use default color for output */
			if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
				printf("%c[%dm", 27, 0);
			}
		} else {
			if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
				printf("%c[%d;%dm", 27, 1, 32);
				v_print_log(VRS_PRINT_DEBUG_MSG, "Keep alive packet\n");
				printf("%c[%dm", 27, 0);
			}
		}
	}

	io_ctx->buf_size = buffer_pos;

	/* Send buffer */
	ret = v_send_packet(io_ctx, &error_num);

	if(ret==SEND_PACKET_SUCCESS) {
		gettimeofday(&tv, NULL);

		/* Update time of sending last payload packet */
		if(s_packet->header.flags & PAY_FLAG) {
			vconn->tv_pay_send.tv_sec = tv.tv_sec;
			vconn->tv_pay_send.tv_usec = tv.tv_usec;
			/* Store time of sending packet in history of sent packets. It is
			 * used for computing RTT and SRTT */
			if(sent_packet != NULL) {
				sent_packet->tv.tv_sec = tv.tv_sec;
				sent_packet->tv.tv_usec = tv.tv_usec;
			}
		}

		/* Update time of sending last acknowledgment packet */
		if(s_packet->header.flags & ACK_FLAG) {
			vconn->tv_ack_send.tv_sec = tv.tv_sec;
			vconn->tv_ack_send.tv_usec = tv.tv_usec;
		}

		/* Update counter of sent packets */
		if(s_packet->header.flags & PAY_FLAG) vconn->count_s_pay++;
		if(s_packet->header.flags & ACK_FLAG) vconn->count_s_ack++;

		/* If the packet was sent full and there are some pending data to send
		 * then modify returned value*/
		if(full_packet == 1) {
			ret = SEND_PACKET_FULL;
		}
	} else {
		/* When packet wasn't sent, then remove this packet from history */
		if(sent_packet != NULL) {
			v_packet_history_rem_packet(C, s_packet->header.payload_id);
		}
	}

	/*v_print_packet_history(&vconn->packet_history);*/

	return ret;
}

/**
 * \brief This function returns RTT of received packet in microseconds.
 */
static unsigned long int packet_rtt(struct VSent_Packet *sent_packet,
		struct timeval *tv)
{
	unsigned long int rtt, tv_start, tv_end;

	tv_start = sent_packet->tv.tv_sec*1000000 + sent_packet->tv.tv_usec;
	tv_end = tv->tv_sec*1000000 + tv->tv_usec;
	rtt = tv_end - tv_start;

	return rtt;
}

/**
 * \brief This function is called, when acknowledgment packet was received.
 *
 * This function processes all ACK and NAK commands and it add not obsolete
 * commands from the history of sent packets to the out queue again. This
 * function also removes positively and negatively acknowledged packets from
 * history of sent packets. ANK ID is updated.
 *
 * \param[in] *C	The verse context.
 *
 * \return	This function returns index of last ACK command in sequence of
 * system command, when ACK and NAK commands are the beginning of system
 * commands, otherwise it returns -2. When no ACK or NAK command was found,
 * then -1 is returned;
 */
static int handle_ack_nak_commands(struct vContext *C)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *vconn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	struct VSent_Packet *sent_packet;
	struct VSent_Command *sent_cmd, *sent_cmd_prev;
	unsigned long int rtt = ULONG_MAX;
	struct timeval tv;
	uint32 ack_id, nak_id;
	int i, ret=-1;

	gettimeofday(&tv, NULL);

	/* Compute SRTT */
	if(r_packet->sys_cmd[0].cmd.id==CMD_ACK_ID) {
		unsigned long int tmp;
		int i=0;

		/* Try to find the smallest RTT from acknowledged packets */
		for(i=0; r_packet->sys_cmd[i].cmd.id!=CMD_RESERVED_ID; i++) {
			if(r_packet->sys_cmd[i].cmd.id==CMD_ACK_ID) {
				sent_packet = v_packet_history_find_packet(&vconn->packet_history,
					r_packet->sys_cmd[i].ack_cmd.pay_id);
				if(sent_packet!=NULL) {
					tmp = packet_rtt(sent_packet, &tv);
					if(tmp<rtt) rtt=tmp;
				}
			}
		}

		if(rtt<ULONG_MAX) {
			/* Computation of SRTT as described in RFC */
			if(vconn->srtt==0) {
				vconn->srtt = rtt;
			} else {
				vconn->srtt = RTT_ALPHA*vconn->srtt + (1-RTT_ALPHA)*rtt;
			}
			v_print_log(VRS_PRINT_DEBUG_MSG, "RTT: %d [us]\n", rtt);
			v_print_log(VRS_PRINT_DEBUG_MSG, "SRTT: %d [us]\n", vconn->srtt);
		}
	}


	/* Process all ACK and NAK commands. ACK and NAK commands should be first
	 * and there should not be other system commands between ACK and NAK
	 * commands. */
	for(i=0; r_packet->sys_cmd[i].cmd.id==CMD_NAK_ID || r_packet->sys_cmd[i].cmd.id==CMD_ACK_ID; i++) {

		if(r_packet->sys_cmd[i].cmd.id==CMD_ACK_ID) {
			/* Check if ACK and NAK commands are the first system commands */
			if(ret!=-2 && ret==i-1) {
				ret = i;
			} else {
				ret = -2;
			}
			/* If this is not the last ACK command in the sequence of
			 * ACK/NAK commands, then remove all packets from history of
			 * sent packet, that are in following sub-sequence of ACK
			 * commands */
			if(r_packet->sys_cmd[i+1].cmd.id==CMD_NAK_ID || r_packet->sys_cmd[i+1].cmd.id==CMD_ACK_ID) {
				/* Remove all acknowledged payload packets from the history
				 * of sent payload packets */
				for(ack_id = r_packet->sys_cmd[i].ack_cmd.pay_id;
						ack_id < r_packet->sys_cmd[i+1].nak_cmd.pay_id;
						ack_id++)
				{
					v_packet_history_rem_packet(C, ack_id);
				}
			} else {
				/* Remove this acknowledged payload packets from the history
				 * of sent payload packets */
				v_packet_history_rem_packet(C, r_packet->sys_cmd[i].ack_cmd.pay_id);
				/* This is the last ACK command in the sequence of ACK/NAK
				 * commands. Update ANK ID. */
				vconn->ank_id = r_packet->sys_cmd[i].ack_cmd.pay_id;
			}
		} else if(r_packet->sys_cmd[i].cmd.id==CMD_NAK_ID) {
			/* Check if ACK and NAK commands are the first system commands */
			if(ret!=-2 && ret==i-1) {
				ret = i;
			} else {
				ret = -2;
			}
			/* Go through the sub-sequence of NAk commands and try to re-send
			 * not-obsolete data from these packets */
			for(nak_id = r_packet->sys_cmd[i].nak_cmd.pay_id;
					nak_id < r_packet->sys_cmd[i+1].ack_cmd.pay_id;
					nak_id++)
			{
				/* Add not obsolete data of lost packet to the outgoing queue */
				/* Update ANK ID */
				sent_packet = v_packet_history_find_packet(&vconn->packet_history, nak_id);
				if(sent_packet != NULL) {
					v_print_log(VRS_PRINT_DEBUG_MSG, "Try to re-send packet: %d\n", nak_id);
					sent_cmd = sent_packet->cmds.last;

					/* Go through all commands in command list and add not
					 * obsolete commands to the outgoing queue */
					while(sent_cmd != NULL) {
						sent_cmd_prev = sent_cmd->prev;
						if(sent_cmd->vbucket != NULL && sent_cmd->vbucket->data != NULL) {

							/* Try to add command back to the outgoing command queue */
							if(v_out_queue_push_head(vsession->out_queue,
									sent_cmd->prio,
									(struct Generic_Cmd*)sent_cmd->vbucket->data) == 1)
							{
								/* Remove bucket from the history of sent commands too */
								v_hash_array_remove_item(&vconn->packet_history.cmd_hist[sent_cmd->id]->cmds, sent_cmd->vbucket->data);

								/* When command was added back to the queue,
								 * then delete only sent command */
								v_list_free_item(&sent_packet->cmds, sent_cmd);

							}
						}
						sent_cmd = sent_cmd_prev;
					}

					/* When all not obsolete commands are added to outgoing
					 * queue, then this packet could be removed from packet
					 * history*/
					v_packet_history_rem_packet(C, nak_id);
				}
			}
		}
	}

	return ret;
}

/**
 * \brief This function handles node commands, when payload packet was received.
 *
 * This function also detects packet loss of previous not received payload
 * packets. All node commands are unpacked from the buffer and added to incoming queue.
 *
 * \param[in]	*C	The verse context.
 *
 * \return		This function returns RECEIVE_PACKET_DELAYED, when delayd packet
 * was received, otherwise it returns RECEIVE_PACKET_SUCCESS.
 */
static int handle_node_commands(struct vContext *C)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VDgramConn *vconn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	struct Ack_Nak_Cmd ack_nak_cmd;

	/* Note: when ACK and NAK command are add to the AckNak history,
	 * then this vector of commands is automatically compressed. */

	/* Was any packet lost since last receiving of packet? */
	if(r_packet->header.payload_id > vconn->last_r_pay+1) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Packet(s) lost: %d - %d\n",
				vconn->last_r_pay+1, r_packet->header.payload_id-1);
		/* Add NAK command to the list of ACK NAK commands */
		ack_nak_cmd.id = CMD_NAK_ID;
		ack_nak_cmd.pay_id = vconn->last_r_pay+1;
		v_ack_nak_history_add_cmd(&vconn->ack_nak, &ack_nak_cmd);
	/* Was some delayed packet received? */
	} else if(r_packet->header.payload_id < vconn->last_r_pay+1) {
		if(is_log_level(VRS_PRINT_WARNING))
			v_print_log(VRS_PRINT_WARNING, "Received unordered packet: %d, expected: %d\n", r_packet->header.payload_id, vconn->last_r_pay+1);
		/* Drop this packet */
		return RECEIVE_PACKET_UNORDERED;
	}

	/* ADD ACK command to the list of ACK NAK commands */
	ack_nak_cmd.id = CMD_ACK_ID;
	ack_nak_cmd.pay_id = r_packet->header.payload_id;
	v_ack_nak_history_add_cmd(&vconn->ack_nak, &ack_nak_cmd);

	/* Check if there are really node commands */
	if(r_packet->data!=NULL) {
		/* Unpack node commands and put them to the queue of incoming commands */
		v_cmd_unpack((char*)r_packet->data, r_packet->data_size, vsession->in_queue);
	}

	return RECEIVE_PACKET_SUCCESS;
}

/**
 * \brief This command handle received packets in OPEN state at client and
 * server in the same way.
 */
int handle_packet_in_OPEN_state(struct vContext *C)
{
	struct VDgramConn *vconn = CTX_current_dgram_conn(C);
	struct VPacket *r_packet = CTX_r_packet(C);
	int ret, first_sys_index, i;

	/* Does packet contains node commands? */
	if(r_packet->header.flags & PAY_FLAG) {
		ret = handle_node_commands(C);
		if(ret == RECEIVE_PACKET_UNORDERED)
			return ret;
		r_packet->acked=0;
	}

	/* Compute RWIN */
	vconn->rwin_peer = r_packet->header.window << vconn->rwin_peer_scale;

	/* Was packet received with any ACK or NAK command? */
	if(r_packet->header.flags & ACK_FLAG) {
		ret = handle_ack_nak_commands(C);
	}

	/* Handle other system commands */
	if(ret>=0) {
		first_sys_index = ret;
	} else {
		first_sys_index = 0;
	}
	for(i=first_sys_index;
			i<MAX_SYSTEM_COMMAND_COUNT && r_packet->sys_cmd[i].cmd.id != CMD_RESERVED_ID;
			i++)
	{
		if(r_packet->sys_cmd[i].cmd.id == CMD_CHANGE_L_ID &&
				r_packet->sys_cmd[i].negotiate_cmd.feature == FTR_FPS &&
				r_packet->sys_cmd[i].negotiate_cmd.count > 0)
		{
			vconn->fps_host = vconn->fps_peer = r_packet->sys_cmd[i].negotiate_cmd.value->real32;
			vconn->tmp_flags |= SYS_CMD_NEGOTIATE_FPS;
		}

		if(r_packet->sys_cmd[i].cmd.id == CMD_CONFIRM_L_ID &&
				r_packet->sys_cmd[i].negotiate_cmd.feature == FTR_FPS &&
				r_packet->sys_cmd[i].negotiate_cmd.count > 0)
		{
			vconn->fps_peer = r_packet->sys_cmd[i].negotiate_cmd.value->real32;
		}
	}

	/* Was packet received with valid ANK ID? */
	if(r_packet->header.flags & ANK_FLAG) {
		/* Remove appropriate ACK and NAK commands form the history of ACK
		 * and NAK commands */
		v_ack_nak_history_remove_cmds(&vconn->ack_nak, r_packet->header.ank_id);
	}

	/* Does peer want to finish this connection? */
	if(r_packet->header.flags & FIN_FLAG) {
		/* Change host state. Main connection loop will do the rest. */
		vconn->host_state = UDP_CLIENT_STATE_CLOSING;
	}

	return RECEIVE_PACKET_SUCCESS;
}
