/*
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

#include <assert.h>

#include "v_layer_commands.h"
#include "v_commands.h"
#include "v_common.h"

extern struct Cmd_Struct cmd_struct[];

/**
 * \brief This function initialize values of command Tag_Set
 */
struct Generic_Cmd *v_layer_set_value_create(const uint32 node_id,
		const uint16 layer_id,
		const uint32 item_id,
		const uint8 data_type,
		const uint8 count,
		const void *value)
{
	int i, cmd_id;
	struct Generic_Cmd *layer_set;

	assert(count<=4);

	/* Tricky part :-) */
	cmd_id = CMD_LAYER_SET_UINT8 + 4*(data_type-1) + (count-1);

	layer_set = (struct Generic_Cmd *)malloc(UINT8_SIZE +
			cmd_struct[cmd_id].size);

	if(layer_set == NULL) {
		v_print_log(VRS_PRINT_ERROR, "Out of memory\n");
		return NULL;
	}

	layer_set->id = cmd_id;
	UINT32(layer_set->data[0]) = node_id;
	UINT16(layer_set->data[UINT32_SIZE]) = layer_id;
	UINT32(layer_set->data[UINT32_SIZE + UINT16_SIZE]) = item_id;

	switch(data_type) {
	case VRS_VALUE_TYPE_UINT8:
		for(i=0; i<count; i++) {
			UINT8(layer_set->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE + i*INT8_SIZE]) = ((uint8*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_UINT16:
		for(i=0; i<count; i++) {
			UINT16(layer_set->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE + i*INT16_SIZE]) = ((uint16*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_UINT32:
		for(i=0; i<count; i++) {
			UINT32(layer_set->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE  + i*INT32_SIZE]) = ((uint32*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_UINT64:
		for(i=0; i<count; i++) {
			UINT64(layer_set->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE + i*INT64_SIZE]) = ((uint64*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_REAL16:
		for(i=0; i<count; i++) {
			REAL16(layer_set->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE + i*REAL16_SIZE]) = ((real16*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_REAL32:
		for(i=0; i<count; i++) {
			REAL32(layer_set->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE + i*REAL32_SIZE]) = ((real32*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_REAL64:
		for(i=0; i<count; i++) {
			REAL64(layer_set->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE + i*REAL64_SIZE]) = ((real64*)value)[i];
		}
		break;
	}

	return layer_set;
}
