/*
 * 
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2011, Jiri Hnidek
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

#ifdef WITH_OPENSSL
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
#define CA_CERT_FILE "/etc/pki/tls/certs/ca-bundle.crt"
#ifdef __APPLE__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "verse_types.h"

#include "vc_main.h"
#include "vc_tcp_connect.h"
#include "vc_udp_connect.h"

#include "v_common.h"
#include "v_fake_commands.h"
#include "v_pack.h"
#include "v_unpack.h"
#include "v_session.h"
#include "v_connection.h"
#include "v_stream.h"

/**
 * \brief This function tries to get username and password from
 * user of client application.
 * \param[in]	*vsession			The session with Verse server
 * \param[out]	*user_credentials	The structure storing authentication data
 */
static int vc_get_username_passwd(struct VSession *vsession,
		struct VUserCredentials *user_credentials)
{
	struct Generic_Cmd *cmd = NULL;
	uint16 count, len;
	int8 share;
	struct User_Authenticate_Cmd *ua_cmd;

	/* Wait for the username and password from user input in client application */
	while(1) {
		count = 0;
		share = 0;
		len = 0;
		cmd = v_out_queue_pop(vsession->out_queue, VRS_DEFAULT_PRIORITY, &count, &share, &len);
		if(cmd != NULL) {
			switch(cmd->id) {
			case FAKE_CMD_USER_AUTHENTICATE:
				ua_cmd = (struct User_Authenticate_Cmd*)cmd;
				v_fake_cmd_print(VRS_PRINT_DEBUG_MSG, cmd);
				if(ua_cmd->username != NULL) {
					if(user_credentials->username != NULL) {
						free(user_credentials->username);
					}
					user_credentials->username = strdup(ua_cmd->username);
				} else {
					v_cmd_destroy(&cmd);
					return 0;
				}
				if(ua_cmd->data != NULL) {
					if(user_credentials->data != NULL) {
						free(user_credentials->data);
					}
					user_credentials->data = strdup(ua_cmd->data);
				}
				v_cmd_destroy(&cmd);
				return 1;
			case FAKE_CMD_CONNECT_TERMINATE:
				v_fake_cmd_print(VRS_PRINT_DEBUG_MSG, cmd);
				v_cmd_destroy(&cmd);
				return 0;
			default:
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"This command %d is not accepted during NEGOTIATE state\n",
						cmd->id);
				v_cmd_destroy(&cmd);
				break;
			}
		}
		/* 1000000 micro seconds is one second
		 * and vsession->fps_host is current FPS of this client */
		usleep(1000000/vsession->fps_host);
	}

	return 0;
}

/**
 * \brief this function is never ending loop of client state STREAM_OPEN
 */
static int vc_STREAM_OPEN_loop(struct vContext *C)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct Connect_Accept_Cmd *conn_accept;
	struct VSession *vsession = CTX_current_session(C);
	struct timeval tv;
	fd_set set;
	int flag, ret, error;

	/* Set socket non-blocking */
	flag = fcntl(io_ctx->sockfd, F_GETFL, 0);
	if( (fcntl(io_ctx->sockfd, F_SETFL, flag | O_NONBLOCK)) == -1) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
		return -1;
	}

	/* Put connect accept command to queue -> call callback function */
	conn_accept = v_connect_accept_create(vsession->avatar_id, vsession->user_id);

	v_in_queue_push(vsession->in_queue, (struct Generic_Cmd*)conn_accept);

	/* "Never ending" loop */
	while(!(vsession->stream_conn->host_state == TCP_CLIENT_STATE_CLOSING ||
			vsession->stream_conn->host_state == TCP_CLIENT_STATE_CLOSED))
	{
		FD_ZERO(&set);
		FD_SET(io_ctx->sockfd, &set);

		/* Use negotiated FPS */
		tv.tv_sec = 0;
		/* 1000000 micro seconds is one second
		 * and vsession->fps_host is current FPS of this client */
		tv.tv_usec = 1000000/vsession->fps_host;

		/* Wait for received data */
		if( (ret = select(io_ctx->sockfd+1, &set, NULL, NULL, &tv)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n",
					__FILE__, __func__,  __LINE__, strerror(errno));
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
		}

		if( (ret = v_STREAM_pack_message(C)) == 0) {
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

	return 1;
}

/**
 * \brief This function negotiate new data connection with server
 *
 * This new negotiated connection could be on the same server or it could be on different host.
 */
int vc_NEGOTIATE_newhost(struct vContext *C, char *host_url)
{
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VMessage *s_message = CTX_s_message(C);
	int ret, error, buffer_pos=0, cmd_rank=0;

	buffer_pos = VERSE_MESSAGE_HEADER_SIZE;

	if(host_url != NULL) {
		v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CONFIRM_L_ID, FTR_HOST_URL, host_url, NULL);
	} else {
		v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CONFIRM_L_ID, FTR_HOST_URL, NULL);
	}

	/* Pack all system commands to the buffer */
	buffer_pos += v_pack_stream_system_commands(s_message, &io_ctx->buf[buffer_pos]);

	/* Update length of message in the header (data in buffer) */
	s_message->header.version = VRS_VERSION;
	s_message->header.len = io_ctx->buf_size = buffer_pos;

	/* Pack header to the beginning of the buffer */
	v_pack_message_header(s_message, io_ctx->buf);

	v_print_send_message(C);

	/* Send command to the server */
	if( (ret = v_tcp_write(io_ctx, &error)) <= 0) {
		return 0;
	} else {
		return 1;
	}

	return 0;
}


/**
 *
 */
