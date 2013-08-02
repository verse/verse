/*
 * $Id: v_session.c 1128 2012-03-16 12:24:02Z jiri $
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

#include <stdlib.h>

#include "v_session.h"

void v_init_session(struct VSession *vsession)
{
	vsession->tcp_thread = 0;
	vsession->udp_thread = 0;
	vsession->peer_hostname = NULL;
	vsession->service = NULL;
	vsession->session_id = 0;
	vsession->dgram_conn = NULL;
	vsession->stream_conn = NULL;
	vsession->ded.str = NULL;
#if defined WITH_PAM
	vsession->conv.appdata_ptr = NULL;
	vsession->conv.conv = NULL;
#endif
	vsession->in_queue = NULL;
	vsession->out_queue = NULL;
	vsession->host_url = NULL;
	vsession->peer_cookie.str = NULL;
	vsession->host_cookie.str = NULL;
	vsession->avatar_id = -1;
	vsession->flags = 0;
	vsession->fps_host = DEFAULT_FPS;	/* Default value */
	vsession->fps_peer = DEFAULT_FPS;	/* Default value */
	vsession->tmp_flags = 0;
	vsession->client_name = NULL;
	vsession->client_version = NULL;
}

void v_destroy_session(struct VSession *vsession)
{
	vsession->tcp_thread = 0;
	vsession->udp_thread = 0;

	if(vsession->dgram_conn != NULL) {
		v_conn_dgram_destroy(vsession->dgram_conn);
		free(vsession->dgram_conn);
		vsession->dgram_conn = NULL;
	}
	if(vsession->stream_conn != NULL) {
		v_conn_stream_destroy(vsession->stream_conn);
		free(vsession->stream_conn);
		vsession->stream_conn = NULL;
	}
	if(vsession->peer_hostname != NULL) {
		free(vsession->peer_hostname);
		vsession->peer_hostname = NULL;
	}
	if(vsession->ded.str != NULL) {
		free(vsession->ded.str);
		vsession->ded.str = NULL;
	}
	if(vsession->service != NULL) {
		free(vsession->service);
		vsession->service = NULL;
	}
	if(vsession->in_queue != NULL) {
		v_in_queue_destroy(&vsession->in_queue);
	}
	if(vsession->out_queue != NULL) {
		v_out_queue_destroy(&vsession->out_queue);
	}
	if(vsession->host_url != NULL) {
		free(vsession->host_url);
		vsession->host_url = NULL;
	}
	if(vsession->peer_cookie.str != NULL) {
		free(vsession->peer_cookie.str);
		vsession->peer_cookie.str = NULL;
	}
	if(vsession->host_cookie.str != NULL) {
		free(vsession->host_cookie.str);
		vsession->host_cookie.str = NULL;
	}
	if(vsession->client_name != NULL) {
		free(vsession->client_name);
		vsession->client_name = NULL;
	}
	if(vsession->client_version != NULL) {
		free(vsession->client_version);
		vsession->client_version = NULL;
	}
}
