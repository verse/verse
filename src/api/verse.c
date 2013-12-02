/*
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

/** \file vc_connection.c
 * This file includes API for verse client.
 */

#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "verse.h"
#include "verse_types.h"

#include "v_common.h"
#include "v_session.h"
#include "v_fake_commands.h"
#include "v_out_queue.h"
#include "v_in_queue.h"
#include "v_commands.h"
#include "v_tag_commands.h"
#include "v_layer_commands.h"

#include "vc_main.h"
#include "vc_tcp_connect.h"

/**
 * Context for this client.
 */
static VC_CTX *vc_ctx = NULL;


static int vc_send_command(const uint8 session_id,
		const uint8 prio,
		struct Generic_Cmd *cmd);
static int vc_init_VC_CTX(void);
static void vc_call_callback_func(const uint8 session_id,
		struct Generic_Cmd *cmd);
static void *vc_new_session_thread(void *arg);


/**
 * \brief This function can set debug level of verse client.
 * \param[in]	debug_level	This parameter can have values defined in verse.h
 */
int vrs_set_debug_level(uint8_t debug_level)
{
	int ret = VRS_FAILURE;

	vc_init_VC_CTX();

	switch(debug_level) {
		case VRS_PRINT_NONE:
		case VRS_PRINT_INFO:
		case VRS_PRINT_ERROR:
		case VRS_PRINT_WARNING:
		case VRS_PRINT_DEBUG_MSG:
			vc_ctx->print_log_level = debug_level;
			ret = VRS_SUCCESS;
			break;
		default:
			v_print_log(VRS_PRINT_ERROR, "Unsupported debug level: %d", debug_level);
			break;
	}

	if(ret == VRS_SUCCESS)
		v_init_print_log(vc_ctx->print_log_level, vc_ctx->log_file);

	return ret;
}

/**
 * \brief This function can set name and version of current verse client.
 */
int vrs_set_client_info(char *name, char *version)
{
	int ret = VRS_FAILURE;

	vc_init_VC_CTX();

	if(vc_ctx->client_name == NULL && name != NULL) {
		vc_ctx->client_name = strdup(name);

		if(vc_ctx->client_version == NULL && version != NULL) {
			vc_ctx->client_version = strdup(version);
		}

		ret = VRS_SUCCESS;
	}

	return ret;
}

/**
 * \brief This function tries to negotiate new FPS with server
 *
 * Note: default FPS of client is 60 FPS. If you will try to negotiation this
 * value with server, then no command will be sent, because there will be no
 * need to change this value.
 */
int vrs_send_fps(const uint8_t session_id,
		const uint8_t prio,
		const float fps)
{
	struct Generic_Cmd *fps_cmd;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	fps_cmd = v_Fps_create(fps, tv.tv_sec, tv.tv_usec);
	return vc_send_command(session_id, prio, fps_cmd);
}


/**
 * \brief This function tries to send Node_Create command to the server.
 *
 * This function does not send this command directly, but this function
 * add Node_Create to the sending queue. Sending of this command could be
 * delayed due to congestion control or flow control. When client sends this
 * command, then this command is sent with special values: node_id==-1,
 * parent_id==avatar_id and user has to be equal of current user. Note: calling
 * of this function does not guarantee delivery of this command, because
 * connection to the server could be lost before command Node_Create is sent to
 * the server. The command is added to the priority queue.
 *
 * \param[in]	session_id	The ID of session with verse server
 * \param[in]	prio		The priority of the command.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_node_create(const uint8_t session_id,
		const uint8_t prio,
		const uint16_t type)
{
	int i;

	if(vc_ctx == NULL) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Basic callback functions were not set.\n");
		return VRS_NO_CB_FUNC;
	} else {
		/* Go through all sessions ... */
		for(i=0; i<vc_ctx->max_sessions; i++) {
			/* ... and try to find session with session_id */
			if(vc_ctx->vsessions[i]!=NULL &&
					vc_ctx->vsessions[i]->session_id==session_id)
			{
				struct Generic_Cmd *node_create_cmd = v_node_create_create(VRS_RESERVED_NODE_ID,
									vc_ctx->vsessions[i]->avatar_id,
									vc_ctx->vsessions[i]->user_id,
									type);

				assert(node_create_cmd != NULL);

				v_out_queue_push_tail(vc_ctx->vsessions[i]->out_queue,
						prio, node_create_cmd);

				return VRS_SUCCESS;
			}
		}
	}

	if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Session %d does not exist.\n", session_id);
	return VRS_FAILURE;

}


/**
 * \brief This function register callback function for command Node_Create
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	node_id		The ID of new created node
 * \param[in]	parent_id	The ID of parent of new created node
 * \param[in]	user_id		The ID of the owner of new created node
 * \param[in]	type		The client defined type of received node
 */
void vrs_register_receive_node_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t parent_id,
		const uint16_t user_id,
		const uint16_t type))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_node_create = func;
}


/**
 * \brief This function tries to send command Node_Destroy to the server.
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of the command.
 * \param[in]	node_id		The ID of node that will be destroyed.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_node_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id)
{
	struct Generic_Cmd *node_destroy_cmd  = v_node_destroy_create(node_id);
	return vc_send_command(session_id, prio, node_destroy_cmd);
}


/**
 * \brief This function register callback function for command Node_Destroy
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of destroyed node
 */
void vrs_register_receive_node_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_node_destroy = func;
}


/**
 * \brief This function tries to send command Node_Subscribe to the server.
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID that client wants to be subscribed for.
 * \param[in]	version		The requested version that client request. The client
 * has to have local copy of this version.
 * \param[in]	crc32		The crc32 of requested version.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_node_subscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32)
{
	struct Generic_Cmd *node_subscribe_cmd = v_node_subscribe_create(node_id, version, crc32);
	return vc_send_command(session_id, prio, node_subscribe_cmd);
}


/**
 * \brief This function register callback function for command Node_Subscribe
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID that client wants to be subscribed for.
 * \param[in]	version		The version of node
 * \param[in]	crc32		The CRC32 of the node
 */
void vrs_register_receive_node_subscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_node_subscribe = func;
}


