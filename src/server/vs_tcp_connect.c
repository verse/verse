/*
 * $Id: vs_tcp_connect.c 1348 2012-09-19 20:08:18Z jiri $
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

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "verse_types.h"

#include "vs_main.h"
#include "vs_tcp_connect.h"
#include "vs_udp_connect.h"
#include "vs_auth_pam.h"
#include "vs_auth_csv.h"
#include "vs_node.h"
#include "vs_sys_nodes.h"

#include "v_common.h"
#include "v_pack.h"
#include "v_unpack.h"
#include "v_connection.h"
#include "v_session.h"
#include "v_stream.h"
#include "v_fake_commands.h"

/**
 * \brief Initialize VConnection at Verse server for potential clients
 * \param[in]	*stream_conn	The pointer at connection
 */
static void vs_init_stream_conn(struct VStreamConn *stream_conn)
{
	v_conn_stream_init(stream_conn);
	stream_conn->host_state = TCP_SERVER_STATE_LISTEN;	/* Server is in listen mode */
}

/**
 * \brief This function does user authentication according settings of the
 * verse server. When user is authenticated, then return user_id, otherwise
 * returns -1.
 */
int vs_user_auth(struct vContext *C, const char *username, const char *data)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	int uid = -1;

	switch(vs_ctx->auth_type) {
		case AUTH_METHOD_CSV_FILE:
			uid = vs_csv_auth_user(C, username, data);
			break;
		case AUTH_METHOD_PAM:
#if defined WITH_PAM
			uid = vs_pam_auth_user(C, username, data);
#else
			uid = -1;
#endif
			break;
		case AUTH_METHOD_LDAP:
			/* TODO: it needs real implementation */
			uid = -1;
			break;
	}

	return uid;
}

/**
 * \brief do TLS negotiation with client application
 * \param[in]	*C	The context of verse server
 * \return This function returns 1, when TLS connection was established and
 * it returns 0, when TLS was not established.
 */
static int vs_TLS_handshake(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VStreamConn *stream_conn = CTX_current_stream_conn(C);

	int flag, ret;

	/* Make sure socket is blocking */
	flag = fcntl(stream_conn->io_ctx.sockfd, F_GETFL, 0);
	if( (fcntl(stream_conn->io_ctx.sockfd, F_SETFL, flag & ~O_NONBLOCK)) == -1) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
		return 0;
	}

	/* Set up SSL */
	if( (stream_conn->io_ctx.ssl = SSL_new(vs_ctx->tls_ctx)) == NULL) {
		v_print_log(VRS_PRINT_ERROR, "Setting up SSL failed.\n");
		ERR_print_errors_fp(v_log_file());
		SSL_free(stream_conn->io_ctx.ssl);
		stream_conn->io_ctx.ssl = NULL;
		close(io_ctx->sockfd);
		return 0;
	}

	/* Bind socket and SSL */
	if (SSL_set_fd(stream_conn->io_ctx.ssl, io_ctx->sockfd) == 0) {
		v_print_log(VRS_PRINT_ERROR, "Failed binding socket descriptor and SSL.\n");
		ERR_print_errors_fp(v_log_file());
		SSL_free(stream_conn->io_ctx.ssl);
		stream_conn->io_ctx.ssl = NULL;
		close(io_ctx->sockfd);
		return 0;
	}

	SSL_set_mode(stream_conn->io_ctx.ssl, SSL_MODE_AUTO_RETRY);

	/* Do TLS handshake and negotiation */
	if( (ret = SSL_accept(stream_conn->io_ctx.ssl)) != 1) {
		int err = SSL_get_error(stream_conn->io_ctx.ssl, ret);
		v_print_log(VRS_PRINT_ERROR, "SSL handshake failed: %d -> %d\n", ret, err);
		ERR_print_errors_fp(v_log_file());
		SSL_free(stream_conn->io_ctx.ssl);
		stream_conn->io_ctx.ssl = NULL;
		close(io_ctx->sockfd);
		return 0;
	}

	v_print_log(VRS_PRINT_DEBUG_MSG, "SSL handshake succeed.\n");
	return 1;
}

/**
 * \brief Teardown TLS connection with client
 * \param[in]	*C	The context of verse server
 * \return This function returns 1, when TLS teardown was successfull, otherwise
 * it returns 0.
 */
static int vs_TLS_teardown(struct vContext *C)
{
	struct VStreamConn *stream_conn = CTX_current_stream_conn(C);
	int ret;

	v_print_log(VRS_PRINT_DEBUG_MSG, "Try to shut down SSL connection.\n");

	ret = SSL_shutdown(stream_conn->io_ctx.ssl);
	if(ret!=1) {
		ret = SSL_shutdown(stream_conn->io_ctx.ssl);
	}

	switch(ret) {
	case 1:
		/* SSL was successfully closed */
		v_print_log(VRS_PRINT_DEBUG_MSG, "SSL connection was shut down.\n");
		break;
	case 0:
	case -1:
	default:
		/* some error occured */
		v_print_log(VRS_PRINT_DEBUG_MSG, "SSL connection was not able to shut down.\n");
		break;
	}

	SSL_free(stream_conn->io_ctx.ssl);
	close(stream_conn->io_ctx.sockfd);

	return 1;
}


/**
 *
 */
static void vs_CLOSING(struct vContext *C)
{
	struct VStreamConn *stream_conn = CTX_current_stream_conn(C);

	if(stream_conn->io_ctx.ssl!=NULL) vs_TLS_teardown(C);

}


/**
 * \brief This function is never ending loop of server state STREAM_OPEN
 */
static int vs_STREAM_OPEN_loop(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct timeval tv;
	fd_set set;
	int flag, ret;

	/* Set socket non-blocking */
	flag = fcntl(io_ctx->sockfd, F_GETFL, 0);
	if( (fcntl(io_ctx->sockfd, F_SETFL, flag | O_NONBLOCK)) == -1) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
		return -1;
	}

	/* "Never ending" loop */
	while(1)
	{
		FD_ZERO(&set);
		FD_SET(io_ctx->sockfd, &set);

		/* Use negotiated FPS */
		tv.tv_sec = 0;
		tv.tv_usec = 1000000/vsession->fps_host;

		/* Wait for recieved data */
		if( (ret = select(io_ctx->sockfd+1, &set, NULL, NULL, &tv)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n",
					__FILE__, __FUNCTION__,  __LINE__, strerror(errno));
			goto end;
			/* Was event on the listen socket */
		} else if(ret>0 && FD_ISSET(io_ctx->sockfd, &set)) {

			if(v_STREAM_handle_messages(C) == 0) {
				goto end;
			}

			/* When some payload data were received, then poke data thread */
			sem_post(&vs_ctx->data.sem);
		}

		if(v_STREAM_send_message(C) == 0 ) {
			goto end;
		}
	}
