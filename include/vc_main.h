/*
 * $Id: vc_main.h 1293 2012-08-09 09:02:01Z jiri $
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
 * author: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

#if !defined VC_MAIN_H
#define VC_MAIN_H

#include <openssl/ssl.h>

#include <stdio.h>

#include "verse_types.h"

#include "v_connection.h"
#include "v_context.h"

#define STATE_EXIT_ERROR			0
#define STATE_EXIT_SUCCESS			1

/**
 * Structure storing pointers at callback functions
 */
typedef struct VFuncStorage {
	void (*receive_connect_accept)(const uint8 sesssion_id,
			const uint16 user_id,				/* ID of user */
			const uint32 parent_id);			/* ID of the verse client */
	void (*receive_connect_terminate)(const uint8 sesssion_id,
			const uint8 error_num);				/* The reason for connection termination */
	void (*receive_user_authenticate)(const uint8 sesssion_id,
			const char *username,				/* Username */
			const uint8 auth_meth_count,		/* Count of supported authentication methods */
			const uint8 *methods);				/* List of methods */

	void (*receive_node_create)(const uint8 session_id,
			const uint32 node_id,
			const uint32 parent_id,
			const uint16 user_id,
			const uint16 type);
	void (*receive_node_destroy)(const uint8 session_id,
			const uint32 node_id);
	void (*receive_node_subscribe)(const uint8 session_id,
			const uint32 node_id,
			const uint32 version,
			const uint32 crc32);
	void (*receive_node_unsubscribe)(const uint8 session_id,
			const uint32 node_id,
			const uint32 version,
			const uint32 crc32);
	void (*receive_node_perm)(const uint8 session_id,
			uint32 node_id,
			uint16 user_id,
			uint8 permissions);
	void (*receive_node_owner)(const uint8 session_id,
			uint32 node_id,
			uint16 user_id);
	void (*receive_node_lock)(const uint8 session_id,
			uint32 node_id,
			uint32 avatar_id);
	void (*receive_node_unlock)(const uint8 session_id,
			uint32 node_id,
			uint32 avatar_id);
	void (*receive_node_link)(const uint8 session_id,
			const uint32 parent_node_id,
			const uint32 child_node_id);

	void (*receive_taggroup_create)(const uint8 session_id,
			const uint32 node_id,
			const uint16 taggroup_id,
			const uint16 type);
	void (*receive_taggroup_destroy)(const uint8 session_id,
			const uint32 node_id,
			const uint16 taggroup_id);
	void (*receive_taggroup_subscribe)(const uint8 session_id,
			const uint32 node_id,
			const uint16 taggroup_id,
			const uint32 crc32,
			const uint32 version);
	void (*receive_taggroup_unsubscribe)(const uint8 session_id,
			const uint32 node_id,
			const uint16 taggroup_id,
			const uint32 crc32,
			const uint32 version);
	void (*receive_tag_create)(const uint8 session_id,
			const uint32 node_id,
			const uint16 taggroup_id,
			const uint16 tag_id,
			const uint8 data_type,
			const uint8 count,
			const uint16 type);
	void (*receive_tag_destroy)(const uint8 session_id,
			const uint32 node_id,
			const uint16 taggroup_id,
			const uint16 tag_id);
	void (*receive_tag_set_value)(const uint8 session_id,
			const uint32 node_id,
			const uint16 taggroup_id,
			const uint16 tag_id,
			const uint8 data_type,
			const uint8 count,
			const void *value);

	void (*receive_layer_create)(const uint8_t session_id,
			const uint32_t node_id,
			const uint16_t parent_layer_id,
			const uint16_t layer_id,
			const uint8_t data_type,
			const uint8_t count,
			const uint16_t type);
	void (*receive_layer_destroy)(const uint8_t session_id,
			const uint32_t node_id,
			const uint16_t layer_id);
	void (*receive_layer_subscribe)(const uint8_t session_id,
			const uint32_t node_id,
			const uint16_t layer_id,
			const uint32_t version,
			const uint32_t crc32);
	void (*receive_layer_unsubscribe)(const uint8_t session_id,
			const uint32_t node_id,
			const uint16_t layer_id,
			const uint32_t version,
			const uint32_t crc32);
	void (*receive_layer_set_value)(const uint8_t session_id,
			const uint32_t node_id,
			const uint16_t layer_id,
			const uint32_t item_id,
			const uint8_t type,
			const uint8_t count,
			const void *value);
	void (*receive_layer_unset_value)(const uint8_t session_id,
			const uint32_t node_id,
			const uint16_t layer_id,
			const uint32_t item_id);
} VFuncStorage;

/**
 * Verse client context storing information about connections and setting
 * specific for client.
 */
typedef struct VC_CTX {
	/* Connections */
	int32					max_connection_attempts;	/**< Maximum number of connection attempts (state REQUEST, PARTOPEN, etc.) */
	int32					connection_attempts_delay;	/**< Delay between connection attempts (milliseconds) */
	int32		 			max_sessions;				/**< Maximum number of connections to servers */
	/* Debug print */
	uint8					print_log_level;			/**< Amount of information printed to log file */
	FILE					*log_file;					/**< File used for logs */
	uint8					rwin_scale;					/**< Scale of Flow Control Window */
	char					*ca_path;
	/* Data for connections */
	struct VSession			**vsessions;				/**< List of sessions and session with connection attempts */
	struct VFuncStorage		vfs;						/**< List of callback functions*/
	uint32					session_counter;			/**< Counter of sessions used for unique session_id */
	/* SSL context */
	SSL_CTX					*tls_ctx;					/**< SSL context for main secured TCP TLS socket */
	SSL_CTX					*dtls_ctx;					/**< SSL context for secured UDP DTLS connections (shared with all connections) */
	/* Information about client */
	char					*client_name;
	char					*client_version;
} VC_CTX;

/* Function prototypes */
void vc_init_func_storage(struct VFuncStorage *vfs);
void vc_load_config_file(struct VC_CTX *ctx);
int vc_init_ctx(struct VC_CTX *ctx);
void vc_free_ctx(struct VC_CTX *ctx);

#endif

