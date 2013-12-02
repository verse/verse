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
#ifdef WITH_KERBEROS
#include <krb5.h>
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "verse_types.h"

#include "vs_main.h"
#include "vs_tcp_connect.h"
#include "vs_udp_connect.h"
#include "vs_auth_pam.h"
#include "vs_auth_csv.h"
#include "vs_auth_ldap.h"
#include "vs_node.h"
#include "vs_handshake.h"
#include "vs_sys_nodes.h"

#include "v_common.h"
#include "v_pack.h"
#include "v_unpack.h"
#include "v_connection.h"
#include "v_session.h"
#include "v_stream.h"
#include "v_fake_commands.h"

#ifdef WSLAY
#include "vs_websocket.h"
#endif

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
	int error, ret;
	void *udp_thread_result;
	unsigned int int_size;
#ifdef WITH_KERBEROS
	char **u_name;
#endif


	/* Try to get size of TCP buffer */
	int_size = sizeof(int_size);
	getsockopt(io_ctx->sockfd, SOL_SOCKET, SO_RCVBUF,
			(void *)&stream_conn->socket_buffer_size, &int_size);

#ifdef WITH_OPENSSL
#ifdef WITH_KERBEROS
	if (vs_ctx->use_krb5 != USE_KERBEROS) {
#endif
		/* Try to do TLS handshake with client */
		if(vs_TLS_handshake(C)!=1) {
			goto end;
		}
#ifdef WITH_KERBEROS
	}
#endif
#endif

#ifdef WITH_KERBEROS
	if (vs_ctx->use_krb5 == USE_KERBEROS) {
		u_name = malloc(sizeof(char *));
		if (vs_kerberos_auth(C, u_name) != 1) {
			goto end;
		}
	}
#endif
	r_message = (struct VMessage*)calloc(1, sizeof(struct VMessage));
	s_message = (struct VMessage*)calloc(1, sizeof(struct VMessage));
	CTX_r_message_set(C, r_message);
	CTX_s_message_set(C, s_message);

#ifdef WITH_KERBEROS
	if (vs_ctx->use_krb5 == USE_KERBEROS) {
		stream_conn->host_state = TCP_SERVER_STATE_RESPOND_KRB_AUTH;

		if (is_log_level(VRS_PRINT_DEBUG_MSG)) {
			printf("%c[%d;%dm", 27, 1, 31);
			v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: RESPOND_krb_auth\n");
			printf("%c[%dm", 27, 0);
		}
	} else {
#endif
	stream_conn->host_state = TCP_SERVER_STATE_RESPOND_METHODS;

	if (is_log_level(VRS_PRINT_DEBUG_MSG)) {
		printf("%c[%d;%dm", 27, 1, 31);
		v_print_log(VRS_PRINT_DEBUG_MSG, "Server TCP state: RESPOND_methods\n");
		printf("%c[%dm", 27, 0);
	}
#ifdef WITH_KERBEROS
	}
