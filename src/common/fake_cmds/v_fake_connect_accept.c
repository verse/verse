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

#include "v_common.h"
#include "v_fake_commands.h"

/**
 * \brief This function print content of fake command Connect_Accept.
 */
void v_fake_connect_accept_print(const unsigned char level, struct Connect_Accept_Cmd *connect_accpet)
{
	v_print_log_simple(level, "\tCONNECT_ACCEPT: UserID: %d, AvatarID: %d\n",
			connect_accpet->user_id, connect_accpet->avatar_id);
}

/**
 * \brief This function initialize members of structure for Connect_Accept command
 */
void v_Connect_Accept_init(struct Connect_Accept_Cmd *connect_accept,
		uint32 avatar_id,
		uint16 user_id)
{
    if(connect_accept != NULL) {
        /* initialize members with values */
    	connect_accept->id = FAKE_CMD_CONNECT_ACCEPT;
    	connect_accept->avatar_id = avatar_id;
    	connect_accept->user_id = user_id;
    }
}

/**
 * \brief this function creates new structure of Connect_Accept command
 */
struct Connect_Accept_Cmd *v_Connect_Accept_create(uint32 avatar_id,
		uint16 user_id)
{
    struct Connect_Accept_Cmd *connect_accept = NULL;
    connect_accept = (struct Connect_Accept_Cmd*)calloc(1, sizeof(struct Connect_Accept_Cmd));
    v_Connect_Accept_init(connect_accept, avatar_id, user_id);
    return connect_accept;
}

/**
 * \brief This function clear members of structure for Connect_Accept command
 */
void v_Connect_Accept_clear(struct Connect_Accept_Cmd *connect_accept)
{
    if(connect_accept != NULL) {
        connect_accept->avatar_id = -1;
        connect_accept->user_id = -1;
    }
}

/**
 * \brief This function destroy Connect_Accept command
 */
void v_Connect_Accept_destroy(struct Connect_Accept_Cmd **connect_accept)
{
    if(connect_accept != NULL) {
        v_Connect_Accept_clear(*connect_accept);
        free(*connect_accept);
        *connect_accept = NULL;
    }
}
