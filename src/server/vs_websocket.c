/*
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Contributor(s): Jiri Hnidek <jiri.hnidek@tul.cz>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <libwebsockets.h>

#include "vs_main.h"
#include "v_common.h"

/**
 * This is callback method for http request
 */
static int callback_http(struct libwebsocket_context *this,
                         struct libwebsocket *wsi,
                         enum libwebsocket_callback_reasons reason,
                         void *user,
                         void *in,
                         size_t len)
{
	(void)this;
	(void)wsi;
	(void)reason;
	(void)user;
	(void)in;
	(void)len;

    return 0;
}


/**
 * This is callback method for verse protocol over websocket
 */
static int callback_verse(struct libwebsocket_context *this,
		struct libwebsocket *wsi,
		enum libwebsocket_callback_reasons reason,
		void *user,
		void *in,
		size_t len)
{
	switch (reason) {
	case LWS_CALLBACK_ESTABLISHED:
		v_print_log(VRS_PRINT_DEBUG_MSG, "New connection established\n");
		break;
	case LWS_CALLBACK_RECEIVE: {
		break;
		}
	default:
		break;
	}

	return 0;
}


/**
 * This is structure with supported protocols
 */
static struct libwebsocket_protocols protocols[] = {
		/* First protocol must always be HTTP handler */
		{
				"http-only",	/* Protocol name*/
				callback_http,	/* Callback function */
				0,				/* Per session data size */
				0,				/* RX buffer size*/
				NULL,			/* Pointer at websocket context */
				0				/* Protocol index */
		},
		{
				"verse",		/* Protocol name*/
				callback_verse,	/* Callback function */
				0,				/* Per session data size */
				0,
				NULL,
				1
		},
		{
				NULL,			/* End of list */
				NULL,
				0,
				0,
				NULL,
				2
		}
};

/**
 * \brief The function with websocket infinite loop
 */
void *vs_websocket_loop(void *arg)
{
	struct VS_CTX *vs_ctx = (struct VS_CTX*)arg;
	struct libwebsocket_context *websocket_ctx;
	struct lws_context_creation_info info;

	info.port = 9000;	/* It will listen on this port */
	info.iface = NULL;	/* It will listen on all interfaces */
#if 0
	/* Try to use SSL for encryption */
	info.ssl_private_key_filepath = vs_ctx->private_cert_file;
	info.ssl_cert_filepath = vs_ctx->public_cert_file;
	info.ssl_ca_filepath = NULL; /* TODO */
	info.ssl_cipher_list = NULL; /* Use default */
#else
	info.ssl_private_key_filepath = NULL;
	info.ssl_ca_filepath = NULL;
	info.ssl_cert_filepath = NULL;
#endif
	info.protocols = protocols;	/* The array of protocols */
	info.extensions = NULL;		/* No extension are used */
	info.options = 0;

	/* Create libwebsockets context representing this server */
	websocket_ctx = libwebsocket_create_context(&info);
	if(websocket_ctx == NULL) {
		v_print_log(VRS_PRINT_ERROR, "libwebsockets init failed\n");
		goto end;
	}

	v_print_log(VRS_PRINT_DEBUG_MSG, "Starting WebSocket loop ...\n");

	/* Infinite loop */
	while(vs_ctx->state != SERVER_STATE_CLOSED) {
		/* libwebsocket_service() will process all waiting events with their
		 * callback functions and then wait 10 ms. The library
		 * libwebcocket could be only a single threaded webserver */
		libwebsocket_service(websocket_ctx, 1000);
		printf("WebSocket ...\n");
	}

	libwebsocket_context_destroy(websocket_ctx);

end:
	v_print_log(VRS_PRINT_DEBUG_MSG, "Exiting WebSocket thread\n");

	pthread_exit(NULL);
	return NULL;
}
