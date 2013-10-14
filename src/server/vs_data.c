/*
 * $Id: vs_data.c 1348 2012-09-19 20:08:18Z jiri $
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

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <assert.h>
#include <errno.h>

#include "verse_types.h"

#include "vs_main.h"
#include "vs_data.h"
#include "vs_user.h"
#include "vs_node.h"
#include "vs_node_access.h"
#include "vs_link.h"
#include "vs_entity.h"
#include "vs_taggroup.h"
#include "vs_tag.h"
#include "vs_layer.h"

#include "v_common.h"
#include "v_context.h"
#include "v_list.h"
#include "v_fake_commands.h"

/**
 * \brief This function handle all received node commands
 * \param[in] *vs_ctx	The pointer at verse server context
 * \param[in] *vsession	The pointer at verse session that sent this command
 * \param[in] *cmd		The pointer at command
 */
static void vs_handle_node_cmd(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd)
{
	pthread_mutex_lock(&vs_ctx->data.mutex);
	switch(cmd->id) {
		case CMD_NODE_CREATE:
			vs_handle_node_create(vs_ctx, vsession, cmd);
			break;
		case FAKE_CMD_NODE_CREATE_ACK:
			vs_handle_node_create_ack(vs_ctx, vsession, cmd);
			break;
		case CMD_NODE_DESTROY:
			vs_handle_node_destroy(vs_ctx, vsession, cmd);
			break;
		case FAKE_CMD_NODE_DESTROY_ACK:
			vs_handle_node_destroy_ack(vs_ctx, vsession, cmd);
			break;
		case CMD_NODE_SUBSCRIBE:
			vs_handle_node_subscribe(vs_ctx, vsession, cmd);
			break;
		case CMD_NODE_UNSUBSCRIBE:
			vs_handle_node_unsubscribe(vs_ctx, vsession, cmd);
			break;
		case CMD_NODE_LINK:
			vs_handle_link_change(vs_ctx, vsession, cmd);
			break;
		case CMD_NODE_PERMISSION:
			vs_handle_node_perm(vs_ctx, vsession, cmd);
			break;
		case CMD_NODE_OWNER:
			vs_handle_node_owner(vs_ctx, vsession, cmd);
			break;
		case CMD_NODE_LOCK:
			vs_handle_node_lock(vs_ctx, vsession, cmd);
			break;
		case CMD_NODE_UNLOCK:
			vs_handle_node_unlock(vs_ctx, vsession, cmd);
			break;
		case FAKE_CMD_NODE_LOCK_ACK:
			/* TODO: handle this fake command */
			break;
		case FAKE_CMD_NODE_UNLOCK_ACK:
			/* TODO: handle this fake command */
			break;
		case CMD_NODE_PRIORITY:
			vs_handle_node_prio(vs_ctx, vsession, cmd);
			break;
		case CMD_TAGGROUP_CREATE:
			vs_handle_taggroup_create(vs_ctx, vsession, cmd);
			break;
		case FAKE_CMD_TAGGROUP_CREATE_ACK:
			vs_handle_taggroup_create_ack(vs_ctx, vsession, cmd);
			break;
		case CMD_TAGGROUP_DESTROY:
			vs_handle_taggroup_destroy(vs_ctx, vsession, cmd);
			break;
		case FAKE_CMD_TAGGROUP_DESTROY_ACK:
			vs_handle_taggroup_destroy_ack(vs_ctx, vsession, cmd);
			break;
		case CMD_TAGGROUP_SUBSCRIBE:
			vs_handle_taggroup_subscribe(vs_ctx, vsession, cmd);
			break;
		case CMD_TAGGROUP_UNSUBSCRIBE:
			vs_handle_taggroup_unsubscribe(vs_ctx, vsession, cmd);
			break;
		case CMD_TAG_CREATE:
			vs_handle_tag_create(vs_ctx, vsession, cmd);
			break;
		case FAKE_CMD_TAG_CREATE_ACK:
			vs_handle_tag_create_ack(vs_ctx, vsession, cmd);
			break;
		case CMD_TAG_DESTROY:
			vs_handle_tag_destroy(vs_ctx, vsession, cmd);
			break;
		case FAKE_CMD_TAG_DESTROY_ACK:
			vs_handle_tag_destroy_ack(vs_ctx, vsession, cmd);
			break;
		case CMD_TAG_SET_UINT8:
		case CMD_TAG_SET_VEC2_UINT8:
		case CMD_TAG_SET_VEC3_UINT8:
		case CMD_TAG_SET_VEC4_UINT8:
			vs_handle_tag_set(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_UINT8,
					cmd->id - CMD_TAG_SET_UINT8 + 1);
			break;
		case CMD_TAG_SET_UINT16:
		case CMD_TAG_SET_VEC2_UINT16:
		case CMD_TAG_SET_VEC3_UINT16:
		case CMD_TAG_SET_VEC4_UINT16:
			vs_handle_tag_set(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_UINT16,
					cmd->id - CMD_TAG_SET_UINT16 + 1);
			break;
		case CMD_TAG_SET_UINT32:
		case CMD_TAG_SET_VEC2_UINT32:
		case CMD_TAG_SET_VEC3_UINT32:
		case CMD_TAG_SET_VEC4_UINT32:
			vs_handle_tag_set(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_UINT32,
					cmd->id - CMD_TAG_SET_UINT32 + 1);
			break;
		case CMD_TAG_SET_UINT64:
		case CMD_TAG_SET_VEC2_UINT64:
		case CMD_TAG_SET_VEC3_UINT64:
		case CMD_TAG_SET_VEC4_UINT64:
			vs_handle_tag_set(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_UINT64,
					cmd->id - CMD_TAG_SET_UINT64 + 1);
			break;
		case CMD_TAG_SET_REAL16:
		case CMD_TAG_SET_VEC2_REAL16:
		case CMD_TAG_SET_VEC3_REAL16:
		case CMD_TAG_SET_VEC4_REAL16:
			vs_handle_tag_set(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_REAL16,
					cmd->id - CMD_TAG_SET_REAL16 + 1);
			break;
		case CMD_TAG_SET_REAL32:
		case CMD_TAG_SET_VEC2_REAL32:
		case CMD_TAG_SET_VEC3_REAL32:
		case CMD_TAG_SET_VEC4_REAL32:
			vs_handle_tag_set(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_REAL32,
					cmd->id - CMD_TAG_SET_REAL32 + 1);
			break;
		case CMD_TAG_SET_REAL64:
		case CMD_TAG_SET_VEC2_REAL64:
		case CMD_TAG_SET_VEC3_REAL64:
		case CMD_TAG_SET_VEC4_REAL64:
			vs_handle_tag_set(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_REAL64,
					cmd->id - CMD_TAG_SET_REAL64 + 1);
			break;
		case CMD_TAG_SET_STRING8:
			vs_handle_tag_set(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_STRING8,
					1);
			break;
		case CMD_LAYER_CREATE:
			vs_handle_layer_create(vs_ctx, vsession, cmd);
			break;
		case FAKE_CMD_LAYER_CREATE_ACK:
			vs_handle_layer_create_ack(vs_ctx, vsession, cmd);
			break;
		case CMD_LAYER_DESTROY:
			vs_handle_layer_destroy(vs_ctx, vsession, cmd);
			break;
		case FAKE_CMD_LAYER_DESTROY_ACK:
			vs_handle_layer_destroy_ack(vs_ctx, vsession, cmd);
			break;
		case CMD_LAYER_SUBSCRIBE:
			vs_handle_layer_subscribe(vs_ctx, vsession, cmd);
			break;
		case CMD_LAYER_UNSUBSCRIBE:
			vs_handle_layer_unsubscribe(vs_ctx, vsession, cmd);
			break;
		case CMD_LAYER_SET_UINT8:
		case CMD_LAYER_SET_VEC2_UINT8:
		case CMD_LAYER_SET_VEC3_UINT8:
		case CMD_LAYER_SET_VEC4_UINT8:
			vs_handle_layer_set_value(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_UINT8,
					cmd->id - CMD_LAYER_SET_UINT8 + 1);
			break;
		case CMD_LAYER_SET_UINT16:
		case CMD_LAYER_SET_VEC2_UINT16:
		case CMD_LAYER_SET_VEC3_UINT16:
		case CMD_LAYER_SET_VEC4_UINT16:
			vs_handle_layer_set_value(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_UINT16,
					cmd->id - CMD_LAYER_SET_UINT16 + 1);
			break;
		case CMD_LAYER_SET_UINT32:
		case CMD_LAYER_SET_VEC2_UINT32:
		case CMD_LAYER_SET_VEC3_UINT32:
		case CMD_LAYER_SET_VEC4_UINT32:
			vs_handle_layer_set_value(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_UINT32,
					cmd->id - CMD_LAYER_SET_UINT32 + 1);
			break;
		case CMD_LAYER_SET_UINT64:
		case CMD_LAYER_SET_VEC2_UINT64:
		case CMD_LAYER_SET_VEC3_UINT64:
		case CMD_LAYER_SET_VEC4_UINT64:
			vs_handle_layer_set_value(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_UINT64,
					cmd->id - CMD_LAYER_SET_UINT64 + 1);
			break;
		case CMD_LAYER_SET_REAL16:
		case CMD_LAYER_SET_VEC2_REAL16:
		case CMD_LAYER_SET_VEC3_REAL16:
		case CMD_LAYER_SET_VEC4_REAL16:
			vs_handle_layer_set_value(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_REAL16,
					cmd->id - CMD_LAYER_SET_REAL16 + 1);
			break;
		case CMD_LAYER_SET_REAL32:
		case CMD_LAYER_SET_VEC2_REAL32:
		case CMD_LAYER_SET_VEC3_REAL32:
		case CMD_LAYER_SET_VEC4_REAL32:
			vs_handle_layer_set_value(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_REAL32,
					cmd->id - CMD_LAYER_SET_REAL32 + 1);
			break;
		case CMD_LAYER_SET_REAL64:
		case CMD_LAYER_SET_VEC2_REAL64:
		case CMD_LAYER_SET_VEC3_REAL64:
		case CMD_LAYER_SET_VEC4_REAL64:
			vs_handle_layer_set_value(vs_ctx, vsession, cmd,
					VRS_VALUE_TYPE_REAL64,
					cmd->id - CMD_LAYER_SET_REAL64 + 1);
			break;
		case CMD_LAYER_UNSET_VALUE:
			vs_handle_layer_unset_value(vs_ctx, vsession, cmd);
			break;
		default:
			v_print_log(VRS_PRINT_WARNING, "Yet unimplemented command id: %d\n", cmd->id);
			break;
	}
	pthread_mutex_unlock(&vs_ctx->data.mutex);
}

