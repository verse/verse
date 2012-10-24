/*
 * $Id: v_layer_commands.h 1331 2012-09-14 07:21:35Z jiri $
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

#ifndef V_LAYER_COMMANDS_H_
#define V_LAYER_COMMANDS_H_

#include "verse_types.h"
#include "v_commands.h"

struct Generic_Cmd *v_layer_create_create(const uint32 node_id,
		const uint16 parent_layer_id,
		const uint16 layer_id,
		const uint8 data_type,
		const uint8 count,
		const uint16 type);

struct Generic_Cmd *v_layer_destroy_create(const uint32 node_id,
		const uint16 layer_id);

struct Generic_Cmd *v_layer_subscribe_create(const uint32 node_id,
		const uint16 layer_id,
		const uint32 version,
		const uint32 crc32);

struct Generic_Cmd *v_layer_unsubscribe_create(const uint32 node_id,
		const uint16 layer_id,
		const uint32 version,
		const uint32 crc32);

struct Generic_Cmd *v_layer_set_value_create(const uint32 node_id,
		const uint16 layer_id,
		const uint32 item_id,
		const uint8 value_type,
		const uint8 count,
		const void *value);

struct Generic_Cmd *v_layer_unset_value_create(const uint32 node_id,
		const uint16 layer_id,
		const uint32 item_id);

#endif /* V_LAYER_COMMANDS_H_ */
