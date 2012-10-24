/*
 * $Id: vc_udp_connect.h 903 2011-08-08 12:38:35Z jiri $
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
 * Authors: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

#ifndef VC_UDP_CONNECT_H
#define VC_UDP_CONNECT_H

#include "v_context.h"
#include "v_connection.h"

void vc_REQUEST_init_send_packet(struct vContext *C);
int vc_REQUEST_send_packet(struct vContext *C);
int vc_REQUEST_handle_packet(struct vContext *C);
int vc_REQUEST_loop(struct vContext *C);

void vc_PARTOPEN_init_send_packet(struct vContext *C);
int vc_PARTOPEN_send_packet(struct vContext *C);
int vc_PARTOPEN_handle_packet(struct vContext *C);
int vc_PARTOPEN_loop(struct vContext *C);

int vc_OPEN_send_packet(struct vContext *C);
int vc_OPEN_handle_packet(struct vContext *C);
int vc_OPEN_loop(struct vContext *C);

void vc_CLOSING_init_send_packet(struct vContext *C);
int vc_CLOSING_send_packet(struct vContext *C);
int vc_CLOSING_handle_packet(struct vContext *C);
int vc_CLOSING_loop(struct vContext *C);

struct VDgramConn *vc_create_client_dgram_conn(struct vContext *C);
int vc_receive_and_handle_packet(struct vContext *C,
		int handle_packet(struct vContext *C));
int vc_send_packet(struct vContext *C);
void* vc_main_dgram_loop(void *arg);

#endif /* VC_UDP_CONNECT_H */
