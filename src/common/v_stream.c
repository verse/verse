/*
 * $Id$
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2013, Jiri Hnidek
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

#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

#include "verse.h"
#include "v_stream.h"
#include "v_common.h"
#include "v_network.h"
#include "v_commands.h"
#include "v_fake_commands.h"
#include "v_session.h"


/**
 * \brief This function handle messages in STREAM OPEN state
 *
 * \param[in] *C The pointer at cpntext
 *
 * \return This function return 1, when it ends successfuly, otherwise it
 * returns 0
 */
int v_STREAM_handle_messages(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	int ret = 1, buffer_pos = 0, i;
	struct VMessage *r_message = CTX_r_message(C);
	struct VSession *vsession = CTX_current_session(C);

	/* Make sure, that buffer contains at least Verse message
	 * header. If this condition is not reached, then somebody tries
	 * to do some very bad things! .. Close this connection. */
	if(io_ctx->buf_size < VERSE_MESSAGE_HEADER_SIZE) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "received buffer too small %d\n",
				io_ctx->buf_size);
		ret = 0;
		goto end;
	}

	/* Reset content of received message */
	memset(r_message, 0, sizeof(struct VMessage));

	/* Unpack Verse message header */
	buffer_pos += v_unpack_message_header(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	/* Unpack all system commands */
	buffer_pos += v_unpack_message_system_commands(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	v_print_receive_message(C);

	/* Handle system commands */
	for(i=0;
			i<MAX_SYSTEM_COMMAND_COUNT && r_message->sys_cmd[i].cmd.id != CMD_RESERVED_ID;
			i++)
	{
		if(r_message->sys_cmd[i].cmd.id == CMD_CHANGE_L_ID &&
				r_message->sys_cmd[i].negotiate_cmd.feature == FTR_FPS &&
				r_message->sys_cmd[i].negotiate_cmd.count > 0)
		{
			vsession->fps_host = vsession->fps_peer = r_message->sys_cmd[i].negotiate_cmd.value->real32;
			vsession->tmp_flags |= SYS_CMD_NEGOTIATE_FPS;
		}

		if(r_message->sys_cmd[i].cmd.id == CMD_CONFIRM_L_ID &&
				r_message->sys_cmd[i].negotiate_cmd.feature == FTR_FPS &&
				r_message->sys_cmd[i].negotiate_cmd.count > 0)
		{
			vsession->fps_peer = r_message->sys_cmd[i].negotiate_cmd.value->real32;
		}
	}

	/* Unpack all node commands and put them to the queue of incomming commands */
	buffer_pos += v_cmd_unpack((char*)&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), vsession->in_queue);

end:
	return ret;
}

/**
 * \brief This function try to pack message that is going to be
 * sent in STREAM OPEN state
 *
 * \param[in] *C The pointer at cpntext
 *
 * \return This function return 1, when there is something to send,
 * it returns -1, when there is nothing to send and it returns 0, when
 * there is some error
 */
