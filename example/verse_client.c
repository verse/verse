/*
 * $Id: verse_client.c 1343 2012-09-18 12:44:50Z jiri $
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2012, Jiri Hnidek
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

/** \file verse_client.c
 * This is example of simple verse client using Verse API from verse.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>

#include "verse.h"

/**
 * 60 frames per seconds
 */
#define FPS 30

/**
 * ID of session used for connection to server
 */
static uint8_t session_id = -1;

/**
 * ID of user used for connection to verse server
 */
static int64_t my_user_id = -1;

/**
 * ID of avatar representing this client on verse server
 */
static int64_t my_avatar_id = -1;

/**
 * ID of node created at verse server for testing of basic operations
 */
static int64_t my_test_node_id = -1;

/**
 * Priority of my test node
 */
static uint8_t my_test_node_prio = VRS_DEFAULT_PRIORITY + 1;

/**
 * ID of taggroup created at verse server for testing of basic operations
 */
static int32_t my_test_taggroup_id = -1;

/**
 * ID of layer created at verse server for testing basci operations
 */
static int64_t my_test_layer_id = -1;

/**
 * My test types for node, tag group and tag
 */
#define MY_TEST_TAG_TYPE			1

#define MY_TEST_TAGGROUP_TYPE		1

#define MY_TEST_LAYER_TYPE			1
#define MY_TEST_CHILD_LAYER_TYPE	2

#define MY_TEST_NODE_TYPE			1

/**
 * \brief Callback function for handling signals.
 * \details Only SIGINT (Ctrl-C) is handled. When first SIGINT is received,
 * then client tries to terminate connection and it set handling to SIGINT
 * to default behavior (terminate application).
 * \param	sig	identifier of signal
 */
static void handle_signal(int sig)
{
	if(sig == SIGINT) {
		printf("%s() try to terminate connection: %d\n",
				__FUNCTION__, session_id);
		vrs_send_connect_terminate(session_id);
		/* Reset signal handling to default behavior */
		signal(SIGINT, SIG_DFL);
	}
}

/**
 * \brief The callback function for command layer_set_value
 */
static void cb_receive_layer_unset_value(const uint8_t session_id,
	     const uint32_t node_id,
	     const uint16_t layer_id,
	     const uint32_t item_id)
{
	printf("%s(): session_id: %u, node_id: %u, layer_id: %d, item_id: %d \n",
			__FUNCTION__, session_id, node_id, layer_id, item_id);
	if(node_id == my_test_node_id && layer_id == my_test_layer_id) {
		/* Test of unsubscribing from layer */
		vrs_send_layer_unsubscribe(session_id, my_test_node_prio, node_id, layer_id, 0);
	}
}

/**
 * \brief The callback function for command layer_set_value
 */