static int vc_NEGOTIATE_token_dtd_loop(struct vContext *C)
{
	struct VSession *vsession = CTX_current_session(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VMessage *r_message = CTX_r_message(C);
	struct VMessage *s_message = CTX_s_message(C);
	struct timeval tv;
	fd_set set;
	int i, ret, error, buffer_pos=0, cmd_rank = 0;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client TCP state: NEGOTIATE_token_dtd\n");
		printf("%c[%dm", 27, 0);
	}

	buffer_pos = VERSE_MESSAGE_HEADER_SIZE;

	/* Get string of server IP */
	if(io_ctx->peer_addr.ip_ver == IPV4) {
		char str_addr[INET_ADDRSTRLEN];

		inet_ntop(AF_INET, &(io_ctx->peer_addr.addr.ipv4.sin_addr), str_addr, sizeof(str_addr));

		/* Assembly string of IP to the URL */
		if(vsession->host_url!=NULL) {
			free(vsession->host_url);
		}
		vsession->host_url = (char*)malloc((20+INET_ADDRSTRLEN)*sizeof(char));

		/* Choose scheme according client preference */
		if(vsession->flags & VRS_SEC_DATA_NONE) {
			if(vsession->flags & VRS_TP_UDP) {
				sprintf(vsession->host_url, "verse-udp-none://%s:*", str_addr);
			} else if(vsession->flags & VRS_TP_TCP) {
				sprintf(vsession->host_url, "verse-tcp-none://%s:*", str_addr);
			} else {
				/* Default transport protocol is UDP */
				sprintf(vsession->host_url, "verse-udp-none://%s:*", str_addr);
			}
		} else if(vsession->flags & VRS_SEC_DATA_TLS) {
			if(vsession->flags & VRS_TP_UDP) {
				sprintf(vsession->host_url, "verse-udp-dtls://%s:*", str_addr);
			} else if(vsession->flags & VRS_TP_TCP) {
				sprintf(vsession->host_url, "verse-tcp-tls://%s:*", str_addr);
			} else {
				/* Default transport protocol is UDP */
				sprintf(vsession->host_url, "verse-udp-dtls://%s:*", str_addr);
			}
		} else {
			/* Default security policy is to use DTLS and default transport protocol
			 * is UDP */
            if(vsession->flags & VRS_TP_TCP) {
                sprintf(vsession->host_url, "verse-tcp-tls://%s:*", str_addr);
            } else if(vsession->flags & VRS_TP_UDP) {
                sprintf(vsession->host_url, "verse-udp-dtls://%s:*", str_addr);
            } else {
                sprintf(vsession->host_url, "verse-udp-dtls://%s:*", str_addr);
            }
		}
	} else if (io_ctx->peer_addr.ip_ver == IPV6) {
		char str_addr[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &(io_ctx->peer_addr.addr.ipv6.sin6_addr), str_addr, sizeof(str_addr));

		if(vsession->host_url != NULL) {
			free(vsession->host_url);
		}
		vsession->host_url = (char*)malloc((22+INET6_ADDRSTRLEN)*sizeof(char));

		/* Choose scheme according client preference */
		if(vsession->flags & VRS_SEC_DATA_NONE) {
			if(vsession->flags & VRS_TP_UDP) {
				sprintf(vsession->host_url, "verse-udp-none://[%s]:*", str_addr);
			} else if(vsession->flags & VRS_TP_TCP) {
				sprintf(vsession->host_url, "verse-tcp-none://[%s]:*", str_addr);
			} else {
				/* Default transport protocol is UDP */
				sprintf(vsession->host_url, "verse-udp-none://[%s]:*", str_addr);
			}
		} else if(vsession->flags & VRS_SEC_DATA_TLS) {
			if(vsession->flags & VRS_TP_UDP) {
				sprintf(vsession->host_url, "verse-udp-dtls://[%s]:*", str_addr);
			} else if(vsession->flags & VRS_TP_TCP) {
				sprintf(vsession->host_url, "verse-tcp-tls://[%s]:*", str_addr);
			} else {
				/* Default transport protocol is UDP */
				sprintf(vsession->host_url, "verse-udp-dtls://[%s]:*", str_addr);
			}
		} else {
			/* Default security policy is to use DTLS and default transport protocol is UDP*/
            if(vsession->flags & VRS_TP_TCP) {
                sprintf(vsession->host_url, "verse-tcp-tls://[%s]:*", str_addr);
            } else if(vsession->flags & VRS_TP_UDP) {
                sprintf(vsession->host_url, "verse-udp-dtls://[%s]:*", str_addr);
            } else {
                sprintf(vsession->host_url, "verse-udp-dtls://[%s]:*", str_addr);
            }
		}
	}

	/* Set up negotiate command of host URL. This URL is fake URL and it
	 * is used for sending IP of server, that client used for connecting
	 * server and preferred transport protocol (UDP) and encryption protocol
	 * (DTLS) */
	v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CHANGE_R_ID, FTR_HOST_URL, vsession->host_url, NULL);

	/* Set time for token */
	gettimeofday(&tv, NULL);
	vsession->host_token.tv.tv_sec = tv.tv_sec;
	vsession->host_token.tv.tv_usec = tv.tv_usec;

	/* Set up negotiate command of host Token */
	v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CONFIRM_R_ID, FTR_TOKEN, vsession->host_token.str, NULL);

	/* Generate random string */
	vsession->peer_token.str = (char*)malloc((TOKEN_SIZE+1)*sizeof(char));
	if (vsession->peer_token.str == NULL) {
		return 0;
	}
	
	for(i=0; i<TOKEN_SIZE; i++) {
		/* Generate only printable characters (debug prints) */
		vsession->peer_token.str[i] = 32 + (char)((float)rand()*94.0/RAND_MAX);
	}
	vsession->peer_token.str[TOKEN_SIZE] = '\0';
	/* Set up negotiate command of peer Token */
	v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CHANGE_R_ID, FTR_TOKEN, vsession->peer_token.str, NULL);

	/* Send confirmation of proposed DED */
	v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CONFIRM_L_ID, FTR_DED, vsession->ded.str, NULL);

	/* Pack all system commands to the buffer */
	buffer_pos += v_pack_stream_system_commands(s_message, &io_ctx->buf[buffer_pos]);

	/* Update length of message in the header (data in buffer) */
	s_message->header.version = VRS_VERSION;
	s_message->header.len = io_ctx->buf_size = buffer_pos;

	/* Pack header to the beginning of the buffer */
	v_pack_message_header(s_message, io_ctx->buf);

	v_print_send_message(C);

	/* Send command to the server */
	if( (ret = v_tcp_write(io_ctx, &error)) <= 0) {
		return 0;
	}

	/* Wait VERSE_TIMEOUT seconds for the respond from the server */
	FD_ZERO(&set);
	FD_SET(io_ctx->sockfd, &set);

	tv.tv_sec = VRS_TIMEOUT;
	tv.tv_usec = 0;

	/* Wait for the event on the socket */
	if( (ret = select(io_ctx->sockfd+1, &set, NULL, NULL, &tv)) == -1) {
		return 0;
	/* Was event on the TCP socket of this session */
	} else if(ret>0 && FD_ISSET(io_ctx->sockfd, &set)) {
		int confirmed_token=0, proposed_url=0;
		buffer_pos = 0;

		/* Try to receive data through SSL connection */
		if( v_tcp_read(io_ctx, &error) <= 0 ) {
			return 0;
		}

		/* Make sure, that buffer contains at verse message header.
		 * If this condition is not reached, then somebody tries
		 * to do some bad things! .. Close this connection. */
		if(io_ctx->buf_size < VERSE_MESSAGE_HEADER_SIZE) {
			return 0;
		}

		/* Unpack Verse message header */
		buffer_pos += v_unpack_message_header(&io_ctx->buf[buffer_pos],
				(io_ctx->buf_size - buffer_pos),
				r_message);

		buffer_pos += v_unpack_message_system_commands(&io_ctx->buf[buffer_pos],
				(io_ctx->buf_size - buffer_pos),
				r_message);

		/* Print received message */
		v_print_receive_message(C);


		for(i=0; i<MAX_SYSTEM_COMMAND_COUNT && r_message->sys_cmd[i].cmd.id != CMD_RESERVED_ID; i++) {
			switch(r_message->sys_cmd[i].cmd.id) {
			case CMD_CONFIRM_R_ID:
				if(r_message->sys_cmd[i].negotiate_cmd.feature==FTR_HOST_URL) {
					if(r_message->sys_cmd[i].negotiate_cmd.count==0) {
						/* Correct behavior */
					}
				} else if(r_message->sys_cmd[i].negotiate_cmd.feature==FTR_TOKEN) {
					if(r_message->sys_cmd[i].negotiate_cmd.count>0) {
						if(strcmp(vsession->peer_token.str,
								  (char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str) == 0)
						{
							gettimeofday(&tv, NULL);
							vsession->peer_token.tv.tv_sec = tv.tv_sec;
							vsession->peer_token.tv.tv_usec = tv.tv_usec;
							confirmed_token = 1;
						} else {
							v_print_log(VRS_PRINT_DEBUG_MSG, " Server confirmed wrong token: %s != %s\n",
									vsession->peer_token.str, r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str);
						}
					}
				}
				break;
			case CMD_CHANGE_L_ID:
				if(r_message->sys_cmd[i].negotiate_cmd.feature==FTR_HOST_URL) {
					if(r_message->sys_cmd[i].negotiate_cmd.count>0) {
						if(vsession->host_url!=NULL) {
							free(vsession->host_url);
						}
						vsession->host_url = strdup((char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str);
						proposed_url = 1;
					}
				}
				break;
			}
		}

		if(confirmed_token==1 && proposed_url==1)
			return 1;
		else
			return 0;
	}

	return 0;
}

/**
 *
 */
static int vc_USRAUTH_data_loop(struct vContext *C,
		struct VUserCredentials *user_credentials)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VStreamConn *stream_conn = CTX_current_stream_conn(C);
	struct VMessage *r_message = CTX_r_message(C);
	struct VMessage *s_message = CTX_s_message(C);
	struct timeval tv;
	fd_set set;
	int i, ret, error, buffer_pos=0, auth_succ=0;
	struct User_Authenticate_Cmd *ua_cmd;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client TCP state: USRAUTH_data\n");
		printf("%c[%dm", 27, 0);
	}

	/* Create user auth command */
	ua_cmd = v_user_auth_create(user_credentials->username,
			user_credentials->auth_meth_count,
			user_credentials->methods,
			user_credentials->data);

	/* Put request for password to the queue */
	v_in_queue_push(vsession->in_queue, (struct Generic_Cmd*)ua_cmd);

	/* Get authentication data (password) from the
	 * client application */
	if( vc_get_username_passwd(vsession, user_credentials) != 1 ) {
		return 0;
	}

	buffer_pos = VERSE_MESSAGE_HEADER_SIZE;

	/* Pack user auth command */
	s_message->sys_cmd[0].ua_req.id = CMD_USER_AUTH_REQUEST;
	strncpy(s_message->sys_cmd[0].ua_req.username, vsession->username, VRS_MAX_USERNAME_LENGTH);
	s_message->sys_cmd[0].ua_req.method_type = user_credentials->methods[0];
	strncpy(s_message->sys_cmd[0].ua_req.data, user_credentials->data, VRS_MAX_DATA_LENGTH);
	s_message->sys_cmd[1].cmd.id = CMD_RESERVED_ID;

	/* Pack all system commands to the buffer */
	buffer_pos += v_pack_stream_system_commands(s_message,
			&stream_conn->io_ctx.buf[buffer_pos]);

	/* Update length of message in the header (data in buffer) */
	s_message->header.version = VRS_VERSION;
	s_message->header.len = stream_conn->io_ctx.buf_size = buffer_pos;

	/* Pack header to the beginning of the buffer */
	v_pack_message_header(s_message, stream_conn->io_ctx.buf);

	/* Print message header and all commands to be sent */
	v_print_send_message(C);

	/* Send command to the server */
	if( (ret = v_tcp_write(&stream_conn->io_ctx, &error)) <= 0) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "v_SSL_write() failed.\n");
		return 0;
	}

	/* Wait VERSE_TIMEOUT seconds for the respond from the server */
	FD_ZERO(&set);
	FD_SET(stream_conn->io_ctx.sockfd, &set);

	tv.tv_sec = VRS_TIMEOUT;
	tv.tv_usec = 0;

	/* Wait for the event on the socket */
	if( (ret = select(stream_conn->io_ctx.sockfd+1, &set, NULL, NULL, &tv)) == -1) {
		v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n", __FILE__, __func__,  __LINE__, strerror(errno));
		return 0;
	/* Was event on the TCP socket of this session */
	} else if(ret>0 && FD_ISSET(stream_conn->io_ctx.sockfd, &set)) {
		buffer_pos = 0;

		/* Try to receive data through SSL connection */
		if( v_tcp_read(&stream_conn->io_ctx, &error) <= 0 ) {
			/* TODO: try to read more data */
			return 0;
		}

		/* Make sure, that buffer contains at verse message header.
		 * If this condition is not reached, then somebody tries
		 * to do some bad things! .. Close this connection. */
		if(stream_conn->io_ctx.buf_size < VERSE_MESSAGE_HEADER_SIZE) {
			v_print_log(VRS_PRINT_ERROR, "Small received buffer: %d\n", stream_conn->io_ctx.buf_size);
			/* TODO: try to read more data */
			return 0;
		}

		/* Unpack Verse message header */
		buffer_pos += v_unpack_message_header(&stream_conn->io_ctx.buf[buffer_pos],
				(stream_conn->io_ctx.buf_size - buffer_pos),
				r_message);

		/* Unpack all system commands */
		buffer_pos += v_unpack_message_system_commands(&stream_conn->io_ctx.buf[buffer_pos],
				stream_conn->io_ctx.buf_size - buffer_pos,
				r_message);

		/* Print received message */
		v_print_receive_message(C);

		/* Process all system commands in this state */
		for(i=0; i<MAX_SYSTEM_COMMAND_COUNT && r_message->sys_cmd[i].cmd.id != CMD_RESERVED_ID; i++) {
			switch(r_message->sys_cmd[i].cmd.id) {
			case CMD_USER_AUTH_FAILURE:
				v_print_log(VRS_PRINT_DEBUG_MSG, "User authentication of user: %s failed.\n", vsession->username);
				return 0;
			case CMD_USER_AUTH_SUCCESS:
				vsession->avatar_id = r_message->sys_cmd[i].ua_succ.avatar_id;
				vsession->user_id = r_message->sys_cmd[i].ua_succ.user_id;
				v_print_log(VRS_PRINT_DEBUG_MSG, "avatar_id: %d, user_id: %d\n",
						vsession->avatar_id, vsession->user_id);
				auth_succ = 1;
				break;
			case CMD_CHANGE_R_ID:
				if(r_message->sys_cmd[i].negotiate_cmd.feature == FTR_TOKEN) {
					if(vsession->host_token.str!=NULL) {
						free(vsession->host_token.str);
						vsession->host_token.str = NULL;
					}
					vsession->host_token.str = strdup((char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str);
				}
				break;
			case CMD_CHANGE_L_ID:
				if(r_message->sys_cmd[i].negotiate_cmd.feature == FTR_DED) {
					if(vsession->ded.str!=NULL) {
						free(vsession->ded.str);
						vsession->ded.str = NULL;
					}
					vsession->ded.str = strdup((char*)r_message->sys_cmd[i].negotiate_cmd.value[0].string8.str);
				}
				break;
			}
		}

		if(auth_succ==1)
			return CMD_USER_AUTH_SUCCESS;
		else
			return 0;
	} else {
		return 0;
	}

	return 0;
}

