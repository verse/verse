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

#include "v_common.h"
#include "v_fake_commands.h"

/**
 * \brief This function print content of fake command User_Authenticate
 */
void v_fake_user_auth_print(const unsigned char level, struct User_Authenticate_Cmd *user_auth)
{
	int i;

	v_print_log_simple(level, "\tCONNECT_AUTHENTICATE: username: %s, ",
			user_auth->username);

	if(user_auth->auth_meth_count > 0) {
		v_print_log_simple(level, "methods: ");
		for(i=0; i<user_auth->auth_meth_count; i++) {
			switch(user_auth->methods[i]) {
			case VRS_UA_METHOD_NONE:
				v_print_log_simple(level, "NONE, ");
				break;
			case VRS_UA_METHOD_PASSWORD:
				v_print_log_simple(level, "PASSWORD, ");
				break;
			case VRS_UA_METHOD_RESERVED:
				v_print_log_simple(level, "RESERVED, ");
				break;
			default:
				v_print_log_simple(level, "Unknown, ");
				break;
			}
		}
	}

	if(user_auth->data != NULL) {
		v_print_log_simple(level, "data: %s", user_auth->data);
	}

	v_print_log_simple(level, "\n");
}

/**
 * \brief This function initialize members of structure for User_Authenticate command
 */
void v_user_auth_init(struct User_Authenticate_Cmd *user_auth,
		const char *username,
		uint8 auth_meth_count,
		uint8 *methods,
		const char *data)
{
	if(user_auth != NULL) {

		/* initialize members with values */
		user_auth->id = FAKE_CMD_USER_AUTHENTICATE;

		if(username != NULL) {
			size_t username_len = strlen(username);
			username_len = (username_len > VRS_MAX_USERNAME_LENGTH) ? VRS_MAX_USERNAME_LENGTH : username_len;
			user_auth->username = strndup(username, username_len);
		} else {
			user_auth->username = NULL;
		}

		user_auth->auth_meth_count = auth_meth_count;

		if(methods != NULL) {
			user_auth->methods = (uint8*)calloc(auth_meth_count, sizeof(uint8));
			memcpy(user_auth->methods, methods, sizeof(uint8)*auth_meth_count);
		} else {
			user_auth->methods = NULL;
		}

		if(data != NULL) {
			size_t data_len = strlen(data);
			data_len = (data_len > VRS_MAX_DATA_LENGTH) ? VRS_MAX_DATA_LENGTH : data_len;
			user_auth->data = strndup(data, data_len);
		} else {
			user_auth->data = NULL;
		}
	}
}

/**
 * \brief this function creates new structure of User_Authenticate command
 */
struct User_Authenticate_Cmd *v_user_auth_create(const char *username,
		uint8 auth_meth_count,
		uint8 *methods,
		const char *data)
{
    struct User_Authenticate_Cmd *user_auth = NULL;
    user_auth = (struct User_Authenticate_Cmd*)calloc(1, sizeof(struct User_Authenticate_Cmd));
    v_user_auth_init(user_auth, username, auth_meth_count, methods, data);
    return user_auth;
}

/**
 * \brief This function clear members of structure for User_Authenticate command
 */
void v_user_auth_clear(struct User_Authenticate_Cmd *user_auth)
{
    if(user_auth != NULL) {
    	if(user_auth->username != NULL) {
    		free(user_auth->username);
    		user_auth->username = NULL;
    	}

    	user_auth->auth_meth_count = 0;

    	if(user_auth->methods != NULL) {
    		free(user_auth->methods);
    		user_auth->methods = NULL;
    	}

    	if(user_auth->data != NULL) {
    		free(user_auth->data);
    		user_auth->data = NULL;
    	}
    }
}

/**
 * \brief This function destroy User_Authenticate command
 */
void v_user_auth_destroy(struct User_Authenticate_Cmd **user_auth)
{
	if(user_auth != NULL) {
		v_user_auth_clear(*user_auth);
		free(*user_auth);
		*user_auth = NULL;
	}
}
