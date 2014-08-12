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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <wslay/wslay.h>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include "v_common.h"

#include "vs_main.h"
#include "vs_websocket.h"
#include "vs_handshake.h"
#include "vs_sys_nodes.h"

#include "v_stream.h"

/*
 * Calculates SHA-1 hash of *src*. The size of *src* is *src_length* bytes.
 * *dst* must be at least SHA1_DIGEST_SIZE.
 */
static void sha1(uint8_t *dst, const uint8_t *src, const size_t src_length)
{
	SHA1(src, src_length, dst);
}


/*
 * Base64-encode *src* and stores it in *dst*.
 * The size of *src* is *src_length*.
 * *dst* must be at least BASE64_ENCODE_RAW_LENGTH(src_length).
 */
static void base64(uint8_t *dst, const uint8_t *src, const size_t src_length)
{
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, src, src_length);
	(void)BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	memcpy(dst, bptr->data, bptr->length-1);
	dst[bptr->length-1] = 0;

	BIO_free_all(b64);
}


/*
 * Create Server's accept key in *dst*.
 * *client_key* is the value of |Sec-WebSocket-Key| header field in
 * client's handshake and it must be length of 24.
 * *dst* must be at least BASE64_ENCODE_RAW_LENGTH(20)+1.
 */
static void create_accept_key(char *dst, const char *client_key)
{
	uint8_t sha1buf[20], key_src[60];
	memcpy(key_src, client_key, WS_CLIENT_KEY_LEN);
	memcpy(key_src + WS_CLIENT_KEY_LEN, WS_GUID, 36);
	sha1(sha1buf, key_src, sizeof(key_src));
	base64((uint8_t*)dst, sha1buf, 20);
	dst[BASE64_ENCODE_RAW_LENGTH(20)] = '\0';
}


/* We parse HTTP header lines of the format
 *   \r\nfield_name: value1, value2, ... \r\n
 *
 * If the caller is looking for a specific value, we return a pointer to the
 * start of that value, else we simply return the start of values list.
 */
static char* http_header_find_field_value(char *header,
		char *field_name,
		char *value)
{
	char *header_end = header + strlen(header) - 1,
	*field_start = header,
	*field_end = NULL,
	*next_crlf = NULL,
	*value_start = NULL;
	int field_name_len = strlen(field_name);

	do {
		field_start = strstr(field_start + 1, field_name);

		if(field_start != NULL) {

			field_end = field_start + field_name_len - 1;

			if(field_start - header >= 2 &&
					field_start[-2] == '\r' &&
					field_start[-1] == '\n' &&
					header_end - field_end >= 1 &&
					field_end[1] == ':')
			{
				/* Found the field */
				break;
			}
		}
	} while(field_start != NULL);

	if(field_start == NULL) {
		return NULL;
	}

	/* Find the field terminator */
	next_crlf = strstr(field_start, "\r\n");

	/* A field is expected to end with \r\n */
	if(next_crlf == NULL) {
		/* Malformed HTTP header! */
		return NULL;
	}

	/* If not looking for a value, then return a pointer to the start
	 * of values string */
	if(value == NULL) {
		return field_end + 2;
	}

	value_start = strstr(field_start, value);

	/* Value not found */
	if(value_start == NULL) {
		return NULL;
	}

	/* Found the value we're looking for */
	if(value_start > next_crlf) {
		/* ... but after the CRLF terminator of the field. */
		return NULL;
	}

	/* The value we found should be properly delineated from the other tokens */
	if(isalnum(value_start[-1]) || isalnum(value_start[strlen(value)])) {
		return NULL;
	}

	return value_start;
}


/*
 * \brief Performs simple HTTP handshake. *fd* is the file descriptor of the
 * connection to the client. This function returns 0 if it succeeds,
 * or returns -1.
 */
