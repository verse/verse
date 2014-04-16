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

#include "v_node_commands.h"
#include "v_commands.h"
#include "v_common.h"

extern struct Cmd_Struct cmd_struct[];

/**
 * \brief This function initialize members of structure for Node_UnSubscribe command
 */
static void _v_node_unsubscribe_init(struct Generic_Cmd *node_unsubscribe,
		const uint32 node_id,
		const uint32 version,
		const uint32 crc32)
{
	if(node_unsubscribe != NULL) {
		/* initialize members with values */
		node_unsubscribe->id = CMD_NODE_UNSUBSCRIBE;
		UINT32(node_unsubscribe->data[0]) = node_id;
		UINT32(node_unsubscribe->data[UINT32_SIZE]) = version;
		UINT32(node_unsubscribe->data[UINT32_SIZE + UINT32_SIZE]) = crc32;
	}
}

/**
 * \brief this function creates new structure of Node_Unsubscribe command
 */
struct Generic_Cmd *v_node_unsubscribe_create(uint32 node_id,
		const uint32 version,
		const uint32 crc32)
{
	struct Generic_Cmd *node_unsubscribe = NULL;
	node_unsubscribe = (struct Generic_Cmd *)malloc(UINT8_SIZE + cmd_struct[CMD_NODE_UNSUBSCRIBE].size);
	_v_node_unsubscribe_init(node_unsubscribe, node_id, version, crc32);
	return node_unsubscribe;
}
