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
 * Authors: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

#ifdef WITH_OPENSSL
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#ifdef __APPLE__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>

#include "verse_types.h"

#include "v_network.h"
#include "v_common.h"
#include "v_connection.h"
#include "v_resend_mechanism.h"
#include "v_context.h"
#include "v_session.h"
#include "v_unpack.h"

#include "vc_main.h"
#include "vc_udp_connect.h"

/* Initialize list of callback functions and command functions */
void vc_init_func_storage(struct VFuncStorage *vfs)
{
	vfs->receive_connect_accept = NULL;
	vfs->receive_connect_terminate = NULL;
	vfs->receive_user_authenticate = NULL;

	vfs->receive_node_create = NULL;
	vfs->receive_node_destroy = NULL;

	vfs->receive_node_subscribe = NULL;
	vfs->receive_node_unsubscribe = NULL;

	vfs->receive_node_perm = NULL;

	vfs->receive_node_link = NULL;

	vfs->receive_node_perm = NULL;
	vfs->receive_node_owner = NULL;

	vfs->receive_node_lock = NULL;
	vfs->receive_node_unlock = NULL;

	vfs->receive_taggroup_create = NULL;
	vfs->receive_taggroup_destroy = NULL;

	vfs->receive_taggroup_subscribe = NULL;
	vfs->receive_taggroup_unsubscribe = NULL;

	vfs->receive_tag_create = NULL;
	vfs->receive_tag_destroy = NULL;

	vfs->receive_tag_set_value = NULL;

	vfs->receive_layer_create = NULL;
	vfs->receive_layer_destroy = NULL;

	vfs->receive_layer_subscribe = NULL;
	vfs->receive_layer_unsubscribe = NULL;

	vfs->receive_layer_set_value = NULL;
	vfs->receive_layer_unset_value = NULL;
}

/**
 * \brief Free verse client context
 */
void vc_free_ctx(VC_CTX *vc_ctx)
{
	int i;
	for(i=0; i<vc_ctx->max_sessions; i++) {
		if(vc_ctx->vsessions[i]!=NULL) {
			v_destroy_session(vc_ctx->vsessions[i]);
			free(vc_ctx->vsessions[i]);
			vc_ctx->vsessions[i] = NULL;
		}
	}
	free(vc_ctx->ca_path);
	if(vc_ctx->client_name) free(vc_ctx->client_name);
	if(vc_ctx->client_version) free(vc_ctx->client_version);
}

/* Fake load of Verse client configuration,
 * TODO: load configuration file from ~/.verse/arv[0] file and when this file
 * does not exist, then create this file with default configuration. */
void vc_load_config_file(VC_CTX *ctx)
{
	ctx->max_connection_attempts = 10;		/* 10 attempts to connect */
	ctx->connection_attempts_delay = 1000;	/* 1 second between attempts */
	ctx->max_sessions = 10;					/* This client can connect maximally to 10 servers */
	ctx->print_log_level = VRS_PRINT_NONE;	/* Client will print all messages to log file */
	ctx->log_file = stdout;					/* Use stdout for log file */
	ctx->rwin_scale = 0;					/* RWIN is multiple of 128B */
	ctx->ca_path = strdup("/etc/pki/tls/certs/");	/* Default directory with CA certificates */
}

/**
 * \brief Initialize verse client context
 */
int vc_init_ctx(struct VC_CTX *vc_ctx)
{
	int i;

	/* Allocate memory for session slots and make session slots NULL */
	vc_ctx->vsessions = (struct VSession**)malloc(sizeof(struct VSession*)*vc_ctx->max_sessions);
	vc_ctx->session_counter = 0;
	for(i=0; i<vc_ctx->max_sessions; i++) {
		vc_ctx->vsessions[i] = NULL;
	}
	/* Initialize callback function */
	vc_init_func_storage(&vc_ctx->vfs);

#ifdef WITH_OPENSSL
	/* Set up the library */
	SSL_library_init();
	ERR_load_BIO_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();

	/* Set up SSL context for TLS */
	if( (vc_ctx->tls_ctx = SSL_CTX_new(TLSv1_client_method())) == NULL ) {
		v_print_log(VRS_PRINT_ERROR, "Setting up SSL_CTX for TLS failed.\n");
		ERR_print_errors_fp(v_log_file());
		return 0;
	}

	/* Load the trust store for TLS */
	if(SSL_CTX_load_verify_locations(vc_ctx->tls_ctx, NULL, vc_ctx->ca_path) != 1) {
		v_print_log(VRS_PRINT_ERROR, "Loading path with CA certificates failed.\n");
		ERR_print_errors_fp(v_log_file());
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Path %s with CA certificates loaded successfully.\n",
				vc_ctx->ca_path);
	}
#endif

#if (defined WITH_OPENSSL) && (OPENSSL_VERSION_NUMBER>=0x10000000)
	/* Set up SSL context for DTSL */
	if( (vc_ctx->dtls_ctx = SSL_CTX_new(DTLSv1_client_method())) == NULL ) {
		v_print_log(VRS_PRINT_ERROR, "Setting up SSL_CTX for DTLS failed.\n");
		ERR_print_errors_fp(v_log_file());
		return 0;
	}

	/* Load the trust store for DTLS */
	if(SSL_CTX_load_verify_locations(vc_ctx->dtls_ctx, NULL, vc_ctx->ca_path) != 1) {
		v_print_log(VRS_PRINT_ERROR, "Loading path with CA certificates failed.\n");
		ERR_print_errors_fp(v_log_file());
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Path %s with CA certificates loaded successfully.\n",
				vc_ctx->ca_path);
	}

	/* Negotiate ciphers with server
	 * For testing: (encryption:none, mac:sha) eNULL:!MDP5
	 * For real use: ALL:ALL */
	if( SSL_CTX_set_cipher_list(vc_ctx->dtls_ctx, "eNULL:!MD5") == 0) {
		v_print_log(VRS_PRINT_ERROR, "Setting ciphers for DTLS failed.\n");
		ERR_print_errors_fp(v_log_file());
		return 0;
	}

	/* This is necessary for DTLS */
	SSL_CTX_set_read_ahead(vc_ctx->dtls_ctx, 1);
#else
#ifdef WITH_OPENSSL
	vc_ctx->dtls_ctx = NULL;
#endif
#endif

	/* Default name and version */
	vc_ctx->client_name = NULL;
	vc_ctx->client_version = NULL;

	pthread_mutex_init(&vc_ctx->mutex, NULL);

	return 1;
}