#endif

	/* "Never ending" loop */
	while(1)
	{
		FD_ZERO(&set);
		FD_SET(io_ctx->sockfd, &set);

		tv.tv_sec = VRS_TIMEOUT;	/* User have to send something in 30 seconds */
		tv.tv_usec = 0;

#ifdef WITH_KERBEROS
		if(vs_ctx->use_krb5 == USE_KERBEROS) {
			goto respond_krb;
		}
#endif

		if( (ret = select(io_ctx->sockfd+1, &set, NULL, NULL, &tv)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "%s:%s():%d select(): %s\n",
					__FILE__, __FUNCTION__,  __LINE__, strerror(errno));
			goto end;
			/* Was event on the listen socket */
		} else if(ret>0 && FD_ISSET(io_ctx->sockfd, &set)) {

			/* Try to receive data through TCP connection */
			if( v_tcp_read(io_ctx, &error) <= 0 ) {
				goto end;
			}
			/* Handle verse handshake at TCP connection */
#ifdef WITH_KERBEROS
respond_krb:
			if (vs_ctx->use_krb5 == USE_KERBEROS) {
				if( (ret = vs_handle_handshake(C, *u_name)) == -1) {
					goto end;
				}
			} else {
#endif
			if( (ret = vs_handle_handshake(C, NULL)) == -1) {
				goto end;
			}
#ifdef WITH_KERBEROS
	}
#endif
			/* When there is something to send, then send it to peer */
			if( ret == 1 ) {
#ifdef WITH_KERBEROS
				if(vs_ctx->use_krb5 != USE_KERBEROS){
#endif
				/* Send response message to the client */
				if( (ret = v_tcp_write(io_ctx, &error)) <= 0) {
					goto end;
				}
#ifdef WITH_KERBEROS
				}
#endif
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
	stream_conn->host_state = TCP_SERVER_STATE_LISTEN;

	/* Clear session flags */
	vsession->flags = 0;

	if(vsession->peer_cookie.str != NULL) {
		free(vsession->peer_cookie.str);
		vsession->peer_cookie.str = NULL;
	}
	if(vsession->ded.str != NULL) {
		free(vsession->ded.str);
		vsession->ded.str = NULL;
	}
	if(vsession->client_name != NULL) {
		free(vsession->client_name);
		vsession->client_name = NULL;
	}
	if(vsession->client_version != NULL) {
		free(vsession->client_version);
		vsession->client_version = NULL;
	}

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
 * \brief This connection tries to handle new connection attempt. When this
 * attempt is successful, then it creates new thread
 */
static int vs_new_stream_conn(struct vContext *C, void *(*conn_loop)(void*))
{
	VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct IO_CTX *io_ctx = CTX_io_ctx(C);
	struct VSession *current_session = NULL;
	socklen_t addr_len;
	int ret, i;
	static uint32 last_session_id = 0;

	/* Try to find free session */
	for(i=0; i< vs_ctx->max_sessions; i++) {
		if(vs_ctx->vsessions[i]->stream_conn->host_state == TCP_SERVER_STATE_LISTEN) {
			/* TODO: lock mutex here */
			current_session = vs_ctx->vsessions[i];
			current_session->stream_conn->host_state = TCP_SERVER_STATE_RESPOND_METHODS;
			current_session->session_id = last_session_id++;
			/* TODO: unlock mutex here */
			break;
		}
	}

	if(current_session != NULL) {
		struct vContext *new_C;
		int flag;

		/* Try to accept client connection (do TCP handshake) */
		if(io_ctx->host_addr.ip_ver==IPV4) {
			/* Prepare IPv4 variables for TCP handshake */
			struct sockaddr_in *client_addr4 = &current_session->stream_conn->io_ctx.peer_addr.addr.ipv4;
			current_session->stream_conn->io_ctx.peer_addr.ip_ver = IPV4;
			addr_len = sizeof(current_session->stream_conn->io_ctx.peer_addr.addr.ipv4);

			/* Try to do TCP handshake */
			if( (current_session->stream_conn->io_ctx.sockfd = accept(io_ctx->sockfd, (struct sockaddr*)client_addr4, &addr_len)) == -1) {
				if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "accept(): %s\n", strerror(errno));
				return 0;
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
				return 0;
			}

			/* Save the IPv6 of the client as string in verse session */
			inet_ntop(AF_INET6, &client_addr6->sin6_addr, current_session->peer_hostname, INET6_ADDRSTRLEN);
		}

		/* Set to this socket flag "no delay" */
		flag = 1;
		if(setsockopt(current_session->stream_conn->io_ctx.sockfd,
				IPPROTO_TCP, TCP_NODELAY, &flag, (socklen_t)sizeof(flag)) == -1)
		{
			if(is_log_level(VRS_PRINT_ERROR)) {
				v_print_log(VRS_PRINT_ERROR,
						"setsockopt: TCP_NODELAY: %d\n",
						strerror(errno));
			}
			return -1;;
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
			return 0;
		}

		/* Try to set thread attributes as detached */
		if( (ret = pthread_attr_setdetachstate(&current_session->tcp_thread_attr, PTHREAD_CREATE_DETACHED)) != 0) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "pthread_attr_setdetachstate(): %s\n", strerror(errno));
			return 0;
		}

		/* Try to create new thread */
		if((ret = pthread_create(&current_session->tcp_thread, &current_session->tcp_thread_attr, conn_loop, (void*)new_C)) != 0) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "pthread_create(): %s\n", strerror(errno));
		}

		/* Destroy thread attributes */
		pthread_attr_destroy(&current_session->tcp_thread_attr);
	} else {
		int tmp_sockfd;
		v_print_log(VRS_PRINT_DEBUG_MSG, "Number of session slot: %d reached\n", vs_ctx->max_sessions);
		/* TODO: return some error. Not only accept and close connection. */
		/* Try to accept client connection (do TCP handshake) */
		if(io_ctx->host_addr.ip_ver == IPV4) {
			/* Prepare IP6 variables for TCP handshake */
			struct sockaddr_in client_addr4;

			addr_len = sizeof(client_addr4);

			/* Try to do TCP handshake */
			if( (tmp_sockfd = accept(io_ctx->sockfd, (struct sockaddr*)&client_addr4, &addr_len)) == -1) {
				if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "accept(): %s\n", strerror(errno));
				return 0;
			}
			/* Close connection (no more free session slots) */
			close(tmp_sockfd);
		} else if(io_ctx->host_addr.ip_ver == IPV6) {
			/* Prepare IP6 variables for TCP handshake */
			struct sockaddr_in6 client_addr6;
			addr_len = sizeof(client_addr6);

			/* Try to do TCP handshake */
			if( (tmp_sockfd = accept(io_ctx->sockfd, (struct sockaddr*)&client_addr6, &addr_len)) == -1) {
				if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "accept(): %s\n", strerror(errno));
				return 0;
			}
			/* Close connection (no more free session slots) */
			close(tmp_sockfd);
		}
		/* TODO: Fix this */
		sleep(1);
	}

	return 1;
}