/**
 * \brief This function tries to send command Node_Unsubscribe to the server.
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID of node that client wants to unsubscribe
 * \param[in]   versing	The flag that specify if versing will be requested.
 * If the versing is equal to -1 (0xFF), then versing will be requested. When the
 * versing is equal to 0 (0x00), then versing will not be requested. Other values
 * are not defined.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_node_unsubscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint8_t versing)
{
	struct Generic_Cmd *node_unsubscribe_cmd = v_node_unsubscribe_create(node_id, 0, versing);
	return vc_send_command(session_id, prio, node_unsubscribe_cmd);
}


/**
 * \brief This function register callback function for command Node_UnSubscribe
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID that client wants to be unsubscribed from.
 * \param[in]	version		The version of node
 * \param[in]	crc32		The CRC32 of the node
 */
void vrs_register_receive_node_unsubscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_node_unsubscribe = func;
}


/**
 * \brief This function tries to send command Node_Permission to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The permissions will be changed for the node with this ID
 * \param[in]	user_id			The permissions will be changed for the user with this ID
 * \param[in]	permissions		The new permissions
 * \param[in]	level			The permissions will be changed for child nodes up to this level
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_node_perm(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id,
		uint16_t user_id,
		uint8_t permissions)
{
	struct Generic_Cmd *node_perm_cmd = v_node_perm_create(node_id, user_id, permissions);
	return vc_send_command(session_id, prio, node_perm_cmd);
}


/**
 * \brief This function register callback function for command Node_Perm
 *
 * \param[in]	(*func)			The pointer at callback function
 * \param[in]	session_id		The ID of session with verse server
 * \param[in]	node_id			The permissions will be changed for the node with this ID
 * \param[in]	user_id			The permissions will be changed for the user with this ID
 * \param[in]	permissions		The new permissions
 */
void vrs_register_receive_node_perm(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint16_t user_id,
		uint8_t permissions))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_node_perm = func;
}


/**
 * \brief This function tries to send command Node_Owner to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The owner will be changed for the node with this ID
 * \param[in]	user_id			The User ID of new owner
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_node_owner(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id,
		uint16_t user_id)
{
	struct Generic_Cmd *node_owner_cmd = v_node_owner_create(node_id, user_id);
	return vc_send_command(session_id, prio, node_owner_cmd);
}


/**
 * \brief This function register callback function for command Node_Owner
 *
 * \param[in]	(*func)			The pointer at callback function
 * \param[in]	session_id		The ID of session with verse server
 * \param[in]	node_id			The owner will be changed for the node with this ID
 * \param[in]	user_id			The User ID of new owner
 *
 */
void vrs_register_receive_node_owner(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint16_t user_id))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_node_owner = func;
}


/**
 * \brief This function tries to send command Node_Lock to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			Verse server will try to lock node with this ID
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_node_lock(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id)
{
	int i;

	if(vc_ctx == NULL) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Basic callback functions were not set.\n");
		return VRS_NO_CB_FUNC;
	} else {
		/* Go through all sessions ... */
		for(i=0; i<vc_ctx->max_sessions; i++) {
			/* ... and try to find session with session_id */
			if(vc_ctx->vsessions[i]!=NULL &&
					vc_ctx->vsessions[i]->session_id==session_id)
			{
				struct Generic_Cmd *node_lock_cmd;
				node_lock_cmd = v_node_lock_create(node_id, vc_ctx->vsessions[i]->avatar_id);

				assert(node_lock_cmd != NULL);

				v_out_queue_push_tail(vc_ctx->vsessions[i]->out_queue,
						prio, node_lock_cmd);

				return VRS_SUCCESS;
			}
		}
	}

	return VRS_FAILURE;
}


/**
 * \brief This function register callback function for command Node_Lock
 *
 * \param[in]	(*func)			The pointer at callback function
 * \param[in]	session_id		The ID of session with verse server
 * \param[in]	node_id			Verse server will try to lock node with this ID
 * \param[in]	avatar_id		The ID of avatar that locked the node
 *
 */
void vrs_register_receive_node_lock(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint32_t avatar_id))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_node_lock = func;
}


/**
 * \brief This function tries to send command Node_UnLock to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			Verse server will try to unlock node with this ID
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_node_unlock(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id)
{
	int i;

	if(vc_ctx == NULL) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Basic callback functions were not set.\n");
		return VRS_NO_CB_FUNC;
	} else {
		/* Go through all sessions ... */
		for(i=0; i<vc_ctx->max_sessions; i++) {
			/* ... and try to find session with session_id */
			if(vc_ctx->vsessions[i]!=NULL &&
					vc_ctx->vsessions[i]->session_id==session_id)
			{
				struct Generic_Cmd *node_unlock_cmd;
				node_unlock_cmd = v_node_unlock_create(node_id, vc_ctx->vsessions[i]->avatar_id);

				assert(node_unlock_cmd != NULL);

				v_out_queue_push_tail(vc_ctx->vsessions[i]->out_queue,
						prio, node_unlock_cmd);

				return VRS_SUCCESS;
			}
		}
	}

	return VRS_FAILURE;
}


/**
 * \brief This function register callback function for command Node_UnLock
 *
 * \param[in]	(*func)			The pointer at callback function
 * \param[in]	session_id		The ID of session with verse server
 * \param[in]	node_id			Verse server will try to unlock node with this ID
 * \param[in]	avatar_id		The ID of avatar that unlocked the node
 *
 */
void vrs_register_receive_node_unlock(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint32_t avatar_id))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_node_unlock = func;
}


/**
 * \brief This function tries to send command Node_Priority to the server.
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID of node, where client wants to change priority
 * \param[in]	node_prio	The new priority of Node
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_node_prio(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint8_t node_prio)
{
	struct Generic_Cmd *node_prio_cmd = v_node_prio_create(node_id, node_prio);
	return vc_send_command(session_id, prio, node_prio_cmd);
}


/**
 * \brief This function tries to send command Node_Link to the server.
 *
 * When client has permission to write to child and parent node, then child node
 * will have new parent node.
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	parent_node_id	The ID of parent node
 * \param[in]	child_node_id	The ID of child node
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_node_link(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t parent_node_id,
		const uint32_t child_node_id)
{
	struct Generic_Cmd *node_link_cmd = v_node_link_create(parent_node_id, child_node_id);
	return vc_send_command(session_id, prio, node_link_cmd);
}


/**
 * \brief This function register callback function for command Node_Link
 *
 * \param[in]	(*func)			The pointer at callback function
 * \param[in]	session_id		The ID of session with verse server
 * \param[in]	parent_node_id	The ID of parent node
 * \param[in]	child_node_id	The ID of child node
 */