static int http_handshake(int sockfd)
{
	char header[16384], accept_key[29], res_header[256];
	char *keyhdstart, *keyhdend;
	size_t header_length = 0, res_header_sent = 0, res_header_length;
	ssize_t ret;
	fd_set set;
	struct timeval timeout_tv, start_tv, current_tv;

	/* Get current time */
	gettimeofday(&start_tv, NULL);

	/* Set default timeout */
	timeout_tv.tv_sec = VRS_TIMEOUT;
	timeout_tv.tv_usec = 0;

	/* Try to read whole header without blocking read, use select and
	 * timeout */
	while(1) {

		FD_ZERO(&set);
		FD_SET(sockfd, &set);

		if( (ret = select(sockfd + 1, &set, NULL, NULL, &timeout_tv)) == -1 ) {
			v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n",
					__FILE__, __FUNCTION__,  __LINE__, strerror(errno));
			return -1;
		/* Was event on the listen socket */
		} else if(ret > 0) {
			if(FD_ISSET(sockfd, &set)) {
				ret = read(sockfd,
						header + header_length,
						sizeof(header) - header_length);

				if(ret == -1) {
					v_print_log(VRS_PRINT_ERROR, "read(): %s\n", strerror(errno));
					return -1;
				} else if(ret == 0) {
					v_print_log(VRS_PRINT_ERROR,
							"HTTP Handshake: Got EOF");
					return -1;
				} else {
					header_length += ret;
					/* Was end of HTTP header reached? */
					if(header_length >= 4 &&
							memcmp(header + header_length - 4, "\r\n\r\n", 4) == 0)
					{
						break;
					} else if(header_length == sizeof(header)) {
						v_print_log(VRS_PRINT_ERROR,
								"HTTP Handshake: Too large HTTP headers\n");
						return -1;
					}
				}
			}
		}

		gettimeofday(&current_tv, NULL);

		/* Update timeout */
		timeout_tv.tv_sec = VRS_TIMEOUT - (current_tv.tv_sec - start_tv.tv_sec);
		timeout_tv.tv_usec = 0;

		/* Where there is no elapsing time, then exit handshake */
		if(timeout_tv.tv_sec <= 0) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"HTTP Handshake: Timed out\n");
			return -1;
		}
	}

	header[header_length] = '\0';

	v_print_log(VRS_PRINT_DEBUG_MSG,
			"HTTP Handshake: received request: %s\n",
			header);

	/* Check if required HTTP headers were received in the request */

	/* Header has to contain field "Upgrade: websocket" */
	if(http_header_find_field_value(header, "Upgrade", "websocket") == NULL) {
		v_print_log(VRS_PRINT_ERROR,
				"HTTP Handshake: Missing required header field 'Upgrade: websocket'\n");
		return -1;
	}
	/* Header has to contain field "Connection: Upgrade" */
	if(http_header_find_field_value(header, "Connection", "Upgrade") == NULL) {
		v_print_log(VRS_PRINT_ERROR,
				"HTTP Handshake: Missing required header field 'Connection: Upgrade'\n");
		return -1;
	}
	/* Client has to send field Sec-WebSocket-Key in HTTP header */
	if( (keyhdstart = http_header_find_field_value(header, "Sec-WebSocket-Key",
			NULL)) == NULL)
	{
		v_print_log(VRS_PRINT_ERROR,
				"HTTP Handshake: Missing required header field 'Sec-WebSocket-Key: SOME_SECRET_KEY'\n");
		return -1;
	}
	/* Requested protocol name has to be equal to "v1.verse.tul.cz" */
	if( http_header_find_field_value(header, "Sec-WebSocket-Protocol",
			WEB_SOCKET_PROTO_NAME) == NULL)
	{
		v_print_log(VRS_PRINT_ERROR,
				"HTTP Handshake: Missing required header field 'Sec-WebSocket-Protocol: %s'\n",
				WEB_SOCKET_PROTO_NAME);
		return -1;
	}

	/* Check the length of WebSocket key */
	for(; *keyhdstart == ' '; ++keyhdstart);
	keyhdend = keyhdstart;
	for(; *keyhdend != '\r' && *keyhdend != ' '; ++keyhdend);
	if(keyhdend - keyhdstart != WS_CLIENT_KEY_LEN) {
		v_print_log(VRS_PRINT_ERROR,
				"HTTP Handshake: Invalid value in Sec-WebSocket-Key\n");
		return -1;
	}

	/* Create accepted key */
	create_accept_key(accept_key, keyhdstart);

	/* Create response for client */
	snprintf(res_header, sizeof(res_header),
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n"
			"Sec-WebSocket-Protocol: %s\r\n"
			"\r\n",
			accept_key,
			WEB_SOCKET_PROTO_NAME);

	/* Send response to the client */
	res_header_length = strlen(res_header);
	while(res_header_sent < res_header_length) {
		while((ret = write(sockfd, res_header + res_header_sent,
				res_header_length - res_header_sent)) == -1 &&
				errno == EINTR);
		if(ret == -1) {
			v_print_log(VRS_PRINT_ERROR, "write(): %s\n", strerror(errno));
			return -1;
		} else {
			res_header_sent += ret;
		}
	}

	v_print_log(VRS_PRINT_DEBUG_MSG, "HTTP Handshake: sent response\n");

	return 0;
}


/**
 * \brief WSLay send callback
 */
