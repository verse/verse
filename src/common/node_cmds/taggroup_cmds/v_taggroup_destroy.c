/*
 * $Id: v_taggroup_destroy.c 984 2011-09-09 13:28:25Z jiri $
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

#include "v_taggroup_commands.h"
#include "v_commands.h"

extern struct Cmd_Struct cmd_struct[];

/**
 * \brief This function initialize structure of TagGroup_Destroy command
 */
static void _v_taggroup_destroy_init(struct Generic_Cmd *taggroup_destroy,
		uint32 node_id,
		uint16 taggroup_id)
{
	if(taggroup_destroy != NULL) {
		taggroup_destroy->id = CMD_TAGGROUP_DESTROY;
		UINT32(taggroup_destroy->data[0]) = node_id;
		UINT16(taggroup_destroy->data[UINT32_SIZE]) = taggroup_id;
	}
}

/**
 * \brief This function creates new structure of TagGroup_Destroy command
 */
struct Generic_Cmd *v_taggroup_destroy_create(uint32 node_id,
		uint16 taggroup_id)
{
	struct Generic_Cmd *taggroup_destroy = NULL;
	taggroup_destroy = (struct Generic_Cmd*)malloc(UINT8_SIZE + cmd_struct[CMD_TAGGROUP_DESTROY].size);
	_v_taggroup_destroy_init(taggroup_destroy, node_id, taggroup_id);
	return taggroup_destroy;
}