void vrs_register_receive_node_link(void (*func)(const uint8_t session_id,
		const uint32_t parent_node_id,
		const uint32_t child_node_id))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_node_link = func;
}


/**
 * \brief This function send command TagGroup_Create to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where new taggroup will be created
 * \param[in]	type			The client defined type of taggroup
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_taggroup_create(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t type)
{
	struct Generic_Cmd *taggroup_create_cmd = v_taggroup_create_create(node_id, -1, type);
	return vc_send_command(session_id, prio, taggroup_create_cmd);
}


/**
 * \brief This function register callback function for command TagGroup_Create
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where taggroup was created
 * \param[in]	taggroup_id	The ID of new taggroup
 * \param[in]	type		The client defined type of received taggroup
 */
void vrs_register_receive_taggroup_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t type))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_taggroup_create = func;
}


/**
 * \brief This function send command TagGroup_Destroy to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where new taggroup will be destroyed
 * \param[in]	taggroup_id		The ID of taggroup that will be destroyed
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_taggroup_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id)
{
	struct Generic_Cmd *taggroup_destroy_cmd = v_taggroup_destroy_create(node_id, taggroup_id);
	return vc_send_command(session_id, prio, taggroup_destroy_cmd);
}


/**
 * \brief This function register callback function for command TagGroup_Destroy
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where taggroup was destroyed
 * \param[in]	taggroup_id	The ID of deleted taggroup
 */
void vrs_register_receive_taggroup_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_taggroup_destroy = func;
}


/**
 * \brief This function send command TagGroup_Subscribe to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where is subscribed taggroup
 * \param[in]	taggroup_id		The ID subscribed taggroup
 * \param[in]	version			The version that client tries to subscribed for
 * \param[in]	crc32			The crc32 of subscribed version (when version is 0,
 * then the crc32 is defined as zero)
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_taggroup_subscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32)
{
	struct Generic_Cmd *taggroup_subscribe_cmd = v_taggroup_subscribe_create(node_id, taggroup_id, version, crc32);
	return vc_send_command(session_id, prio, taggroup_subscribe_cmd);
}

/**
 * \brief This function register callback function for command TagGroup_Subscribe
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where taggroup was subscribed
 * \param[in]	taggroup_id	The ID of subscribed taggroup
 * \param[in]	version		The version that client subscribed for
 * \param[in]	crc32		The crc32 of subscribed version (when version is 0,
 * then the crc32 is defined as zero)
 */
void vrs_register_receive_taggroup_subscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_taggroup_subscribe = func;
}

/**
 * \brief This function send command TagGroup_Unsubscribe to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where is unsubscribed taggroup
 * \param[in]	taggroup_id		The ID of unsubscribed taggroup
 * \param[in]   versing			The flag that specify if versioning will be requested.
 * If the versing is equal to -1 (0xFF), then versing will be requested. When the
 * versing is equal to 0 (0x00), then versing will not be requested. Other values
 * are not defined.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_taggroup_unsubscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint8_t versing)
{
	struct Generic_Cmd *taggroup_unsubscribe_cmd = v_taggroup_unsubscribe_create(node_id, taggroup_id, 0, versing);
	return vc_send_command(session_id, prio, taggroup_unsubscribe_cmd);
}

/**
 * \brief This function register callback function for command TagGroup_UnSubscribe
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where taggroup was unsubscribed
 * \param[in]	taggroup_id	The ID of unsubscribed taggroup
 * \param[in]	version		The version of unsubscibed taggroup (generated by server)
 * \param[in]	crc32		The crc32 of unsubscribed version (when version is 0,
 * then the crc32 is defined as zero)
 */
void vrs_register_receive_taggroup_unsubscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_taggroup_unsubscribe = func;
}

/**
 * \brief This function send command Tag_Create to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where new tag will be created
 * \param[in]	taggroup_id		The ID of taggroup, where new tag will be created
 * \param[in]	data_type		The type of value stored in tag
 * \param[in]	count			The number of values stored in one tag (1,2,3,4)
 * \param[in]	type			The client defined type of received tag
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_tag_create(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type)
{
	struct Generic_Cmd *tag_create_cmd = v_tag_create_create(node_id, taggroup_id, -1, data_type, count, type);
	return vc_send_command(session_id, prio, tag_create_cmd);
}


/**
 * \brief This function register callback function for command TagGroup_Create
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where tag was created
 * \param[in]	taggroup_id	The ID of taggroup, where new tag was created
 * \param[in]	tag_id		The ID of new tag
 * \param[in]	data_type	The type of value stored in tag
 * \param[in]	count		The number of values stored in one tag (1,2,3,4)
 * \param[in]	type		The client defined type of received tag
 */
void vrs_register_receive_tag_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_tag_create = func;
}


/**
 * \brief This function send command Tag_Destroy to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where new tag will be destroyed
 * \param[in]	taggroup_id		The ID of taggroup, where new tag will be destroyed
 * \param[in[	tag_id			The ID of tag that will be destroyed
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_tag_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id)
{
	struct Generic_Cmd *tag_destroy_cmd = v_tag_destroy_create(node_id, taggroup_id, tag_id);
	return vc_send_command(session_id, prio, tag_destroy_cmd);
}


/**
 * \brief This function register callback function for command Tag_Destroy
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where tag was destroyed
 * \param[in]	taggroup_id	The ID of taggroup, where tag was destroyed
 * \param[in]	tag_id		The ID of destroyed tag
 */
void vrs_register_receive_tag_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_tag_destroy = func;
}


/**
 * \brief This function sends command Tag_Set_Value to the server
 *
 * \param[in]	session_id	The ID of session with verse server
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID of node, where value of tag will be set
 * \param[in]	taggroup_id	The ID of taggroup, where value of tag will be set
 * \param[in]	tag_id		The ID of tag
 * \param[in]	type		The type of value
 * \param[in]	count		The count of values (1,2,3,4)
 * \param[in]	*value		The pointer at value(s)
 *
 *  * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_tag_set_value(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const void *value)
{
	struct Generic_Cmd *tag_set_cmd = v_tag_set_create(node_id, taggroup_id, tag_id, data_type, count, value);
	return vc_send_command(session_id, prio, tag_set_cmd);
}


/**
 * \brief This function register callback function for command Tag_Set_Value
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where value of tag was set
 * \param[in]	taggroup_id	The ID of taggroup, where value of tag was set
 * \param[in]	tag_id		The ID of tag
 * \param[in]	type		The type of value
 * \param[in]	count		The count of values (1,2,3,4)
 * \param[in]	*value		The pointer at value(s)
 */
