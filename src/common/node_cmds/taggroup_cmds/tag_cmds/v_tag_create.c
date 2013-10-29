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
#include <string.h>

#include "v_tag_commands.h"
#include "v_commands.h"
#include "v_common.h"

extern struct Cmd_Struct cmd_struct[];

/**
 * \brief This function initialize values of structure
 */
static void _v_tag_create_init(struct Generic_Cmd *tag_create,
		const uint32 node_id,
		const uint16 taggroup_id,
		const uint16 tag_id,
		const uint8 data_type,
		const uint8 count,
		const uint16 type)
{
	if(tag_create != NULL) {
		tag_create->id = CMD_TAG_CREATE;
		UINT32(tag_create->data[0]) = node_id;
		UINT16(tag_create->data[UINT32_SIZE]) = taggroup_id;
		UINT16(tag_create->data[UINT32_SIZE + UINT16_SIZE]) = tag_id;
		UINT8(tag_create->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]) = data_type;
		UINT8(tag_create->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + UINT8_SIZE]) = count;
		UINT16(tag_create->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + UINT8_SIZE + UINT8_SIZE]) = type;
	}
}

/**
 * \brief This function creates new Tag_Create command
 */
struct Generic_Cmd *v_tag_create_create(const uint32 node_id,
		const uint16 taggroup_id,
		const uint16 tag_id,
		const uint8 data_type,
		const uint8 count,
		const uint16 type)
{
	struct Generic_Cmd *tag_create = NULL;
	tag_create = (struct Generic_Cmd *)malloc(UINT8_SIZE + cmd_struct[CMD_TAG_CREATE].size);
	_v_tag_create_init(tag_create, node_id, taggroup_id, tag_id, data_type, count, type);
	return tag_create;
}
