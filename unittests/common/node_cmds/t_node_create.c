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

#include <check.h>

#include "v_common.h"
#include "v_commands.h"
#include "v_node_commands.h"
#include "v_in_queue.h"
#include "v_out_queue.h"

#define CHUNK_NUM	3
#define CHUNK_SIZE	4

/* Structure for storing testing "vectors" */
typedef struct NC_cmd_values {
	uint16	user_id;
	uint32	parent_id;
	uint32	node_id;
	uint16	type;
} NC_cmd_values;

/* Testing "vectors" */
static struct NC_cmd_values cmd_values[CHUNK_NUM][CHUNK_SIZE] = {
		/* UserID and ParentID is shared */
		{
			{1001, 1, 501, 301},
			{1001, 1, 502, 302},
			{1001, 1, 503, 303},
			{1001, 1, 504, 304}
		},
		/* UserID is shared */
		{
			{1002, 2, 503, 300},
			{1002, 3, 504, 300},
			{1002, 4, 505, 300},
			{1002, 5, 506, 300}
		},
		/* Nothing is shared */
		{
			{1003, 4, 505, 301},
			{1004, 5, 506, 301},
			{1005, 6, 507, 301},
			{1006, 7, 508, 301}
		}
};

/**
 * \brief Test simple creation and destroying of Node_Create command
 */
START_TEST ( test_Node_Create_create )
{
	struct Generic_Cmd *node_create = NULL;
	const uint32 node_id = 65538;
	const uint32 parent_id = 65535;
	const uint16 user_id = 1001;
	const uint16 type = 333;

	node_create = v_node_create_create(node_id, parent_id, user_id, type);

	fail_unless( node_create != NULL,
			"Node_Create create failed");
	fail_unless( node_create->id == CMD_NODE_CREATE,
			"Node_Create OpCode: %d != %d", node_create->id, CMD_NODE_CREATE);

	fail_unless( UINT16(node_create->data[0]) == user_id,
			"Node_Create User_ID: %d != %d", UINT16(node_create->data[0]), user_id);
	fail_unless( UINT32(node_create->data[2]) == parent_id,
			"Node_Create Parent_ID: %d != %d", UINT32(node_create->data[2]), parent_id);
	fail_unless( UINT32(node_create->data[2+4]) == node_id,
			"Node_Create Node_ID: %d != %d", UINT32(node_create->data[2+4]), node_id);
	fail_unless( UINT16(node_create->data[2+4+4]) == type,
			"Node_Create Type: %d != %d", UINT16(node_create->data[2+4+4]), type);

	v_cmd_destroy(&node_create);

	fail_unless( node_create == NULL,
			"Node_Create destroy failed");
}
END_TEST

/**
 * \brief Test adding and removing Node_Create commands to queue intended for
 * incoming commands.
 */