void vrs_register_receive_tag_set_value(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const void *value))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_tag_set_value = func;
}


/**
 * \brief This function sends command layer_create to verse server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be created
 * \param[in]	parent_layer_id	The ID of parent layer (0xFFFF means no parent layer layer)
 * \param[in]	data_type		The type of value (uint8, uint16, uint32, etc.)
 * \param[in]	count			The count of values in one layer item
 * \param[in]	type			The client defined type
 *
 * \return	This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_layer_create(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t parent_layer_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type)
{
	struct Generic_Cmd *layer_create_cmd = v_layer_create_create(node_id, parent_layer_id, 0xFFFF, data_type, count, type);
	return vc_send_command(session_id, prio, layer_create_cmd);
}


/**
 * \brief This function register callback function for command Layer_Create
 */
void vrs_register_receive_layer_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t parent_layer_id,
		const uint16_t layer_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_layer_create = func;
}


/**
 * \brief This function sends command layer_destroy to verse server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be destroyed
 * \param[in]	layer_id		The ID of layer that will be destroyed
 *
 * \return	This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_layer_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id)
{
	struct Generic_Cmd *layer_destroy_cmd = v_layer_destroy_create(node_id, layer_id);
	return vc_send_command(session_id, prio, layer_destroy_cmd);
}


/**
 * \brief This function register callback function for command Layer_Destroy
 */
void vrs_register_receive_layer_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_layer_destroy = func;
}


/**
 * \brief This function sends command layer_subscribe to verse server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be destroyed
 * \param[in]	layer_id		The ID of layer that will be destroyed
 * \param[in]	version			The version that client wants to subscribe to
 * \param[in]	crc32			The CRC32 code
 *
 * \return	This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_layer_subscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32)
{
	struct Generic_Cmd *layer_subscribe_cmd = v_layer_subscribe_create(node_id, layer_id, version, crc32);
	return vc_send_command(session_id, prio, layer_subscribe_cmd);
}


/**
 * \brief This function register callback function for command Layer_Subscribe
 */
void vrs_register_receive_layer_subscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_layer_subscribe = func;
}


/**
 * \brief This function sends command layer_subscribe to verse server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be destroyed
 * \param[in]	layer_id		The ID of layer that will be destroyed
 * \param[in]	versing			The flag if client requires versing of this layer
 *
 * \return	This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_layer_unsubscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint8_t versing)
{
	struct Generic_Cmd *layer_unsubscribe_cmd = v_layer_unsubscribe_create(node_id, layer_id, 0, versing);
	return vc_send_command(session_id, prio, layer_unsubscribe_cmd);
}


/**
 * \brief This function register callback function for command Layer_UnSubscribe
 */
void vrs_register_receive_layer_unsubscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_layer_unsubscribe = func;
}


/**
 * \brief This function sets value of layer item
 */
int vrs_send_layer_set_value(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id,
		const uint8_t type,
		const uint8_t count,
		const void *value)
{
	struct Generic_Cmd *layer_set_value_cmd = v_layer_set_value_create(node_id, layer_id, item_id, type, count, value);
	return vc_send_command(session_id, prio, layer_set_value_cmd);
}


/**
 * \brief This function register callback function for command Layer_Set_Value
 */
void vrs_register_receive_layer_set_value(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id,
		const uint8_t type,
		const uint8_t count,
		const void *value))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_layer_set_value = func;
}


/**
 * \brief This function unset value in layer
 */
int vrs_send_layer_unset_value(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id)
{
	struct Generic_Cmd *layer_unset_value_cmd = v_layer_unset_value_create(node_id, layer_id, item_id);
	return vc_send_command(session_id, prio, layer_unset_value_cmd);
}


/**
 * \brief This function register callback function for command Layer_Value_Unset
 */
void vrs_register_receive_layer_unset_value(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_layer_unset_value = func;
}


/**
 * \brief This function register callback function for command Connect_Accept
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server
 * \param[in]	user_id		The ID of user account
 * \param[in]	avatar_id	The ID of avatar and avatar node
 */
void vrs_register_receive_connect_accept(void (*func)(const uint8_t session_id,
      const uint16_t user_id,
      const uint32_t avatar_id))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_connect_accept = func;
}


/**
 * \brief This function register callback function for situation, when
 * connection to verse server is closed or lost.
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	error_num	The error code
 */
void vrs_register_receive_connect_terminate(void (*func)(const uint8_t session_id,
		const uint8_t error_num))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_connect_terminate = func;
}


/**
 * \brief This function register callback function for situation, when server
 * require user authentication from the user
 *
 * \param[in]	(*func)				The pointer at callback function
 * \param[in]	session_id			The ID of session with verse server.
 * \param[in]	*username			The string with username
 * \param[in]	auth_methods_count	The number of previous authentication attempts to the server
 * \param[in]	*methods			The list of supported authentication methods
 */
void vrs_register_receive_user_authenticate(void (*func)(const uint8_t session_id,
		const char *username,
		const uint8_t auth_meth_count,
		const uint8_t *methods))
{
	vc_init_VC_CTX();
	vc_ctx->vfs.receive_user_authenticate = func;
}


/**
 * \brief This function tries to connect to verse server
 * \param[in]	*hostname	The string with hostname of the server
 * \param[in]	*service	The string with port number of the server
 * \param[in]	flags		The flags with options of connection
 * \param[out]	*session_id	There will be stored ID of session with verse server
 *
 * \return This function will return VRS_SUCCESS (0), when the client was able
 * to start connection to verse server.
 */