end:

	/* Set socket blocking again */
	flag = fcntl(io_ctx->sockfd, F_GETFL, 0);
	if( (fcntl(io_ctx->sockfd, F_SETFL, flag & ~O_NONBLOCK)) == -1) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * \brief This function is called, when server is in NEGOTIATE newhost state
 */
static int vs_NEGOTIATE_newhost_loop(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VMessage *r_message = CTX_r_message(C);
	int i, buffer_pos=0;

	/* Reset content of received message */
	memset(r_message, 0, sizeof(struct VMessage));

	/* Unpack Verse message header */
	buffer_pos += v_unpack_message_header(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	/* Unpack all system commands */
	buffer_pos += v_unpack_message_system_commands(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	v_print_receive_message(C);

	/* Process all system commands */
	for(i=0; i<MAX_SYSTEM_COMMAND_COUNT && r_message->sys_cmd[i].cmd.id!=CMD_RESERVED_ID; i++) {
		switch(r_message->sys_cmd[i].cmd.id) {
		case CMD_CONFIRM_L_ID:
			if(r_message->sys_cmd[i].negotiate_cmd.feature==FTR_HOST_URL) {
				if(r_message->sys_cmd[i].negotiate_cmd.count == 1) {
					if(vsession->host_url!=NULL &&
							strcmp(vsession->host_url, (char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str) == 0) {
						return 1;
					}
				}
			}
			break;
		}
	}

	return 0;
}


/**
 * \brief This function is called, when server is in NEGOTIATE_cookie_ded state
 *
 * This function can create new thread for datagrame connection
 */
static int vs_NEGOTIATE_cookie_ded_loop(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VMessage *r_message = CTX_r_message(C);
	struct VMessage *s_message = CTX_s_message(C);
	int i, j, error;
	unsigned short buffer_pos = 0;
	int host_url_proposed = 0,
			host_cookie_proposed = 0,
			peer_cookie_confirmed = 0,
			ded_confirmed = 0;
	struct timeval tv;
	struct VURL url;
	int ret = 0;

	/* Reset content of received message */
	memset(r_message, 0, sizeof(struct VMessage));

	/* Unpack Verse message header */
	buffer_pos += v_unpack_message_header(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	/* Unpack all system commands */
	buffer_pos += v_unpack_message_system_commands(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	v_print_receive_message(C);

	/* Process all received system commands */
	for(i = 0;
			i<MAX_SYSTEM_COMMAND_COUNT &&
			r_message->sys_cmd[i].cmd.id != CMD_RESERVED_ID;
			i++)
	{
		switch(r_message->sys_cmd[i].cmd.id) {
		case CMD_CHANGE_R_ID:
			/* Client has to propose url in this state */
			if(r_message->sys_cmd[i].negotiate_cmd.feature == FTR_HOST_URL) {
				if(r_message->sys_cmd[i].negotiate_cmd.count > 0) {
					if(vsession->host_url != NULL) {
						free(vsession->host_url);
						vsession->host_url = NULL;
					}
					/* Only first proposed URL will be used */
					vsession->host_url = strdup((char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str);
					/* Check if proposed URL is correct */
					ret = v_parse_url(vsession->host_url, &url);
					if(ret == 1)
						host_url_proposed = 1;
					else
						host_url_proposed = 0;
				}
			/* Client has to propose host cookie in this state */
			} else if(r_message->sys_cmd[i].negotiate_cmd.feature == FTR_COOKIE) {
				if(r_message->sys_cmd[i].negotiate_cmd.count > 0) {
					if(vsession->host_cookie.str != NULL) {
						free(vsession->host_cookie.str);
					}
					vsession->host_cookie.str = strdup((char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str);
					host_cookie_proposed = 1;
				}
			} else {
				v_print_log(VRS_PRINT_WARNING, "This feature id: %d is not supported in this state\n",
						r_message->sys_cmd[i].negotiate_cmd.feature);
			}
			break;
		case CMD_CONFIRM_R_ID:
			/* Client has to confirm peer cookie in this state */
			if(r_message->sys_cmd[i].negotiate_cmd.feature == FTR_COOKIE) {
				if (r_message->sys_cmd[i].negotiate_cmd.count == 1) {
					if(vsession->peer_cookie.str != NULL &&
						strcmp(vsession->peer_cookie.str, (char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str) == 0)
					{
						gettimeofday(&tv, NULL);
						vsession->peer_cookie.tv.tv_sec = tv.tv_sec;
						vsession->peer_cookie.tv.tv_usec = tv.tv_usec;
						peer_cookie_confirmed = 1;
					}
				}
			} else {
				v_print_log(VRS_PRINT_WARNING, "This feature id: %d is not supported in this state\n",
						r_message->sys_cmd[i].negotiate_cmd.feature);
			}
			break;
		case CMD_CONFIRM_L_ID:
			/* Client has to confirm DED in this state */
			if(r_message->sys_cmd[i].negotiate_cmd.feature == FTR_DED) {
				if(r_message->sys_cmd[i].negotiate_cmd.count == 1) {
					if(vsession->ded.str != NULL &&
							strcmp(vsession->ded.str, (char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str) == 0)
					{
						ded_confirmed = 1;
					} else {
						printf("%s != %s\n", vsession->ded.str, (char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str);
					}
				}
			}
			break;
		default:
			v_print_log(VRS_PRINT_WARNING, "This command id: %d is not supported in this state\n",
					r_message->sys_cmd[i].cmd.id);
			break;
		}
	}


	/* Send response on cookie request */
	if(host_url_proposed==1 &&
			host_cookie_proposed==1 &&
			peer_cookie_confirmed==1 &&
			ded_confirmed==1)
	{
		struct vContext *new_C;
		char trans_proto[4];
		char sec_proto[5];
		int cmd_rank = 0;

		buffer_pos = VERSE_MESSAGE_HEADER_SIZE;

		/* Copy address of peer */
		memcpy(&vsession->peer_address, &io_ctx->peer_addr, sizeof(struct VNetworkAddress));

		/* Do not confirm proposed URL */
		v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CONFIRM_R_ID, FTR_HOST_URL, NULL);

		/* Find first unused port from port range */
		for(i=vs_ctx->port_low, j=0; i<vs_ctx->port_high; i++, j++) {
			if(!(vs_ctx->port_list[j].flag & SERVER_PORT_USED)) {
				vsession->dgram_conn->io_ctx.host_addr.port = vs_ctx->port_list[j].port_number;
				vs_ctx->port_list[j].flag |= SERVER_PORT_USED;
				break;
			}
		}

		/* Do not allow unsecure TCP data connection */
		if(url.transport_protocol == VRS_TP_TCP) {
			url.security_protocol = VRS_SEC_DATA_TLS;
		}

		/* Copy settings about data connection to the session */
		vsession->flags |= url.security_protocol;
		vsession->flags |= url.transport_protocol;

		if(vsession->flags & VRS_TP_UDP) {
			strncpy(trans_proto, "udp", 3);
			trans_proto[3] = '\0';

			/* Create copy of new Verse context for new thread */
			new_C = (struct vContext*)calloc(1, sizeof(struct vContext));
			memcpy(new_C, C, sizeof(struct vContext));

			/* Try to create new thread */
			if((ret = pthread_create(&vsession->udp_thread, NULL, vs_main_dgram_loop, (void*)new_C)) != 0) {
				if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "pthread_create(): %s\n", strerror(errno));
				return 0;
			}

			/* Wait for datagram thread to be in LISTEN state */
			while(vsession->dgram_conn->host_state != UDP_SERVER_STATE_LISTEN) {
				/* Sleep 1 milisecond */
				usleep(1000);
			}
		} else if(vsession->flags & VRS_TP_TCP) {
			strncpy(trans_proto, "tcp", 3);
			trans_proto[4] = '\0';
		}

#if OPENSSL_VERSION_NUMBER>=0x10000000
		if(url.security_protocol==VRS_SEC_DATA_NONE ||
				!(vs_ctx->security_protocol & VRS_SEC_DATA_TLS))
		{
			strncpy(sec_proto, "none", 4);
			sec_proto[4] = '\0';
		} else if(url.security_protocol==VRS_SEC_DATA_TLS) {
			if(vsession->flags & VRS_TP_UDP) {
				strncpy(sec_proto, "dtls", 4);
				sec_proto[4] = '\0';
			} else if(vsession->flags & VRS_TP_TCP) {
				strncpy(sec_proto, "tls", 3);
				sec_proto[3] = '\0';
			}
		} else {
			strncpy(sec_proto, "none", 4);
			sec_proto[4] = '\0';
		}
#else
		strncmp(sec_proto, "none", 4);
		sec_proto[4] = '\0';
#endif

		vsession->host_url = calloc(UCHAR_MAX, sizeof(char));
		if(url.ip_ver==IPV6) {
			sprintf(vsession->host_url, "verse-%s-%s://[%s]:%d",
					trans_proto,
					sec_proto,
					url.node,
					vsession->dgram_conn->io_ctx.host_addr.port);
		} else {
			sprintf(vsession->host_url, "verse-%s-%s://%s:%d",
					trans_proto,
					sec_proto,
					url.node,
					vsession->dgram_conn->io_ctx.host_addr.port);
		}
		v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CHANGE_L_ID, FTR_HOST_URL, vsession->host_url, NULL);

		/* Set time for the host cookie */
		gettimeofday(&tv, NULL);
		vsession->host_cookie.tv.tv_sec = tv.tv_sec;
		vsession->host_cookie.tv.tv_usec = tv.tv_usec;

		/* Send confirmation about host cookie */
		v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CONFIRM_R_ID, FTR_COOKIE,
				vsession->host_cookie.str, NULL);

		/* Pack all system commands to the buffer */
		buffer_pos += v_pack_stream_system_commands(s_message, &io_ctx->buf[buffer_pos]);

		/* Update length of message in the header (data in buffer) */
		s_message->header.version = VRS_VERSION;
		s_message->header.len = io_ctx->buf_size = buffer_pos;

		/* Pack header to the beginning of the buffer */
		v_pack_message_header(s_message, io_ctx->buf);

		v_print_send_message(C);

		/* Send command to the client */
		if( v_SSL_write(io_ctx, &error) > 0) {
			ret = 1;
		}
	}

	v_clear_url(&url);

	return ret;
}


/**
 *
 */
static int vs_RESPOND_userauth_loop(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VMessage *r_message = CTX_r_message(C);
	struct VMessage *s_message = CTX_s_message(C);
	int i, ret, error, cmd_rank = 0;
	unsigned short buffer_pos = 0;

	/* Reset content of received message */
	memset(r_message, 0, sizeof(struct VMessage));

	/* Unpack Verse message header */
	buffer_pos += v_unpack_message_header(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	/* Unpack all system commands */
	buffer_pos += v_unpack_message_system_commands(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	v_print_receive_message(C);

	for(i=0; i<MAX_SYSTEM_COMMAND_COUNT && r_message->sys_cmd[i].cmd.id!=CMD_RESERVED_ID; i++) {
		if(r_message->sys_cmd[i].cmd.id == CMD_USER_AUTH_REQUEST) {
			if(r_message->sys_cmd[i].ua_req.method_type == VRS_UA_METHOD_PASSWORD) {
				int user_id;

				/* Do user authentication */
				if((user_id = vs_user_auth(C,
						r_message->sys_cmd[i].ua_req.username,
						r_message->sys_cmd[i].ua_req.data)) != -1)
				{
					long int avatar_id;

					pthread_mutex_lock(&vs_ctx->data.mutex);
					avatar_id = vs_node_new_avatar_node(vs_ctx, vsession, user_id);
					pthread_mutex_unlock(&vs_ctx->data.mutex);

					if(avatar_id == -1) {
						v_print_log(VRS_PRINT_ERROR, "Failed to create avatar node\n");
						return 0;
					}

					buffer_pos = VERSE_MESSAGE_HEADER_SIZE;

					/* Save user_id to the session and send it in
					 * connect_accept command */
					vsession->user_id = user_id;
					vsession->avatar_id = avatar_id;

					s_message->sys_cmd[cmd_rank].ua_succ.id = CMD_USER_AUTH_SUCCESS;
					s_message->sys_cmd[cmd_rank].ua_succ.user_id = user_id;
					s_message->sys_cmd[cmd_rank].ua_succ.avatar_id = avatar_id;
					cmd_rank++;

					/* Generate random string for coockie */
					vsession->peer_cookie.str = (char*)calloc((COOKIE_SIZE+1), sizeof(char));
					for(i=0; i<COOKIE_SIZE; i++) {
						/* Generate only printable characters (debug prints) */
						vsession->peer_cookie.str[i] = 32 + (char)((float)rand()*94.0/RAND_MAX);
					}
					vsession->peer_cookie.str[COOKIE_SIZE] = '\0';
					/* Set up negotiate command of the host cookie */
					v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CHANGE_R_ID, FTR_COOKIE,
							vsession->peer_cookie.str, NULL);

					/* Load DED from configuration and save it to the session */
					vsession->ded.str = strdup(vs_ctx->ded);
					v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CHANGE_L_ID, FTR_DED,
							vsession->ded.str, NULL);

					buffer_pos += v_pack_stream_system_commands(s_message, &io_ctx->buf[buffer_pos]);

					s_message->header.len = io_ctx->buf_size = buffer_pos;
					s_message->header.version = VRS_VERSION;
					/* Pack header to the beginning of the buffer */
					v_pack_message_header(s_message, io_ctx->buf);

					v_print_send_message(C);

					/* Send command to the client */
					if( (ret = v_SSL_write(io_ctx, &error)) <= 0) {
						return 0;
					} else {
						return 1;
					}
				} else {

					buffer_pos = VERSE_MESSAGE_HEADER_SIZE;

					s_message->sys_cmd[0].ua_fail.id = CMD_USER_AUTH_FAILURE;
					s_message->sys_cmd[0].ua_fail.count = 0;

					s_message->sys_cmd[1].cmd.id = CMD_RESERVED_ID;

					buffer_pos += v_pack_stream_system_commands(s_message, &io_ctx->buf[buffer_pos]);

					s_message->header.len = io_ctx->buf_size = buffer_pos;
					s_message->header.version = VRS_VERSION;
					/* Pack header to the beginning of the buffer */
					v_pack_message_header(s_message, io_ctx->buf);

					v_print_send_message(C);

					/* Send command to the client */
					if( (ret = v_SSL_write(io_ctx, &error)) <= 0) {
						/* When error is SSL_ERROR_ZERO_RETURN, then SSL connection was closed by peer */
						return 0;
					} else {
						return 0;
					}
				}
			}
		}
	}
	return 0;
}

/**
 * \brief This function process received message and send respond to the client
 * if client sent CMD_USER_AUTH_REQUEST with method type UA_METHOD_NONE. The
 * response contains list of supported authentication methods (only Password
 * method is supported now)
 * \param[in]	*C	The pointer at Verse context
 * \return		The function returns 1, when response was sent to the client
 * and it returns 0, when all needs were not meet or error occurred.
 */
static int vs_RESPOND_methods_loop(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VMessage *r_message = CTX_r_message(C);
	struct VMessage *s_message = CTX_s_message(C);
	int i, ret, error, auth_req = 0,
			client_name_proposed = 0,
			client_version_proposed = 0;
	unsigned short buffer_pos = 0;

	/* Reset content of received message */
	memset(r_message, 0, sizeof(struct VMessage));

	/* Unpack Verse message header */
	buffer_pos += v_unpack_message_header(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	/* Unpack all system commands */
	buffer_pos += v_unpack_message_system_commands(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	v_print_receive_message(C);

	for(i=0; i<MAX_SYSTEM_COMMAND_COUNT && r_message->sys_cmd[i].cmd.id!=CMD_RESERVED_ID; i++) {
		switch(r_message->sys_cmd[i].cmd.id) {
		case CMD_USER_AUTH_REQUEST:
			if(r_message->sys_cmd[i].ua_req.method_type == VRS_UA_METHOD_NONE) {
				auth_req = 1;
			} else {
				v_print_log(VRS_PRINT_WARNING, "This auth method id: %d is not supported in this state\n", r_message->sys_cmd[i].ua_req.method_type);
			}
			break;
		case CMD_CHANGE_L_ID:
			/* Client could propose client name and version */
			if(r_message->sys_cmd[i].negotiate_cmd.feature == FTR_CLIENT_NAME) {
				if(r_message->sys_cmd[i].negotiate_cmd.count > 0) {
					/* Only first proposed client name will be used */
					vsession->client_name = strdup((char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str);
					client_name_proposed = 1;
				}
			} else if(r_message->sys_cmd[i].negotiate_cmd.feature == FTR_CLIENT_VERSION) {
				if(r_message->sys_cmd[i].negotiate_cmd.count > 0) {
					/* Only first proposed client name will be used */
					vsession->client_version = strdup((char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str);
					client_version_proposed = 1;
				}
			}
			break;
		default:
			v_print_log(VRS_PRINT_WARNING,
					"This command id: %d is not supported in this state\n",
					r_message->sys_cmd[i].cmd.id);
			break;
		}
	}

	if(auth_req == 1) {
		int cmd_rank = 0;
		/* VRS_UA_METHOD_NONE is not supported method. Send list of
		 * supported methods. Current implementation supports
		 * only PASSWORD method now. */

		s_message->sys_cmd[cmd_rank].ua_fail.id = CMD_USER_AUTH_FAILURE;
		/* List of supported methods */
		s_message->sys_cmd[cmd_rank].ua_fail.count = 1;
		s_message->sys_cmd[cmd_rank].ua_fail.method[0] = VRS_UA_METHOD_PASSWORD;
		cmd_rank++;

		/* Send confirmation about client name */
		if(client_name_proposed == 1) {
			v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CONFIRM_L_ID, FTR_CLIENT_NAME,
					vsession->client_name, NULL);
		}

		/* Send confirmation about client version only in situation, when
		 * client proposed client name too */
		if(client_version_proposed == 1) {
			if(client_name_proposed == 1) {
				v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CONFIRM_L_ID, FTR_CLIENT_VERSION,
						vsession->client_version, NULL);
			} else {
				/* Client version without client name is not allowed */
				v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CONFIRM_L_ID, FTR_CLIENT_VERSION,
						NULL);
			}
		}

		buffer_pos = VERSE_MESSAGE_HEADER_SIZE;

		/* Pack system commands to the buffer */
		buffer_pos += v_pack_stream_system_commands(s_message,&io_ctx->buf[buffer_pos]);

		s_message->header.len = io_ctx->buf_size = buffer_pos;
		s_message->header.version = VRS_VERSION;
		/* Pack header to the beginning of the buffer */
		v_pack_message_header(s_message, io_ctx->buf);

		/* Debug print of command to be send */
		if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
			v_print_send_message(C);
		}

		/* Send command to the client */
		if( (ret = v_SSL_write(io_ctx, &error)) <= 0) {
			/* When error is SSL_ERROR_ZERO_RETURN, then SSL connection was closed by peer */
			return 0;
		} else {
			return 1;
		}
	}

	return 0;
}


/**
 * \brief Main function for new thread. This thread is created for new
 * connection with client. This thread will try to authenticate new user
 * and negotiate new udp port. */
void *vs_tcp_conn_loop(void *arg)
{
	struct vContext *C = (struct vContext*)arg;
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VStreamConn *stream_conn = CTX_current_stream_conn(C);
	struct VMessage *r_message=NULL, *s_message=NULL;
	struct timeval tv;
	fd_set set;
	int error, ret, user_auth_attempts=0;
	void *udp_thread_result;
	unsigned int int_size;

	/* Try to get size of TCP buffer */
	int_size = sizeof(int_size);
	getsockopt(io_ctx->sockfd, SOL_SOCKET, SO_RCVBUF,
			(void *)&stream_conn->socket_buffer_size, &int_size);

	/* Try to do TLS handshake with client */
	if(vs_TLS_handshake(C)!=1) {
		goto end;
	}

	r_message = (struct VMessage*)calloc(1, sizeof(struct VMessage));
	s_message = (struct VMessage*)calloc(1, sizeof(struct VMessage));
	CTX_r_message_set(C, r_message);
	CTX_s_message_set(C, s_message);

	stream_conn->host_state = TCP_SERVER_STATE_RESPOND_METHODS;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: RESPOND_methods\n");
		printf("%c[%dm", 27, 0);
	}

	user_auth_attempts = 0;

	/* "Never ending" loop */
	while(1)
	{
		FD_ZERO(&set);
		FD_SET(io_ctx->sockfd, &set);

		tv.tv_sec = VRS_TIMEOUT;	/* User have to send something in 30 seconds */
		tv.tv_usec = 0;

		if( (ret = select(io_ctx->sockfd+1, &set, NULL, NULL, &tv)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n",
					__FILE__, __FUNCTION__,  __LINE__, strerror(errno));
			goto end;
			/* Was event on the listen socket */
		} else if(ret>0 && FD_ISSET(io_ctx->sockfd, &set)) {

			/* Try to receive data through SSL connection */
			if( v_SSL_read(io_ctx, &error) <= 0 ) {
				goto end;
			}

			/* Make sure, that buffer contains at least Verse message
			 * header. If this condition is not reached, then somebody tries
			 * to do some very bad things! .. Close this connection. */
			if(io_ctx->buf_size < VERSE_MESSAGE_HEADER_SIZE) {
				if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "received buffer too small %d\n",
						io_ctx->buf_size);
				goto end;
			}

			switch(stream_conn->host_state) {
			case TCP_SERVER_STATE_RESPOND_METHODS:
				ret = vs_RESPOND_methods_loop(C);
				if(ret==1) {
					stream_conn->host_state = TCP_SERVER_STATE_RESPOND_USRAUTH;
					if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
						printf("%c[%d;%dm", 27, 1, 31);
						v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: RESPOND_userauth\n");
						printf("%c[%dm", 27, 0);
					}
				} else {
					goto end;
				}
				break;
			case TCP_SERVER_STATE_RESPOND_USRAUTH:
				ret = vs_RESPOND_userauth_loop(C);
				if(ret==1) {
					stream_conn->host_state = TCP_SERVER_STATE_NEGOTIATE_COOKIE_DED;

					if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
						printf("%c[%d;%dm", 27, 1, 31);
						v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: NEGOTIATE_cookie_ded\n");
						printf("%c[%dm", 27, 0);
					}
				} else {
					user_auth_attempts++;
					if(user_auth_attempts >= MAX_USER_AUTH_ATTEMPTS) {
						goto end;
					}
				}
				break;
			case TCP_SERVER_STATE_NEGOTIATE_COOKIE_DED:
				ret = vs_NEGOTIATE_cookie_ded_loop(C);
				if(ret==1) {
					stream_conn->host_state = TCP_SERVER_STATE_NEGOTIATE_NEWHOST;

					if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
						printf("%c[%d;%dm", 27, 1, 31);
						v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: NEGOTIATE_newhost\n");
						printf("%c[%dm", 27, 0);
					}
				} else {
					goto end;
				}
				break;
			case TCP_SERVER_STATE_NEGOTIATE_NEWHOST:
				/* Wait VERSE_TIMEOT seconds to confirming proposed URL */
				ret = vs_NEGOTIATE_newhost_loop(C);
				if(ret==1) {
					if(vsession->flags & VRS_TP_UDP) {
						/* When URL was confirmed, then go to the end state */
						goto end;
					} else if (vsession->flags & VRS_TP_TCP) {
						stream_conn->host_state = TCP_SERVER_STATE_STREAM_OPEN;

						if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
							printf("%c[%d;%dm", 27, 1, 31);
							v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: STREAM_OPEN\n");
							printf("%c[%dm", 27, 0);
						}

						vs_STREAM_OPEN_loop(C);
						goto end;
					}
				} else {
					/* When thread was not confirmed, then try to cancel
					 * UDP thread*/
					if(pthread_cancel(vsession->udp_thread) != 0) {
						v_print_log(VRS_PRINT_DEBUG_MSG, "UDP thread was not canceled\n");
					}
					goto end;
				}
				break;

			}
		} else {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "No response in %d seconds\n", VRS_TIMEOUT);
			goto end;
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

	/* Receive and Send messages are not neccessary any more */
	if(r_message!=NULL) {
		free(r_message);
		r_message = NULL;
		CTX_r_message_set(C, NULL);
	}
	if(s_message!=NULL) {
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

	/* Was udp thread created? */
	if(vsession->udp_thread != 0 ) {
		/* Wait for UDP thread (this is blocking operation) */
		v_print_log(VRS_PRINT_DEBUG_MSG, "Waiting for join with UDP thread ...\n");
		if(pthread_join(vsession->udp_thread, &udp_thread_result) != 0) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "UDP thread was not joined\n");
		}
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


/**
 * \brief Main Verse server loop. Server waits for connect attempts, responds to attempts
 * and creates per connection threads
 */
int vs_main_stream_loop(VS_CTX *vs_ctx)
{
	struct vContext *C;
	struct IO_CTX *io_ctx = &vs_ctx->tcp_io_ctx;
	struct timeval start, tv;
	fd_set set;
	int count, tmp, i, ret;
	static uint32 last_session_id = 0;

	/* Allocate context for server */
	C = (struct vContext*)calloc(1, sizeof(struct vContext));
	/* Set up client context, connection context and IO context */
	CTX_server_ctx_set(C, vs_ctx);
	CTX_client_ctx_set(C, NULL);
	CTX_io_ctx_set(C, io_ctx);
	CTX_current_dgram_conn_set(C, NULL);

	/* Get time of the start of the server */
	gettimeofday(&start, NULL);

	/* Seed random number generator */
#ifdef __APPLE__
	sranddev();
	/* Other BSD based systems probably support this or similar function too. */
#else
	/* Other systems have to use this evil trick */
	srand(start.tv_sec - start.tv_usec);
#endif

	/* "Never ending" listen loop */
	while(vs_ctx->state == SERVER_STATE_READY) {
		/* Debug print */
		gettimeofday(&tv, NULL);
		if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
			if(tv.tv_sec==start.tv_sec)
				v_print_log(VRS_PRINT_DEBUG_MSG, "\t+0s");
			else
				v_print_log(VRS_PRINT_DEBUG_MSG, "\t+%lds", (long int)(tv.tv_sec - start.tv_sec));
			v_print_log_simple(VRS_PRINT_DEBUG_MSG, "\tServer listen on TCP port: %d\n", vs_ctx->tcp_io_ctx.host_addr.port);
		}

		/* Set up set of sockets */
		FD_ZERO(&set);
		FD_SET(io_ctx->sockfd, &set);

		/* We will wait one second for connect attempt, then debug print will
		 * be print again */
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		/* Wait for event on listening socket */
		if( (ret = select(io_ctx->sockfd+1, &set, NULL, NULL, &tv)) == -1 ) {
			int err = errno;
			if(err==EINTR) {
				break;
			} else {
				if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n", __FILE__, __FUNCTION__,  __LINE__, strerror(err));
				return -1;
			}
			/* Was event on the listen socket */
		} else if(ret>0 && FD_ISSET(io_ctx->sockfd, &set)) {
			struct VSession *current_session = NULL;
			socklen_t addr_len;
			int i;

			v_print_log(VRS_PRINT_DEBUG_MSG, "Connection attempt\n");

			/* Try to find free session */
			for(i=0; i< vs_ctx->max_sessions; i++) {
				if(vs_ctx->vsessions[i]->stream_conn->host_state==TCP_SERVER_STATE_LISTEN) {
					/* TODO: lock mutex here */
					current_session = vs_ctx->vsessions[i];
					current_session->stream_conn->host_state=TCP_SERVER_STATE_RESPOND_METHODS;
					current_session->session_id = last_session_id++;
					/* TODO: unlock mutex here */
					break;
				}
			}

			if(current_session != NULL) {
				struct vContext *new_C;

				/* Try to accept client connection (do TCP handshake) */
				if(io_ctx->host_addr.ip_ver==IPV4) {
					/* Prepare IPv4 variables for TCP handshake */
					struct sockaddr_in *client_addr4 = &current_session->stream_conn->io_ctx.peer_addr.addr.ipv4;
					current_session->stream_conn->io_ctx.peer_addr.ip_ver = IPV4;
					addr_len = sizeof(current_session->stream_conn->io_ctx.peer_addr.addr.ipv4);

					/* Try to do TCP handshake */
					if( (current_session->stream_conn->io_ctx.sockfd = accept(io_ctx->sockfd, (struct sockaddr*)client_addr4, &addr_len)) == -1) {
						if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "accept(): %s\n", strerror(errno));
						continue;
					}

					/* Save the IPv4 of the client as string in verse session */
					inet_ntop(AF_INET, &client_addr4->sin_addr, current_session->peer_hostname, INET_ADDRSTRLEN);
				} else if(io_ctx->host_addr.ip_ver==IPV6) {
					/* Prepare IPv6 variables for TCP handshake */
					struct sockaddr_in6 *client_addr6 = &current_session->stream_conn->io_ctx.peer_addr.addr.ipv6;
					current_session->stream_conn->io_ctx.peer_addr.ip_ver = IPV6;
					addr_len = sizeof(current_session->stream_conn->io_ctx.peer_addr.addr.ipv6);

					/* Try to do TCP handshake */
					if( (current_session->stream_conn->io_ctx.sockfd = accept(io_ctx->sockfd, (struct sockaddr*)client_addr6, &addr_len)) == -1) {
						if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "accept(): %s\n", strerror(errno));
						continue;
					}

					/* Save the IPv6 of the client as string in verse session */
					inet_ntop(AF_INET6, &client_addr6->sin6_addr, current_session->peer_hostname, INET6_ADDRSTRLEN);
				}

				CTX_current_session_set(C, current_session);
				CTX_current_stream_conn_set(C, current_session->stream_conn);
				CTX_io_ctx_set(C, &current_session->stream_conn->io_ctx);

				if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
					v_print_log(VRS_PRINT_DEBUG_MSG, "New connection from: ");
					v_print_addr_port(VRS_PRINT_DEBUG_MSG, &(current_session->stream_conn->io_ctx.peer_addr));
					v_print_log_simple(VRS_PRINT_DEBUG_MSG, "\n");
				}

				/* Duplicate verse context for new thread */
				new_C = (struct vContext*)calloc(1, sizeof(struct vContext));
				memcpy(new_C, C, sizeof(struct vContext));

				/* Try to initialize thread attributes */
				if( (ret = pthread_attr_init(&current_session->tcp_thread_attr)) !=0 ) {
					if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "pthread_attr_init(): %s\n", strerror(errno));
					continue;
				}

				/* Try to set thread attributes as detached */
				if( (ret = pthread_attr_setdetachstate(&current_session->tcp_thread_attr, PTHREAD_CREATE_DETACHED)) != 0) {
					if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "pthread_attr_setdetachstate(): %s\n", strerror(errno));
					continue;
				}

				/* Try to create new thread */
				if((ret = pthread_create(&current_session->tcp_thread, &current_session->tcp_thread_attr, vs_tcp_conn_loop, (void*)new_C)) != 0) {
					if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "pthread_create(): %s\n", strerror(errno));
				}

				/* Destroy thread attributes */
				pthread_attr_destroy(&current_session->tcp_thread_attr);
			} else {
				int tmp_sockfd;
				v_print_log(VRS_PRINT_DEBUG_MSG, "Number of session slot: %d reached\n", vs_ctx->max_sessions);
				/* TODO: return some error. Not only accept and close connection. */
				/* Try to accept client connection (do TCP handshake) */
				if(io_ctx->host_addr.ip_ver==IPV4) {
					/* Prepare IP6 variables for TCP handshake */
					struct sockaddr_in client_addr4;

					addr_len = sizeof(client_addr4);

					/* Try to do TCP handshake */
					if( (tmp_sockfd = accept(io_ctx->sockfd, (struct sockaddr*)&client_addr4, &addr_len)) == -1) {
						if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "accept(): %s\n", strerror(errno));
						continue;
					}
					/* Close connection (no more free session slots) */
					close(tmp_sockfd);
				} else if(io_ctx->host_addr.ip_ver==IPV6) {
					/* Prepare IP6 variables for TCP handshake */
					struct sockaddr_in6 client_addr6;
					addr_len = sizeof(client_addr6);

					/* Try to do TCP handshake */
					if( (tmp_sockfd = accept(io_ctx->sockfd, (struct sockaddr*)&client_addr6, &addr_len)) == -1) {
						if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "accept(): %s\n", strerror(errno));
						continue;
					}
					/* Close connection (no more free session slots) */
					close(tmp_sockfd);
				}
				/* TODO: Fix this */
				sleep(1);
			}
		}
	}

	count = 0;
	while( (vs_ctx->state == SERVER_STATE_CLOSING) && (count < VRS_TIMEOUT) ) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Wait for Server state CLOSED\n");

		/* Check if there are still some pending connection */
		tmp = 0;
		for(i=0; i<vs_ctx->max_sessions; i++) {
			if(vs_ctx->vsessions[i] != NULL) {
				if(vs_ctx->vsessions[i]->stream_conn != NULL &&
					vs_ctx->vsessions[i]->stream_conn->host_state != TCP_SERVER_STATE_LISTEN ) {
					tmp++;
				}
				/* TODO: cancel thread with closed connection to speed up server exit
					pthread_kill(vs_ctx->vsessions[i]->tcp_thread, SIGALRM);
					pthread_join(vs_ctx->vsessions[i]->tcp_thread, NULL);
				*/

			}
		}
		if(tmp==0) {
			vs_ctx->state = SERVER_STATE_CLOSED;
		} else {
			sleep(1);
		}
	}

	free(C);

	return 1;
}


