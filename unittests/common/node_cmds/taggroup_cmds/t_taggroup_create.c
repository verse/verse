/*
 * $Id: t_taggroup_create.c 1267 2012-07-23 19:10:28Z jiri $
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
#include <check.h>

#include "v_taggroup_commands.h"
#include "v_commands.h"
#include "v_in_queue.h"
#include "v_out_queue.h"
#include "v_common.h"

/* Structure for storing testing "vectors" */
typedef struct TGC_cmd_values {
	uint32	node_id;
	uint16	taggroup_id;
	uint16	type;
} TGC_cmd_values;

#define CHUNK_NUM	2
#define CHUNK_SIZE	2

#define BLENDER		1
#define GIMP		2
#define INKSCAPE	3
#define KRITA		4

/* Testing "vectors" */
static struct TGC_cmd_values cmd_values[CHUNK_NUM][CHUNK_SIZE] = {
		{
				{10001, 10, BLENDER},
				{10001, 11, GIMP},
		},
		{
				{10001, 2, INKSCAPE},
				{10002, 3, KRITA},
		},
};

START_TEST (_test_TagGroup_Create_create)
{
	struct Generic_Cmd *taggroup_create = NULL;
	const uint32 node_id = 65538;
	const uint16 taggroup_id = 5;
	const uint16 type = 12345;

	taggroup_create = v_taggroup_create_create(node_id, taggroup_id, type);

	fail_unless( taggroup_create != NULL,
			"TagGroup_Create create failed");
	fail_unless( taggroup_create->id == CMD_TAGGROUP_CREATE,
			"TagGroup_Create OpCode: %d != %d", taggroup_create->id, CMD_TAGGROUP_CREATE);

	fail_unless( UINT32(taggroup_create->data[0]) == node_id,
			"TagGroup_Create Node_ID: %d != %d", UINT32(taggroup_create->data[0]), node_id);
	fail_unless( UINT16(taggroup_create->data[UINT32_SIZE]) == taggroup_id,
			"TagGroup_Create TagGroup_ID: %d != %d", UINT16(taggroup_create->data[UINT32_SIZE]), taggroup_id);
	fail_unless( UINT16(taggroup_create->data[UINT32_SIZE + UINT16_SIZE]) == type,
			"TagGroup_Create Type: %d != %d", UINT16(taggroup_create->data[UINT32_SIZE + UINT16_SIZE]), type);

	v_cmd_destroy(&taggroup_create);

	fail_unless( taggroup_create == NULL,
			"TagGroup_Create destroy failed");

}
END_TEST

/**
 * \brief The function for testing adding and removing TagGroup_Create commands
 * to/from queue for incoming commands
 */
