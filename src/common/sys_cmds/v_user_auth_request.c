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

#include <string.h>

#include "v_sys_commands.h"
#include "v_commands.h"
#include "v_common.h"
#include "v_pack.h"
#include "v_unpack.h"

/**
 * \brief This function prints USER_AUTH_REQUEST command
 */
void v_print_user_auth_request(const unsigned char level, struct User_Authentication_Request *ua_request_cmd)
{
	char hidden_password[VRS_MAX_DATA_LENGTH];
	int i;

	v_print_log_simple(level, "\tUSER_AUTH_REQUEST, ");
	v_print_log_simple(level, "Username: %s, ", ua_request_cmd->username);
	v_print_log_simple(level, "Type: ");
	switch(ua_request_cmd->method_type) {
		case VRS_UA_METHOD_RESERVED:
			v_print_log_simple(level, "Reserved");
			break;
		case VRS_UA_METHOD_NONE:
			v_print_log_simple(level, "None");
			break;
		case VRS_UA_METHOD_PASSWORD:
			for(i=0; ((char*)ua_request_cmd->data)[i]!=0; i++)
				hidden_password[i] = '*';
			hidden_password[i] = '\0';
			/* v_print_log_simple(level, "Password: %s", (char*)ua_request_cmd->data); */
			v_print_log_simple(level, "Password: %s", hidden_password);
			break;
	}
	v_print_log_simple(level, "\n");
}

/**
 * \brief This function unpacks USER_AUTH_REQUEST command from the buffer
 */
int v_raw_unpack_user_auth_request(const char *buffer, ssize_t buffer_size, struct User_Authentication_Request *ua_request_cmd)
{
	unsigned short buffer_pos = 0;
	unsigned char username_len=0, password_len=0, length=0;
	int i;

	if(buffer_size < (1+1)) {
		v_print_log(VRS_PRINT_WARNING, "Buffer size: %d < minimal command length: %d\n.", buffer_size, 1+1);
		ua_request_cmd->username[0] = '\0';
		ua_request_cmd->method_type = VRS_UA_METHOD_RESERVED;
		return buffer_size;
	}

	/* Unpack command ID */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &ua_request_cmd->id);

	/* Unpack length of the command */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &length);

	/* Check if buffer size is not smaller, then length of command */
	if(buffer_size < length) {
		v_print_log(VRS_PRINT_WARNING, "Buffer size: %d < command length: %d\n.", buffer_size, length);
		ua_request_cmd->username[0] = '\0';
		ua_request_cmd->method_type = VRS_UA_METHOD_RESERVED;
		return buffer_size;
	}

	/* Check if the command has minimal command length: ID_len + Lenght_len +
	 * String_Length_len + Username_len + Method_Type_len. Note: there should
	 * be some other server rule for length of username, but this is only
	 * implementation check. */
	if(length < (1+1+1+1+1)) {
		v_print_log(VRS_PRINT_WARNING, "Command length: %d < (1+1+1+1+1)\n.", length);
		ua_request_cmd->username[0] = '\0';
		ua_request_cmd->method_type = VRS_UA_METHOD_RESERVED;
		return length;
	}

	/* Unpack length of username */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &username_len);

	/* Check for zero username length */
	if(username_len==0) {
		v_print_log(VRS_PRINT_WARNING, "Zero length of username\n.");
		ua_request_cmd->username[0] = '\0';
		ua_request_cmd->method_type = VRS_UA_METHOD_RESERVED;
		return length;
	/* Check if the length of the username is not bigger then length of the
	 * (command - other necessary fields) */
	} else if((1+1+1+username_len+1)>length) {
		v_print_log(VRS_PRINT_WARNING, "The: 1+1+1+Username_length:%d+1 is bigger then Command_length:%d.\n",
				username_len, length);
		ua_request_cmd->username[0] = '\0';
		ua_request_cmd->method_type = VRS_UA_METHOD_RESERVED;
		return length;
	/* Check if the length of the username is not bigger then maximal length
	 * of the username */
	} else if(username_len>VRS_MAX_USERNAME_LENGTH) {
		v_print_log(VRS_PRINT_WARNING, "Length of username: %d is bigger then VRS_MAX_USERNAME_LENGTH: %d.\n",
				username_len, VRS_MAX_USERNAME_LENGTH);
		ua_request_cmd->username[0] = '\0';
		ua_request_cmd->method_type = VRS_UA_METHOD_RESERVED;
		return length;
	/* Unpack own username */
	} else {
		for(i=0; i<username_len; i++) {
			buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], (unsigned char*)&ua_request_cmd->username[i]);
		}
		ua_request_cmd->username[username_len] = '\0';
	}

	/* Unpack type of user authentication */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &ua_request_cmd->method_type);

	/* Unpack data specific for each authentication method */
	switch(ua_request_cmd->method_type) {
		case VRS_UA_METHOD_NONE:
			break;
		case VRS_UA_METHOD_PASSWORD:
			/* Check if it is possible to unpack length of the password and
			 * password itself. Minimal implementation password length is
			 * considered as one. Note: there should be some other security
			 * check of minimal password length. */
			if(length < (1+1+1+username_len+1+1+1)) {
				v_print_log(VRS_PRINT_WARNING, "Length of command: %d is smaller then: (1+1+1+username_len:%d+1+1+1).\n",
						length, username_len);
				ua_request_cmd->username[0] = '\0';
				ua_request_cmd->method_type = VRS_UA_METHOD_RESERVED;
				return length;
			}
			/* Unpack length of password */
			buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &password_len);

			/* Zero length password (such password should not be used) */
			if(password_len==0) {
				v_print_log(VRS_PRINT_WARNING, "Zero length password\n.");
				ua_request_cmd->username[0] = '\0';
				ua_request_cmd->method_type = VRS_UA_METHOD_RESERVED;
				return buffer_pos;
			/* Check if the length of the password is not bigger then length of the
			 * (command - other necessary fields) */
			} else if((1+1+1+username_len+1+1+password_len)>length) {
				v_print_log(VRS_PRINT_WARNING, "The: 1+1+1+Username_length:%d+1+1+Password_length:%d is bigger then Command_length:%d.\n",
						username_len, password_len, length);
				ua_request_cmd->username[0] = '\0';
				ua_request_cmd->method_type = VRS_UA_METHOD_RESERVED;
				return length;
			/* Check if the length of the password is not bigger then maximal length
			 * of the password */
			} else if(password_len>VRS_MAX_DATA_LENGTH) {
				v_print_log(VRS_PRINT_WARNING, "Length of password: %d is bigger then VRS_MAX_DATA_LENGTH: %d.\n",
						password_len, VRS_MAX_DATA_LENGTH);
				ua_request_cmd->username[0] = '\0';
				ua_request_cmd->method_type = VRS_UA_METHOD_RESERVED;
				return length;
			} else {
				/* Unpack own password */
				for(i=0; i<password_len; i++) {
					buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos],
							(unsigned char*)&((char*)ua_request_cmd->data)[i]);
				}
				((char*)ua_request_cmd->data)[password_len] = '\0';
			}
			break;
		default:
			return length;
	}

	/* Check if length and buffer_pos match */
	if(buffer_pos!=length) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s: buffer_pos: %d != length: %d\n",
				__FUNCTION__, buffer_pos, length);
		return length;
	}

	return buffer_pos;
}