/**
 *
 */
static int vc_USRAUTH_none_loop(struct vContext *C,
		struct VUserCredentials *user_credentials)
{
	struct VSession *vsession = CTX_current_session(C);
	struct VC_CTX *vc_ctx = CTX_client_ctx(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VMessage *r_message = CTX_r_message(C);
	struct VMessage *s_message = CTX_s_message(C);
	struct timeval tv;
	fd_set set;
	int ret, i, error, buffer_pos=0, cmd_rank=0;
	struct User_Authenticate_Cmd *ua_cmd;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client TCP state: USRAUTH_none\n");
		printf("%c[%dm", 27, 0);
	}

	/* Create user auth command */
	ua_cmd = v_user_auth_create(NULL, 0, NULL, NULL);

	/* Put request for username to the queue */
	v_in_queue_push(vsession->in_queue, (struct Generic_Cmd*)ua_cmd);

	if( vc_get_username_passwd(vsession, user_credentials) != 1) {
		return 0;
	}

	/* Save username to the Verse session */
	vsession->username = strdup(user_credentials->username);

	/* Send USER_AUTH_REQUEST with METHOD_NONE to get list of supported method
	 * types */
	s_message->sys_cmd[cmd_rank].ua_req.id = CMD_USER_AUTH_REQUEST;
	strncpy(s_message->sys_cmd[0].ua_req.username, vsession->username, VRS_MAX_USERNAME_LENGTH);
	s_message->sys_cmd[cmd_rank].ua_req.method_type = VRS_UA_METHOD_NONE;
	cmd_rank++;

