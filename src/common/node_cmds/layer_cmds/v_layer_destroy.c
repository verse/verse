/*
 * $Id: v_layer_destroy.c 1335 2012-09-15 11:37:26Z jiri $
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
#include <string.h>

#include "v_layer_commands.h"
#include "v_commands.h"
#include "v_common.h"

extern struct Cmd_Struct cmd_struct[];

/**
 * \brief This function initialize values of structure
 */
static void _v_layer_destroy_init(struct Generic_Cmd *layer_destroy,
		const uint32 node_id,
		const uint16 layer_id)
{
	if(layer_destroy != NULL) {
		layer_destroy->id = CMD_LAYER_DESTROY;
		UINT32(layer_destroy->data[0]) = node_id;
		UINT16(layer_destroy->data[UINT32_SIZE]) = layer_id;
	}
}

/**
 * \brief This function is like constructor of object. It allocate new memory
 * for this structure and set up values of structure.
 */
struct Generic_Cmd *v_layer_destroy_create(const uint32 node_id,
		const uint16 layer_id)
{
	struct Generic_Cmd *layer_destroy = NULL;
	layer_destroy = (struct Generic_Cmd *)malloc(UINT8_SIZE + cmd_struct[CMD_LAYER_DESTROY].size);
	_v_layer_destroy_init(layer_destroy, node_id, layer_id);
	return layer_destroy;
}

