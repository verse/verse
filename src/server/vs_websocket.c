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
#include <wslay.h>
#include <errno.h>
#include <unistd.h>

#include <wslay.h>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include "vs_main.h"
#include "vs_websocket.h"
#include "v_common.h"
#include <fcntl.h>

/*
 * Calculates SHA-1 hash of *src*. The size of *src* is *src_length* bytes.
 * *dst* must be at least SHA1_DIGEST_SIZE.
 */
void sha1(uint8_t *dst, const uint8_t *src, size_t src_length)
{
	SHA1(src, src_length, dst);
}

/*
 * Base64-encode *src* and stores it in *dst*.
 * The size of *src* is *src_length*.
 * *dst* must be at least BASE64_ENCODE_RAW_LENGTH(src_length).
 */
void base64(uint8_t *dst, const uint8_t *src, size_t src_length)
{
	BIO *bmem, *b64;
	BUF_MEM *bptr;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, src, src_length);
	BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	memcpy(dst, bptr->data, bptr->length-1);
	dst[bptr->length-1] = 0;

	BIO_free_all(b64);
}

#define BASE64_ENCODE_RAW_LENGTH(length) ((((length) + 2)/3)*4)
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/*
 * Create Server's accept key in *dst*.
 * *client_key* is the value of |Sec-WebSocket-Key| header field in
 * client's handshake and it must be length of 24.
 * *dst* must be at least BASE64_ENCODE_RAW_LENGTH(20)+1.
 */
void create_accept_key(char *dst, const char *client_key)
{
	uint8_t sha1buf[20], key_src[60];
	memcpy(key_src, client_key, 24);
	memcpy(key_src+24, WS_GUID, 36);
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
static char*
http_header_find_field_value(char *header, char *field_name, char *value)
{
	char *header_end,
	*field_start,
	*field_end,
	*next_crlf,
	*value_start;
	int field_name_len;

	/* Pointer to the last character in the header */
	header_end = header + strlen(header) - 1;

	field_name_len = strlen(field_name);

	field_start = header;

	do{
		field_start = strstr(field_start+1, field_name);

		field_end = field_start + field_name_len - 1;

		if(field_start != NULL
				&& field_start - header >= 2
				&& field_start[-2] == '\r'
				&& field_start[-1] == '\n'
						&& header_end - field_end >= 1
						&& field_end[1] == ':')
		{
			break; /* Found the field */
		}
		else
		{
			continue; /* This is not the one; keep looking. */
		}
	} while(field_start != NULL);

	if(field_start == NULL)
		return NULL;

	/* Find the field terminator */
	next_crlf = strstr(field_start, "\r\n");

	/* A field is expected to end with \r\n */
	if(next_crlf == NULL)
		return NULL; /* Malformed HTTP header! */

	/* If not looking for a value, then return a pointer to the start of values string */
	if(value == NULL)
		return field_end+2;

	value_start = strstr(field_start, value);

	/* Value not found */
	if(value_start == NULL)
		return NULL;

	/* Found the value we're looking for */
	if(value_start > next_crlf)
		return NULL; /* ... but after the CRLF terminator of the field. */

	/* The value we found should be properly delineated from the other tokens */
	if(isalnum(value_start[-1]) || isalnum(value_start[strlen(value)]))
		return NULL;

	return value_start;
}

/*
 * WIP: Performs simple HTTP handshake. *fd* is the file descriptor of the
 * connection to the client. This function returns 0 if it succeeds,
 * or returns -1.
 */
static int http_handshake(int fd)
{
	char header[16384], accept_key[29], *keyhdstart, *keyhdend, res_header[256];
	size_t header_length = 0, res_header_sent = 0, res_header_length;
	ssize_t r;
	while(1) {
		while((r = read(fd, header+header_length,
				sizeof(header)-header_length)) == -1 && errno == EINTR);
		if(r == -1) {
			perror("read");
			return -1;
		} else if(r == 0) {
			fprintf(stderr, "HTTP Handshake: Got EOF");
			return -1;
		} else {
			header_length += r;
			if(header_length >= 4 &&
					memcmp(header+header_length-4, "\r\n\r\n", 4) == 0) {
				break;
			} else if(header_length == sizeof(header)) {
				fprintf(stderr, "HTTP Handshake: Too large HTTP headers");
				return -1;
			}
		}
	}

	if(http_header_find_field_value(header, "Upgrade", "websocket") == NULL ||
			http_header_find_field_value(header, "Connection", "Upgrade") == NULL ||
			(keyhdstart = http_header_find_field_value(header, "Sec-WebSocket-Key", NULL)) == NULL) {
		fprintf(stderr, "HTTP Handshake: Missing required header fields");
		return -1;
	}
	for(; *keyhdstart == ' '; ++keyhdstart);
	keyhdend = keyhdstart;
	for(; *keyhdend != '\r' && *keyhdend != ' '; ++keyhdend);
	if(keyhdend-keyhdstart != 24) {
		printf("%s\n", keyhdstart);
		fprintf(stderr, "HTTP Handshake: Invalid value in Sec-WebSocket-Key");
		return -1;
	}
	create_accept_key(accept_key, keyhdstart);
	snprintf(res_header, sizeof(res_header),
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n"
			"\r\n", accept_key);
	res_header_length = strlen(res_header);
	while(res_header_sent < res_header_length) {
		while((r = write(fd, res_header+res_header_sent,
				res_header_length-res_header_sent)) == -1 &&
				errno == EINTR);
		if(r == -1) {
			perror("write");
			return -1;
		} else {
			res_header_sent += r;
		}
	}
	return 0;
}


/**
 * \brief WIP: Wslay send callback
 */
ssize_t send_callback(wslay_event_context_ptr ctx,
		const uint8_t *data,
		size_t len,
		int flags,
		void *user_data)
{
	struct VSession *session = (struct VSession*)user_data;
	ssize_t r;
	int sflags = 0;

#ifdef MSG_MORE
if(flags & WSLAY_MSG_MORE) {
	sflags |= MSG_MORE;
}
#endif

	while((r = send(session->stream_conn->io_ctx.sockfd, data, len, sflags)) == -1 &&
			errno == EINTR);
	if(r == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		} else {
			wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		}
	}
	printf("WS Callback Send data\n");
	return r;
}


