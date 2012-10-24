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
 * \brief This function print content of fake command Tag_Create_Ack.
 */
void v_fake_tag_create_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd)
{
	struct Tag_Create_Ack_Cmd *tag_create_ack = (struct Tag_Create_Ack_Cmd *)cmd;
	v_print_log_simple(level, "\tTag_Create_Ack: Node_ID: %d, TagGroup_ID: %d, Tag_ID %d\n",
			tag_create_ack->node_id, tag_create_ack->taggroup_id, tag_create_ack->tag_id);
}

/**
 * \brief This function initialize members of structure for Tag_Create_Ack command
 */
static void v_tag_create_ack_init(struct Tag_Create_Ack_Cmd *tag_create_ack,
		uint32 node_id,
		uint16 taggroup_id,
		uint16 tag_id)
{
    if(tag_create_ack != NULL) {
        /* initialize members with values */
    	tag_create_ack->id = FAKE_CMD_TAG_CREATE_ACK;
    	tag_create_ack->node_id = node_id;
    	tag_create_ack->taggroup_id = taggroup_id;
    	tag_create_ack->tag_id = tag_id;
    }
}

/**
 * \brief this function creates new structure of Tag_Create_Ack command
 */
struct Generic_Cmd *v_tag_create_ack_create(uint32 node_id,
		uint16 taggroup_id,
		uint16 tag_id)
{
    struct Tag_Create_Ack_Cmd *tag_create_ack = NULL;
    tag_create_ack = (struct Tag_Create_Ack_Cmd*)calloc(1, sizeof(struct Tag_Create_Ack_Cmd));
    v_tag_create_ack_init(tag_create_ack, node_id, taggroup_id, tag_id);
    return (struct Generic_Cmd *)tag_create_ack;
}

/**
 * \brief This function clear members of structure for Tag_Create_Ack command
 */
static void v_tag_create_ack_clear(struct Tag_Create_Ack_Cmd *tag_create_ack)
{
    if(tag_create_ack != NULL) {
        tag_create_ack->node_id = -1;
        tag_create_ack->taggroup_id = -1;
        tag_create_ack->tag_id = -1;
    }
}

/**
 * \brief This function destroy Tag_Create_Ack command
 */
void v_tag_create_ack_destroy(struct Generic_Cmd **cmd)
{
	struct Tag_Create_Ack_Cmd **tag_create_ack = (struct Tag_Create_Ack_Cmd **)cmd;
    if(tag_create_ack != NULL) {
        v_tag_create_ack_clear(*tag_create_ack);
        free(*tag_create_ack);
        *tag_create_ack = NULL;
    }
}