/**
 * \brief Main Verse server loop. Server waits for connect attempts, responds to attempts
 * and creates per connection threads
 */
int vs_main_listen_loop(VS_CTX *vs_ctx)
{
	struct vContext *C;
	struct timeval start, tv;
	fd_set set;
	int count, tmp, i, ret;
	int sockfd;

	/* Allocate context for server */
	C = (struct vContext*)calloc(1, sizeof(struct vContext));
	/* Set up client context, connection context and IO context */
	CTX_server_ctx_set(C, vs_ctx);
	CTX_client_ctx_set(C, NULL);
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
#ifdef WSLAY
			v_print_log_simple(VRS_PRINT_DEBUG_MSG,
					"\tServer listen on (TCP port: %d, WebSocket port: %d)\n",
					vs_ctx->tcp_io_ctx.host_addr.port,
					vs_ctx->ws_io_ctx.host_addr.port);
#else
			v_print_log_simple(VRS_PRINT_DEBUG_MSG,
					"\tServer listen on TCP port: %d\n",
					vs_ctx->tcp_io_ctx.host_addr.port);
#endif
		}

		/* Set up set of sockets */
		FD_ZERO(&set);
		FD_SET(vs_ctx->tcp_io_ctx.sockfd, &set);
		/* When Verse is compiled with support of WebSocket, then listen on
		 * WebSocket port too */
#ifdef WSLAY
		FD_SET(vs_ctx->ws_io_ctx.sockfd, &set);
		sockfd = (vs_ctx->tcp_io_ctx.sockfd > vs_ctx->ws_io_ctx.sockfd) ?
				vs_ctx->tcp_io_ctx.sockfd : vs_ctx->ws_io_ctx.sockfd;
#else
		sockfd = vs_ctx->tcp_io_ctx.sockfd;
#endif

		/* We will wait one second for connect attempt, then debug print will
		 * be print again */
		tv.tv_sec = 1;
		tv.tv_usec = 0;


		/* Wait for event on listening sockets */
		if( (ret = select(sockfd+1, &set, NULL, NULL, &tv)) == -1 ) {
			int err = errno;
			if(err==EINTR) {
				break;
			} else {
				if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR,
						"%s:%s():%d select(): %s\n",
						__FILE__,
						__FUNCTION__,
						__LINE__,
						strerror(err));
				return -1;
			}
			/* Was event on the listen socket */
		} else if(ret>0) {
			if (FD_ISSET(vs_ctx->tcp_io_ctx.sockfd, &set))
			{
				v_print_log(VRS_PRINT_DEBUG_MSG, "TCP Connection attempt\n");
				CTX_io_ctx_set(C, &vs_ctx->tcp_io_ctx);
				vs_new_stream_conn(C, vs_tcp_conn_loop);
#ifdef WSLAY
			} else if(FD_ISSET(vs_ctx->ws_io_ctx.sockfd, &set)) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "WebSocket Connection attempt\n");
				CTX_io_ctx_set(C, &vs_ctx->ws_io_ctx);
				vs_new_stream_conn(C, vs_websocket_loop);