ssize_t vs_send_ws_callback_data(wslay_event_context_ptr ctx,
		const uint8_t *data,
		size_t len,
		int flags,
		void *user_data)
{
	struct vContext *C = (struct vContext*)user_data;
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	ssize_t ret = 0;
	int sflags = 0;

#ifdef MSG_MORE
	if(flags & WSLAY_MSG_MORE) {
		sflags |= MSG_MORE;
	}
#endif

	/* Try to send all data */
	while((ret = send(io_ctx->sockfd, data, len, sflags)) == -1 &&
			errno == EINTR) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"WS Callback Send Data, len: %ld, flags: %d -> %ld\n",
				len, sflags, ret);
	}

	v_print_log(VRS_PRINT_DEBUG_MSG,
			"WS Callback Send Data, len: %ld, flags: %d -> ret: %ld\n",
			len, sflags, ret);

	if(ret == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		} else {
			wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		}
	}

	return ret;
}


/**
 * \brief WSLay receive callback
 */
ssize_t vs_recv_ws_callback_data(wslay_event_context_ptr ctx,
		uint8_t *buf,
		size_t len,
		int flags,
		void *user_data)
{
	struct vContext *C = (struct vContext*)user_data;
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	ssize_t ret;

	(void)flags;

	/* Try to receive all data from TCP stack */
	while((ret = recv(io_ctx->sockfd, buf, len, 0)) == -1 &&
			errno == EINTR) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"WS Callback Received Data, len: %ld, flags: %d -> ret: %ld\n",
				len, flags, ret);
	}

	v_print_log(VRS_PRINT_DEBUG_MSG,
			"WS Callback Received Data, len: %ld, flags: %d -> ret: %ld\n",
			len, flags, ret);

	if(ret == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		} else {
			wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		}
	} else if(ret == 0) {
		/* Unexpected EOF is also treated as an error */
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		ret = -1;
	}

	return ret;
}


/**
 * \brief WebSocket callback function for received message
 */
void vs_ws_recv_msg_callback(wslay_event_context_ptr wslay_ctx,
		const struct wslay_event_on_msg_recv_arg *arg,
		void *user_data)
{
	struct vContext *C = (struct vContext*)user_data;
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VSession *session = CTX_current_session(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	int ret;
	unsigned int i;

	if(!wslay_is_ctrl_frame(arg->opcode)) {

		/* Verse server uses only binary message for communication */
		if(arg->opcode == WSLAY_BINARY_FRAME) {
		    struct wslay_event_msg msgarg;

			v_print_log(VRS_PRINT_DEBUG_MSG,
					"WS Callback received binary message\n");

			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Binary dump\n");

			/* Print dump of received data */
			for(i=0; i<arg->msg_length; i++) {
				v_print_log_simple(VRS_PRINT_DEBUG_MSG, "%d,", arg->msg[i]);
			}
			v_print_log_simple(VRS_PRINT_DEBUG_MSG, "\n");

			/* Copy received data to IO context */
			memcpy(io_ctx->buf,	arg->msg, arg->msg_length);
			io_ctx->buf_size = arg->msg_length;

			if(session->stream_conn->host_state == TCP_SERVER_STATE_STREAM_OPEN) {
				if(v_STREAM_handle_messages(C) != 0) {
					/* When some payload data were received, then poke data thread */
					sem_post(vs_ctx->data.sem);
				} else {
					/* End connection */
					session->stream_conn->host_state = TCP_SERVER_STATE_CLOSING;

					/* Try to close connection with WebSocket client */
					wslay_event_queue_close(wslay_ctx,
							WSLAY_CODE_PROTOCOL_ERROR,
							(uint8_t*)"Wrong command",	/* Close message */
							13);	/* The length of close message */
					return;
				}
			} else {
				if( vs_handle_handshake(C) == -1 ) {
					/* End connection */
					session->stream_conn->host_state = TCP_SERVER_STATE_CLOSING;

					/* Try to close connection with WebSocket client */
					wslay_event_queue_close(wslay_ctx,
							WSLAY_CODE_PROTOCOL_ERROR,
							(uint8_t*)"Wrong command",	/* Close message */
							13);	/* The length of close message */
					return;
				}
			}

			/* TODO: optionally send message fragmented, when it is needed using:
			 * wslay_event_queue_fragmented_msg() */

		    msgarg.opcode = WSLAY_BINARY_FRAME;
		    msgarg.msg = (uint8_t*)io_ctx->buf;
		    msgarg.msg_length = io_ctx->buf_size;

		    /* Queue message for sending */
		    if((ret = wslay_event_queue_msg(wslay_ctx, &msgarg)) != 0) {
				v_print_log(VRS_PRINT_ERROR,
						"Unable to queue message to WebSocket: %d\n", ret);
				return;
		    } else {
		    	v_print_log(VRS_PRINT_DEBUG_MSG,
		    			"WebSocket message successfully queued\n");
		    }

		} else if(arg->opcode == WSLAY_TEXT_FRAME) {
			v_print_log(VRS_PRINT_ERROR, "WebSocket text frame is not supported\n");
			return;
		}

	} else {
		/* Print opcode of control message */
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"WS Callback Received Ctrl Message: opcode: %d\n",
				arg->opcode);

		/* Is it closing message? */
		if(arg->opcode & WSLAY_CONNECTION_CLOSE) {

			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Close message with code: %d, message: %s\n",
					arg->status_code,
					arg->msg);

			/* When this control message was received at second time, then
			 * switch to the state CLOSED. Otherwise switch to the state
			 * CLOSING */
			if(session->stream_conn->host_state == TCP_SERVER_STATE_CLOSING) {
				session->stream_conn->host_state = TCP_SERVER_STATE_CLOSED;
			} else {
				session->stream_conn->host_state = TCP_SERVER_STATE_CLOSING;
				/* When server wasn't in the state closing, then send
				 * confirmation to the client, that this connection will be
				 * closed */
				wslay_event_queue_close(wslay_ctx,
						WSLAY_CODE_NORMAL_CLOSURE,
						(uint8_t*)"Closing connection",
						15);
			}
		}
	}
}