int vrs_send_connect_request(const char *hostname,
		const char *service,
		const uint16_t flags,
		uint8_t *session_id)
{
	struct VSession *vsession = NULL;
	uint16 _flags = flags;
	int already_connected = 0, i, ret;

	/* Check if CTX was initialized (initialization is done) */
	if(vc_ctx==NULL) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Basic callback functions were not set.\n");
		return VRS_NO_CB_FUNC;
	} else {
		/* Check if all needed callback functions was set up */
		if(vc_ctx->vfs.receive_connect_accept==NULL) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "receive_connect_accept() callback functions was not set.\n");
			return VRS_NO_CB_CONN_FUNC;
		}
		if(vc_ctx->vfs.receive_connect_terminate==NULL) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "receive_connect_terminate() callback functions was not set.\n");
			return VRS_NO_CB_TERM_FUNC;
		}
		if(vc_ctx->vfs.receive_user_authenticate==NULL) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "receive_user_authenticat() callback functions was not set.\n");
			return VRS_NO_CB_USER_AUTH;
		}
	}

	/* Set security protocol */
#if OPENSSL_VERSION_NUMBER>=0x10000000
	/* Check consistency of flags */
	if((_flags & VRS_SEC_DATA_NONE) && (_flags & VRS_SEC_DATA_TLS)) {
		if(is_log_level(VRS_PRINT_ERROR))
			v_print_log(VRS_PRINT_ERROR, "VRS_SEC_DATA_NONE or VRS_SEC_DATA_TLS could be set, not both.\n");
		return VRS_FAILURE;
	}
#else
	if (_flags & VRS_SEC_DATA_TLS) {
		v_print_log(VRS_PRINT_WARNING, "flag VRS_SEC_DATA_TLS could be set due to low version of OpenSSL (at least 1.0 is required).\n");
		_flags &= ~VRS_SEC_DATA_TLS;
		_flags |= VRS_SEC_DATA_NONE;
	}
#endif

	/* Set transport protocol */
	if((_flags & VRS_TP_UDP) && (_flags & VRS_TP_TCP)) {
		if(is_log_level(VRS_PRINT_ERROR))
			v_print_log(VRS_PRINT_ERROR, "VRS_TP_UDP or VRS_TP_TCP could be set, not both.\n");
		return VRS_FAILURE;
	} else if(!(_flags & VRS_TP_UDP) && !(_flags & VRS_TP_TCP)) {
		/* When no transport protocol is selected, then use UDP as default */
		_flags |= VRS_TP_UDP;
	}

	pthread_mutex_lock(&vc_ctx->mutex);

	/* Check if this client isn't already connected to this server or isn't
	 * trying to connect to the server with hostname:service */
	for(i=0; i<vc_ctx->max_sessions; i++) {
		if(vc_ctx->vsessions[i] != NULL) {
			if(strcmp(vc_ctx->vsessions[i]->peer_hostname, hostname) == 0 &&
					strcmp(vc_ctx->vsessions[i]->service, service) == 0) {
				v_print_log(VRS_PRINT_ERROR, "Client already connected to this server.\n");
				already_connected = 1;
				break;
			}
		}
	}

	if(already_connected == 0) {
		/* Try to find free verse session slot */
		for(i=0; i<vc_ctx->max_sessions; i++) {
			/* When free VSession slot is found, then create new session */
			if(vc_ctx->vsessions[i]==NULL) {
				vsession = (struct VSession*)malloc(sizeof(struct VSession));
				v_init_session(vsession);
				vsession->peer_hostname = strdup(hostname);
				vsession->service = strdup(service);
				/* Copy flags */
				vsession->flags = _flags;
				vsession->in_queue = (struct VInQueue*)calloc(1, sizeof(VInQueue));
				v_in_queue_init(vsession->in_queue, IN_QUEUE_DEFAULT_MAX_SIZE);
				vsession->out_queue = (struct VOutQueue*)calloc(1, sizeof(VOutQueue));
				v_out_queue_init(vsession->out_queue, OUT_QUEUE_DEFAULT_MAX_SIZE);

				vc_ctx->vsessions[i] = vsession;
				break;
			}
		}
	}

	pthread_mutex_unlock(&vc_ctx->mutex);

	if(already_connected == 1) {
		return VRS_FAILURE;
	}

	/* Check if we found free slot for new session */
	if(vsession == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Maximal count of sessions: %d reached.\n",
				vc_ctx->max_sessions);
		return VRS_FAILURE;
	}

	vsession->session_id = vc_ctx->session_counter++;
	*session_id = vsession->session_id;

	/* Try to initialize thread attributes */
	if( (ret = pthread_attr_init(&vsession->tcp_thread_attr)) != 0 ) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "pthread_attr_init(): %s\n", strerror(errno));
		return VRS_FAILURE;
	}

	/* Try to set thread attributes as detached */
	if( (ret = pthread_attr_setdetachstate(&vsession->tcp_thread_attr, PTHREAD_CREATE_DETACHED)) != 0) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "pthread_attr_setdetachstate(): %s\n", strerror(errno));
		return VRS_FAILURE;
	}

	/* Create thread for new client session */
	if(pthread_create(&vsession->tcp_thread, &vsession->tcp_thread_attr, vc_new_session_thread, (void*)vsession) != 0) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Thread creation failed.\n");
		pthread_mutex_lock(&vc_ctx->mutex);
		v_destroy_session(vsession);
		free(vsession);
		vc_ctx->vsessions[i] = NULL;
		pthread_mutex_unlock(&vc_ctx->mutex);
		return VRS_FAILURE;
	}

	/* Destroy thread attributes */
	pthread_attr_destroy(&vsession->tcp_thread_attr);

	return VRS_SUCCESS;
}


/**
 * \brief This function calls appropriate callback functions, when some system
 * or node commands are in incoming queue
 *
 * \param[in]	session_id			The ID of session with verse server.
 *
 * \return This function returns VRS_SUCCESS, when session was found and when
 * at least basic callback functions were registered.
 */
int vrs_callback_update(const uint8_t session_id)
{
	static Generic_Cmd *cmd;
	int i;

	/* Check if CTX was initialized */
	if(vc_ctx == NULL) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Basic callback functions were not set.\n");
		return VRS_NO_CB_FUNC;
	} else {
		int session_found = 0;

		pthread_mutex_lock(&vc_ctx->mutex);

		/* Go through all sessions ... */
		for(i = 0; i<vc_ctx->max_sessions; i++) {
			/* ... and try to find connection with session_id */
			if(vc_ctx->vsessions[i] != NULL &&
					vc_ctx->vsessions[i]->session_id == session_id) {

				/* Pop all incoming commands from queue */
				while(v_in_queue_cmd_count(vc_ctx->vsessions[i]->in_queue) > 0) {
					cmd = v_in_queue_pop(vc_ctx->vsessions[i]->in_queue);

					vc_call_callback_func(session_id, cmd);

					v_cmd_destroy(&cmd);
				}

				session_found = 1;
				break;
			}
		}

		pthread_mutex_unlock(&vc_ctx->mutex);

		if(session_found == 1) {
			return VRS_SUCCESS;
		}
	}

	v_print_log(VRS_PRINT_ERROR, "Invalid session_id: %d.\n", session_id);

	return VRS_FAILURE;
}


