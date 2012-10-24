/*
 * $Id: v_node_commands.h 1268 2012-07-24 08:14:52Z jiri $
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2010, Jiri Hnidek
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
 * authors: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

#if !defined V_NODE_COMMAND_H
#define V_NODE_COMMAND_H

#include <stdio.h>
#include <sys/types.h>

#include "verse_types.h"

struct Generic_Cmd *v_node_create_create(uint32 node_id,
		uint32 parent_id,
		uint16 user_id,
		uint16 type);

struct Generic_Cmd *v_node_destroy_create(uint32 node_id);

struct Generic_Cmd *v_node_subscribe_create(uint32 node_id,
		uint32 version,
		uint32 crc32);

struct Generic_Cmd *v_node_unsubscribe_create(uint32 node_id,
		const uint32 version,
		const uint32 crc32);

struct Generic_Cmd *v_node_prio_create(uint32 node_id, uint8 prio);

struct Generic_Cmd *v_node_link_create(uint32 parent_node_id, uint32 child_node_id);

struct Generic_Cmd *v_node_perm_create(uint32 node_id, uint16 user_id, uint8 permissions);

struct Generic_Cmd *v_node_owner_create(uint32 node_id, uint16 user_id);

struct Generic_Cmd *v_node_lock_create(uint32 node_id, uint32 avatar_id);

struct Generic_Cmd *v_node_unlock_create(uint32 node_id, uint32 avatar_id);

#endif