/**
 * \brief The function with WebSocket infinite loop
 */
void *vs_websocket_loop(void *arg)
{
	/* The vContext is passed as *user_data* in callback functions. */
	struct vContext *C = (struct vContext*)arg;
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VStreamConn *stream_conn = CTX_current_stream_conn(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VMessage *r_message=NULL, *s_message=NULL;
	wslay_event_context_ptr wslay_ctx;
	fd_set read_set, write_set;
	struct timeval tv;
	int ret, flags;
	unsigned int int_size;

	struct wslay_event_callbacks callbacks = {
			vs_recv_ws_callback_data,
			vs_send_ws_callback_data,
			NULL,
			NULL,
			NULL,
			NULL,
			vs_ws_recv_msg_callback
	};

	/* Set socket non-blocking */
	flags = fcntl(io_ctx->sockfd, F_GETFL, 0);
	if( fcntl(io_ctx->sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
		goto end;
	}

	/* Listen for HTTP request from web client and try to do
	 * WebSocket handshake */
	if(http_handshake(io_ctx->sockfd) != 0 ) {
		goto end;
	}

	/* Try to get size of TCP buffer */
	int_size = sizeof(int_size);
	if( getsockopt(io_ctx->sockfd, SOL_SOCKET, SO_RCVBUF,
			(void *)&stream_conn->socket_buffer_size, &int_size) != 0)
	{
		v_print_log(VRS_PRINT_ERROR, "Unable to get TCP buffer size\n");
		goto end;
	}

	r_message = (struct VMessage*)calloc(1, sizeof(struct VMessage));
	s_message = (struct VMessage*)calloc(1, sizeof(struct VMessage));

	if(r_message == NULL || s_message == NULL) {
		v_print_log(VRS_PRINT_ERROR, "Out of memory\n");
		goto end;
	}

	CTX_r_message_set(C, r_message);
	CTX_s_message_set(C, s_message);

	/* Try to initialize WebSocket server context */
	if(wslay_event_context_server_init(&wslay_ctx, &callbacks, C) != 0) {
		v_print_log(VRS_PRINT_ERROR,
				"Unable to initialize webSocket server context\n");
		goto end;
	}

	/* Set initial state */
	stream_conn->host_state = TCP_SERVER_STATE_RESPOND_METHODS;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: RESPOND_methods\n");
		printf("%c[%dm", 27, 0);
	}

	/* "Never ending" loop */
	while(vsession->stream_conn->host_state != TCP_SERVER_STATE_CLOSED &&
			( wslay_event_want_read(wslay_ctx) == 1 ||
					wslay_event_want_write(wslay_ctx) == 1) )
	{

		/* When server is going to stop, then close connection with WebSocket
		 * client */
		if(vs_ctx->state != SERVER_STATE_READY) {

			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Closing WebSocket connection: Server shutdown\n");

			stream_conn->host_state = TCP_SERVER_STATE_CLOSING;

			/* Try to close connection with WebSocket client */
			wslay_event_queue_close(wslay_ctx,
					WSLAY_CODE_GOING_AWAY,
					(uint8_t*)"Server shutdown",	/* Close message */
					15);	/* The length of close message s*/
		}

		/* Initialize read set */
		FD_ZERO(&read_set);
		if(wslay_event_want_read(wslay_ctx) == 1) {
			/*
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Waiting for WebSocket message ...\n");
			*/
			FD_SET(io_ctx->sockfd, &read_set);
		}

		/* Initialize write set */
		FD_ZERO(&write_set);
		if(wslay_event_want_write(wslay_ctx) == 1) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Going to write message to WebSocket ...\n");
			FD_SET(io_ctx->sockfd, &write_set);
		}

		/* Set timeout for select() */
		if(stream_conn->host_state == TCP_SERVER_STATE_STREAM_OPEN) {
			/* Use negotiated FPS */
			tv.tv_sec = 0;
			tv.tv_usec = 1000000/vsession->fps_host;
		} else {
			/* User have to send something in 30 seconds */
			tv.tv_sec = VRS_TIMEOUT;
			tv.tv_usec = 0;
		}

		if( (ret = select(io_ctx->sockfd + 1,
				&read_set,
				&write_set,
				NULL,			/* Don't care about exception */
				&tv)) == -1) {
			v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n",
					__FILE__, __FUNCTION__,  __LINE__, strerror(errno));
			goto end;
			/* Was event on the listen socket */
		} else if(ret > 0) {
			if(FD_ISSET(io_ctx->sockfd, &read_set)) {
				if( wslay_event_recv(wslay_ctx) != 0 ) {
					goto end;
				}
			}
			if (FD_ISSET(io_ctx->sockfd, &write_set)) {
				if( wslay_event_send(wslay_ctx) != 0 ) {
					goto end;
				}
			}
		} else if(ret == 0 && stream_conn->host_state != TCP_SERVER_STATE_STREAM_OPEN) {
			/* When handshake is not finished during VRS_TIMEOT seconds, then
			 * close connection with WebSocket client. */

			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Closing WebSocket connection: Handshake timed-out\n");

			stream_conn->host_state = TCP_SERVER_STATE_CLOSING;

			wslay_event_queue_close(wslay_ctx,
					WSLAY_CODE_PROTOCOL_ERROR,
					(uint8_t*)"Handshake timed-out",	/* Close message */
					19);	/* The length of close message */
		}

		if(stream_conn->host_state == TCP_SERVER_STATE_STREAM_OPEN) {
			/* Check if there is any command in outgoing queue
			 * and eventually pack these commands to buffer */
			if((ret = v_STREAM_pack_message(C)) == 0 ) {
				goto end;
			}

			/* When at least one command was packed to buffer, then
			 * queue this buffer to WebSocket layer */
			if(ret == 1) {
				struct wslay_event_msg msgarg;
			    msgarg.opcode = WSLAY_BINARY_FRAME;
			    msgarg.msg = (uint8_t*)io_ctx->buf;
			    msgarg.msg_length = io_ctx->buf_size;
			    wslay_event_queue_msg(wslay_ctx, &msgarg);
			}
		}
	}