/**
 * \brief This function tries to send username and some authentication data
 * (usually password) to the verse server
 *
 * This function should be called, when client receive User_Authenticate
 * command and callback function registered with
 * register_receive_user_authenticate() is called.
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	*username	The string of username
 * \param[in]	auth_type	The authentication method
 * \param[in]	data_length	The length of authentication data in bytes
 * \param[in]	*data		The pointer at authentication data
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_user_authenticate(const uint8_t session_id,
		const char *username,
		const uint8_t auth_type,
		const char *data)
{
	struct User_Authenticate_Cmd *user_auth;
	uint8 methods[1];
	methods[0] = auth_type;
	user_auth = v_user_auth_create(username, 1, methods, data);
	return vc_send_command(session_id, VRS_DEFAULT_PRIORITY, (struct Generic_Cmd*)user_auth);
}


/**
 * \brief This function switch connection to CLOSING state (start teardown,
 * exit thread) and close connection to verse server.
 *
 * \param[in]	session_id	The ID of session with verse server.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int vrs_send_connect_terminate(const uint8_t session_id)
{
	struct Connect_Terminate_Cmd *conn_term;
	conn_term = v_Connect_Terminate_create(0);
	return vc_send_command(session_id, VRS_DEFAULT_PRIORITY, (struct Generic_Cmd*)conn_term);
}


/**
 * \brief Return error message for error_num returned by Verse API functions.
 *
 * This function should return some string with detail description of error,
 * but it doesn't do anything now.
 *
 * \param[in]	error_num	The identifier of error
 *
 * \return	This function should return string with detail description of
 * errror.
 */
char *vrs_strerror(const uint32_t error_num)
{
	char *error_string = NULL;
	switch(error_num) {
	default:
		error_string = "Unknown error";
		break;
	}
	return error_string;
}


/**
 * \brief This function is generic function for putting command to outgoing
 * queue. Note: not all commands uses this function (node_create, node_lock
 * and node_unlock).
 */
static int vc_send_command(const uint8 session_id,
		const uint8 prio,
		struct Generic_Cmd *cmd)
{
	int i;

	assert(cmd != NULL);

	if(vc_ctx == NULL) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Basic callback functions were not set.\n");
		free(cmd);
		return VRS_NO_CB_FUNC;
	} else {
		/* Go through all sessions ... */
		for(i=0; i<vc_ctx->max_sessions; i++) {
			/* ... and try to find session with session_id */
			if(vc_ctx->vsessions[i]!=NULL &&
					vc_ctx->vsessions[i]->session_id==session_id)
			{
				v_out_queue_push_tail(vc_ctx->vsessions[i]->out_queue,
						prio, cmd);
				return VRS_SUCCESS;
			}
		}
	}

	if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "Session %d does not exist.\n", session_id);
	free(cmd);
	return VRS_FAILURE;
}


/**
 * \brief This functions calls registered callback function for appropriate
 * commands
 */