/**
 * \brief This function packs USER_AUTH_REQUEST command
 */
int v_raw_pack_user_auth_request(char *buffer, const struct User_Authentication_Request *ua_request_cmd)
{
	int ret, length;
	unsigned short buffer_pos = 0;

	/* Pack command ID first */
	buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], ua_request_cmd->id);

	/* Pack length of the command: id_len + cmd_len + str_len + username_len + method_id_len */
	length = 1 + 1 + 1 + strlen(ua_request_cmd->username) + 1;
	switch(ua_request_cmd->method_type) {
		case VRS_UA_METHOD_NONE:
			break;
		case VRS_UA_METHOD_PASSWORD:
			length += 1 + strlen((char*)ua_request_cmd->data);
			break;
	}

	/* Pack length of the command */
	buffer_pos += v_cmd_pack_len(&buffer[buffer_pos], length);

	/* Try to pack username */
	if( (ret = vnp_raw_pack_string8(&buffer[buffer_pos], (char*)ua_request_cmd->username)) == 0) {
		v_print_log(VRS_PRINT_ERROR, "Too long username: %s\n", ua_request_cmd->username);
		return 0;
	} else {
		buffer_pos += ret;
	}

	switch(ua_request_cmd->method_type) {
		case VRS_UA_METHOD_NONE:
			/* Pack method type */
			buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], ua_request_cmd->method_type);
			break;
		case VRS_UA_METHOD_PASSWORD:
			/* Pack method type */
			buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], ua_request_cmd->method_type);
			/* Try to pack password to the buffer */
			if( (ret = vnp_raw_pack_string8(&buffer[buffer_pos], (char*)ua_request_cmd->data)) == 0) {
				v_print_log(VRS_PRINT_ERROR, "Too long password: %s\n", ua_request_cmd->data);
				return 0;
			} else {
				buffer_pos += ret;
			}
			break;
	}

	/* Check if length and buffer_pos match */
	if(buffer_pos!=length) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s: buffer_pos: %d != length: %d\n",
				__FUNCTION__, buffer_pos, length);
		return length;
	}

	return buffer_pos;
}
