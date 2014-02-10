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

#ifdef WITH_OPENSSL
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#ifdef __APPLE__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>

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

#ifdef WITH_OPENSSL
/**
 * \brief do TLS negotiation with client application
 * \param[in]	*C	The context of verse server
 * \return This function returns 1, when TLS connection was established and
 * it returns 0, when TLS was not established.
 */
int vs_TLS_handshake(struct vContext *C)
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
int vs_TLS_teardown(struct vContext *C)
{
	struct VStreamConn *stream_conn = CTX_current_stream_conn(C);
	int ret;

	v_print_log(VRS_PRINT_DEBUG_MSG, "Try to shut down SSL connection.\n");

	ret = SSL_shutdown(stream_conn->io_ctx.ssl);
	if(ret != 1) {
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

	return 1;
}
#endif

/**
 * \brief This function do TLS teardown, when it is reuired and it
 * closes socket.
 */
void vs_CLOSING(struct vContext *C)
{
	struct VStreamConn *stream_conn = CTX_current_stream_conn(C);

#ifdef WITH_OPENSSL
	if(stream_conn->io_ctx.ssl != NULL) vs_TLS_teardown(C);
#endif

	close(stream_conn->io_ctx.sockfd);
}


/**
 * \brief This function is never ending loop of server state STREAM_OPEN.
 * This loop is used, when Verse server uses TCP for data exchange (not
 * UDP nor WebSocket)
 */
int vs_STREAM_OPEN_tcp_loop(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct timeval tv;
	fd_set set;
	int flag, ret, error;

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

			/* Try to receive data through SSL connection */
			if( v_tcp_read(io_ctx, &error) <= 0 ) {
				goto end;
			}

			if(v_STREAM_handle_messages(C) == 0) {
				goto end;
			}

			/* When some payload data were received, then poke data thread */
			sem_post(vs_ctx->data.sem);
		}

		if( (ret = v_STREAM_pack_message(C)) == 0 ) {
			goto end;
		}

		/* Send command to the client */
		if(ret == 1) {
			if( v_tcp_write(io_ctx, &error) <= 0) {
				goto end;
			}
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
int vs_NEGOTIATE_newhost_loop(struct vContext *C)
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
 * This function can create new thread for datagram connection
 */
int vs_NEGOTIATE_cookie_ded_loop(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VMessage *r_message = CTX_r_message(C);
	struct VMessage *s_message = CTX_s_message(C);
	int i, j, ret;
	unsigned short buffer_pos = 0;
	int host_url_proposed = 0,
			host_cookie_proposed = 0,
			peer_cookie_confirmed = 0,
			ded_confirmed = 0,
			client_name_proposed = 0,
			client_version_proposed = 0;
	struct timeval tv;
	struct VURL url;

	/* Reset content of received message */
	memset(r_message, 0, sizeof(struct VMessage));

	/* Unpack Verse message header */
	buffer_pos += v_unpack_message_header(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	/* Unpack all system commands */
	buffer_pos += v_unpack_message_system_commands(&io_ctx->buf[buffer_pos], (io_ctx->buf_size - buffer_pos), r_message);

	v_print_receive_message(C);

	/* Process all received system commands */
	for(i=0;
			i<MAX_SYSTEM_COMMAND_COUNT &&
			r_message->sys_cmd[i].cmd.id!=CMD_RESERVED_ID;
			i++)
	{
		switch(r_message->sys_cmd[i].cmd.id) {
		case CMD_CHANGE_R_ID:
			/* Client has to propose url in this state */
			if(r_message->sys_cmd[i].negotiate_cmd.feature == FTR_HOST_URL) {
				if(r_message->sys_cmd[i].negotiate_cmd.count > 0) {
					if(vsession->host_url!=NULL) {
						free(vsession->host_url);
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
				ret = 0;
				goto end;
			}

			/* Wait for datagram thread to be in LISTEN state */
			while(vsession->dgram_conn->host_state != UDP_SERVER_STATE_LISTEN) {
				/* Sleep 1 milisecond */
				usleep(1000);
			}
		} else if(vsession->flags & VRS_TP_TCP) {
			strncpy(trans_proto, "tcp", 3);
			trans_proto[3] = '\0';
		} else if(vsession->flags & VRS_TP_WEBSOCKET) {
			strncpy(trans_proto, "wss", 3);
			trans_proto[3] = '\0';
		}

#if (defined WITH_OPENSSL) && OPENSSL_VERSION_NUMBER>=0x10000000
		if(url.security_protocol==VRS_SEC_DATA_NONE ||
				!(vs_ctx->security_protocol & VRS_SEC_DATA_TLS))
		{
			strncpy(sec_proto, "none", 4);
			sec_proto[4] = '\0';
		} else if(url.security_protocol==VRS_SEC_DATA_TLS) {
			if(vsession->flags & VRS_TP_UDP) {
				strncpy(sec_proto, "dtls", 4);
				sec_proto[4] = '\0';
			} else if((vsession->flags & VRS_TP_TCP) ||
					(vsession->flags & VRS_TP_WEBSOCKET))
			{
				strncpy(sec_proto, "tls", 3);
				sec_proto[3] = '\0';
			}
		} else {
			strncpy(sec_proto, "none", 4);
			sec_proto[4] = '\0';
		}
#else
		strncpy(sec_proto, "none", 4);
		sec_proto[4] = '\0';
#endif

		/* Free proposed and now obsolete URL */
		if(vsession->host_url != NULL) {
			free(vsession->host_url);
			vsession->host_url = NULL;
		}

		/* Set right host URL */
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

		/* Pack all system commands to the buffer */
		buffer_pos += v_pack_stream_system_commands(s_message, &io_ctx->buf[buffer_pos]);

		/* Update length of message in the header (data in buffer) */
		s_message->header.version = VRS_VERSION;
		s_message->header.len = io_ctx->buf_size = buffer_pos;

		/* Pack header to the beginning of the buffer */
		v_pack_message_header(s_message, io_ctx->buf);

		v_print_send_message(C);

		ret = 1;
	} else {
		ret = 0;
	}

end:
	v_clear_url(&url);

	return ret;
}


/**
 *
 */
int vs_RESPOND_userauth_loop(struct vContext *C)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VMessage *r_message = CTX_r_message(C);
	struct VMessage *s_message = CTX_s_message(C);
	int i, cmd_rank = 0;
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
					avatar_id = vs_create_avatar_node(vs_ctx, vsession, user_id);
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

					return 1;
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

					return 1;
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
int vs_RESPOND_methods_loop(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VSession *vsession = CTX_current_session(C);
	struct VMessage *r_message = CTX_r_message(C);
	struct VMessage *s_message = CTX_s_message(C);
	int i, auth_req = 0,
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
				v_print_log(VRS_PRINT_WARNING,
						"This auth method id: %d is not supported in this state\n",
						r_message->sys_cmd[i].ua_req.method_type);
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

		return 1;
	}

	return 0;
}

/**
 * \brief This function handles messages received during verse handshake
 * and it can create new thread for datagram connection, when datagram
 * connection was negotiated.
 */
int vs_handle_handshake(struct vContext *C)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VStreamConn *stream_conn = CTX_current_stream_conn(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	int ret;

	/* Make sure, that buffer contains at least Verse message
	 * header. If this condition is not reached, then somebody tries
	 * to do some very bad things! .. Close this connection. */
	if(io_ctx->buf_size < VERSE_MESSAGE_HEADER_SIZE) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "received buffer too small %d\n",
				io_ctx->buf_size);
		return -1;
	}

	switch(stream_conn->host_state) {
	case TCP_SERVER_STATE_RESPOND_METHODS:
		ret = vs_RESPOND_methods_loop(C);
		if(ret == 1) {
			stream_conn->host_state = TCP_SERVER_STATE_RESPOND_USRAUTH;
			if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
				printf("%c[%d;%dm", 27, 1, 31);
				v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: RESPOND_userauth\n");
				printf("%c[%dm", 27, 0);
			}
		} else {
			return -1;
		}
		break;
	case TCP_SERVER_STATE_RESPOND_USRAUTH:
		ret = vs_RESPOND_userauth_loop(C);
		if(ret == 1) {
			stream_conn->host_state = TCP_SERVER_STATE_NEGOTIATE_COOKIE_DED;

			if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
				printf("%c[%d;%dm", 27, 1, 31);
				v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: NEGOTIATE_cookie_ded\n");
				printf("%c[%dm", 27, 0);
			}
		} else {
			vsession->usr_auth_att++;
			if(vsession->usr_auth_att >= MAX_USER_AUTH_ATTEMPTS) {
				return -1;
			}
		}
		break;
	case TCP_SERVER_STATE_NEGOTIATE_COOKIE_DED:
		ret = vs_NEGOTIATE_cookie_ded_loop(C);
		if(ret == 1) {
			stream_conn->host_state = TCP_SERVER_STATE_NEGOTIATE_NEWHOST;

			if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
				printf("%c[%d;%dm", 27, 1, 31);
				v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: NEGOTIATE_newhost\n");
				printf("%c[%dm", 27, 0);
			}
		} else {
			return -1;
		}
		break;
	case TCP_SERVER_STATE_NEGOTIATE_NEWHOST:
		/* Wait VERSE_TIMEOT seconds to confirming proposed URL */
		ret = vs_NEGOTIATE_newhost_loop(C);
		if(ret == 1) {
			if(vsession->flags & VRS_TP_UDP) {
				/* When URL was confirmed, then go to the end state */
				return -1;
			} else if(vsession->flags & VRS_TP_TCP) {
				stream_conn->host_state = TCP_SERVER_STATE_STREAM_OPEN;

				if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
					printf("%c[%d;%dm", 27, 1, 31);
					v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: STREAM_OPEN\n");
					printf("%c[%dm", 27, 0);
				}

				vs_STREAM_OPEN_tcp_loop(C);
				return -1;
			} else if(vsession->flags & VRS_TP_WEBSOCKET) {
				stream_conn->host_state = TCP_SERVER_STATE_STREAM_OPEN;

				if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
					printf("%c[%d;%dm", 27, 1, 31);
					v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: STREAM_OPEN\n");
					printf("%c[%dm", 27, 0);
				}
			}
		} else {
			/* When thread was not confirmed, then try to cancel
			 * UDP thread*/
			if(pthread_cancel(vsession->udp_thread) != 0) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "UDP thread was not canceled\n");
			}
			return -1;
		}
		break;

	}

	return 1;
}