/**
 * \brief This is function of main data thread. It waits for new data in
 * incoming queues of session, that are in OPEN/CLOSEREQ states.
 */
void *vs_data_loop(void *arg)
{
	struct VS_CTX *vs_ctx = (struct VS_CTX*)arg;
	struct Generic_Cmd *cmd;
	struct timespec ts;
	struct timeval tv;
	int i, ret = 0;

	gettimeofday(&tv, NULL);
	ts.tv_sec = tv.tv_sec + 1;
	ts.tv_nsec = 1000*tv.tv_usec;

	while(vs_ctx->state != SERVER_STATE_CLOSED) {
#ifdef __linux__
		ret = sem_timedwait(vs_ctx->data.sem, &ts);
#elif __APPLE__
        /* Fast fix */
        ret = sem_wait(vs_ctx->data.sem);
#endif
		if(ret == 0) {
			for(i=0; i<vs_ctx->max_sessions; i++) {
				if(vs_ctx->vsessions[i]->dgram_conn->host_state == UDP_SERVER_STATE_OPEN ||
						vs_ctx->vsessions[i]->stream_conn->host_state == TCP_SERVER_STATE_STREAM_OPEN)
				{
					/* Pop all data of incoming messages from queue */
					while(v_in_queue_cmd_count(vs_ctx->vsessions[i]->in_queue) > 0) {
						cmd = v_in_queue_pop(vs_ctx->vsessions[i]->in_queue);
						vs_handle_node_cmd(vs_ctx, vs_ctx->vsessions[i], cmd);
						v_cmd_destroy(&cmd);
					}
				}
			}
		} else {
			if(errno == ETIMEDOUT) {
				ts.tv_sec++;
			}
		}
	}

	v_print_log(VRS_PRINT_DEBUG_MSG, "Exiting data thread\n");

	pthread_exit(NULL);
	return NULL;
}
