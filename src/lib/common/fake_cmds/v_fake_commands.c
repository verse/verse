/*
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
#include <assert.h>

#include "v_commands.h"
#include "v_fake_commands.h"
#include "v_common.h"
#include "v_list.h"
#include "v_cmd_queue.h"

/**
 * \brief Print content of node and fake commands according command ID
 */
void v_fake_cmd_print(const unsigned char level,
		const struct Generic_Cmd *cmd)
{
	switch(cmd->id) {
	case FAKE_CMD_CONNECT_ACCEPT:
		v_fake_connect_accept_print(level, (struct Connect_Accept_Cmd*)cmd);
		break;
	case FAKE_CMD_CONNECT_TERMINATE:
		v_fake_connect_terminate_print(level, (struct Connect_Terminate_Cmd*)cmd);
		break;
	case FAKE_CMD_USER_AUTHENTICATE:
		v_fake_user_auth_print(level, (struct User_Authenticate_Cmd*)cmd);
		break;
	case FAKE_CMD_FPS:
		v_fake_fps_print(level, (struct Fps_Cmd*)cmd);
		break;
	case FAKE_CMD_NODE_CREATE_ACK:
		v_fake_node_create_ack_print(level, cmd);
		break;
	case FAKE_CMD_NODE_DESTROY_ACK:
		v_fake_node_destroy_ack_print(level, cmd);
		break;
	case FAKE_CMD_TAGGROUP_CREATE_ACK:
		v_fake_taggroup_create_ack_print(level, cmd);
		break;
	case FAKE_CMD_TAGGROUP_DESTROY_ACK:
		v_fake_taggroup_destroy_ack_print(level, cmd);
		break;
	case FAKE_CMD_TAG_CREATE_ACK:
		v_fake_tag_create_ack_print(level, cmd);
		break;
	case FAKE_CMD_TAG_DESTROY_ACK:
		v_fake_tag_destroy_ack_print(level, cmd);
		break;
	case FAKE_CMD_LAYER_CREATE_ACK:
		v_fake_layer_create_ack_print(level, cmd);
		break;
	case FAKE_CMD_LAYER_DESTROY_ACK:
		v_fake_layer_destroy_ack_print(level, cmd);
		break;
	default:
		v_print_log_simple(level, "This fake command ID: %d is not supported yet\n", cmd->id);
		break;
	}
}

/**
 * \brief This function destroy fake command.
 *
 * This function should called only from v_cmd_destroy, when cmd->id is smaller
 * than 32.
 */
void v_fake_cmd_destroy(struct Generic_Cmd **cmd)
{
	switch((*cmd)->id) {
	case FAKE_CMD_CONNECT_ACCEPT:
		v_Connect_Accept_destroy((struct Connect_Accept_Cmd**)cmd);
		break;
	case FAKE_CMD_CONNECT_TERMINATE:
		v_Connect_Terminate_destroy((struct Connect_Terminate_Cmd**)cmd);
		break;
	case FAKE_CMD_USER_AUTHENTICATE:
		v_user_auth_destroy((struct User_Authenticate_Cmd**)cmd);
		break;
	case FAKE_CMD_FPS:
		v_Fps_destroy((struct Fps_Cmd**)cmd);
		break;
	case FAKE_CMD_NODE_CREATE_ACK:
		v_fake_node_create_ack_destroy(cmd);
		break;
	case FAKE_CMD_NODE_DESTROY_ACK:
		v_fake_node_destroy_ack_destroy(cmd);
		break;
	case FAKE_CMD_TAGGROUP_CREATE_ACK:
		v_fake_taggroup_create_ack_destroy(cmd);
		break;
	case FAKE_CMD_TAGGROUP_DESTROY_ACK:
		v_fake_taggroup_destroy_ack_destroy(cmd);
		break;
	case FAKE_CMD_TAG_CREATE_ACK:
		v_tag_create_ack_destroy(cmd);
		break;
	case FAKE_CMD_TAG_DESTROY_ACK:
		v_tag_destroy_ack_destroy(cmd);
		break;
	case FAKE_CMD_LAYER_CREATE_ACK:
		v_fake_layer_create_ack_destroy(cmd);
		break;
	case FAKE_CMD_LAYER_DESTROY_ACK:
		v_fake_layer_destroy_ack_destroy(cmd);
		break;
	default:
		v_print_log(VRS_PRINT_ERROR, "This fake command id: %d could not be destroyed\n", (*cmd)->id);
		assert(0);
		break;
	}
}

/**
 * \brief This function returns size of structure for storing of commands.
 * Returned value is in bytes
 */