/**
 * \brief WIP: Wslay receive callback
 */
ssize_t recv_callback(wslay_event_context_ptr ctx,
		uint8_t *buf,
		size_t len,
		int flags,
		void *user_data)
{
	struct VSession *session = (struct VSession*)user_data;
	ssize_t r;
	(void)flags;
	while((r = recv(session->stream_conn->io_ctx.sockfd, buf, len, 0)) == -1 &&
			errno == EINTR);
	if(r == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		} else {
			wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		}
	} else if(r == 0) {
		/* Unexpected EOF is also treated as an error */
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		r = -1;
	}
	printf("Data recv callback, len: %ld, flags: %d\n", len, flags);
	return r;
}


/**
 * \brief WIP: recive message
 */
void on_msg_recv_callback(wslay_event_context_ptr ctx,
		const struct wslay_event_on_msg_recv_arg *arg,
		void *user_data)
{
	struct VSession *session = (struct VSession*)user_data;
	/* Echo back non-control message */
	if(!wslay_is_ctrl_frame(arg->opcode)) {
		struct wslay_event_msg msgarg;
		msgarg.opcode = arg->opcode;
		msgarg.msg = arg->msg;
		msgarg.msg_length = arg->msg_length;
		wslay_event_queue_msg(ctx, &msgarg);
	}

	memcpy(session->stream_conn->io_ctx.buf, arg->msg, arg->msg_length);
	session->stream_conn->io_ctx.buf[arg->msg_length] = '\0';
	printf("WS Callback Received data: %s, opcode: %d\n",
			session->stream_conn->io_ctx.buf,
			arg->opcode);
}


/**
 * \brief The function with websocket infinite loop
 */
void *vs_websocket_loop(void *arg)
{
	struct vContext *C = (struct vContext*)arg;
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	/*
	 * This structure is passed as *user_data* in callback functions.
	 */
	struct VSession *vsession = CTX_current_session(C);
	wslay_event_context_ptr wslay_ctx;
	fd_set read_set, write_set;
	struct timeval tv;
	int ret;
	int flags;

	struct wslay_event_callbacks callbacks = {
			recv_callback,
			send_callback,
			NULL,
			NULL,
			NULL,
			NULL,
			on_msg_recv_callback
	};

	/* Set blocking */
	flags = fcntl(io_ctx->sockfd, F_GETFL, 0);
	fcntl(io_ctx->sockfd, F_SETFL, flags & ~O_NONBLOCK);

	http_handshake(io_ctx->sockfd);

	/* Set non-blocking */
	flags = fcntl(io_ctx->sockfd, F_GETFL, 0);
	fcntl(io_ctx->sockfd, F_SETFL, flags | O_NONBLOCK);

	wslay_event_context_server_init(&wslay_ctx, &callbacks, vsession);

	/* "Never ending" loop */
	while(1)
	{
		/* Initialize read set */
		FD_ZERO(&read_set);
		FD_SET(io_ctx->sockfd, &read_set);

		/* Initialize write set */
		FD_ZERO(&write_set);
		FD_SET(io_ctx->sockfd, &write_set);

		tv.tv_sec = VRS_TIMEOUT;	/* User have to send something in 30 seconds */
		tv.tv_usec = 0;

		if( (ret = select(io_ctx->sockfd+1,
				&read_set,
				&write_set,
				NULL,			/* Exception*/
				&tv)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n",
					__FILE__, __FUNCTION__,  __LINE__, strerror(errno));
			goto end;
			/* Was event on the listen socket */
		} else if(ret>0){
			if(FD_ISSET(io_ctx->sockfd, &read_set)) {
				if( wslay_event_recv(wslay_ctx) != 0 ) {
					goto end;
				}
			} else if (FD_ISSET(io_ctx->sockfd, &write_set)) {
				if( wslay_event_send(wslay_ctx) != 0 ) {
					goto end;
				}
			}
		}
	}

end:
	v_print_log(VRS_PRINT_DEBUG_MSG, "Exiting WebSocket thread\n");

	pthread_exit(NULL);
	return NULL;
}