static void vc_call_callback_func(const uint8 session_id,
		struct Generic_Cmd *cmd)
{
	switch(cmd->id) {
	case FAKE_CMD_CONNECT_ACCEPT:
		if(vc_ctx->vfs.receive_connect_accept != NULL) {
			vc_ctx->vfs.receive_connect_accept(session_id,
					((struct Connect_Accept_Cmd*)cmd)->user_id,
					((struct Connect_Accept_Cmd*)cmd)->avatar_id);
		}
		break;
	case FAKE_CMD_CONNECT_TERMINATE:
		if(vc_ctx->vfs.receive_connect_terminate != NULL) {
			vc_ctx->vfs.receive_connect_terminate(session_id,
					((struct Connect_Terminate_Cmd*)cmd)->error_num);
		}
		break;
	case FAKE_CMD_USER_AUTHENTICATE:
		if(vc_ctx->vfs.receive_user_authenticate != NULL) {
			vc_ctx->vfs.receive_user_authenticate(session_id,
					((struct User_Authenticate_Cmd*)cmd)->username,
					((struct User_Authenticate_Cmd*)cmd)->auth_meth_count,
					((struct User_Authenticate_Cmd*)cmd)->methods);
		}
		break;
	case CMD_NODE_CREATE:
		if(vc_ctx->vfs.receive_node_create != NULL) {
			vc_ctx->vfs.receive_node_create(session_id,
					UINT32(cmd->data[UINT16_SIZE+UINT32_SIZE]),
					UINT32(cmd->data[UINT16_SIZE]),
					UINT16(cmd->data[0]),
					UINT16(cmd->data[UINT16_SIZE+UINT32_SIZE+UINT32_SIZE]));
		}
		break;
	case CMD_NODE_DESTROY:
		if(vc_ctx->vfs.receive_node_destroy != NULL) {
			vc_ctx->vfs.receive_node_destroy(session_id,
					UINT32(cmd->data[0]));
		}
		break;
	case CMD_NODE_SUBSCRIBE:
		if(vc_ctx->vfs.receive_node_subscribe != NULL) {
			vc_ctx->vfs.receive_node_subscribe(session_id,
					UINT32(cmd->data[0]),
					UINT32(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE + UINT32_SIZE]));
		}
		break;
	case CMD_NODE_UNSUBSCRIBE:
		if(vc_ctx->vfs.receive_node_unsubscribe != NULL) {
			vc_ctx->vfs.receive_node_unsubscribe(session_id,
					UINT32(cmd->data[0]),
					UINT32(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE + UINT32_SIZE]));
		}
		break;
	case CMD_NODE_PERMISSION:
		if(vc_ctx->vfs.receive_node_perm != NULL) {
			vc_ctx->vfs.receive_node_perm(session_id,
					UINT32(cmd->data[UINT16_SIZE+UINT8_SIZE]),
					UINT16(cmd->data[0]),
					UINT8(cmd->data[UINT16_SIZE]));
		}
		break;
	case CMD_NODE_OWNER:
		if(vc_ctx->vfs.receive_node_owner != NULL) {
			vc_ctx->vfs.receive_node_owner(session_id,
					UINT32(cmd->data[UINT16_SIZE]),
					UINT16(cmd->data[0]));
		}
		break;
	case CMD_NODE_LOCK:
		if(vc_ctx->vfs.receive_node_lock != NULL) {
			vc_ctx->vfs.receive_node_lock(session_id,
					UINT32(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[0]));
		}
		break;
	case CMD_NODE_UNLOCK:
		if(vc_ctx->vfs.receive_node_unlock != NULL) {
			vc_ctx->vfs.receive_node_unlock(session_id,
					UINT32(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[0]));
		}
		break;
	case CMD_NODE_LINK:
		if(vc_ctx->vfs.receive_node_link != NULL) {
			vc_ctx->vfs.receive_node_link(session_id,
					UINT32(cmd->data[0]),
					UINT32(cmd->data[UINT32_SIZE]));
		}
		break;
	case CMD_TAGGROUP_CREATE:
		if(vc_ctx->vfs.receive_taggroup_create != NULL) {
			vc_ctx->vfs.receive_taggroup_create(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE+UINT16_SIZE]));
		}
		break;
	case CMD_TAGGROUP_DESTROY:
		if(vc_ctx->vfs.receive_taggroup_destroy) {
			vc_ctx->vfs.receive_taggroup_destroy(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]));
		}
		break;
	case CMD_TAGGROUP_SUBSCRIBE:
		if(vc_ctx->vfs.receive_taggroup_subscribe) {
			vc_ctx->vfs.receive_taggroup_subscribe(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE+UINT16_SIZE]),
					UINT32(cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE]));
		}
		break;
	case CMD_TAGGROUP_UNSUBSCRIBE:
		if(vc_ctx->vfs.receive_taggroup_unsubscribe) {
			vc_ctx->vfs.receive_taggroup_unsubscribe(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE+UINT16_SIZE]),
					UINT32(cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE]));
		}
		break;
	case CMD_TAG_CREATE:
		if(vc_ctx->vfs.receive_tag_create != NULL) {
			vc_ctx->vfs.receive_tag_create(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					UINT8(cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]),
					UINT8(cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + UINT8_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + UINT8_SIZE + UINT8_SIZE]));
		}
		break;
	case CMD_TAG_DESTROY:
		if(vc_ctx->vfs.receive_tag_destroy != NULL) {
			vc_ctx->vfs.receive_tag_destroy(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]));
		}
		break;
	case CMD_TAG_SET_UINT8:
	case CMD_TAG_SET_VEC2_UINT8:
	case CMD_TAG_SET_VEC3_UINT8:
	case CMD_TAG_SET_VEC4_UINT8:
		if(vc_ctx->vfs.receive_tag_set_value != NULL) {
			vc_ctx->vfs.receive_tag_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_UINT8,
					cmd->id - CMD_TAG_SET_UINT8 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]);
		}
		break;
	case CMD_TAG_SET_UINT16:
	case CMD_TAG_SET_VEC2_UINT16:
	case CMD_TAG_SET_VEC3_UINT16:
	case CMD_TAG_SET_VEC4_UINT16:
		if(vc_ctx->vfs.receive_tag_set_value != NULL) {
			vc_ctx->vfs.receive_tag_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_UINT16,
					cmd->id - CMD_TAG_SET_UINT16 + 1 ,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]);
		}
		break;
	case CMD_TAG_SET_UINT32:
	case CMD_TAG_SET_VEC2_UINT32:
	case CMD_TAG_SET_VEC3_UINT32:
	case CMD_TAG_SET_VEC4_UINT32:
		if(vc_ctx->vfs.receive_tag_set_value != NULL) {
			vc_ctx->vfs.receive_tag_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_UINT32,
					cmd->id - CMD_TAG_SET_UINT32 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]);
		}
		break;
	case CMD_TAG_SET_UINT64:
	case CMD_TAG_SET_VEC2_UINT64:
	case CMD_TAG_SET_VEC3_UINT64:
	case CMD_TAG_SET_VEC4_UINT64:
		if(vc_ctx->vfs.receive_tag_set_value != NULL) {
			vc_ctx->vfs.receive_tag_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_UINT64,
					cmd->id - CMD_TAG_SET_UINT64 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]);
		}
		break;
	case CMD_TAG_SET_REAL16:
	case CMD_TAG_SET_VEC2_REAL16:
	case CMD_TAG_SET_VEC3_REAL16:
	case CMD_TAG_SET_VEC4_REAL16:
		if(vc_ctx->vfs.receive_tag_set_value != NULL) {
			vc_ctx->vfs.receive_tag_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_REAL16,
					cmd->id - CMD_TAG_SET_REAL16 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]);
		}
		break;
	case CMD_TAG_SET_REAL32:
	case CMD_TAG_SET_VEC2_REAL32:
	case CMD_TAG_SET_VEC3_REAL32:
	case CMD_TAG_SET_VEC4_REAL32:
		if(vc_ctx->vfs.receive_tag_set_value != NULL) {
			vc_ctx->vfs.receive_tag_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_REAL32,
					cmd->id - CMD_TAG_SET_REAL32 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]);
		}
		break;
	case CMD_TAG_SET_REAL64:
	case CMD_TAG_SET_VEC2_REAL64:
	case CMD_TAG_SET_VEC3_REAL64:
	case CMD_TAG_SET_VEC4_REAL64:
		if(vc_ctx->vfs.receive_tag_set_value != NULL) {
			vc_ctx->vfs.receive_tag_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_REAL64,
					cmd->id - CMD_TAG_SET_REAL64 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]);
		}
		break;
	case CMD_TAG_SET_STRING8:
		if(vc_ctx->vfs.receive_tag_set_value != NULL) {
			vc_ctx->vfs.receive_tag_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_STRING8,
					1,
					PTR(cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]));
		}
		break;
	case CMD_LAYER_CREATE:
		if(vc_ctx->vfs.receive_layer_create != NULL) {
			vc_ctx->vfs.receive_layer_create(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					UINT8(cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]),
					UINT8(cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + UINT8_SIZE]),
					UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + UINT8_SIZE  + UINT8_SIZE]));
		}
		break;
	case CMD_LAYER_DESTROY:
		if(vc_ctx->vfs.receive_layer_destroy != NULL) {
			vc_ctx->vfs.receive_layer_destroy(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]));
		}
		break;
	case CMD_LAYER_SUBSCRIBE:
		if(vc_ctx->vfs.receive_layer_subscribe != NULL) {
			vc_ctx->vfs.receive_layer_subscribe(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE+UINT16_SIZE]),
					UINT32(cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE]));
		}
		break;
	case CMD_LAYER_UNSUBSCRIBE:
		if(vc_ctx->vfs.receive_layer_unsubscribe != NULL) {
			vc_ctx->vfs.receive_layer_unsubscribe(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE+UINT16_SIZE]),
					UINT32(cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE]));
		}
		break;
	case CMD_LAYER_UNSET_VALUE:
		if(vc_ctx->vfs.receive_layer_unset_value != NULL) {
			vc_ctx->vfs.receive_layer_unset_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE + UINT16_SIZE]));
		}
		break;
	case CMD_LAYER_SET_UINT8:
	case CMD_LAYER_SET_VEC2_UINT8:
	case CMD_LAYER_SET_VEC3_UINT8:
	case CMD_LAYER_SET_VEC4_UINT8:
		if(vc_ctx->vfs.receive_layer_set_value != NULL) {
			vc_ctx->vfs.receive_layer_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_UINT8,
					cmd->id - CMD_LAYER_SET_UINT8 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE]);
		}
		break;
	case CMD_LAYER_SET_UINT16:
	case CMD_LAYER_SET_VEC2_UINT16:
	case CMD_LAYER_SET_VEC3_UINT16:
	case CMD_LAYER_SET_VEC4_UINT16:
		if(vc_ctx->vfs.receive_layer_set_value != NULL) {
			vc_ctx->vfs.receive_layer_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_UINT16,
					cmd->id - CMD_LAYER_SET_UINT16 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE]);
		}
		break;
	case CMD_LAYER_SET_UINT32:
	case CMD_LAYER_SET_VEC2_UINT32:
	case CMD_LAYER_SET_VEC3_UINT32:
	case CMD_LAYER_SET_VEC4_UINT32:
		if(vc_ctx->vfs.receive_layer_set_value != NULL) {
			vc_ctx->vfs.receive_layer_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_UINT32,
					cmd->id - CMD_LAYER_SET_UINT32 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE]);
		}
		break;
	case CMD_LAYER_SET_UINT64:
	case CMD_LAYER_SET_VEC2_UINT64:
	case CMD_LAYER_SET_VEC3_UINT64:
	case CMD_LAYER_SET_VEC4_UINT64:
		if(vc_ctx->vfs.receive_layer_set_value != NULL) {
			vc_ctx->vfs.receive_layer_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_UINT64,
					cmd->id - CMD_LAYER_SET_UINT64 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE]);
		}
		break;
	case CMD_LAYER_SET_REAL16:
	case CMD_LAYER_SET_VEC2_REAL16:
	case CMD_LAYER_SET_VEC3_REAL16:
	case CMD_LAYER_SET_VEC4_REAL16:
		if(vc_ctx->vfs.receive_layer_set_value != NULL) {
			vc_ctx->vfs.receive_layer_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_REAL16,
					cmd->id - CMD_LAYER_SET_REAL16 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE]);
		}
		break;
	case CMD_LAYER_SET_REAL32:
	case CMD_LAYER_SET_VEC2_REAL32:
	case CMD_LAYER_SET_VEC3_REAL32:
	case CMD_LAYER_SET_VEC4_REAL32:
		if(vc_ctx->vfs.receive_layer_set_value != NULL) {
			vc_ctx->vfs.receive_layer_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_REAL32,
					cmd->id - CMD_LAYER_SET_REAL32 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE]);
		}
		break;
	case CMD_LAYER_SET_REAL64:
	case CMD_LAYER_SET_VEC2_REAL64:
	case CMD_LAYER_SET_VEC3_REAL64:
	case CMD_LAYER_SET_VEC4_REAL64:
		if(vc_ctx->vfs.receive_layer_set_value != NULL) {
			vc_ctx->vfs.receive_layer_set_value(session_id,
					UINT32(cmd->data[0]),
					UINT16(cmd->data[UINT32_SIZE]),
					UINT32(cmd->data[UINT32_SIZE + UINT16_SIZE]),
					VRS_VALUE_TYPE_REAL64,
					cmd->id - CMD_LAYER_SET_REAL64 + 1,
					&cmd->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE]);
		}
		break;
	default:
		v_print_log(VRS_PRINT_ERROR, "This command: %d is not supported yet\n", cmd->id);
		break;
	}
}