START_TEST( test_Node_Create_in_queue )
{
	struct VInQueue *in_queue = v_in_queue_create();
	struct Generic_Cmd *_node_create, *node_create = NULL;
	uint32 node_id;
	uint32 parent_id;
	uint16 user_id, type;
	int i, j;

	for(i=0; i<CHUNK_NUM; i++) {
		/* Push commands to the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			user_id = cmd_values[i][j].user_id;
			parent_id = cmd_values[i][j].parent_id;
			node_id = cmd_values[i][j].node_id;
			type = cmd_values[i][j].type;

			node_create = v_node_create_create(node_id, parent_id, user_id, type);
			v_in_queue_push(in_queue, node_create);
		}

		/* Pop commands from the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			user_id = cmd_values[i][j].user_id;
			parent_id = cmd_values[i][j].parent_id;
			node_id = cmd_values[i][j].node_id;
			type = cmd_values[i][j].type;

			_node_create = v_in_queue_pop(in_queue);

			fail_unless( _node_create != NULL,
					"Node_Create pop failed");
			fail_unless( _node_create->id == CMD_NODE_CREATE,
					"Node_Create OpCode: %d != %d", _node_create->id, CMD_NODE_CREATE);

			fail_unless( UINT16(_node_create->data[0]) == user_id,
					"Node_Create User_ID: %d != %d", UINT16(_node_create->data[0]), user_id);
			fail_unless( UINT32(_node_create->data[2]) == parent_id,
					"Node_Create Parent_ID: %d != %d", UINT32(_node_create->data[2]), parent_id);
			fail_unless( UINT32(_node_create->data[2+4]) == node_id,
					"Node_Create Node_ID: %d != %d", UINT32(_node_create->data[2+4]), node_id);
			fail_unless( UINT16(_node_create->data[2+4+4]) == type,
					"Node_Create Type: %d != %d", UINT16(_node_create->data[2+4+4]), type);

			v_cmd_destroy(&_node_create);

			fail_unless( _node_create == NULL,
					"Node_Create destroy failed");
		}
	}

	v_in_queue_destroy(&in_queue);
}
END_TEST

/**
 * \brief Test adding Node_Create command to queue for outgoing commands.
 */
START_TEST( test_Node_Create_out_queue )
{
	struct VOutQueue *out_queue = v_out_queue_create();
	struct Generic_Cmd *_node_create, *node_create = NULL;
	uint32 node_id;
	uint32 parent_id;
	uint16 user_id, type, count, len;
	int8 share;
	int i, j;

	for(i=0; i<CHUNK_NUM; i++) {
		/* Push commands to the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			user_id = cmd_values[i][j].user_id;
			parent_id = cmd_values[i][j].parent_id;
			node_id = cmd_values[i][j].node_id;
			type = cmd_values[i][j].type;

			node_create = v_node_create_create(node_id, parent_id, user_id, type);
			v_out_queue_push_tail(out_queue, VRS_DEFAULT_PRIORITY, (struct Generic_Cmd*)node_create);
		}

		/* Pop commands from the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			user_id = cmd_values[i][j].user_id;
			parent_id = cmd_values[i][j].parent_id;
			node_id = cmd_values[i][j].node_id;
			type = cmd_values[i][j].type;

			count = 0;
			share = 0;
			len = 65535;

			_node_create = v_out_queue_pop(out_queue, VRS_DEFAULT_PRIORITY, &count, &share, &len);

			fail_unless( _node_create != NULL,
					"Node_Create pop failed");
			fail_unless( _node_create->id == CMD_NODE_CREATE,
					"Node_Create OpCode: %d != %d", _node_create->id, CMD_NODE_CREATE);

			fail_unless( UINT16(_node_create->data[0]) == user_id,
					"Node_Create User_ID: %d != %d", UINT16(_node_create->data[0]), user_id);
			fail_unless( UINT32(_node_create->data[2]) == parent_id,
					"Node_Create Parent_ID: %d != %d", UINT32(_node_create->data[2]), parent_id);
			fail_unless( UINT32(_node_create->data[2+4]) == node_id,
					"Node_Create Node_ID: %d != %d", UINT32(_node_create->data[2+4]), node_id);
			fail_unless( UINT16(_node_create->data[2+4+4]) == type,
					"Node_Create Type: %d != %d", UINT16(_node_create->data[2+4+4]), type);

			v_cmd_destroy((struct Generic_Cmd**)&_node_create);
		}
	}

	v_out_queue_destroy(&out_queue);
}
END_TEST

/**
 * \brief Test packing and unpacking Node_Create commands
 */
START_TEST( test_Node_Create_pack_unpack )
{
	struct VOutQueue *out_queue = v_out_queue_create();
	struct VInQueue *in_queue = v_in_queue_create();
	struct Generic_Cmd *_node_create, *node_create = NULL;
	uint32 node_id;
	uint32 parent_id;
	uint16 user_id, type, count, len, buffer_len, buffer_pos = 0;
	int8 share;
	int i, j;
	char buffer[65535] = {0,};
	uint8 last_cmd_id;

	for(i=0; i<CHUNK_NUM; i++) {
		last_cmd_id = -1;
		/* Push CHUNK_SIZE commands to the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			user_id = cmd_values[i][j].user_id;
			parent_id = cmd_values[i][j].parent_id;
			node_id = cmd_values[i][j].node_id;
			type = cmd_values[i][j].type;

			node_create = v_node_create_create(node_id, parent_id, user_id, type);
			v_out_queue_push_tail(out_queue, VRS_DEFAULT_PRIORITY, node_create);
		}

		/* Pop CHUNK_SIZE commands from the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			count = 0;
			share = 0;
			len = 65535;

			_node_create = v_out_queue_pop(out_queue, VRS_DEFAULT_PRIORITY, &count, &share, &len);

			if(_node_create->id != last_cmd_id) {
				if(count == 0) {
					len = v_cmd_size(_node_create);
				}
				buffer_pos += v_cmd_pack(&buffer[buffer_pos], _node_create, len, share);
			} else {
				buffer_pos += v_cmd_pack(&buffer[buffer_pos], _node_create, 0, share);
			}

			last_cmd_id = _node_create->id;

			v_cmd_destroy(&_node_create);
		}
	}

	fail_unless( buffer_pos > 0,
			"Buffer size is zero");

	/* Buffer is sent and received at this point */
	buffer_len = buffer_pos;
	buffer_pos = 0;

	/* Unpack commands from the buffer and push them to the queue of incoming
	 * commands*/
	buffer_pos += v_cmd_unpack(&buffer[buffer_pos], buffer_len, in_queue);

	fail_unless( buffer_pos == buffer_len,
			"Unpacked buffer size: %d != packed buffer size: %d", buffer_pos, buffer_len);

	for(i=0; i<CHUNK_NUM; i++) {
		/* Pop commands from the queue of incoming commands */
		for(j=0; j<CHUNK_SIZE; j++) {
			user_id = cmd_values[i][j].user_id;
			parent_id = cmd_values[i][j].parent_id;
			node_id = cmd_values[i][j].node_id;
			type = cmd_values[i][j].type;

			_node_create = v_in_queue_pop(in_queue);

			fail_unless( _node_create != NULL,
					"Node_Create pop failed");
			fail_unless( _node_create->id == CMD_NODE_CREATE,
					"Node_Create OpCode: %d != %d", _node_create->id, CMD_NODE_CREATE);

			fail_unless( UINT16(_node_create->data[0]) == user_id,
					"Node_Create User_ID: %d != %d", UINT16(_node_create->data[0]), user_id);
			fail_unless( UINT32(_node_create->data[2]) == parent_id,
					"Node_Create Parent_ID: %d != %d", UINT32(_node_create->data[2]), parent_id);
			fail_unless( UINT32(_node_create->data[2+4]) == node_id,
					"Node_Create Node_ID: %d != %d", UINT32(_node_create->data[2+4]), node_id);
			fail_unless( UINT16(_node_create->data[2+4+4]) == type,
					"Node_Create Type: %d != %d", UINT16(_node_create->data[2+4+4]), type);

			v_cmd_destroy(&_node_create);
		}
	}

	v_in_queue_destroy(&in_queue);
	v_out_queue_destroy(&out_queue);
}
END_TEST

/**
 * \brief This function creates test suite for Node_Create command
 */
struct Suite *node_create_suite(void)
{
	struct Suite *suite = suite_create("Node_Create_Cmd");
	struct TCase *tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_Node_Create_create);
	tcase_add_test(tc_core, test_Node_Create_in_queue);
	tcase_add_test(tc_core, test_Node_Create_out_queue);
	tcase_add_test(tc_core, test_Node_Create_pack_unpack);

	suite_add_tcase(suite, tc_core);

	return suite;
}