/**
 * \brief Destroy stream part of verse context
 */
void vs_destroy_stream_ctx(VS_CTX *vs_ctx)
{
	if(vs_ctx->tls_ctx != NULL) SSL_CTX_free(vs_ctx->tls_ctx);
	if(vs_ctx->dtls_ctx != NULL) SSL_CTX_free(vs_ctx->dtls_ctx);
}


/**
 * \brief Initialize verse server context for connection at TCP
 */
int vs_init_stream_ctx(VS_CTX *vs_ctx)
{
	int i, flag;

	/* Set up the library */
	SSL_library_init();
	ERR_load_BIO_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();

	/* Set up SSL context for TLS  */
	if( (vs_ctx->tls_ctx = SSL_CTX_new(TLSv1_server_method())) == NULL ) {
		v_print_log(VRS_PRINT_ERROR, "Setting up SSL_CTX failed.\n");
		ERR_print_errors_fp(v_log_file());
		return -1;
	}

	/* Load certificate chain file from CA */
	if(vs_ctx->ca_cert_file != NULL) {
		if(SSL_CTX_use_certificate_chain_file(vs_ctx->tls_ctx, vs_ctx->ca_cert_file) != 1) {
			v_print_log(VRS_PRINT_ERROR, "TLS: Loading certificate chain file: %s failed.\n",
					vs_ctx->ca_cert_file);
			ERR_print_errors_fp(v_log_file());
			return -1;
		}
	}

	/* Load certificate with public key for TLS */
	if(SSL_CTX_use_certificate_file(vs_ctx->tls_ctx, vs_ctx->public_cert_file, SSL_FILETYPE_PEM) != 1) {
		v_print_log(VRS_PRINT_ERROR, "TLS: Loading certificate file: %s failed.\n",
				vs_ctx->public_cert_file);
		ERR_print_errors_fp(v_log_file());
		return -1;
	}

	/* Load private key for TLS */
	if(SSL_CTX_use_PrivateKey_file(vs_ctx->tls_ctx, vs_ctx->private_cert_file, SSL_FILETYPE_PEM) != 1) {
		v_print_log(VRS_PRINT_ERROR, "TLS: Loading private key file: %s failed.\n",
				vs_ctx->private_cert_file);
		ERR_print_errors_fp(v_log_file());
		return -1;
	}

	/* Check the consistency of a private key with the corresponding
	 * certificate loaded into ssl_ctx */
	if(SSL_CTX_check_private_key(vs_ctx->tls_ctx) != 1) {
		v_print_log(VRS_PRINT_ERROR, "TLS: Private key does not match the certificate public key\n");
		ERR_print_errors_fp(v_log_file());
		return -1;
	}

	/* When CA certificate file was set, then try to load it */
	if(vs_ctx->ca_cert_file != NULL) {
		if(SSL_CTX_load_verify_locations(vs_ctx->tls_ctx, vs_ctx->ca_cert_file, NULL) != 1) {
			v_print_log(VRS_PRINT_ERROR, "TLS: Loading CA certificate file: %s failed.\n",
					vs_ctx->ca_cert_file);
			ERR_print_errors_fp(v_log_file());
			return -1;
		}
	}

#if OPENSSL_VERSION_NUMBER>=0x10000000

	/* Set up SSL context for DTLS  */
	if( (vs_ctx->dtls_ctx = SSL_CTX_new(DTLSv1_server_method())) == NULL ) {
		v_print_log(VRS_PRINT_ERROR, "Setting up SSL_CTX failed.\n");
		ERR_print_errors_fp(v_log_file());
		return -1;
	}

	/* Load certificate chain file from CA */
	if(vs_ctx->ca_cert_file != NULL) {
		if(SSL_CTX_use_certificate_chain_file(vs_ctx->dtls_ctx, vs_ctx->ca_cert_file) != 1) {
			v_print_log(VRS_PRINT_ERROR, "DTLS: Loading certificate chain file: %s failed.\n",
					vs_ctx->ca_cert_file);
			ERR_print_errors_fp(v_log_file());
			return -1;
		}
	}

	/* Load certificate with public key for DTLS */
	if (SSL_CTX_use_certificate_file(vs_ctx->dtls_ctx, vs_ctx->public_cert_file, SSL_FILETYPE_PEM) != 1) {
		v_print_log(VRS_PRINT_ERROR, "DTLS: Loading certificate file: %s failed.\n",
						vs_ctx->public_cert_file);
		ERR_print_errors_fp(v_log_file());
		return -1;
	}

	/* Load private key for DTLS */
	if(SSL_CTX_use_PrivateKey_file(vs_ctx->dtls_ctx, vs_ctx->private_cert_file, SSL_FILETYPE_PEM) != 1) {
		v_print_log(VRS_PRINT_ERROR, "DTLS: Loading private key file: %s failed.\n",
						vs_ctx->private_cert_file);
		ERR_print_errors_fp(v_log_file());
		return -1;
	}

	/* Check the consistency of a private key with the corresponding
	 * certificate loaded into ssl_ctx */
	if(SSL_CTX_check_private_key(vs_ctx->dtls_ctx) != 1) {
		v_print_log(VRS_PRINT_ERROR, "DTLS: Private key does not match the certificate public key\n");
		ERR_print_errors_fp(v_log_file());
		return -1;
	}

	/* When CA certificate file was set, then try to load it */
	if(vs_ctx->ca_cert_file != NULL) {
		if(SSL_CTX_load_verify_locations(vs_ctx->dtls_ctx, vs_ctx->ca_cert_file, NULL) != 1) {
			v_print_log(VRS_PRINT_ERROR, "DTLS: Loading CA certificate file: %s failed.\n",
					vs_ctx->ca_cert_file);
			ERR_print_errors_fp(v_log_file());
			return -1;
		}
	}

	/* Set up callback functions for DTLS cookie */
	SSL_CTX_set_cookie_generate_cb(vs_ctx->dtls_ctx, vs_dtls_generate_cookie);
	SSL_CTX_set_cookie_verify_cb(vs_ctx->dtls_ctx, vs_dtls_verify_cookie);
	/* Accept all cipher including NULL cipher (testing) */
	if( SSL_CTX_set_cipher_list(vs_ctx->dtls_ctx, "ALL:NULL:eNULL:aNULL") == 0) {
		v_print_log(VRS_PRINT_ERROR, "Setting ciphers for DTLS failed.\n");
		ERR_print_errors_fp(v_log_file());
		return 0;
	}
	/* DTLS require this */
	SSL_CTX_set_read_ahead(vs_ctx->dtls_ctx, 1);
#else
	vs_ctx->dtls_ctx = NULL;
#endif

	vs_ctx->connected_clients = 0;

	/* Allocate buffer for incoming packets */
	if ( (vs_ctx->tcp_io_ctx.buf = (char*)calloc(MAX_PACKET_SIZE, sizeof(char))) == NULL) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "calloc(): %s\n", strerror(errno));
		return -1;
	}

	/* "Address" of server */
	if(vs_ctx->tcp_io_ctx.host_addr.ip_ver == IPV4) {		/* IPv4 */

		/* Create socket which server uses for listening for new connections */
		if ( (vs_ctx->tcp_io_ctx.sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1 ) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "socket(): %s\n", strerror(errno));
			return -1;
		}

		/* Set socket non-blocking */
