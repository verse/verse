/*
 * $Id: v_context.h 635 2010-09-23 20:16:31Z jiri $
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

#if !defined V_CONTEXT_H
#define V_CONTEXT_H

/* Structure for storing errors of receiving and sending packets */
typedef struct VError {
	unsigned int		error_num;
	unsigned int		system_error_num;
} VError;

/* Structure storing all necessary information for client and server */
typedef struct vContext {
	struct VS_CTX		*vs_ctx;		/* Context for server (configuration) */
	struct VC_CTX		*vc_ctx;		/* Context for client (configuration) */
	struct IO_CTX		*io_ctx;		/* Context for receiving and sending data */
	struct VSession		*session;		/* Current session */
	struct VDgramConn	*dgram_conn;	/* Current datagram connection */
	struct VStreamConn	*stream_conn;	/* Current stream connection */
	struct VPacket		*r_packet;		/* Structure for storing received packet */
	struct VPacket		*s_packet;		/* Structure for storing packet to be sent */
	struct VMessage		*r_message;		/* Structure for storing received message */
	struct VMessage		*s_message;		/* Structure for storing message to be sent */
	struct VError		error;			/* Structure for storing errors */
} vContext;

struct VS_CTX *CTX_server_ctx(struct vContext *C);
void CTX_server_ctx_set(struct vContext *C, struct VS_CTX *vs_ctx);

struct VC_CTX *CTX_client_ctx(struct vContext *C);
void CTX_client_ctx_set(struct vContext *C, struct VC_CTX *vc_ctx);

struct IO_CTX *CTX_io_ctx(struct vContext *C);
void CTX_io_ctx_set(struct vContext *C, struct IO_CTX *io_ctx);

struct VSession *CTX_current_session(struct vContext *C);
void CTX_current_session_set(struct vContext *C, struct VSession *vsession);

struct VStreamConn *CTX_current_stream_conn(struct vContext *C);
void CTX_current_stream_conn_set(struct vContext *C, struct VStreamConn *stream_conn);

struct VDgramConn *CTX_current_dgram_conn(struct vContext *C);
void CTX_current_dgram_conn_set(struct vContext *C, struct VDgramConn *dgram_conn);

struct VPacket *CTX_r_packet(struct vContext *C);
void CTX_r_packet_set(struct vContext *C, struct VPacket *r_packet);

struct VPacket *CTX_s_packet(struct vContext *C);
void CTX_s_packet_set(struct vContext *C, struct VPacket *s_packet);

struct VMessage *CTX_r_message(struct vContext *C);
void CTX_r_message_set(struct vContext *C, struct VMessage *r_message);

struct VMessage *CTX_s_message(struct vContext *C);
void CTX_s_message_set(struct vContext *C, struct VMessage *s_message);

#endif
