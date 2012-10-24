/*
 * $Id: v_tag_set.c 1276 2012-07-27 19:53:27Z jiri $
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
#include <string.h>

#include <assert.h>

#include "v_tag_commands.h"
#include "v_commands.h"
#include "v_common.h"

extern struct Cmd_Struct cmd_struct[];

/**
 * \brief This function initialize values of command Tag_Set
 */
struct Generic_Cmd *v_tag_set_create(const uint32 node_id,
		const uint16 taggroup_id,
		const uint16 tag_id,
		const uint8 value_type,
		const uint8 count,
		const void *value)
{
	int i, cmd_id;
	struct Generic_Cmd *tag_set;

	assert(count<=4);

	/* Tricky part :-) */
	cmd_id = CMD_TAG_SET_UINT8 + 4*(value_type-1) + (count-1);

	tag_set = (struct Generic_Cmd *)malloc(UINT8_SIZE +
			cmd_struct[cmd_id].size);

	tag_set->id = cmd_id;
	UINT32(tag_set->data[0]) = node_id;
	UINT16(tag_set->data[UINT32_SIZE]) = taggroup_id;
	UINT16(tag_set->data[UINT32_SIZE + UINT16_SIZE]) = tag_id;

	switch(value_type) {
	case VRS_VALUE_TYPE_UINT8:
		for(i=0; i<count; i++) {
			UINT8(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + i*INT8_SIZE]) = ((uint8*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_UINT16:
		for(i=0; i<count; i++) {
			UINT16(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + i*INT16_SIZE]) = ((uint16*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_UINT32:
		for(i=0; i<count; i++) {
			UINT32(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE  + i*INT32_SIZE]) = ((uint32*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_UINT64:
		for(i=0; i<count; i++) {
			UINT64(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + i*INT64_SIZE]) = ((uint64*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_REAL16:
		for(i=0; i<count; i++) {
			REAL16(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + i*REAL16_SIZE]) = ((real16*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_REAL32:
		for(i=0; i<count; i++) {
			REAL32(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + i*REAL32_SIZE]) = ((real32*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_REAL64:
		for(i=0; i<count; i++) {
			REAL64(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + i*REAL64_SIZE]) = ((real64*)value)[i];
		}
		break;
	case VRS_VALUE_TYPE_STRING8:
		if((char*)value != NULL) {
			char *name = (char*)value;
			size_t string8_len = strlen(name);

			/* The length of the name can't be longer then 255 bytes. Crop it, when it
			 * is necessary */
			if(string8_len > VRS_STRING8_MAX_SIZE) {
				v_print_log(VRS_PRINT_WARNING, "Cropping string8 %s(length:%d) to length: %d\n",
						name, string8_len, VRS_STRING8_MAX_SIZE);
				string8_len = VRS_STRING8_MAX_SIZE;
			}
			PTR(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]) = strndup(name, string8_len);
		}
		break;
	}

	return tag_set;
}