	/* Set up negotiate command of client name and version */
	if(vc_ctx->client_name != NULL) {
		v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CHANGE_L_ID, FTR_CLIENT_NAME, vc_ctx->client_name, NULL);
		if(vc_ctx->client_version != NULL) {
			v_add_negotiate_cmd(s_message->sys_cmd, cmd_rank++, CMD_CHANGE_L_ID, FTR_CLIENT_VERSION, vc_ctx->client_version, NULL);
		}
	}

	buffer_pos = VERSE_MESSAGE_HEADER_SIZE;

	/* Pack all system commands to the buffer */
	buffer_pos += v_pack_stream_system_commands(s_message, &io_ctx->buf[buffer_pos]);

	/* Set length of the message in the header */
	s_message->header.version = VRS_VERSION;
	s_message->header.len = io_ctx->buf_size = buffer_pos;

	/* Pack header to the beginning of the buffer */
	v_pack_message_header(s_message, io_ctx->buf);

	/* Print header of message and all system commands to
	 * be send to the peer */
	v_print_send_message(C);

	/* Send message to the server */
	if( (ret = v_tcp_write(io_ctx, &error)) <= 0) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "v_SSL_write() failed.\n");
		return 0;
	}

	/* Wait VERSE_TIMEOUT seconds the for respond from the server */
	FD_ZERO(&set);
	FD_SET(io_ctx->sockfd, &set);

	tv.tv_sec = VRS_TIMEOUT;
	tv.tv_usec = 0;

	/* Wait for the event on the socket */
	if( (ret = select(io_ctx->sockfd + 1, &set, NULL, NULL, &tv)) == -1) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "select(): %s\n", strerror(errno));
		return 0;
	/* Was event on the TCP socket of this session */
	} else if(ret > 0 && FD_ISSET(io_ctx->sockfd, &set)) {
		buffer_pos = 0;

		/* Try to receive data through SSL connection */
		if( v_tcp_read(io_ctx, &error) <= 0 ) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "v_SSL_read() failed.\n");
			return 0;
		}

		/* Make sure, that buffer contains at least command ID and length
		 * of the ID. If this condition is not reached, then somebody tries
		 * to do some very bad things! .. Close this connection. */
		if(io_ctx->buf_size < VERSE_MESSAGE_HEADER_SIZE) {
			/* TODO: try to read more data from socket */
			return 0;
		}

		/* Unpack Verse message header */
		buffer_pos += v_unpack_message_header(&io_ctx->buf[buffer_pos],
				(io_ctx->buf_size - buffer_pos),
				r_message);

		/* Unpack all system commands */
		buffer_pos += v_unpack_message_system_commands(&io_ctx->buf[buffer_pos],
				(io_ctx->buf_size - buffer_pos),
				r_message);

		/* Print received message */
		v_print_receive_message(C);

		/* Process all system commands, that are supported in this state */
		for(i = 0; i < MAX_SYSTEM_COMMAND_COUNT && r_message->sys_cmd[i].cmd.id != CMD_RESERVED_ID; i++) {
			switch(r_message->sys_cmd[i].cmd.id) {
			case CMD_USER_AUTH_FAILURE:
				if(r_message->sys_cmd[i].ua_fail.count == 0) {
					v_print_log(VRS_PRINT_DEBUG_MSG, "User authentication of user: %s failed.\n", vsession->username);
					return 0;
				} else {
					/* Go through the list of supported authentication methods and
					 * copy them to the user credentials */
					int j;
					user_credentials->auth_meth_count = r_message->sys_cmd[i].ua_fail.count;
					user_credentials->methods = calloc(user_credentials->auth_meth_count, sizeof(uint8));
					for(j=0; j<r_message->sys_cmd[i].ua_fail.count; j++) {
						user_credentials->methods[j] = r_message->sys_cmd[i].ua_fail.method[j];
					}
					return CMD_USER_AUTH_FAILURE;
				}
				break;
			case CMD_USER_AUTH_SUCCESS:
				/* Strange, server should not authenticate user without password */
				/* Save this to the session */
				vsession->user_id = r_message->sys_cmd[i].ua_succ.user_id;
				vsession->avatar_id = r_message->sys_cmd[i].ua_succ.avatar_id;
				return CMD_USER_AUTH_SUCCESS;
				break;
			}
		}
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Connection timed out\n");
		return 0;
	}

	return 0;
}

