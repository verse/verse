/*
 * $Id$
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
 * \brief This function prints content of the USER_AUTH_FAILURE command
 */
void v_print_user_auth_failure(const unsigned char level, struct User_Authentication_Failure *ua_fail)
{
	int i;

	v_print_log_simple(level, "\tUSER_AUTH_FAILURE, ");

	if(ua_fail->count>0) {
		v_print_log_simple(level, "Methods: ");
		for(i=0; i<ua_fail->count; i++) {
			switch(ua_fail->method[i]) {
				case VRS_UA_METHOD_RESERVED:
					v_print_log_simple(level, "Reserved, ");
					break;
				case VRS_UA_METHOD_NONE:
					v_print_log_simple(level, "None, ");
					break;
				case VRS_UA_METHOD_PASSWORD:
					v_print_log_simple(level, "Password, ");
					break;
			}
		}
	} else {
		v_print_log_simple(level, "Access not granted.");
	}
	v_print_log_simple(level, "\n");
}

/**
 * \brief This function unpacks USER_AUTH_FAILURE command from the buffer
 */
int v_raw_unpack_user_auth_failure(const char *buffer,
		ssize_t buffer_size,
		struct User_Authentication_Failure *ua_fail)
{
	unsigned short buffer_pos = 0;
	unsigned char length;
	int i;

	/* Check if buffer size is bigger then minimal size of user auth failure
	 * command*/
	if(buffer_size < 1+1) {
		v_print_log(VRS_PRINT_WARNING, "Buffer size: %d < minimal command length: %d\n.", buffer_size, 1+1);
		ua_fail->count = 0;
		return buffer_size;
	}

	/* Unpack command ID */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &ua_fail->id);

	/* Unpack length of the command */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &length);

	/* Security check: check if the length of the command is not bigger then
	 * size of received buffer */
	if(buffer_size < length) {
		v_print_log(VRS_PRINT_WARNING, "Buffer size: %d < command length: %d\n.", buffer_size, length);
		ua_fail->count = 0;
		return buffer_size;
	}

	/* Compute count of proposed user authentication methods */
	ua_fail->count = length - 1 - 1;

	/* Save proposed methods to the array */
	for(i=0; i<ua_fail->count && i < VRS_MAX_UA_METHOD_COUNT; i++) {
		buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &ua_fail->method[i]);
	}
	/* Update count of the proceeded method types */
	ua_fail->count = i;

	/* Check if length and buffer_pos match */
	if(buffer_pos!=length) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s: buffer_pos: %d != length: %d\n",
				__FUNCTION__, buffer_pos, length);
		return length;
	}

	return buffer_pos;
}

/**
 * \brief This function packs USER_AUTH_FAILURE command to the buffer
 */
int v_raw_pack_user_auth_failure(char *buffer, const struct User_Authentication_Failure *ua_fail)
{
	int i, count;
	unsigned short buffer_pos = 0;
	unsigned char length;

	/* Pack command ID */
	buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], ua_fail->id);

	/* Compute length of the command */
	count = (ua_fail->count<=VRS_MAX_UA_METHOD_COUNT) ? ua_fail->count : VRS_MAX_UA_METHOD_COUNT;

	/* Compute length of the command: id_len + cmd_len + count */
	length = 1 + 1 + count;

	/* Pack length of the command */
	buffer_pos += v_cmd_pack_len(&buffer[buffer_pos], length);

	/* Pack list of supported methods */
	for(i=0; i<count; i++) {
		buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], ua_fail->method[i]);
	}

	/* Check if length and buffer_pos match */
	if(buffer_pos!=length) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s: buffer_pos: %d != length: %d\n",
				__FUNCTION__, buffer_pos, length);
		return length;
	}

	return buffer_pos;
}