/*		flag = fcntl(vs_ctx->io_ctx.sockfd, F_GETFL, 0);
		if( (fcntl(vs_ctx->io_ctx.sockfd, F_SETFL, flag | O_NONBLOCK)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
			return -1;
		}*/

		/* Set socket to reuse address */
		flag = 1;
		if( setsockopt(vs_ctx->tcp_io_ctx.sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "setsockopt(): %s\n", strerror(errno));
			return -1;
		}

		vs_ctx->tcp_io_ctx.host_addr.addr.ipv4.sin_family = AF_INET;
		vs_ctx->tcp_io_ctx.host_addr.addr.ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
		vs_ctx->tcp_io_ctx.host_addr.addr.ipv4.sin_port = htons(vs_ctx->port);
		vs_ctx->tcp_io_ctx.host_addr.port = vs_ctx->port;

		/* Bind address and socket */
		if( bind(vs_ctx->tcp_io_ctx.sockfd, (struct sockaddr*)&(vs_ctx->tcp_io_ctx.host_addr.addr.ipv4), sizeof(vs_ctx->tcp_io_ctx.host_addr.addr.ipv4)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "bind(): %s\n", strerror(errno));
			return -1;
		}

		/* Create queue for TCP connection attempts */
		if( listen(vs_ctx->tcp_io_ctx.sockfd, vs_ctx->max_sessions) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "listen(): %s\n", strerror(errno));
			return -1;
		}
	}
	else if(vs_ctx->tcp_io_ctx.host_addr.ip_ver == IPV6) {	/* IPv6 */

		/* Create socket which server uses for listening for new connections */
		if ( (vs_ctx->tcp_io_ctx.sockfd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) == -1 ) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "socket(): %s\n", strerror(errno));
			return -1;
		}

		/* Set socket non-blocking */