#endif
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


#ifdef WITH_OPENSSL
/**
 * \brief Destroy stream part of verse context
 */
void vs_destroy_stream_ctx(VS_CTX *vs_ctx)
{
	if(vs_ctx->tls_ctx != NULL) {
		SSL_CTX_free(vs_ctx->tls_ctx);
		vs_ctx->tls_ctx = NULL;
	}
	if(vs_ctx->dtls_ctx != NULL) {
		SSL_CTX_free(vs_ctx->dtls_ctx);
		vs_ctx->dtls_ctx = NULL;
	}
}


/*
 * \brief Initialize OpenSSl of Verse server
 */
static int vs_init_ssl(VS_CTX *vs_ctx)
{
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

	return 1;
}
#endif


/**
 * \brief Initialize IO context of Verse server used for incomming connections (TCP, WebSocket)
 */
static int vs_init_io_ctx(struct IO_CTX *io_ctx,
		unsigned short port,
		int max_sessions)
{
	int flag;

	/* Allocate buffer for incoming packets */
	if ( (io_ctx->buf = (char*)calloc(MAX_PACKET_SIZE, sizeof(char))) == NULL) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "calloc(): %s\n", strerror(errno));
		return -1;
	}

	/* "Address" of server */
	if(io_ctx->host_addr.ip_ver == IPV4) {		/* IPv4 */

		/* Create socket which server uses for listening for new connections */
		if ( (io_ctx->sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1 ) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "socket(): %s\n", strerror(errno));
			return -1;
		}

		/* Set socket to reuse address */
		flag = 1;
		if( setsockopt(io_ctx->sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "setsockopt(): %s\n", strerror(errno));
			return -1;
		}

		io_ctx->host_addr.addr.ipv4.sin_family = AF_INET;
		io_ctx->host_addr.addr.ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
		io_ctx->host_addr.addr.ipv4.sin_port = htons(port);
		io_ctx->host_addr.port = port;

		/* Bind address and socket */
		if( bind(io_ctx->sockfd,
				(struct sockaddr*)&(io_ctx->host_addr.addr.ipv4),
				sizeof(io_ctx->host_addr.addr.ipv4)) == -1)
		{
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "bind(): %s\n", strerror(errno));
			return -1;
		}

	}
	else if(io_ctx->host_addr.ip_ver == IPV6) {	/* IPv6 */

		/* Create socket which server uses for listening for new connections */
		if ( (io_ctx->sockfd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) == -1 ) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "socket(): %s\n", strerror(errno));
			return -1;
		}

		/* Set socket to reuse address */
		flag = 1;
		if( setsockopt(io_ctx->sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "setsockopt(): %s\n", strerror(errno));
			return -1;
		}

		io_ctx->host_addr.addr.ipv6.sin6_family = AF_INET6;
		io_ctx->host_addr.addr.ipv6.sin6_addr = in6addr_any;
		io_ctx->host_addr.addr.ipv6.sin6_port = htons(port);
		io_ctx->host_addr.addr.ipv6.sin6_flowinfo = 0; /* Obsolete value */
		io_ctx->host_addr.addr.ipv6.sin6_scope_id = 0;
		io_ctx->host_addr.port = port;

		/* Bind address and socket */
		if( bind(io_ctx->sockfd,
				(struct sockaddr*)&(io_ctx->host_addr.addr.ipv6),
				sizeof(io_ctx->host_addr.addr.ipv6)) == -1)
		{
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "bind(): %d\n", strerror(errno));
			return -1;
		}

	}

	/* Create queue for TCP connection attempts, set maximum number of
	 * attempts in listen queue */
	if( listen(io_ctx->sockfd, max_sessions) == -1) {
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "listen(): %s\n", strerror(errno));
		return -1;
	}

	/* Set up flag for V_CTX of server */
	io_ctx->flags = 0;

	/* Set all bytes of buffer for incoming packet to zero */
	memset(io_ctx->buf, 0, MAX_PACKET_SIZE);

	return 1;
}


