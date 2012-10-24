/*
 * $Id$
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

#include <stdlib.h>

#include "v_common.h"
#include "v_fake_commands.h"

/**
 * \brief This function print content of fake command Layer_Create_Ack.
 */
void v_fake_layer_create_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd)
{
	struct Layer_Create_Ack_Cmd *layer_create_ack = (struct Layer_Create_Ack_Cmd *)cmd;
	v_print_log_simple(level, "\tLayer_Create_Ack: Node_ID: %d, Layer_ID: %d\n",
			layer_create_ack->node_id,
			layer_create_ack->layer_id);
}

/**
 * \brief This function initialize members of structure for Layer_Create_Ack command
 */
static void v_fake_layer_create_ack_init(struct Layer_Create_Ack_Cmd *layer_create_ack,
		uint32 node_id,
		uint16 layer_id)
{
    if(layer_create_ack != NULL) {
        /* initialize members with values */
    	layer_create_ack->id = FAKE_CMD_LAYER_CREATE_ACK;
    	layer_create_ack->node_id = node_id;
    	layer_create_ack->layer_id = layer_id;
    }
}

/**
 * \brief this function creates new structure of Layer_Create_Ack command
 */
struct Generic_Cmd *v_fake_layer_create_ack_create(uint32 node_id,
		uint16 layer_id)
{
    struct Layer_Create_Ack_Cmd *layer_create_ack = NULL;
    layer_create_ack = (struct Layer_Create_Ack_Cmd*)calloc(1, sizeof(struct Layer_Create_Ack_Cmd));
    v_fake_layer_create_ack_init(layer_create_ack, node_id, layer_id);
    return (struct Generic_Cmd *)layer_create_ack;
}

/**
 * \brief This function clear members of structure for Layer_Create_Ack command
 */
static void v_fake_layer_create_ack_clear(struct Layer_Create_Ack_Cmd *layer_create_ack)
{
    if(layer_create_ack != NULL) {
        layer_create_ack->node_id = -1;
        layer_create_ack->layer_id = -1;
    }
}

/**
 * \brief This function destroy Layer_Create_Ack command
 */
void v_fake_layer_create_ack_destroy(struct Generic_Cmd **cmd)
{
	struct Layer_Create_Ack_Cmd **layer_create_ack = (struct Layer_Create_Ack_Cmd **)cmd;
    if(layer_create_ack != NULL) {
        v_fake_layer_create_ack_clear(*layer_create_ack);
        free(*layer_create_ack);
        *layer_create_ack = NULL;
    }
}