/**
 * \brief Destroy Verse client TCP connection and SSL context
 * \param[in]	*stream_conn	The pointer at TCP connection
 */
void vc_destroy_stream_conn(struct VStreamConn *stream_conn)
{
#ifdef WITH_OPENSSL
	if(stream_conn->io_ctx.ssl != NULL) {
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
		stream_conn->io_ctx.ssl = NULL;
		stream_conn->io_ctx.bio = NULL;
	}
#endif

	if(stream_conn->io_ctx.sockfd != -1) {
		close(stream_conn->io_ctx.sockfd);
		stream_conn->io_ctx.sockfd = -1;
	}

}

#ifdef WITH_OPENSSL

/**
 * \brief Verify if hostname specified in certificate is the same as required
 * hostname
 *
 * \param[in]	*cert		Pointer at certificate of server.
 * \param[in]	*hostname	The hostname of server we are connecting to.
 *
 * \return This function return 1, when hostnames are same. Otherwise it
 * returns 0.
 */
int verify_cert_hostname(X509 *cert, const char *hostname)
{
	int extcount, i, j, ret = 0;
	char name[256];
	X509_NAME *subj;
	const char *extstr;
	CONF_VALUE *nval;
	const unsigned char *data;
	X509_EXTENSION *ext;
	const X509V3_EXT_METHOD *meth;
	STACK_OF(CONF_VALUE) *val;

	if( (extcount = X509_get_ext_count(cert)) > 0) {
		for(i = 0; ret != 1 && i < extcount;  i++) {
			ext = X509_get_ext(cert, i);
			extstr = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));
			if(strcasecmp(extstr, "subjectAltName") == 0) {
				if( (meth = X509V3_EXT_get(ext)) == NULL) {
					break;
				}
				data = ext->value->data;

				val = meth->i2v(meth, meth->d2i(0, &data, ext->value->length), 0);
				for(j = 0;  j < sk_CONF_VALUE_num(val);  j++) {
					nval = sk_CONF_VALUE_value(val, j);
					if(strcasecmp(nval->name, "DNS") == 0) {
						if(strcasecmp(nval->value, hostname) == 0) {
							v_print_log(VRS_PRINT_DEBUG_MSG,
									"subjectAltName: %s == %s\n",
									nval->value, hostname);
							ret = 1;
							break;
						} else {
							v_print_log(VRS_PRINT_DEBUG_MSG,
									"subjectAltName: %s != %s\n",
									nval->value, hostname);
						}
					}
				}
			}
		}
	}

	if(ret != 1 && (subj = X509_get_subject_name(cert)) != NULL &&
			X509_NAME_get_text_by_NID(subj, NID_commonName, name, sizeof(name)) > 0) {
		name[sizeof(name) - 1] = '\0';
		if(strcasecmp(name, hostname) == 0 ) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "%s == %s\n",
					name, hostname);
			ret = 1;
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "%s != %s\n",
					name, hostname);
		}
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "No hostname in certificate\n");
	}

	return ret;
}
#endif

/**
 * \brief Create new TCP connection to the server
 *
 * \param[in]	*ctx		The pointer at Verse client context
 * \param[in]	*node		The string with address of Verse server
 * \param[in]	*service	The string with port number or name of service
 * 							defined in /etc/serices
 *
 * \return	This function returns pointer at new TCP connection
 */