int v_STREAM_pack_message(struct vContext *C)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VStreamConn *conn = CTX_current_stream_conn(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VMessage *s_message = CTX_s_message(C);
	struct Generic_Cmd *cmd;
	int ret = 1, error, queue_size, buffer_pos = 0, prio_cmd_count, cmd_rank=0;
	int8 cmd_share;
	int16 prio, max_prio, min_prio;
	uint16 cmd_count, cmd_len, prio_win, swin, sent_size, tot_cmd_size;
	real32 prio_sum_high, prio_sum_low, r_prio;

	/* Is here something to send? */
	if((v_out_queue_get_count(vsession->out_queue) > 0) ||
			(vsession->tmp_flags & SYS_CMD_NEGOTIATE_FPS))
	{

		/* Get current size of data in TCP outgoing buffer */
		if( (error = ioctl(io_ctx->sockfd, SIOCOUTQ, &queue_size)) == -1 ) {
			perror("ioctl()");
			return 0;
		}

		/* Compute, how many data could be added to the TCP stack? */
		swin = conn->socket_buffer_size - queue_size;

		buffer_pos = VERSE_MESSAGE_HEADER_SIZE;

		s_message->sys_cmd[0].cmd.id = CMD_RESERVED_ID;

		/* When negotiated and used FPS is different, then pack negotiate command
		 * for FPS */
		if(vsession->fps_host != vsession->fps_peer) {
			cmd_rank += v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank,
					CMD_CHANGE_L_ID, FTR_FPS, &vsession->fps_host, NULL);
		} else {
			if(vsession->tmp_flags & SYS_CMD_NEGOTIATE_FPS) {
				cmd_rank += v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank,
						CMD_CONFIRM_L_ID, FTR_FPS, &vsession->fps_peer, NULL);
				/* Send confirmation only once for received system command */
				vsession->tmp_flags &= ~SYS_CMD_NEGOTIATE_FPS;
			}
		}

		buffer_pos += v_pack_stream_system_commands(s_message, &io_ctx->buf[buffer_pos]);

		max_prio = v_out_queue_get_max_prio(vsession->out_queue);
		min_prio = v_out_queue_get_min_prio(vsession->out_queue);

		prio_sum_high = v_out_queue_get_prio_sum_high(vsession->out_queue);
		prio_sum_low = v_out_queue_get_prio_sum_low(vsession->out_queue);

		v_print_log(VRS_PRINT_DEBUG_MSG, "Packing prio queues, cmd count: %d\n",
				v_out_queue_get_count(vsession->out_queue));

		/* Go through all priorities and pick commands from priority queues */
		for(prio = max_prio; prio >= min_prio; prio--)
		{
			prio_cmd_count = v_out_queue_get_count_prio(vsession->out_queue, prio);

			if(prio_cmd_count > 0) {

				r_prio = v_out_queue_get_prio(vsession->out_queue, prio);

				/* Compute size of buffer that could be occupied by
				 * commands from this queue */
				if(prio >= VRS_DEFAULT_PRIORITY) {
					prio_win = ((swin - buffer_pos)*r_prio)/prio_sum_high;
				} else {
					prio_win = ((swin - buffer_pos)*r_prio)/prio_sum_low;
				}

				/* Debug print */
				v_print_log(VRS_PRINT_DEBUG_MSG, "Queue: %d, count: %d, r_prio: %6.3f, prio_win: %d\n",
						prio, prio_cmd_count, r_prio, prio_win);

				/* Get total size of commands that were stored in queue (sent_size) */
				sent_size = 0;
				tot_cmd_size = 0;

				while(prio_cmd_count > 0) {
					cmd_share = 0;
					cmd_count = 0;
					cmd_len = prio_win - sent_size;

					/* Pack commands from queues with high priority to the buffer */
					cmd = v_out_queue_pop(vsession->out_queue, prio, &cmd_count, &cmd_share, &cmd_len);
					if(cmd != NULL) {

						/* Is this command fake command? */
						if(cmd->id < MIN_CMD_ID) {
							if(cmd->id == FAKE_CMD_CONNECT_TERMINATE) {
								/* TODO */
							} else if(cmd->id == FAKE_CMD_FPS) {
								struct Fps_Cmd *fps_cmd = (struct Fps_Cmd*)cmd;
								/* Change value of FPS. It will be sent in negotiate command
								 * until it is confirmed be the peer (server) */
								vsession->fps_host = fps_cmd->fps;
							}
						} else {
							buffer_pos += tot_cmd_size = v_cmd_pack(&io_ctx->buf[buffer_pos], cmd, v_cmd_size(cmd), 0);
							v_cmd_print(VRS_PRINT_DEBUG_MSG, cmd);
							sent_size += tot_cmd_size;
						}

						/* It is not neccessary to put cmd to history of sent commands,
						 * when TCP is used. */
						v_cmd_destroy(&cmd);
						prio_cmd_count--;
					} else {
						break;
					}
				}
			}
		}

		s_message->header.len = io_ctx->buf_size = buffer_pos;
		s_message->header.version = VRS_VERSION;

		/* Pack header to the beginning of the buffer */
		v_pack_message_header(s_message, io_ctx->buf);

		/* Debug print of command to be send */
		if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
			v_print_send_message(C);
		}

		ret = 1;

	}

	return ret;
}