end:
	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: CLOSING\n");
		printf("%c[%dm", 27, 0);
	}

	/* Set up TCP CLOSING state (non-blocking) */
	vs_CLOSING(C);

	/* Receive and Send messages are not necessary any more */
	if(r_message != NULL) {
		free(r_message);
		r_message = NULL;
		CTX_r_message_set(C, NULL);
	}
	if(s_message != NULL) {
		free(s_message);
		s_message = NULL;
		CTX_s_message_set(C, NULL);
	}

	/* TCP connection is considered as CLOSED, but it is not possible to use
	 * this connection for other client */
	stream_conn->host_state = TCP_SERVER_STATE_CLOSED;

	/* NULL pointer at stream connection */
	CTX_current_stream_conn_set(C, NULL);

	/* Set TCP connection to CLOSED */
	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: CLOSED\n");
		printf("%c[%dm", 27, 0);
	}


	pthread_mutex_lock(&vs_ctx->data.mutex);
	/* Unsubscribe this session (this avatar) from all nodes */
	vs_node_free_avatar_reference(vs_ctx, vsession);
	/* Try to destroy avatar node */
	vs_node_destroy_avatar_node(vs_ctx, vsession);
	pthread_mutex_unlock(&vs_ctx->data.mutex);

	/* This session could be used again for authentication */
	stream_conn->host_state=TCP_SERVER_STATE_LISTEN;

	/* Clear session flags */
	vsession->flags = 0;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: LISTEN\n");
		printf("%c[%dm", 27, 0);
	}

	free(C);
	C = NULL;

	pthread_exit(NULL);
	return NULL;
}