int v_fake_cmd_struct_size(const struct Generic_Cmd *cmd)
{
	int size = 0;

	switch(cmd->id) {
	case FAKE_CMD_CONNECT_ACCEPT:
		size = sizeof(struct Connect_Accept_Cmd);
		break;
	case FAKE_CMD_CONNECT_TERMINATE:
		size = sizeof(struct Connect_Terminate_Cmd);
		break;
	case FAKE_CMD_USER_AUTHENTICATE:
		size = sizeof(struct User_Authenticate_Cmd);
		break;
	case FAKE_CMD_FPS:
		size = sizeof(struct Fps_Cmd);
		break;
	default:
		v_print_log_simple(VRS_PRINT_WARNING, "This fake command ID: %d is not supported yet\n", cmd->id);
		assert(0);
		break;
	}

	return size;
}

struct VCommandQueue *v_fake_cmd_queue_create(uint8 id, uint8 copy_bucket, uint8 fake_cmds)
{
	struct VCommandQueue *cmd_queue = NULL;
	uint16 flag = (copy_bucket==1) ? HASH_COPY_BUCKET : 0;

	switch(id) {

		case FAKE_CMD_CONNECT_ACCEPT:		/* Fake command for client (handshake was finished) */
			if(fake_cmds==1) {
				/* Connect_Accept_Data */
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct Connect_Accept_Cmd);
				cmd_queue->flag = 0;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256  | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;

		case FAKE_CMD_CONNECT_TERMINATE:	/* Fake command for client (teardown was finished or some error occur)*/
			if(fake_cmds==1) {
				/* Connect_Terminate_Data */
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct Connect_Terminate_Cmd);
				cmd_queue->flag = 0;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256  | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;

		case FAKE_CMD_USER_AUTHENTICATE:	/* Fake command for client (authentication and negotiation finished) */
			if(fake_cmds==1) {
				/* User_Authenticate_Data */
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct User_Authenticate_Cmd);
				cmd_queue->flag = 0;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256 | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;

		case FAKE_CMD_FPS:
			if(fake_cmds==1) {
				/* Negotiation of FPS */
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct Fps_Cmd);
				cmd_queue->flag = REMOVE_HASH_DUPS;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256 | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;

		case FAKE_CMD_NODE_CREATE_ACK:
			/* Node_Create_Ack */
			if(fake_cmds==1) {
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct Node_Create_Ack_Cmd);
				cmd_queue->flag = REMOVE_HASH_DUPS;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256 | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;

		case FAKE_CMD_NODE_DESTROY_ACK:
			/* Node_Destroy_Ack */
			if(fake_cmds==1) {
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct Node_Destroy_Ack_Cmd);
				cmd_queue->flag = REMOVE_HASH_DUPS;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256 | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;
		case FAKE_CMD_TAGGROUP_CREATE_ACK:
			/* TagGroup_Create_Ack */
			if(fake_cmds==1) {
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct TagGroup_Create_Ack_Cmd);
				cmd_queue->flag = REMOVE_HASH_DUPS;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256 | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;
		case FAKE_CMD_TAGGROUP_DESTROY_ACK:
			/* TagGroup_Destroy_Ack */
			if(fake_cmds==1) {
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct TagGroup_Destroy_Ack_Cmd);
				cmd_queue->flag = REMOVE_HASH_DUPS;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256 | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;
		case FAKE_CMD_TAG_CREATE_ACK:
			if(fake_cmds==1) {
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct Tag_Create_Ack_Cmd);
				cmd_queue->flag = 0;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256  | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;
		case FAKE_CMD_TAG_DESTROY_ACK:
			if(fake_cmds==1) {
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct Tag_Destroy_Ack_Cmd);
				cmd_queue->flag = 0;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256  | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;
		case FAKE_CMD_LAYER_CREATE_ACK:
			if(fake_cmds==1) {
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct Layer_Create_Ack_Cmd);
				cmd_queue->flag = 0;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256  | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;
		case FAKE_CMD_LAYER_DESTROY_ACK:
			if(fake_cmds==1) {
				cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
				cmd_queue->item_size = sizeof(struct Layer_Destroy_Ack_Cmd);
				cmd_queue->flag = 0;
				v_hash_array_init(&cmd_queue->cmds,
						HASH_MOD_256  | flag,
						0,
						cmd_queue->item_size);
			} else {
				cmd_queue = NULL;
			}
			break;
			break;
		default:
			cmd_queue = NULL;
			break;
	}

	return cmd_queue;
}