/**
 * \brief Initialize IO context for TCP connection
 */
static int vs_init_tcp_io_ctx(VS_CTX *vs_ctx)
{
	return vs_init_io_ctx(&vs_ctx->tcp_io_ctx, vs_ctx->tcp_port, vs_ctx->max_sessions);
}

#ifdef WSLAY
/**
 * \brief Initialize IO context for WebSocket TCP connection
 */
static int vs_init_websocket_io_ctx(VS_CTX *vs_ctx)
{
	return vs_init_io_ctx(&vs_ctx->ws_io_ctx, vs_ctx->ws_port, vs_ctx->max_sessions);
}
#endif

/**
 * \brief Initialize list of verse sessions
 */
static int vs_init_sessions(VS_CTX *vs_ctx)
{
	int i;

	vs_ctx->connected_clients = 0;

	/* Initialize list of connections */
	vs_ctx->vsessions = (struct VSession**)calloc(vs_ctx->max_sessions, sizeof(struct VSession*));
	for (i=0; i<vs_ctx->max_sessions; i++) {
		if( (vs_ctx->vsessions[i] = (struct VSession*)calloc(1, sizeof(struct VSession))) == NULL ) {
			if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "calloc(): %s\n", strerror(errno));
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

#ifdef WITH_KERBEROS
/**
 *
 */
int vs_init_kerberos(VS_CTX *vs_ctx){
	char *name;
	krb5_error_code krb5_err;

	/* Using Kerberos */
	krb5_err = krb5_init_context(&vs_ctx->krb5_ctx);
	if (krb5_err) {
		v_print_log(VRS_PRINT_ERROR, "krb5_init_context: %d: %s\n", krb5_err,
				krb5_get_error_message(vs_ctx->krb5_ctx, krb5_err));
		return -1;
	}
	/** TODO
	 * resolv service and domain names
	 */
	if ((krb5_err = krb5_sname_to_principal(vs_ctx->krb5_ctx, "localhost",
			"verse",
			KRB5_NT_SRV_HST, &vs_ctx->krb5_principal))) {
		v_print_log(VRS_PRINT_ERROR, "krb5_sname_to_principal: %d: %s\n",
				krb5_err, krb5_get_error_message(vs_ctx->krb5_ctx, krb5_err));
		return -1;
	}
	krb5_unparse_name(vs_ctx->krb5_ctx, vs_ctx->krb5_principal, &name);
	v_print_log(VRS_PRINT_DEBUG_MSG, "Kerberos principal: %s\n", name);

	return 1;
}
#endif
/**
 * \brief Initialize verse server context for connection at TCP
 */
int vs_init_stream_ctx(VS_CTX *vs_ctx)
{
	int ret;

#ifdef WITH_OPENSSL
	/* Will be Kerberos used? */
#ifdef WITH_KERBEROS
	if (vs_ctx->use_krb5 != USE_KERBEROS) {
#endif
		ret = vs_init_ssl(vs_ctx);
		if(ret != 1) {
			return ret;
		}
#ifdef WITH_KERBEROS
	}
#endif
#endif
#ifdef WITH_KERBEROS
	/* Will be Kerberos used? */
	if (vs_ctx->use_krb5 == USE_KERBEROS) {
		ret = vs_init_kerberos(vs_ctx);
		if (ret != 1) {
			return ret;
		}
	}
#endif
	ret = vs_init_tcp_io_ctx(vs_ctx);
	if(ret != 1) {
		return ret;
	}

#ifdef WSLAY
	ret = vs_init_websocket_io_ctx(vs_ctx);
	if(ret != 1) {
		return ret;
	}
#endif

	ret = vs_init_sessions(vs_ctx);
	if(ret != 1) {
		return ret;
	}

	return 1;
}