struct VStreamConn *vc_create_client_stream_conn(const struct VC_CTX *ctx,
		const char *node,
		const char *service,
		uint8 *error)
{
	struct VStreamConn *stream_conn = NULL;
	struct addrinfo hints, *result, *rp;
	int sockfd = -1, ret, flag;
	unsigned int int_size;
#ifndef __APPLE__
	struct timeval tv;
#endif

	*error = VRS_CONN_TERM_RESERVED;

	/* Seed random number generator,  */
#ifdef __APPLE__
	sranddev();
	/* Other BSD based systems probably support this or similar function too. */
#else
	/* Other systems have to use this evil trick */
	gettimeofday(&tv, NULL);
	srand(tv.tv_sec - tv.tv_usec);
#endif

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;		/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;	/* Allow stream protocol */
	hints.ai_flags = 0;					/* No flags required */
	hints.ai_protocol = IPPROTO_TCP;	/* Allow TCP protocol only */

	v_print_log(VRS_PRINT_DEBUG_MSG, "Try to connect to: %s:%s\n", node, service);

	if( (ret = getaddrinfo(node, service, &hints, &result)) !=0 ) {
		v_print_log(VRS_PRINT_ERROR, "getaddrinfo(): %s\n", gai_strerror(ret));
		*error = VRS_CONN_TERM_HOST_UNKNOWN;
		return NULL;
	}

	/* Try to use addrinfo from getaddrinfo() */
	for(rp=result; rp!=NULL; rp=rp->ai_next) {
		if( (sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
			v_print_log(VRS_PRINT_ERROR, "socket(): %s\n", strerror(errno));
			continue;
		}
		else {
			/* Try to "connect" to this address ... the client will be able to send and
			 * receive packets only from this address. */
			if(connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
				break;
			close(sockfd);
			sockfd = -1;
		}
	}

	if(rp == NULL) {
		v_print_log(VRS_PRINT_ERROR,
				"Could not connect to the [%s]:%s\n", node, service);
		freeaddrinfo(result);
		*error = VRS_CONN_TERM_SERVER_DOWN;
		return NULL;
	}

	if ( (stream_conn = (struct VStreamConn*)calloc(1, sizeof(struct VStreamConn))) == NULL) {
		v_print_log(VRS_PRINT_ERROR,
				"calloc(): %s\n", strerror(errno));
		freeaddrinfo(result);
		close(sockfd);
		sockfd = -1;
		*error = VRS_CONN_TERM_ERROR;
		return NULL;
	}

	v_conn_stream_init(stream_conn);

	/* Set address of peer and host */
	if(rp->ai_family==AF_INET) {
		/* Address type of client */
		stream_conn->io_ctx.host_addr.ip_ver = IPV4;
		stream_conn->io_ctx.host_addr.protocol = TCP;
		/* Address of peer */
		stream_conn->io_ctx.peer_addr.ip_ver = IPV4;
		stream_conn->io_ctx.peer_addr.protocol = TCP;
		stream_conn->io_ctx.peer_addr.port = ntohs(((struct sockaddr_in*)rp->ai_addr)->sin_port);
		memcpy(&stream_conn->io_ctx.peer_addr.addr.ipv4, rp->ai_addr, rp->ai_addrlen);
		/* Address of peer (reference in connection) */
		stream_conn->peer_address.ip_ver = IPV4;
		stream_conn->peer_address.protocol = TCP;
		stream_conn->peer_address.port = ntohs(((struct sockaddr_in*)rp->ai_addr)->sin_port);
		memcpy(&stream_conn->peer_address.addr.ipv4, rp->ai_addr, rp->ai_addrlen);
	}
	else if(rp->ai_family==AF_INET6) {
		/* Address type of client */
		stream_conn->io_ctx.host_addr.ip_ver = IPV6;
		stream_conn->io_ctx.host_addr.protocol = TCP;
		/* Address of peer */
		stream_conn->io_ctx.peer_addr.ip_ver = IPV6;
		stream_conn->io_ctx.peer_addr.protocol = TCP;
		stream_conn->io_ctx.peer_addr.port = ntohs(((struct sockaddr_in6*)rp->ai_addr)->sin6_port);
		memcpy(&stream_conn->io_ctx.peer_addr.addr.ipv6, rp->ai_addr, rp->ai_addrlen);
		/* Address of peer (reference in connection) */
		stream_conn->peer_address.ip_ver = IPV6;
		stream_conn->peer_address.protocol = UDP;
		stream_conn->peer_address.port = ntohs(((struct sockaddr_in6*)rp->ai_addr)->sin6_port);
		memcpy(&stream_conn->peer_address.addr.ipv6, rp->ai_addr, rp->ai_addrlen);
	}

	/* Use first successfully assigned socket */
	stream_conn->io_ctx.sockfd = sockfd;

	freeaddrinfo(result);

	/* Try to get size of TCP buffer */
	int_size = sizeof(int_size);
	if( getsockopt(stream_conn->io_ctx.sockfd, SOL_SOCKET, SO_RCVBUF,
			(void *)&stream_conn->socket_buffer_size, &int_size) != 0 )
	{
		v_print_log(VRS_PRINT_ERROR, "getsockopt(): %s\n", strerror(errno));
		close(stream_conn->io_ctx.sockfd);
		stream_conn->io_ctx.sockfd = -1;
		free(stream_conn);
		*error = VRS_CONN_TERM_ERROR;
		return NULL;
	}

	/* Make sure socket is blocking */
	flag = fcntl(stream_conn->io_ctx.sockfd, F_GETFL, 0);
	if( (fcntl(stream_conn->io_ctx.sockfd, F_SETFL, flag & ~O_NONBLOCK)) == -1) {
		v_print_log(VRS_PRINT_ERROR, "fcntl(): %s\n", strerror(errno));
		close(stream_conn->io_ctx.sockfd);
		stream_conn->io_ctx.sockfd = -1;
		free(stream_conn);
		*error = VRS_CONN_TERM_ERROR;
		return NULL;
	}

#ifdef WITH_OPENSSL
	if(ctx->tls_ctx != NULL) {
		long cert_verify_res;
		X509* cert = NULL;
		/* Set up SSL */
		if( (stream_conn->io_ctx.ssl=SSL_new(ctx->tls_ctx)) == NULL) {
			v_print_log(VRS_PRINT_ERROR, "Setting up SSL failed.\n");
			ERR_print_errors_fp(v_log_file());
			close(stream_conn->io_ctx.sockfd);
			stream_conn->io_ctx.sockfd = -1;
			free(stream_conn);
			*error = VRS_CONN_TERM_ERROR;
			return NULL;
		}

		/* Bind socket and SSL */
		if (SSL_set_fd(stream_conn->io_ctx.ssl, stream_conn->io_ctx.sockfd) == 0) {
			v_print_log(VRS_PRINT_ERROR,
					"Failed binding socket descriptor and SSL.\n");
			ERR_print_errors_fp(v_log_file());
			SSL_free(stream_conn->io_ctx.ssl);
			stream_conn->io_ctx.ssl = NULL;
			close(stream_conn->io_ctx.sockfd);
			stream_conn->io_ctx.sockfd = -1;
			free(stream_conn);
			*error = VRS_CONN_TERM_ERROR;
			return NULL;
		}

		SSL_set_mode(stream_conn->io_ctx.ssl, SSL_MODE_AUTO_RETRY);

		/* Set certificates of CA */
		if(SSL_CTX_load_verify_locations(ctx->tls_ctx, CA_CERT_FILE, NULL) != 1) {
			v_print_log(VRS_PRINT_ERROR,
					"Failed to set CA certificates file.\n");
			ERR_print_errors_fp(v_log_file());
			SSL_free(stream_conn->io_ctx.ssl);
			stream_conn->io_ctx.ssl = NULL;
			close(stream_conn->io_ctx.sockfd);
			stream_conn->io_ctx.sockfd = -1;
			free(stream_conn);
			*error = VRS_CONN_TERM_ERROR;
			return NULL;
		}

		/* Do TLS handshake and negotiation */
		if( SSL_connect(stream_conn->io_ctx.ssl) == -1) {
			v_print_log(VRS_PRINT_ERROR, "SSL connection failed\n");
			ERR_print_errors_fp(v_log_file());
			SSL_free(stream_conn->io_ctx.ssl);
			stream_conn->io_ctx.ssl = NULL;
			close(stream_conn->io_ctx.sockfd);
			stream_conn->io_ctx.sockfd = -1;
			free(stream_conn);
			*error = VRS_CONN_TERM_ERROR;
			return NULL;
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "SSL connection established.\n");
		}

		/* Verify a server certificate was presented during the handshake */
		cert = SSL_get_peer_certificate(stream_conn->io_ctx.ssl);
		if(NULL == cert) {
			v_print_log(VRS_PRINT_ERROR, "Server did not sent certificate\n");
			ERR_print_errors_fp(v_log_file());
			SSL_free(stream_conn->io_ctx.ssl);
			stream_conn->io_ctx.ssl = NULL;
			close(stream_conn->io_ctx.sockfd);
			stream_conn->io_ctx.sockfd = -1;
			free(stream_conn);
			*error = VRS_CONN_TERM_ERROR;
			return NULL;
		}

		/* Verify the result of chain verification
		 * Verification performed according to RFC 4158. */
		cert_verify_res = SSL_get_verify_result(stream_conn->io_ctx.ssl);
		if(cert_verify_res != X509_V_OK) {
			v_print_log(VRS_PRINT_ERROR,
					"Server sent wrong certificate: %s.\n",
					X509_verify_cert_error_string(cert_verify_res));
			ERR_print_errors_fp(v_log_file());
			SSL_free(stream_conn->io_ctx.ssl);
			stream_conn->io_ctx.ssl = NULL;
			close(stream_conn->io_ctx.sockfd);
			stream_conn->io_ctx.sockfd = -1;
			free(stream_conn);
			*error = VRS_CONN_TERM_ERROR;
			if(cert != NULL) {
				X509_free(cert);
			}
			return NULL;
		}

		/* Get hostname from X509 certificate and check
		 * if certificate contains desired hostname. */
		if(verify_cert_hostname(cert, node) != 1) {
			v_print_log(VRS_PRINT_ERROR, "Certificate with wrong hostname.\n");
			ERR_print_errors_fp(v_log_file());
			SSL_free(stream_conn->io_ctx.ssl);
			stream_conn->io_ctx.ssl = NULL;
			close(stream_conn->io_ctx.sockfd);
			stream_conn->io_ctx.sockfd = -1;
			free(stream_conn);
			*error = VRS_CONN_TERM_ERROR;
			if(cert != NULL) {
				X509_free(cert);
			}
			return NULL;
		}

		/* Debug print: ciphers, certificates, etc. */
		if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"SSL connection uses: %s cipher.\n",
					SSL_get_cipher(stream_conn->io_ctx.ssl));
		}

		if(cert != NULL) {
			X509_free(cert);
		}
	}
