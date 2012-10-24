/*
 * $Id: v_node_create.c 1267 2012-07-23 19:10:28Z jiri $
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
 * \brief This function initialize members of structure for Node_Create command
 */
static void _v_node_create_init(struct Generic_Cmd *node_create,
		uint32 node_id,
		uint32 parent_id,
		uint16 user_id,
		uint16 type)
{
	if(node_create != NULL) {
		/* initialize members with values */
		node_create->id = CMD_NODE_CREATE;
		UINT16(node_create->data[0]) = user_id;
		UINT32(node_create->data[UINT16_SIZE]) = parent_id;
		UINT32(node_create->data[UINT16_SIZE + UINT32_SIZE]) = node_id;
		UINT16(node_create->data[UINT16_SIZE + UINT32_SIZE + UINT32_SIZE]) = type;
	}
}

/**
 * \brief this function creates new structure of Node_Create command
 */
struct Generic_Cmd *v_node_create_create(uint32 node_id,
		uint32 parent_id,
		uint16 user_id,
		uint16 type)
{
	struct Generic_Cmd *node_create = NULL;
	node_create = (struct Generic_Cmd *)malloc(UINT8_SIZE + cmd_struct[CMD_NODE_CREATE].size);
	_v_node_create_init(node_create, node_id, parent_id, user_id, type);
	return node_create;
}
