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

#include "verse.h"
#include "v_stream.h"
#include "v_common.h"
#include "v_network.h"
#include "v_commands.h"
#include "v_session.h"

/**
 * \brief
 */
int v_STREAM_OPEN_loop(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VMessage *r_message = CTX_r_message(C);
	struct VMessage *s_message = CTX_s_message(C);
	struct VSession *vsession = CTX_current_session(C);
	struct Generic_Cmd *cmd;
	struct timeval tv;
	fd_set set;
	int ret, error, buffer_pos = 0, prio_count;
	int8 cmd_share;
	int16 prio, max_prio, min_prio;
	uint16 cmd_count, cmd_len, prio_win, swin, sent_size, tot_cmd_size;
	real32 prio_sum_high, prio_sum_low, r_prio;

	/* "Never ending" loop */
	while(1)
	{
		FD_ZERO(&set);
		FD_SET(io_ctx->sockfd, &set);

		/* TODO: negotiate FPS */
		tv.tv_sec = 0;
		tv.tv_usec = 1000000/60;

		/* Wait for recieved data */
		if( (ret = select(io_ctx->sockfd+1, &set, NULL, NULL, &tv)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n",
					__FILE__, __FUNCTION__,  __LINE__, strerror(errno));
			goto end;
			/* Was event on the listen socket */
		} else if(ret>0 && FD_ISSET(io_ctx->sockfd, &set)) {

			/* Try to receive data through SSL connection */
			if( v_SSL_read(io_ctx, &error) <= 0 ) {
				goto end;
			}

			/* Make sure, that buffer contains at least Verse message
			 * header. If this condition is not reached, then somebody tries
			 * to do some very bad things! .. Close this connection. */
			if(io_ctx->buf_size < VERSE_MESSAGE_HEADER_SIZE) {
				if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "received buffer too small %d\n",
						io_ctx->buf_size);
				goto end;
			}

			/* Reset content of received message */
			memset(r_message, 0, sizeof(struct VMessage));

			/* Unpack Verse message header */
			buffer_pos += v_unpack_message_header(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

			/* TODO: Unpack all system commands */

			v_print_receive_message(C);

			/* Unpack all node commands and put them to the queue of incomming commands */
			v_cmd_unpack((char*)&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), vsession->in_queue);
		}

		/* Is here something to send? */
		if(v_out_queue_get_count(vsession->out_queue) > 0) {

			buffer_pos = VERSE_MESSAGE_HEADER_SIZE;

			/* TODO: send system command too */
			s_message->sys_cmd[0].cmd.id = CMD_RESERVED_ID;

			/* TODO: Compute, how many data could be added to the TCP stack? */
			swin = 10000;

			max_prio = v_out_queue_get_max_prio(vsession->out_queue);
			min_prio = v_out_queue_get_min_prio(vsession->out_queue);

			prio_sum_high = v_out_queue_get_prio_sum_high(vsession->out_queue);
			prio_sum_low = v_out_queue_get_prio_sum_low(vsession->out_queue);

			v_print_log(VRS_PRINT_DEBUG_MSG, "Packing prio queues, cmd count: %d\n",
					v_out_queue_get_count(vsession->out_queue));

			/* Go through all priorities and pick commands from priority queues */
			for(prio = max_prio; prio >= min_prio; prio--)
			{
				prio_count = v_out_queue_get_count_prio(vsession->out_queue, prio);

				if(prio_count > 0) {
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
							prio, prio_count, r_prio, prio_win);

					/* Get total size of commands that were stored in queue (sent_size) */
					tot_cmd_size = 0;

					while(prio_count > 0) {
						cmd_share = 0;
						cmd_count = 0;

						/* Pack commands from queues with high priority to the buffer */
						cmd = v_out_queue_pop(vsession->out_queue, prio, &cmd_count, &cmd_share, &cmd_len);
						buffer_pos += v_cmd_pack(&io_ctx->buf[buffer_pos], cmd, v_cmd_size(cmd), 0);
						v_cmd_print(VRS_PRINT_DEBUG_MSG, cmd);

						sent_size += tot_cmd_size;

						prio_count--;
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

			/* Send command to the client */
			if( (ret = v_SSL_write(io_ctx, &error)) <= 0) {
				/* When error is SSL_ERROR_ZERO_RETURN, then SSL connection was closed by peer */
				return 0;
			}
		}
	}
end:
	return 0;
}