#else
	(void)ctx;
#endif

	return stream_conn;
}

/**
 * \brief Main TCP loop responsible for user authentication
 * \param[in]	*vc_ctx		The pointer at Verse client context
 * \param[in]	*vsession	The pointer at Verse session
 *
 */
void vc_main_stream_loop(struct VC_CTX *vc_ctx, struct VSession *vsession)
{
	struct vContext *C=NULL;
	struct VStreamConn *stream_conn=NULL;
	struct VMessage *r_message=NULL, *s_message=NULL;
	struct VUserCredentials user_credentials;
	int ret;
	uint8 error = 0;
	uint8 *udp_thread_result;

	struct Connect_Terminate_Cmd *conn_term;

	/* Initialize new TCP connection to the server. */
	if( (stream_conn = vc_create_client_stream_conn(vc_ctx, vsession->peer_hostname, vsession->service, &error)) == NULL ) {
		goto closed;
	}

	/* Set up client context, connection context and IO context */
	C = (struct vContext*)calloc(1, sizeof(struct vContext));
	r_message = (struct VMessage*)calloc(1, sizeof(struct VMessage));
	s_message = (struct VMessage*)calloc(1, sizeof(struct VMessage));
	CTX_server_ctx_set(C, NULL);
	CTX_client_ctx_set(C, vc_ctx);
	CTX_current_session_set(C, vsession);
	CTX_current_dgram_conn_set(C, NULL);
	CTX_current_stream_conn_set(C, stream_conn);
	CTX_io_ctx_set(C, &stream_conn->io_ctx);
	CTX_r_packet_set(C, NULL);
	CTX_s_packet_set(C, NULL);
	CTX_r_message_set(C, r_message);
	CTX_s_message_set(C, s_message);
	vsession->stream_conn = stream_conn;

	/* Update client and server states */
	stream_conn->host_state = TCP_CLIENT_STATE_USRAUTH_NONE;

	/* Initialize user credentials */
	user_credentials.username = NULL;
	user_credentials.data = NULL;
	user_credentials.auth_meth_count = 0;
	user_credentials.methods = NULL;

	/* Get list of supported authentication methods */
	ret = vc_USRAUTH_none_loop(C, &user_credentials);
	switch(ret) {
	case 0:
		error = -1;
		goto closing;
		break;
	case CMD_USER_AUTH_FAILURE:
		goto data;
		break;
	case CMD_USER_AUTH_SUCCESS:
		goto token_ded;
		break;
	}

data:
	/* Update client and server states */
	stream_conn->host_state = TCP_CLIENT_STATE_USRAUTH_DATA;

	/* Try to authenticate user with username and password */
	ret = vc_USRAUTH_data_loop(C, &user_credentials);

	/* Clear and free user credentials data from client application */
	if(user_credentials.username != NULL) {
		free(user_credentials.username);
		user_credentials.username = NULL;
	}
	if(user_credentials.data != NULL) {
		unsigned int i, len = strlen(user_credentials.data);
		for(i = 0; i < len; i++) {
			user_credentials.data[i] = '\0';
		}
		free(user_credentials.data);
		user_credentials.data = NULL;
	}
	user_credentials.auth_meth_count = 0;
	if(user_credentials.methods != NULL) {
		free(user_credentials.methods);
		user_credentials.methods = NULL;
	}

	switch (ret) {
		case 0:
			error = VRS_CONN_TERM_AUTH_FAILED;
			goto closing;
		case CMD_USER_AUTH_SUCCESS:
			goto token_ded;
		default:
			break;
	}

token_ded:
	/* Update client and server states */
	stream_conn->host_state = TCP_CLIENT_STATE_NEGOTIATE_TOKEN_DED;

	/* Try to negotiate new UDP port for communication */
	ret = vc_NEGOTIATE_token_dtd_loop(C);
	switch (ret) {
	case 0:
		error = VRS_CONN_TERM_ERROR;
		goto closing;
	case 1:
		goto newhost;
		break;
	default:
		break;
	}

newhost:
	/* Update client and server states */
	stream_conn->host_state = TCP_CLIENT_STATE_NEGOTIATE_NEWHOST;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client TCP state: NEGOTIATE_newhost\n");
		printf("%c[%dm", 27, 0);
	}

	if( vsession->flags & VRS_TP_UDP ) {
		/* Try to create new UDP thread */
		if((ret = pthread_create(&vsession->udp_thread, NULL, vc_main_dgram_loop, (void*)C)) != 0) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "pthread_create(): %s\n", strerror(errno));
		}
	} else if (vsession->flags & VRS_TP_TCP ) {
		/* Negotiate new TCP connection or confirm existing TCP connection */
		ret = vc_NEGOTIATE_newhost(C, vsession->host_url);
		switch (ret) {
		case 0:
			error = VRS_CONN_TERM_ERROR;
			stream_conn->host_state = TCP_CLIENT_STATE_CLOSING;
			goto closing;
		case 1:
			goto stream_open;
			break;
		default:
			break;
		}
	} else {
		stream_conn->host_state = TCP_CLIENT_STATE_CLOSING;
	}

	goto closing;