START_TEST( _test_TagGroup_Create_in_queue )
{
	struct VInQueue *in_queue = v_in_queue_create();
	struct Generic_Cmd *_taggroup_create, *taggroup_create = NULL;
	uint32 node_id;
	uint16 taggroup_id;
	uint16 type;
	int i, j;

	for(i=0; i<CHUNK_NUM; i++) {
		/* Push commands to the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			node_id = cmd_values[i][j].node_id;
			taggroup_id = cmd_values[i][j].taggroup_id;
			type = cmd_values[i][j].type;

			taggroup_create = v_taggroup_create_create(node_id, taggroup_id, type);
			v_in_queue_push(in_queue, taggroup_create);
		}

		/* Pop commands from the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			node_id = cmd_values[i][j].node_id;
			taggroup_id = cmd_values[i][j].taggroup_id;
			type = cmd_values[i][j].type;

			_taggroup_create = v_in_queue_pop(in_queue);

			fail_unless( _taggroup_create != NULL,
					"TagGroup_Create pop failed");
			fail_unless( _taggroup_create->id == CMD_TAGGROUP_CREATE,
					"TagGroup_Create OpCode: %d != %d", _taggroup_create->id, CMD_TAGGROUP_CREATE);

			fail_unless( UINT32(_taggroup_create->data[0]) == node_id,
					"TagGroup_Create Node_ID: %d != %d", UINT32(_taggroup_create->data[0]), node_id);
			fail_unless( UINT16(_taggroup_create->data[UINT32_SIZE]) == taggroup_id,
					"TagGroup_Create TagGroup_ID: %d != %d", UINT16(_taggroup_create->data[UINT32_SIZE]), taggroup_id);
			fail_unless( UINT16(_taggroup_create->data[UINT32_SIZE + UINT16_SIZE]) == type,
					"TagGroup_Create Type: %d != %d",
					UINT16(_taggroup_create->data[UINT32_SIZE + UINT16_SIZE]),
					type);

			v_cmd_destroy(&_taggroup_create);

			fail_unless( _taggroup_create == NULL,
					"TagGroup_Create destroy failed");

		}
	}

	v_in_queue_destroy(&in_queue);
}
END_TEST

/**
 * \brief The function for testing adding and removing TagGroup_Create commands
 * to/from queue for outgoing commands
 */
START_TEST( _test_TagGroup_Create_out_queue )
{
	struct VOutQueue *out_queue = v_out_queue_create();
	struct Generic_Cmd *_taggroup_create, *taggroup_create = NULL;
	uint32 node_id;
	uint16 taggroup_id, count, len;
	uint16 type;
	int8 share;
	int i, j;

	for(i=0; i<CHUNK_NUM; i++) {
		/* Push commands to the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			node_id = cmd_values[i][j].node_id;
			taggroup_id = cmd_values[i][j].taggroup_id;
			type = cmd_values[i][j].type;

			taggroup_create = v_taggroup_create_create(node_id, taggroup_id, type);
			v_out_queue_push_tail(out_queue, VRS_DEFAULT_PRIORITY, (struct Generic_Cmd*)taggroup_create);
		}

		/* Pop commands from the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			node_id = cmd_values[i][j].node_id;
			taggroup_id = cmd_values[i][j].taggroup_id;
			type = cmd_values[i][j].type;
			count = 0;
			share = 0;
			len = 65535;

			_taggroup_create = v_out_queue_pop(out_queue, VRS_DEFAULT_PRIORITY, &count, &share, &len);

			fail_unless( _taggroup_create != NULL,
					"TagGroup_Create pop failed");
			fail_unless( _taggroup_create->id == CMD_TAGGROUP_CREATE,
					"TagGroup_Create OpCode: %d != %d", _taggroup_create->id, CMD_TAGGROUP_CREATE);

			fail_unless( UINT32(_taggroup_create->data[0]) == node_id,
					"TagGroup_Create Node_ID: %d != %d", UINT32(_taggroup_create->data[0]), node_id);
			fail_unless( UINT16(_taggroup_create->data[UINT32_SIZE]) == taggroup_id,
					"TagGroup_Create TagGroup_ID: %d != %d", UINT16(_taggroup_create->data[UINT32_SIZE]), taggroup_id);
			fail_unless( UINT16(_taggroup_create->data[UINT32_SIZE + UINT16_SIZE]) == type,
					"TagGroup_Create Type: %d != %d",
					UINT16(_taggroup_create->data[UINT32_SIZE + UINT16_SIZE]),
					type);

			v_cmd_destroy(&_taggroup_create);

			fail_unless( _taggroup_create == NULL,
					"TagGroup_Create destroy failed");
		}
	}

	v_out_queue_destroy(&out_queue);
}
END_TEST

/**
 * \brief This function tests packing and unpacking commands to/from buffer
 */
START_TEST( _test_TagGroup_Create_pack_unpack )
{
	struct VOutQueue *out_queue = v_out_queue_create();
	struct VInQueue *in_queue = v_in_queue_create();
	struct Generic_Cmd *_taggroup_create, *taggroup_create = NULL;
	uint32 node_id;
	uint16 taggroup_id, count, len, buffer_len, buffer_pos = 0;
	uint16 type;
	int8 share;
	int i, j, last_cmd_count=0;
	char buffer[65535] = {0,};
	uint8 last_cmd_id=-1;

	for(i=0; i<CHUNK_NUM; i++) {
		last_cmd_id = -1;
		last_cmd_count = 0;

		/* Push CHUNK_SIZE commands to the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			node_id = cmd_values[i][j].node_id;
			taggroup_id = cmd_values[i][j].taggroup_id;
			type = cmd_values[i][j].type;

			taggroup_create = v_taggroup_create_create(node_id, taggroup_id, type);
			v_out_queue_push_tail(out_queue, VRS_DEFAULT_PRIORITY, taggroup_create);
		}

		/* Pop CHUNK_SIZE commands from the queue */
		for(j=0; j<CHUNK_SIZE; j++) {
			count = 0;
			share = 0;
			len = 65535;

			_taggroup_create = v_out_queue_pop(out_queue, VRS_DEFAULT_PRIORITY, &count, &share, &len);
			v_cmd_print(VRS_PRINT_DEBUG_MSG, _taggroup_create);

			if( (_taggroup_create->id != last_cmd_id) || (last_cmd_count <= 0)) {
				if(count == 0) {
					len = v_cmd_size(_taggroup_create);
				}
				buffer_pos += v_cmd_pack(&buffer[buffer_pos], _taggroup_create, len, share);
				last_cmd_count = count;
			} else {
				buffer_pos += v_cmd_pack(&buffer[buffer_pos], _taggroup_create, 0, share);
			}

			last_cmd_id = _taggroup_create->id;

			v_cmd_destroy(&_taggroup_create);

			last_cmd_count--;
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
			node_id = cmd_values[i][j].node_id;
			taggroup_id = cmd_values[i][j].taggroup_id;
			type = cmd_values[i][j].type;

			_taggroup_create = v_in_queue_pop(in_queue);

			fail_unless( _taggroup_create != NULL,
					"TagGroup_Create pop failed");
			fail_unless( _taggroup_create->id == CMD_TAGGROUP_CREATE,
					"TagGroup_Create OpCode: %d != %d", _taggroup_create->id, CMD_TAGGROUP_CREATE);

			fail_unless( UINT32(_taggroup_create->data[0]) == node_id,
					"TagGroup_Create Node_ID: %d != %d", UINT32(_taggroup_create->data[0]), node_id);
			fail_unless( UINT16(_taggroup_create->data[UINT32_SIZE]) == taggroup_id,
					"TagGroup_Create TagGroup_ID: %d != %d", UINT16(_taggroup_create->data[UINT32_SIZE]), taggroup_id);
			fail_unless( UINT16(_taggroup_create->data[UINT32_SIZE + UINT16_SIZE]) == type,
					"TagGroup_Create Type: %d != %d",
					UINT16(_taggroup_create->data[UINT32_SIZE + UINT16_SIZE]),
					type);

			v_cmd_destroy(&_taggroup_create);
		}
	}

	v_in_queue_destroy(&in_queue);
	v_out_queue_destroy(&out_queue);
}
END_TEST

/**
 * \brief This function creates test suite for TagGroup_Create command
 */
struct Suite *taggroup_create_suite(void)
{
	struct Suite *suite = suite_create("TagGroup_Create_Cmd");
	struct TCase *tc_core = tcase_create("Core");

	tcase_add_test(tc_core, _test_TagGroup_Create_create);
	tcase_add_test(tc_core, _test_TagGroup_Create_in_queue);
	tcase_add_test(tc_core, _test_TagGroup_Create_out_queue);
	tcase_add_test(tc_core, _test_TagGroup_Create_pack_unpack);

	suite_add_tcase(suite, tc_core);

	return suite;
}
