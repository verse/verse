/*
 * $Id: v_cmd_queue.c 1268 2012-07-24 08:14:52Z jiri $
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

#include "verse_types.h"

#include "v_cmd_queue.h"
#include "v_commands.h"
#include "v_node_commands.h"
#include "v_fake_commands.h"

/**
 * \brief This function create new queue for commands with id
 *
 * \param[in] id			ID of commands in the queue
 * \param[in] copy_bucket	The flag that influence if data of commands will be copied
 * to the bucket or bucket will contain only pointer at original data.
 *
 * \return This function returns pointer at command
 */
struct VCommandQueue *v_cmd_queue_create(uint8 id, uint8 copy_bucket, uint8 fake_cmds)
{
	struct VCommandQueue *cmd_queue = NULL;

	if(id >= MIN_CMD_ID) {
		cmd_queue = v_node_cmd_queue_create(id, copy_bucket);
	} else {
		cmd_queue = v_fake_cmd_queue_create(id, copy_bucket, fake_cmds);
	}

	return cmd_queue;
}

void v_cmd_queue_destroy(struct VCommandQueue *cmd_queue)
{
	v_hash_array_destroy(&cmd_queue->cmds);
}