stream_open:
	/* Update client state */
	stream_conn->host_state = TCP_CLIENT_STATE_STREAM_OPEN;

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client TCP state: STREAM_OPEN\n");
		printf("%c[%dm", 27, 0);
	}

	ret = vc_STREAM_OPEN_loop(C);
	switch (ret) {
	case 0:
		error = VRS_CONN_TERM_ERROR;
		break;
	case 1:
		break;
	default:
		break;
	}

	stream_conn->host_state = TCP_CLIENT_STATE_CLOSING;

closing:
	/* Wait until datagram connection is negotiated and established */
	while(vsession->stream_conn->host_state == TCP_CLIENT_STATE_NEGOTIATE_NEWHOST) {
		/* Sleep 1 milisecond */
		usleep(1000);
	}

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client TCP state: CLOSING\n");
		printf("%c[%dm", 27, 0);
	}

	if(r_message!=NULL) {
		CTX_r_message_set(C, NULL);
		free(r_message);
	}
	if(s_message!=NULL) {
		CTX_s_message_set(C, NULL);
		free(s_message);
	}

	/* Destroy stream connection */
	if(stream_conn != NULL) {
		vc_destroy_stream_conn(stream_conn);
		CTX_current_stream_conn_set(C, NULL);
	}

	if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Client TCP state: CLOSED\n");
		printf("%c[%dm", 27, 0);
	}

	/* If UDP thread was created, when wait for UDP thread */
	if(vsession->udp_thread != 0) {
		/* Wait for UDP thread (this is blocking operation) */
		v_print_log(VRS_PRINT_DEBUG_MSG, "Waiting for join with UDP thread ...\n");
		if(pthread_join(vsession->udp_thread, (void*)&udp_thread_result) != 0) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "UDP thread was not joined\n");
		}
		/* Get error from udp thread */
		error = *udp_thread_result;
	}

closed:
	conn_term = v_connect_terminate_create(error);
	v_in_queue_push(vsession->in_queue, (struct Generic_Cmd*)conn_term);

	/* Wait in loop to free vsession->in_queue */
	while( v_in_queue_size(vsession->in_queue) > 0) {
		/* Sleep 1 milisecond */
		usleep(1000);
	}

	free(C);
}