/*		flag = fcntl(vs_ctx->io_ctx.sockfd, F_GETFL, 0);
		if( (fcntl(vs_ctx->io_ctx.sockfd, F_SETFL, flag | O_NONBLOCK)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
			return -1;
		}*/

		/* Set socket to reuse address */
		flag = 1;
		if( setsockopt(vs_ctx->tcp_io_ctx.sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "setsockopt(): %s\n", strerror(errno));
			return -1;
		}

		vs_ctx->tcp_io_ctx.host_addr.addr.ipv6.sin6_family = AF_INET6;
		vs_ctx->tcp_io_ctx.host_addr.addr.ipv6.sin6_addr = in6addr_any;
		vs_ctx->tcp_io_ctx.host_addr.addr.ipv6.sin6_port = htons(vs_ctx->port);
		vs_ctx->tcp_io_ctx.host_addr.addr.ipv6.sin6_flowinfo = 0; /* Obsolete value */
		vs_ctx->tcp_io_ctx.host_addr.addr.ipv6.sin6_scope_id = 0;
		vs_ctx->tcp_io_ctx.host_addr.port = vs_ctx->port;

		/* Bind address and socket */
		if( bind(vs_ctx->tcp_io_ctx.sockfd, (struct sockaddr*)&(vs_ctx->tcp_io_ctx.host_addr.addr.ipv6), sizeof(vs_ctx->tcp_io_ctx.host_addr.addr.ipv6)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "bind(): %d\n", strerror(errno));
			return -1;
		}

		/* Create queue for TCP connection attempts, set maximum number of
		 * attempts in listen queue */
		if( listen(vs_ctx->tcp_io_ctx.sockfd, vs_ctx->max_sessions) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "listen(): %s\n", strerror(errno));
			return -1;
		}
	}

	/* Set up flag for V_CTX of server */
	vs_ctx->tcp_io_ctx.flags = 0;

	/* Set all bytes of buffer for incoming packet to zero */
	memset(vs_ctx->tcp_io_ctx.buf, 0, MAX_PACKET_SIZE);

	/* Initialize list of connections */
	vs_ctx->vsessions = (struct VSession**)calloc(vs_ctx->max_sessions, sizeof(struct VSession*));
	for (i=0; i<vs_ctx->max_sessions; i++) {
		if( (vs_ctx->vsessions[i] = (struct VSession*)calloc(1, sizeof(struct VSession))) == NULL ) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "malloc(): %s\n", strerror(errno));
			return -1;
		}
		/* Set up verse session */
		v_init_session(vs_ctx->vsessions[i]);
		/* Set up input and output queues */
		vs_ctx->vsessions[i]->in_queue = (struct VInQueue*)calloc(1, sizeof(VInQueue));
		v_in_queue_init(vs_ctx->vsessions[i]->in_queue, vs_ctx->in_queue_max_size);
		vs_ctx->vsessions[i]->out_queue = (struct VOutQueue*)calloc(1, sizeof(VOutQueue));
		v_out_queue_init(vs_ctx->vsessions[i]->out_queue, vs_ctx->out_queue_max_size);
		/* Allocate memory for TCP connection */
		vs_ctx->vsessions[i]->stream_conn = (struct VStreamConn*)calloc(1, sizeof(struct VStreamConn));
		/* Allocate memory for peer hostname */
		vs_ctx->vsessions[i]->peer_hostname = (char*)calloc(INET6_ADDRSTRLEN, sizeof(char));
		/* Initialize TCP connection */
		vs_init_stream_conn(vs_ctx->vsessions[i]->stream_conn);
		/* Allocate memory for UDP connection */
		vs_ctx->vsessions[i]->dgram_conn = (struct VDgramConn*)calloc(1, sizeof(struct VDgramConn));
		/* Initialize UDP connection */
		vs_init_dgram_conn(vs_ctx->vsessions[i]->dgram_conn);
		/* Initialize Avatar ID */
		vs_ctx->vsessions[i]->avatar_id = -1;
#if defined WITH_PAM
		/* PAM authentication stuff */
		vs_ctx->vsessions[i]->conv.conv = vs_pam_conv;
		vs_ctx->vsessions[i]->conv.appdata_ptr = NULL;
		vs_ctx->vsessions[i]->pamh = NULL;
#endif
	}
	return 1;
}
