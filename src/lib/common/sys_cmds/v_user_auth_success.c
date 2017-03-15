/*
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

#include "v_sys_commands.h"
#include "v_commands.h"
#include "v_common.h"
#include "v_pack.h"
#include "v_unpack.h"

/**
 * \brief This function prints content of the USER_AUTH_SUCCESS command
 */
void v_print_user_auth_success(const unsigned char level, struct User_Authentication_Success *ua_suc)
{
	v_print_log_simple(level, "\tUSER_AUTH_SUCCESS, UserID: %d, AvatarID: %d\n",
			ua_suc->user_id, ua_suc->avatar_id);
}

/**
 * \brief This function unpacks USER_AUTH_SUCESS command from the buffer
 */
int v_raw_unpack_user_auth_success(const char *buffer,
		ssize_t buffer_size,
		struct User_Authentication_Success *ua_suc)
{
	unsigned short buffer_pos = 0;
	unsigned char length;

	/* Check if buffer size is bigger then minimal size of user authentication
	 * success command*/
	if(buffer_size < 1+1+2+4) {
		v_print_log(VRS_PRINT_WARNING, "Buffer size: %d < minimal command length: %d\n.", buffer_size, 1+1+2+4);
		ua_suc->user_id = VRS_RESERVED_USER_ID;
		ua_suc->avatar_id = VRS_RESERVED_AVATAR_ID;
		return buffer_size;
	}
	/* Unpack command ID */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &ua_suc->id);

	/* Unpack length of the command (It has to be always 2!) */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &length);

	/* Check if length and buffer_pos match (It has to be always 8!) */
	if(length!=(1+1+2+4)) {
		v_print_log(VRS_PRINT_WARNING, "%s: buffer_pos: %d != length: %d\n",
				__func__, buffer_pos, length);
		ua_suc->user_id = VRS_RESERVED_USER_ID;
		ua_suc->avatar_id = VRS_RESERVED_AVATAR_ID;
		return (1+1+2+4);
	}

	/* Unpack User_ID */
	buffer_pos += vnp_raw_unpack_uint16(&buffer[buffer_pos], &ua_suc->user_id);

	/* Unpack Avatar_ID */
	buffer_pos += vnp_raw_unpack_uint32(&buffer[buffer_pos], &ua_suc->avatar_id);

	/* Check if length and buffer_pos match (It has to be always 8!) */
	if(buffer_pos!=length) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s: buffer_pos: %d != length: %d\n",
				__func__, buffer_pos, length);
		return (1+1+2+4);
	}

	return buffer_pos;
}

/**
 * \brief This function packs USER_AUTH_SUCCESS command to the buffer
 */
int v_raw_pack_user_auth_success(char *buffer, const struct User_Authentication_Success *ua_suc)
{
	unsigned short buffer_pos = 0;

	/* Pack command ID */
	buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], ua_suc->id);
	/* Pack length of the command */
	buffer_pos += v_cmd_pack_len(&buffer[buffer_pos], 1+1+2+4);
	/* Pack User_ID */
	buffer_pos += vnp_raw_pack_uint16(&buffer[buffer_pos], ua_suc->user_id);
	/* Pack Avatar_ID */
	buffer_pos += vnp_raw_pack_uint32(&buffer[buffer_pos], ua_suc->avatar_id);

	return buffer_pos;
}
