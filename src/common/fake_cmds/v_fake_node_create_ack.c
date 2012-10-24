/*
 * $Id$
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

#include "v_common.h"
#include "v_fake_commands.h"

/**
 * \brief This function print content of fake command Node_Create_Ack.
 */
void v_fake_node_create_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd)
{
	struct Node_Create_Ack_Cmd *node_create_ack = (struct Node_Create_Ack_Cmd *)cmd;
	v_print_log_simple(level, "\tNode_Create_Ack: Node_ID: %d\n",
			node_create_ack->node_id);
}

/**
 * \brief This function initialize members of structure for Node_Create_Ack command
 */
static void v_fake_node_create_ack_init(struct Node_Create_Ack_Cmd *node_create_ack,
		uint32 node_id)
{
    if(node_create_ack != NULL) {
        /* initialize members with values */
    	node_create_ack->id = FAKE_CMD_NODE_CREATE_ACK;
    	node_create_ack->node_id = node_id;
    }
}

/**
 * \brief this function creates new structure of Node_Create_Ack command
 */
struct Generic_Cmd *v_fake_node_create_ack_create(uint32 node_id)
{
    struct Node_Create_Ack_Cmd *node_create_ack = NULL;
    node_create_ack = (struct Node_Create_Ack_Cmd*)calloc(1, sizeof(struct Node_Create_Ack_Cmd));
    v_fake_node_create_ack_init(node_create_ack, node_id);
    return (struct Generic_Cmd *)node_create_ack;
}

/**
 * \brief This function clear members of structure for Node_Create_Ack command
 */
static void v_fake_node_create_ack_clear(struct Node_Create_Ack_Cmd *node_create_ack)
{
    if(node_create_ack != NULL) {
        node_create_ack->node_id = -1;
    }
}

/**
 * \brief This function destroy Node_Create_Ack command
 */
void v_fake_node_create_ack_destroy(struct Generic_Cmd **cmd)
{
	struct Node_Create_Ack_Cmd **node_create_ack = (struct Node_Create_Ack_Cmd **)cmd;
    if(node_create_ack != NULL) {
        v_fake_node_create_ack_clear(*node_create_ack);
        free(*node_create_ack);
        *node_create_ack = NULL;
    }
}
