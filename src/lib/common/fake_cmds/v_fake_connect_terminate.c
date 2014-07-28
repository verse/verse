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
 * \brief This function print content of fake command Connect_Terminate
 */
void v_fake_connect_terminate_print(const unsigned char level, struct Connect_Terminate_Cmd *connect_terminate)
{
	v_print_log_simple(level, "\tCONNECT_TERMINATE: error: %d\n",
			connect_terminate->error_num);
}

/**
 * \brief This function initialize members of structure for Connect_Terminate command
 */
void v_connect_terminate_init(struct Connect_Terminate_Cmd *conn_term,
		uint8 error)
{
    if(conn_term != NULL) {
        /* initialize members with values */
    	conn_term->id = FAKE_CMD_CONNECT_TERMINATE;
    	conn_term->error_num = error;
    }
}

/**
 * \brief this function creates new structure of Connect_Terminate command
 */
struct Connect_Terminate_Cmd *v_connect_terminate_create(uint8 error)
{
    struct Connect_Terminate_Cmd *conn_term = NULL;
    conn_term = (struct Connect_Terminate_Cmd*)calloc(1, sizeof(struct Connect_Terminate_Cmd));
    v_connect_terminate_init(conn_term, error);
    return conn_term;
}

/**
 * \brief This function clear members of structure for Connect_Terminate command
 */
void v_connect_terminate_clear(struct Connect_Terminate_Cmd *conn_term)
{
    if(conn_term != NULL) {
        conn_term->error_num = 0;
    }
}

/**
 * \brief This function destroy Connect_Terminate command
 */
void v_connect_terminate_destroy(struct Connect_Terminate_Cmd **conn_term)
{
    if(conn_term != NULL) {
        v_connect_terminate_clear(*conn_term);
        free(*conn_term);
        *conn_term = NULL;
    }
}