static void cb_receive_layer_set_value(const uint8_t session_id,
	     const uint32_t node_id,
	     const uint16_t layer_id,
	     const uint32_t item_id,
	     const uint8_t type,
	     const uint8_t count,
	     const void *value)
{
	int i;

	printf("%s(): session_id: %u, node_id: %u, layer_id: %d, item_id: %d, data_type: %d, count: %d, value(s): ",
			__FUNCTION__, session_id, node_id, layer_id, item_id, type, count);

	switch(type) {
	case VRS_VALUE_TYPE_UINT8:
		for(i=0; i<count; i++) {
			printf("%d, ", ((uint8_t*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_UINT16:
		for(i=0; i<count; i++) {
			printf("%d, ", ((uint16_t*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_UINT32:
		for(i=0; i<count; i++) {
			printf("%d, ", ((uint32_t*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_UINT64:
		for(i=0; i<count; i++) {
			printf("%ld, ", ((uint64_t*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_REAL16:
		for(i=0; i<count; i++) {
			/* TODO: convert half-float to float and print it as float value */
			printf("%x, ", ((uint16_t*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_REAL32:
		for(i=0; i<count; i++) {
			printf("%6.3f, ", ((float*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_REAL64:
		for(i=0; i<count; i++) {
			printf("%6.3f, ", ((double*)value)[i]);
		}
		break;
	default:
		printf("Unknown type");
		break;
	}
	printf("\n");

	if(node_id == my_test_node_id && layer_id == my_test_layer_id) {
		/* Test of sending layer unset value */
		vrs_send_layer_unset_value(session_id, my_test_node_prio, node_id, layer_id, item_id);
	}
}

/**
 * \brief The callback function for command layer_destroy
 */
static void cb_receive_layer_destroy(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id)
{
	printf("%s(): session_id: %u, node_id: %u, layer_id: %d\n",
			__FUNCTION__, session_id, node_id, layer_id);
}

static void cb_receive_layer_create(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t parent_layer_id,
		const uint16_t layer_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type)
{
	printf("%s(): session_id: %u, node_id: %u, parent_layer_id: %d, layer_id: %d, data_type: %d, count: %d, type: %d\n",
			__FUNCTION__, session_id, node_id, parent_layer_id, layer_id, data_type, count, type);

	if(node_id == my_test_node_id && type == MY_TEST_LAYER_TYPE) {
		uint8_t uint8_t_val[4] = {123, 124, 125, 126};
		uint16_t uint16_t_val[4] = {1234, 456, 7890, 4321};
		uint32_t uint32_t_val[4] = {12345, 67890, 98765, 43210};
		uint64_t uint64_t_val[4] = {123456, 789012, 345678, 9012345};
		float real16_t_val[4] = {123.45, 678.90, -123.45, -678.90};
		float real32_t_val[4] = {1234.567, -8901.234, 5678.901, -2345.678};
		double real64_t_val[4] = {-1234567.890, 0.1234, 100000.9999, -0.000001234};
		void *value;

		my_test_layer_id = layer_id;

		/* Test of subscribing to layer */
		vrs_send_layer_subscribe(session_id, my_test_node_prio, node_id, layer_id, 0, 0);

		/* Test of creating child layer */
		vrs_send_layer_create(session_id, my_test_node_prio,
				node_id, layer_id, VRS_VALUE_TYPE_REAL32, 2, MY_TEST_CHILD_LAYER_TYPE);

		/* Test sending some values of tag types */
		switch(data_type) {
		case VRS_VALUE_TYPE_UINT8:
			value = uint8_t_val;
			break;
		case VRS_VALUE_TYPE_UINT16:
			value = uint16_t_val;
			break;
		case VRS_VALUE_TYPE_UINT32:
			value = uint32_t_val;
			break;
		case VRS_VALUE_TYPE_UINT64:
			value = uint64_t_val;
			break;
		case VRS_VALUE_TYPE_REAL16:
			/* TODO: convert float values to half-float values */
			value = real16_t_val;
			break;
		case VRS_VALUE_TYPE_REAL32:
			value = real32_t_val;
			break;
		case VRS_VALUE_TYPE_REAL64:
			value = real64_t_val;
			break;
		default:
			value = NULL;
			break;
		}

		/* Test of sending set value */
		vrs_send_layer_set_value(session_id, my_test_node_prio,
				node_id, layer_id, 10, data_type, count, value);

	}

	if(node_id == my_test_node_id &&
			parent_layer_id == my_test_layer_id &&
			type == MY_TEST_CHILD_LAYER_TYPE) {
		/* Test of destroying parent layer */
		vrs_send_layer_destroy(session_id, my_test_node_prio, node_id, parent_layer_id);
	}
}

/**
 * \brief Callback function for receiving command tag_set_value
 */
static void cb_receive_tag_set_value(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t value_type,
		const uint8_t count,
		const void *value)
{
	int i;

	printf("%s() session_id: %u, node_id: %u, taggroup_id: %u, tag_id: %u, type: %d, count: %d, value(s): ",
				__FUNCTION__, session_id, node_id, taggroup_id, tag_id, value_type, count);

	switch(value_type) {
	case VRS_VALUE_TYPE_UINT8:
		for(i=0; i<count; i++) {
			printf("%d, ", ((uint8_t*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_UINT16:
		for(i=0; i<count; i++) {
			printf("%d, ", ((uint16_t*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_UINT32:
		for(i=0; i<count; i++) {
			printf("%d, ", ((uint32_t*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_UINT64:
		for(i=0; i<count; i++) {
			printf("%ld, ", ((uint64_t*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_REAL16:
		for(i=0; i<count; i++) {
			/* TODO: convert half-float to float and print it as float value */
			printf("%x, ", ((uint16_t*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_REAL32:
		for(i=0; i<count; i++) {
			printf("%6.3f, ", ((float*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_REAL64:
		for(i=0; i<count; i++) {
			printf("%6.3f, ", ((double*)value)[i]);
		}
		break;
	case VRS_VALUE_TYPE_STRING8:
		printf("%s", (char*)value);
		break;
	default:
		printf("Unknown type");
		break;
	}
	printf("\n");

	if(node_id == my_test_node_id && taggroup_id == my_test_taggroup_id) {
		/* Test of tag create */
		vrs_send_tag_destroy(session_id, my_test_node_prio, node_id, taggroup_id, tag_id);
	}

}

static void cb_receive_tag_destroy(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id)
{
	printf("%s() session_id: %d, node_id: %d, taggroup_id: %d, tag_id: %d\n",
				__FUNCTION__, session_id, node_id, taggroup_id, tag_id);

	if(node_id == my_test_node_id && taggroup_id == my_test_taggroup_id) {
		/* Test of unsubscribing from the tag group */
		vrs_send_taggroup_unsubscribe(session_id, my_test_node_prio, node_id, taggroup_id, 0);

		/* Test of deleting the tag group */
		vrs_send_taggroup_destroy(session_id, my_test_node_prio, node_id, taggroup_id);
	}
}

static void cb_receive_tag_create(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type)
{
	printf("%s() session_id: %d, node_id: %d, taggroup_id: %d, tag_id: %d, data_type: %d, count: %d, type: %d\n",
				__FUNCTION__, session_id, node_id, taggroup_id, tag_id, data_type, count, type);

	if(node_id == my_test_node_id && taggroup_id == my_test_taggroup_id) {
		void *value = NULL;
		uint8_t uint8_t_val[4] = {123, 124, 125, 126};
		uint16_t uint16_t_val[4] = {1234, 456, 7890, 4321};
		uint32_t uint32_t_val[4] = {12345, 67890, 98765, 43210};
		uint64_t uint64_t_val[4] = {123456, 789012, 345678, 9012345};
		float real16_t_val[4] = {123.45, 678.90, -123.45, -678.90};
		float real32_t_val[4] = {1234.567, -8901.234, 5678.901, -2345.678};
		double real64_t_val[4] = {-1234567.890, 0.1234, 100000.9999, -0.000001234};
		char *string8_t_val = "Ahoj";

		/* Test sending some values of tag types */
		switch(data_type) {
		case VRS_VALUE_TYPE_UINT8:
			value = uint8_t_val;
			break;
		case VRS_VALUE_TYPE_UINT16:
			value = uint16_t_val;
			break;
		case VRS_VALUE_TYPE_UINT32:
			value = uint32_t_val;
			break;
		case VRS_VALUE_TYPE_UINT64:
			value = uint64_t_val;
			break;
		case VRS_VALUE_TYPE_REAL16:
			/* TODO: convert float values to half-float values */
			value = real16_t_val;
			break;
		case VRS_VALUE_TYPE_REAL32:
			value = real32_t_val;
			break;
		case VRS_VALUE_TYPE_REAL64:
			value = real64_t_val;
			break;
		case VRS_VALUE_TYPE_STRING8:
			value = string8_t_val;
			break;
		default:
			value = NULL;
			break;
		}

		vrs_send_tag_set_value(session_id, my_test_node_prio, node_id,
				taggroup_id, tag_id, data_type, count, value);
	}
}

static void cb_receive_taggroup_destroy(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id)
{
	printf("%s() session_id: %d, node_id: %d, taggroup_id: %d\n",
				__FUNCTION__, session_id, node_id, taggroup_id);

	if(node_id == my_test_node_id && taggroup_id == my_test_taggroup_id) {
		/* Try to lock my own node */
		vrs_send_node_lock(session_id, my_test_node_prio, node_id);
	}
}

static void cb_receive_taggroup_create(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t type)
{
	printf("%s() session_id: %d, node_id: %d, taggroup_id: %d, type: %d\n",
				__FUNCTION__, session_id, node_id, taggroup_id, type);

	if(node_id == my_test_node_id && type == MY_TEST_TAGGROUP_TYPE) {
		/* Set up name of my tag group */
		my_test_taggroup_id = taggroup_id;

		/* Test that it's not possible to create tag group with the same name in
		 * on node. */
		vrs_send_taggroup_create(session_id, my_test_node_prio, node_id, type);

		/* Test of subscribing to tag group */
		vrs_send_taggroup_subscribe(session_id, my_test_node_prio, node_id, taggroup_id, 0, 0);

		/* Test of uint8 tag create */
		vrs_send_tag_create(session_id, my_test_node_prio, node_id,
				taggroup_id, VRS_VALUE_TYPE_UINT8, 3, MY_TEST_TAG_TYPE);
#if 0
		/* Test of string tag create */
		vrs_send_tag_create(session_id, my_test_node_prio, node_id,
				taggroup_id, VRS_VALUE_TYPE_STRING8, 1, MY_TEST_TAG_TYPE);
#endif
	}
}

/**
 * \brief This function is callback function for receiving of node_create
 * command. It means, that this function is called, when verse_callback_update
 * is executed and there are any data in appropriate incoming queue.
 *
 * \param[in]	session_id	The ID of session with some verse server
 * \param[in]	node_id		The ID of new node
 * \param[in]	parent_id	The ID of parent node
 * \param[in]	user_id		The User ID of the owner of this node
 */
static void cb_receive_node_create(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t parent_id,
		const uint16_t user_id,
		const uint16_t type)
{
	printf("%s() session_id: %d, node_id: %d, parent_id: %d, user_id: %d, type: %d\n",
			__FUNCTION__, session_id, node_id, parent_id, user_id, type);

	/* Is node special node? */
	if(node_id==VRS_ROOT_NODE_ID) {
		printf("\tRoot Node\n");
	} else if(node_id==VRS_AVATAR_PARENT_NODE_ID) {
		printf("\tParent of Avatar Nodes\n");
		/* Try to delete node, that could not be deleted */
		vrs_send_node_destroy(session_id, VRS_DEFAULT_PRIORITY, node_id);
	} else if(node_id==VRS_USERS_PARENT_NODE_ID) {
		printf("\tParent of User Nodes\n");
		/* Example of node subscribe command */
		vrs_send_node_subscribe(session_id, VRS_DEFAULT_PRIORITY, node_id, 0, 0);
	} else if(node_id==VRS_SCENE_PARENT_NODE_ID) {
		printf("\tParent of Scene Nodes\n");
		/* Example of node un-subscribe command */
		vrs_send_node_unsubscribe(session_id, VRS_DEFAULT_PRIORITY, node_id, 0);
	} else if(node_id==VRS_ROOT_NODE_ID) {
		printf("\tUser Node of SuperUser\n");
	} else if(parent_id==VRS_USERS_PARENT_NODE_ID) {
		printf("\tUser Node\n");
	} else if(node_id==VRS_RESERVED_NODE_ID) {
		printf("\tReservered Node ID (should never be received by client from server)\n");
	} else if(node_id>=VRS_FIRST_COMMON_NODE_ID) {
		printf("\tCommon Node\n");
	}

	/* My Avatar Node */
	if(node_id == my_avatar_id) {
		/* Client should subscribe to his avarat node to be able receive information
		 * about nodes, that was created by this client */
		printf("\tMy Avatar Node\n");
		vrs_send_node_subscribe(session_id, VRS_DEFAULT_PRIORITY, node_id, 0, 0);

		/* Test of sending wrong node_perm command (client doesn't own this node) */
		vrs_send_node_perm(session_id, VRS_DEFAULT_PRIORITY, node_id, my_user_id, VRS_PERM_NODE_READ | VRS_PERM_NODE_WRITE);
	}

	/* Who is owner of this node? */
	if(my_user_id != -1 && my_user_id == user_id) {
		printf("\tNode owned by me\n");
	} else if(user_id==VRS_SUPER_USER_UID) {
		printf("\tNode owned by super user (server)\n");
	} else {
		printf("\tNode owned by someone else\n");
	}

	/* Who created this node? */
	if(my_avatar_id != -1 && my_avatar_id == parent_id) {
		printf("\tNode created from this client\n");
		printf("\tParent of this node is my avatar node: %d\n", parent_id);
	} else {
		printf("\tNode created by somebody else\n");
		printf("\tParent of this node is: %d\n", parent_id);
	}

	/* Example of subscribing to node created from this client */
	if((my_user_id != -1 && my_user_id == user_id) &&
			(my_avatar_id != -1 && my_avatar_id == parent_id))
	{
		my_test_node_id = node_id;

		/* Test of subscribing to my own node */
		vrs_send_node_subscribe(session_id, VRS_DEFAULT_PRIORITY, node_id, 0, 0);

		/* Test of changing priority of my node */
		vrs_send_node_prio(session_id, VRS_DEFAULT_PRIORITY, node_id, my_test_node_prio);

		/* Test of changing parent of my node */
		vrs_send_node_link(session_id, VRS_DEFAULT_PRIORITY, VRS_SCENE_PARENT_NODE_ID, node_id);

		/* Test of adding new node permission */
		vrs_send_node_perm(session_id, VRS_DEFAULT_PRIORITY, node_id, 100, VRS_PERM_NODE_READ | VRS_PERM_NODE_WRITE);
	}
}

/**
 * \brief This function is callback function for receiving of command Node_Destroy
 *
 * It means, that this function is called, when verse_callback_update
 * is executed and there is any Node_Destroy command in incoming queue.
 *
 * \param[in]	session_id	The ID of session with some verse server
 * \param[in]	node_id		The ID of destroyed node
 */
static void cb_receive_node_destroy(const uint8_t session_id,
		const uint32_t node_id)
{
	printf("%s() session_id: %d, node_id: %d\n",
			__FUNCTION__, session_id, node_id);

	/* Everything was tested. Exit. */
	if(node_id == my_test_node_id) {
		vrs_send_connect_terminate(session_id);
	}
}

/**
 * \brief This function is callback function for receiving of command Node_Owner
 */
static void cb_receive_node_owner(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t user_id)
{
	printf("%s() session_id: %d, node_id: %d, user_id of new owner: %d\n",
			__FUNCTION__, session_id, node_id, user_id);

	if(node_id == my_test_node_id) {
		/* Try to delete "my" own node */
		vrs_send_node_destroy(session_id, my_test_node_prio, node_id);
	}
}

/**
 * \brief This function is callback function for receiving of command Node_Perm
 */
static void cb_receive_node_perm(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t user_id,
		const uint8_t perm)
{
	printf("%s() session_id: %d, node_id: %d, user_id: %d, permission: %d\n",
			__FUNCTION__, session_id, node_id, user_id, perm);

	if(user_id == my_user_id) {
		printf("\tPermission for me\n");
	} else if (user_id == 65535) {
		printf("\tPermission for other users\n");
	} else {
		printf("\tPermission for user: %d\n", user_id);
	}

	if(perm & VRS_PERM_NODE_READ) {
		printf("\tUser can read node\n");
	}

	if(perm & VRS_PERM_NODE_WRITE) {
		printf("\tUser can write node\n");
	}
}

/**
 * \brief This function is callback function for receiving of command Node_Lock
 */
static void cb_receive_node_lock(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t avatar_id)
{
	printf("%s() session_id: %d, node_id: %d, avatar_id: %d\n",
			__FUNCTION__, session_id, node_id, avatar_id);

	if(node_id == my_test_node_id && avatar_id == my_avatar_id) {
		vrs_send_node_unlock(session_id, my_test_node_prio, node_id);
	}
}

/**
 * \brief This function is callback function for receiving of command Node_UnLock
 */
static void cb_receive_node_unlock(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t avatar_id)
{
	printf("%s() session_id: %d, node_id: %d, avatar_id: %d\n",
			__FUNCTION__, session_id, node_id, avatar_id);

	if(node_id == my_test_node_id) {
		/* Test of changing owner of the node */
		vrs_send_node_owner(session_id, VRS_DEFAULT_PRIORITY, node_id, 1001);
	}
}

/**
 * \brief This function is callback function for receiving of command Node_Link.
 *
 * It means, that this function is called, when verse_callback_update
 * is executed and there is any Node_Link command in incoming queue.
 *
 * \param[in]	session_id		The ID of session with some verse server
 * \param[in]	parent_node_id	The ID of new parent node
 * \param[in]	child_node_id	The ID of child node
 */
static void cb_receive_node_link(const uint8_t session_id,
		const uint32_t parent_node_id,
		const uint32_t child_node_id)
{
	printf("%s() session_id: %d, parent_node_id: %d, child_node_id: %d\n",
			__FUNCTION__, session_id, parent_node_id, child_node_id);

	if(parent_node_id == VRS_SCENE_PARENT_NODE_ID && child_node_id == my_test_node_id) {

		/* Test of creating new Tag Group in my own node */
		vrs_send_taggroup_create(session_id, my_test_node_prio, child_node_id, MY_TEST_TAGGROUP_TYPE);

		/* Test of creating new Layer in my own node */
		vrs_send_layer_create(session_id, my_test_node_prio, my_test_node_id, 0xFFFF, VRS_VALUE_TYPE_REAL64, 4, MY_TEST_LAYER_TYPE);
	}
}

/**
 * \brief Callback function for receiving accept connect request.
 *
 * \param[in]	session_id	ID of session with the Verse server, because Verse
 * client could be connected to more Verse server.
 * \param[in]	user_id		unique ID of user.
 * \param[in]	avatar_id	unique ID of avatar.
 */
static void cb_receive_connect_accept(const uint8_t session_id,
      const uint16_t user_id,
      const uint32_t avatar_id)
{
	printf("%s() session_id: %d, user_id: %d, avatar_id: %d\n",
			__FUNCTION__, session_id, user_id, avatar_id);

	my_avatar_id = avatar_id;
	my_user_id = user_id;

	/* When client receive connect accept, then it is ready to subscribe
	 * to the root node of the node tree. Id of root node is still 0. This
	 * function is called with level 1. It means, that this client will be
	 * subscribed to the root node and its child nodes (1, 2, 3) */
	vrs_send_node_subscribe(session_id, VRS_DEFAULT_PRIORITY, 0, 0, 0);

	/* Check if server allow double subscribe? */
	vrs_send_node_subscribe(session_id, VRS_DEFAULT_PRIORITY, 1, 0, 0);

	/* Try to create new node */
	vrs_send_node_create(session_id, VRS_DEFAULT_PRIORITY, MY_TEST_NODE_TYPE);

	/* Change FPS value */
	vrs_send_fps(session_id, VRS_DEFAULT_PRIORITY+1, FPS);
}

/**
 * \brief Callback function for receiving termination of connection.
 *
 * \param[in]	session_id	ID of session with the Verse server, because Verse
 * client could be connected to more Verse server.
 * \param[in]	error_num	reason of connection termination.
 */
static void cb_receive_connect_terminate(const uint8_t session_id,
		const uint8_t error_num)
{
	printf("%s() session_id: %d, error_num: %d\n",
			__FUNCTION__, session_id, error_num);
	switch(error_num) {
	case VRS_CONN_TERM_AUTH_FAILED:
		printf("User authentication failed\n");
		break;
	case VRS_CONN_TERM_HOST_DOWN:
		printf("Host is not accesible\n");
		break;
	case VRS_CONN_TERM_HOST_UNKNOWN:
		printf("Host could not be found\n");
		break;
	case VRS_CONN_TERM_SERVER_DOWN:
		printf("Server is not running\n");
		break;
	case VRS_CONN_TERM_TIMEOUT:
		printf("Connection timout\n");
		break;
	case VRS_CONN_TERM_ERROR:
		printf("Conection with server was broken\n");
		break;
	case VRS_CONN_TERM_SERVER:
		printf("Connection was terminated by server\n");
		break;
	case VRS_CONN_TERM_CLIENT:
		printf("Connection was terminated by client\n");
		break;
	default:
		printf("Unknown error\n");
		break;
	}
	exit(EXIT_SUCCESS);
}

/**
 * \brief Callback function for user authentication.
 *
 * \details When username has zero length (this callback function is called
 * at the first time), then we have to send only username.
 * When username is not zero length, then right authentication method has to
 * be chosen (only password is supported)
 *
 * \param[in]	session_id	ID of session with the Verse server, because Verse
 * client could be connected to more Verse server.
 * \param[in]	*username	string with username. When this pointer is NULL,
 * then this is first authentication attempt.
 * \param[in]	auth_methods_count	number of authentication methods.
 * \param[in]	*methods	array of authentication methods supported by Verse server.
 */
static void cb_receive_user_authenticate(const uint8_t session_id,
		const char *username,
		const uint8_t auth_methods_count,
		const uint8_t *methods)
{
	static int attempts=0;	/* Store number of authentication attempt for this session. */
	char name[64];
	char *password;
	int i, is_passwd_supported=0;

	/* Debug print */
	printf("%s() username: %s, auth_methods_count: %d, methods: ",
			__FUNCTION__, username, auth_methods_count);
	for(i=0; i<auth_methods_count; i++) {
		printf("%d, ", methods[i]);
		if(methods[i] == VRS_UA_METHOD_PASSWORD)
			is_passwd_supported = 1;
	}
	printf("\n");

	/* Get username, when it is requested */
	if(username == NULL) {
		printf("Username: ");
		scanf("%s", name);
		attempts = 0;	/* Reset counter of auth. attempt. */
		vrs_send_user_authenticate(session_id, name, 0, NULL);
	} else {
		if(is_passwd_supported==1) {
			strcpy(name, username);
			/* Print this warning, when previous authentication attempt failed. */
			if(attempts>0)
				printf("Permission denied, please try again.\n");
			/* Get password from user */
			password = getpass("Password: ");
			attempts++;
			vrs_send_user_authenticate(session_id, name, VRS_UA_METHOD_PASSWORD, password);
		} else {
			printf("ERROR: Verse server does not support password authentication method\n");
		}
	}
}

/**
 * \brief This function set debug level of verse library
 */
static int set_debug_level(char *debug_level)
{
	int ret;
	if( strcmp(debug_level, "debug") == 0) {
		ret = vrs_set_debug_level(VRS_PRINT_DEBUG_MSG);
		return ret;
	} else if( strcmp(debug_level, "warning") == 0 ) {
		ret = vrs_set_debug_level(VRS_PRINT_WARNING);
		return ret;
	} else if( strcmp(debug_level, "error") == 0 ) {
		ret = vrs_set_debug_level(VRS_PRINT_ERROR);
		return ret;
	} else if( strcmp(debug_level, "info") == 0 ) {
		ret = vrs_set_debug_level(VRS_PRINT_INFO);
		return ret;
	} else if( strcmp(debug_level, "none") == 0 ) {
		ret = vrs_set_debug_level(VRS_PRINT_NONE);
		return ret;
	} else {
		printf("Unsupported debug level: %s\n", debug_level);
		return 0;
	}

	return 0;
}

/**
 * \brief This function print help of verse_client command (options and
 * parameters )
 */
static void print_help(char *prog_name)
{
	printf("\n Usage: %s [OPTION...] server\n\n", prog_name);
	printf("  This program is example of Verse client\n\n");
	printf("  Options:\n");
	printf("   -h               display this help and exit\n");
	printf("   -t protocol      transport protocol [udp|tcp] used for data exchange\n");
	printf("   -s security      security of data exchange [none|tls]\n");
	printf("   -c compresion    compression used for data exchange [none|addrshare]\n");
	printf("   -d debug_level   use debug level [none|info|error|warning|debug]\n\n");
}

/**
 * \brief Main function of verse_client program.
 *
 * \details This program requires one argument (address of Verse server).
 * This function set up handling of signals (SIGINIT). This function register
 * all necessary callback functions and then send connect request to the
 * server. Then callback update function is called 30 times per
 * seconds (30 FPS).
 *
 * \param	argc	number of arguments
 * \param	argv	array of arguments
 *
 * \return	Returns 0, when client is finished successfully and non-zero value,
 * when it is finished with some error.
 */
int main(int argc, char *argv[])
{
	int error_num, opt, ret, flags = VRS_SEC_DATA_NONE;

	/* When client was started with some arguments */
	if(argc>1) {
		/* Parse all options */
		while( (opt = getopt(argc, argv, "hs:t:d:")) != -1) {
			switch(opt) {
				case 's':
					if(strcmp(optarg, "none") == 0) {
						flags |= VRS_SEC_DATA_NONE;
						flags &= ~VRS_SEC_DATA_TLS;
					}else if(strcmp(optarg, "tls") == 0) {
						flags &= ~VRS_SEC_DATA_NONE;
						flags |= VRS_SEC_DATA_TLS;
					} else {
						printf("ERROR: unsupported security\n\n");
						print_help(argv[0]);
						exit(EXIT_FAILURE);
					}
					break;
				case 't':
					if(strcmp(optarg, "udp") == 0) {
						flags |= VRS_TP_UDP;
						flags &= ~VRS_TP_TCP;
					} else if(strcmp(optarg, "tcp") == 0) {
						flags &= ~VRS_TP_UDP;
						flags |= VRS_TP_TCP;
					} else {
						printf("ERROR: unsupported transport protocol\n\n");
						print_help(argv[0]);
						exit(EXIT_FAILURE);
					}
					break;
				case 'c':
					if(strcmp(optarg, "none") == 0) {
						flags |= VRS_CMD_CMPR_NONE;
						flags &= ~VRS_CMD_CMPR_ADDR_SHARE;
					} else if(strcmp(optarg, "addrshare") == 0) {
						flags &= ~VRS_CMD_CMPR_NONE;
						flags |= VRS_CMD_CMPR_ADDR_SHARE;
					} else {
						printf("ERROR: unsupported command compression\n\n");
						print_help(argv[0]);
						exit(EXIT_FAILURE);
					}
					break;
				case 'd':
					ret = set_debug_level(optarg);
					if(ret != VRS_SUCCESS) {
						print_help(argv[0]);
						exit(EXIT_FAILURE);
					}
					break;
				case 'h':
					print_help(argv[0]);
					exit(EXIT_SUCCESS);
				case ':':
					exit(EXIT_FAILURE);
				case '?':
					exit(EXIT_FAILURE);
			}
		}

		/* The last argument has to be server name, not option */
		if(optind+1 != argc) {
			printf("Error: last argument has to be server name\n\n");
			print_help(argv[0]);
			exit(EXIT_FAILURE);
		}
	} else {
		printf("Error: no server specified\n\n");
		print_help(argv[0]);
		return EXIT_FAILURE;
	}

	/* Handle SIGINT signal. The handle_signal function will try to terminate
	 * connection. */
	signal(SIGINT, handle_signal);

	/* Register callback functions */
	vrs_register_receive_user_authenticate(cb_receive_user_authenticate);
	vrs_register_receive_connect_accept(cb_receive_connect_accept);
	vrs_register_receive_connect_terminate(cb_receive_connect_terminate);

	vrs_register_receive_node_create(cb_receive_node_create);
	vrs_register_receive_node_destroy(cb_receive_node_destroy);

	vrs_register_receive_node_owner(cb_receive_node_owner);
	vrs_register_receive_node_perm(cb_receive_node_perm);

	vrs_register_receive_node_lock(cb_receive_node_lock);
	vrs_register_receive_node_unlock(cb_receive_node_unlock);

	vrs_register_receive_node_link(cb_receive_node_link);

	vrs_register_receive_taggroup_create(cb_receive_taggroup_create);
	vrs_register_receive_taggroup_destroy(cb_receive_taggroup_destroy);

	vrs_register_receive_tag_create(cb_receive_tag_create);
	vrs_register_receive_tag_destroy(cb_receive_tag_destroy);

	vrs_register_receive_tag_set_value(cb_receive_tag_set_value);

	vrs_register_receive_layer_create(cb_receive_layer_create);
	vrs_register_receive_layer_destroy(cb_receive_layer_destroy);

	vrs_register_receive_layer_set_value(cb_receive_layer_set_value);
	vrs_register_receive_layer_unset_value(cb_receive_layer_unset_value);

	/* Send connect request to the server (it will also create independent thread for connection) */
	error_num = vrs_send_connect_request(argv[optind], "12345", flags, &session_id);
	if(error_num != VRS_SUCCESS) {
		printf("ERROR: %s\n", vrs_strerror(error_num));
		return EXIT_FAILURE;
	}

	/* Never ending loop */
	while(1) {
		vrs_callback_update(session_id);
		usleep(1000000/FPS);
	}

	return EXIT_SUCCESS;
}