/**
 * \brief This function initialize context of client.
 *
 * \return This function returns 1, when initialization was successful. It
 * returns 0 otherwise.
 */
static int vc_init_VC_CTX(void)
{
	/* When this is first connection attempts, then initialize CTX */
	if(vc_ctx==NULL) {
		if( (vc_ctx = (struct VC_CTX*)malloc(sizeof(VC_CTX))) == NULL) return 0;

		/* Load verse client configuration from file */
		vc_load_config_file(vc_ctx);

		/* Initialize prints to the log file */
		v_init_print_log(vc_ctx->print_log_level, vc_ctx->log_file);

		/* Initialize of verse client context */
		if(vc_init_ctx(vc_ctx) == 0) return 0;
	}

	return 1;
}


/**
 * \brief This function is main function of new thread for client
 * \param *arg	The pointer at verse client session
 */
static void *vc_new_session_thread(void *arg)
{
	struct VSession *vsession = (struct VSession*)arg;
	int i;

	/* Start "never ending" thread of connection*/
	vc_main_stream_loop(vc_ctx, vsession);

	pthread_mutex_lock(&vc_ctx->mutex);

	/* Find this session in Verse client session list */
	for(i=0; i<vc_ctx->max_sessions; i++) {
		if(vc_ctx->vsessions[i] != NULL) {
			if(strcmp(vc_ctx->vsessions[i]->peer_hostname, vsession->peer_hostname)==0 &&
					strcmp(vc_ctx->vsessions[i]->service, vsession->service)==0) {
				vc_ctx->vsessions[i] = NULL;
				break;
			}
		}
	}

	/* When connection is closed, then destroy this session*/
	v_destroy_session(vsession);
	free(vsession);

	pthread_mutex_unlock(&vc_ctx->mutex);

	return NULL;
}
