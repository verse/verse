/*
 * $Id: v_context.c 528 2010-08-17 09:55:35Z jiri $
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

#include "v_context.h"

/* Verse server CTX */
struct VS_CTX *CTX_server_ctx(struct vContext *C) { return C->vs_ctx; }
void CTX_server_ctx_set(struct vContext *C, struct VS_CTX *vs_ctx) { C->vs_ctx = vs_ctx; }

/* Verse client CTX */
struct VC_CTX *CTX_client_ctx(struct vContext *C) { return C->vc_ctx; }
void CTX_client_ctx_set(struct vContext *C, struct VC_CTX *vc_ctx) { C->vc_ctx = vc_ctx; }

/* IO CTX */
struct IO_CTX *CTX_io_ctx(struct vContext *C) {	return C->io_ctx; }
void CTX_io_ctx_set(struct vContext *C, struct IO_CTX *io_ctx) { C->io_ctx = io_ctx; }

/* Verse session */
struct VSession *CTX_current_session(struct vContext *C) { return C->session; }
void CTX_current_session_set(struct vContext *C, struct VSession *vsession) { C->session = vsession; }

/* TCP connection */
struct VStreamConn *CTX_current_stream_conn(struct vContext *C) { return C->stream_conn; }
void CTX_current_stream_conn_set(struct vContext *C, struct VStreamConn *stream_conn) { C->stream_conn = stream_conn; }

/* UDP connection */
struct VDgramConn *CTX_current_dgram_conn(struct vContext *C) { return C->dgram_conn; }
void CTX_current_dgram_conn_set(struct vContext *C, struct VDgramConn *dgram_conn) { C->dgram_conn = dgram_conn; }

/* Received packet */
struct VPacket *CTX_r_packet(struct vContext *C) { return C->r_packet; }
void CTX_r_packet_set(struct vContext *C, struct VPacket *r_packet) { C->r_packet= r_packet; }

/* Packet to be send or already sent */
struct VPacket *CTX_s_packet(struct vContext *C) { return C->s_packet; }
void CTX_s_packet_set(struct vContext *C, struct VPacket *s_packet) { C->s_packet= s_packet; }

/* Received message */
struct VMessage *CTX_r_message(struct vContext *C) { return C->r_message; }
void CTX_r_message_set(struct vContext *C, struct VMessage *r_message) { C->r_message= r_message; }

/* Message to be send or already sent */
struct VMessage *CTX_s_message(struct vContext *C) { return C->s_message; }
void CTX_s_message_set(struct vContext *C, struct VMessage *s_message) { C->s_message= s_message; }
