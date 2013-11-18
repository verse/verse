/*
 * $Id: vs_main.c 1348 2012-09-19 20:08:18Z jiri $
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
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "verse.h"

#include "verse_types.h"

#include "vs_main.h"
#include "vs_tcp_connect.h"
#include "vs_udp_connect.h"
#include "vs_auth_csv.h"
#include "vs_auth_ldap.h"
#include "vs_data.h"
#include "vs_node.h"
#include "vs_config.h"

#include "v_common.h"
#include "v_session.h"
#include "v_network.h"

struct VS_CTX *local_vs_ctx = NULL;

/**
 * \brief Load information about user accounts created after server started.
 */
static int vs_reload_user_accounts(struct VS_CTX *vs_ctx)
{
	int ret = 0;

	switch (vs_ctx->auth_type) {
	case AUTH_METHOD_CSV_FILE:
		vs_load_new_user_accounts_csv_file(vs_ctx);
		break;
	case AUTH_METHOD_LDAP:
		vs_load_new_user_accounts_ldap_server(vs_ctx);
		break;
	default:
		break;
	}

	return ret;
}

/**
 * \brief Callback function for handling signals.
 * \param	sig	identifier of signal
 */
void vs_handle_signal(int sig)
{
	if(sig == SIGINT) {
		struct VS_CTX *vs_ctx = local_vs_ctx;
		int i;

		/* Since now server will not be able to accept new clients */
		vs_ctx->state = SERVER_STATE_CLOSING;

		for(i=0; i<vs_ctx->max_sessions; i++) {
			if(vs_ctx->vsessions[i] != NULL &&
					vs_ctx->vsessions[i]->stream_conn != NULL &&
					vs_ctx->vsessions[i]->stream_conn->host_state != TCP_SERVER_STATE_LISTEN) {

				if(vs_ctx->vsessions[i]->dgram_conn != NULL &&
						vs_ctx->vsessions[i]->dgram_conn->host_state != UDP_SERVER_STATE_CLOSED) {
					v_print_log(VRS_PRINT_DEBUG_MSG, "Try to terminate connection: %d\n",
							vs_ctx->vsessions[i]->dgram_conn->host_id);
					/* Try to close session in friendly way */
					vs_close_dgram_conn(vs_ctx->vsessions[i]->dgram_conn);
				} else if(vs_ctx->vsessions[i]->stream_conn != NULL) {
					/* TODO: close stream connection (strange state) */
				} else {
					/* TODO: destroy session (strange state) */
				}
			}
		}
	}
	if(sig == SIGUSR1){
		vs_reload_user_accounts(local_vs_ctx);
	}
}

/**
 * \brief Load default valued for configuration of verse server to the Verse server
 * context.
 * \param	vs_ctx	The Verse server context.
 */
static void vs_load_default_values(struct VS_CTX *vs_ctx)
{
	int i, j;
	vs_ctx->max_connection_attempts = 10;
	vs_ctx->max_sessions = 10;
	vs_ctx->max_sockets = vs_ctx->max_sessions;
	vs_ctx->flag = SERVER_DEBUG_MODE;		/* SERVER_MULTI_SOCKET_MODE | SERVER_REQUIRE_SECURE_CONNECTION */
	vs_ctx->stream_protocol = TCP;			/* For new connection attempts is used TCP protocol */
	vs_ctx->dgram_protocol = VRS_DGRAM_TP_UDP;	/* For data exchange UDP protocol could be used */
#if OPENSSL_VERSION_NUMBER>=0x10000000
	vs_ctx->security_protocol = VRS_DGRAM_SEC_NONE | VRS_DGRAM_SEC_DTLS;
#else
	vs_ctx->security_protocol = VRS_DGRAM_SEC_NONE;
#endif
	vs_ctx->port = 12345;				/* Port number for listening */

	vs_ctx->port_low = 20000;			/* The lowest port number for client-server connection */
	vs_ctx->port_high = vs_ctx->port_low + vs_ctx->max_sockets;
	/* Initialize list of free ports */
	vs_ctx->port_list = (struct VS_Port*)malloc(sizeof(struct VS_Port)*vs_ctx->max_sockets);
	for(i=vs_ctx->port_low, j=0; i<vs_ctx->port_high; i++, j++) {
		vs_ctx->port_list[j].port_number = vs_ctx->port_low + j;
		vs_ctx->port_list[j].flag = 0;
	}

	vs_ctx->io_ctx.host_addr.ip_ver = IPV6;

	vs_ctx->print_log_level = VRS_PRINT_DEBUG_MSG;
	vs_ctx->log_file = stdout;

	vs_ctx->private_cert_file = strdup("./pki/private.key.pem");
	vs_ctx->ca_cert_file = NULL;
	vs_ctx->public_cert_file = strdup("./pki/certificate.pem");

	vs_ctx->hostname = strdup("localhost");
	vs_ctx->ded = strdup("http://uni-verse.org/verse.ded");

	vs_ctx->auth_type = AUTH_METHOD_CSV_FILE;
	vs_ctx->csv_user_file = strdup("./config/users.csv");

	vs_ctx->default_perm = VRS_PERM_NODE_READ;

	vs_ctx->cc_meth = CC_NONE;		/* "List" of allowed methods of Congestion Control */
	vs_ctx->fc_meth = FC_NONE;		/* "List" of allowed methods of Flow Control */

	vs_ctx->rwin_scale = 7;			/*  Default scale of Flow Control Window */
	vs_ctx->use_krb5 = NO_KERBEROS; /* Not using kerberos as default */
	vs_ctx->krb5_keytab = NULL;		/* Will use defaul keytab */
}

/**
 * \brief Loads configuration of verse server from configuration files
 * \details This function tries to open specified configuration file. When
 * config_file is NULL, then it tries to open default configuration file:
 * /etc/verse. If config_file is NULL and verse is not able to open
 * /etc/verse, then default values are used.
 * \param[out]	vs_ctx		The Verse server context.
 * \param[in]	config_file	The string with relative or absolute path of configuration file.
 */
static void vs_load_config_file(struct VS_CTX *vs_ctx, const char *config_file)
{
	/* Load default values first */
	vs_load_default_values(vs_ctx);

	/* When no configuration file is specified, then load default values */
	if(config_file==NULL) {
		/* Try to open default configuration file */
		vs_read_config_file(vs_ctx, DEFAULT_SERVER_CONFIG_FILE);
	} else {
		/* Try to open configuration file */
		vs_read_config_file(vs_ctx, config_file);
	}
}

/**
 * \brief Load information about all user accounts
 */
static int vs_load_user_accounts(struct VS_CTX *vs_ctx)
{
	int ret = 0;

	switch (vs_ctx->auth_type) {
		case AUTH_METHOD_CSV_FILE:
			ret = vs_load_user_accounts_csv_file(vs_ctx);
			break;
		case AUTH_METHOD_LDAP:
			ret = vs_load_user_accounts_ldap_server(vs_ctx);
			break;
		case AUTH_METHOD_LDAP_LOAD_AT_LOGIN:
			ret = vs_load_saved_ldap_users(vs_ctx);
			break;
		default:
			break;
	}

	return ret;
}

/**
 * \brief This function add fake other users account to the list of user accounts
 */
static int vs_add_other_users_account(struct VS_CTX *vs_ctx)
{
	struct VSUser *other_users;
	int ret = 0;

	other_users = (struct VSUser*)calloc(1, sizeof(struct VSUser));

	if(other_users != NULL) {
		other_users->username = strdup("others");
		other_users->password = NULL;
		other_users->user_id = VRS_OTHER_USERS_UID;
		other_users->realname = strdup("Other Users");
		other_users->fake_user = 1;

		v_list_add_tail(&vs_ctx->users, other_users);

		vs_ctx->other_users = other_users;
		ret = 1;
	}

	return ret;
}

/**
 * \brief This function add fake superuser account to the list of user accounts
 */
static int vs_add_superuser_account(struct VS_CTX *vs_ctx)
{
	struct VSUser *user;
	int ret = 0;

	user = (struct VSUser*)calloc(1, sizeof(struct VSUser));

	if(user != NULL) {
		user->username = strdup("superuser");
		user->password = NULL;
		user->user_id = VRS_SUPER_USER_UID;
		user->realname = strdup("Super User");
		user->fake_user = 1;

		v_list_add_head(&vs_ctx->users, user);

		ret = 1;
	}

	return ret;
}

/**
 * \brief Destroy Verse server context
 *
 * \param[in]	vs_ctx	The Verse server context.
 */
static void vs_destroy_ctx(struct VS_CTX *vs_ctx)
{
	int i;

	/* Free all data shared at verse server */
	vs_node_destroy_branch(vs_ctx, vs_ctx->data.root_node, 0);

	/* Destroy list of connections */
	for (i=0; i<vs_ctx->max_sessions; i++) {
		if(vs_ctx->vsessions[i] != NULL) {
			v_destroy_session(vs_ctx->vsessions[i]);
			free(vs_ctx->vsessions[i]);
			vs_ctx->vsessions[i] = NULL;
		}
	}

	free(vs_ctx->vsessions); vs_ctx->vsessions = NULL;

	free(vs_ctx->port_list); vs_ctx->port_list = NULL;

	free(vs_ctx->io_ctx.buf); vs_ctx->io_ctx.buf = NULL;

	free(vs_ctx->private_cert_file); vs_ctx->private_cert_file = NULL;
	if(vs_ctx->ca_cert_file != NULL) {
		free(vs_ctx->ca_cert_file);
		vs_ctx->ca_cert_file = NULL;
	}
	free(vs_ctx->public_cert_file); vs_ctx->public_cert_file = NULL;

	free(vs_ctx->hostname); vs_ctx->hostname = NULL;

	free(vs_ctx->ded); vs_ctx->ded = NULL;

	if(vs_ctx->csv_user_file != NULL) {
		free(vs_ctx->csv_user_file);
		vs_ctx->csv_user_file = NULL;
	}
	v_list_free(&vs_ctx->users);
}

#if 0
/**
 * \brief Initialize verse server as real UNIX server
 * \param	vs_ctx	The Verse server context.
 */
static int vs_init_server(struct VS_CTX *vs_ctx)
{
	/* TODO: detach from standard file descriptors
	 * detach from all terminals
	 * detach from parent process, etc. */
	return 1;
}
#endif

/**
 * \brief Endless loop, waiting for signals.
 */
static void vs_signal_thread(void *signal_set){
	int s, sig;
	sigset_t *set = (sigset_t *)signal_set;

	while(1){
		if((s = sigwait(set, &sig)) != 0){
			v_print_log(VRS_PRINT_WARNING, "sigwait: %d", s);
		}
	}
}

/**
 * \brief Configure handling of received signals
 * \param	vs_ctx	The Verse server context.
 */
static int vs_config_signal_handling(void)
{
	int s;
	sigset_t set;
	pthread_t thread;
	struct sigaction a_INT;
	struct sigaction a_USR1;

	/* Handle SIGUSR1 signal. The handle_signal function will try to load
	 * new user accounts */
	sigemptyset(&set);
	a_USR1.sa_handler = &vs_handle_signal;
	a_USR1.sa_mask = set;
	a_USR1.sa_flags = 0;
	a_USR1.sa_restorer = NULL;
	sigaction(SIGUSR1, &a_USR1, NULL);

	/* Handle SIGINT signal. The handle_signal function will try to terminate
	 * all connections. */
	sigaddset(&set, SIGUSR1);
	a_INT.sa_handler = &vs_handle_signal;
	a_INT.sa_mask = set;
	a_INT.sa_flags = SA_RESETHAND;
	a_INT.sa_restorer = NULL;
	sigaction(SIGINT, &a_INT, NULL );

	/* Try to create thread which handles the signals */
	if((s = pthread_create(&thread, NULL, vs_signal_thread, (void *) &set)) != 0){
		v_print_log(VRS_PRINT_WARNING, "pthread_create: %d", s);
		return -1;
	}

	sigaddset(&set, SIGINT);

	/* Try to block the signals for this tread and any other new threads */
	if ((s = pthread_sigmask(SIG_BLOCK, &set, NULL )) != 0) {
		v_print_log(VRS_PRINT_WARNING, "pthread_sigmask: %d", s);
	}

	return 1;
}

/**
 * \brief This function sets debug level of verse server.
 */
static int vs_set_debug_level(char *debug_level)
{
	FILE *log_file = v_log_file();

	if( strcmp(debug_level, "debug") == 0) {
		v_init_print_log(VRS_PRINT_DEBUG_MSG, log_file);
		return 1;
	} else if( strcmp(debug_level, "warning") == 0 ) {
		v_init_print_log(VRS_PRINT_WARNING, log_file);
		return 1;
	} else if( strcmp(debug_level, "error") == 0 ) {
		v_init_print_log(VRS_PRINT_ERROR, log_file);
		return 1;
	} else if( strcmp(debug_level, "info") == 0 ) {
		v_init_print_log(VRS_PRINT_INFO, log_file);
		return 1;
	} else if( strcmp(debug_level, "none") == 0 ) {
		v_init_print_log(VRS_PRINT_NONE, log_file);
		return 1;
	} else {
		v_print_log(VRS_PRINT_ERROR, "Unsupported debug level: %s\n", debug_level);
		return 0;
	}
}

/**
 * \brief This function prints usage of Verse server and exit.
 */
static void vs_print_help(char *prog_name)
{
	printf("\n Usage: %s [OPTION...]\n\n", prog_name);
	printf("  Start Verse server\n\n");
	printf("  Options:\n");
	printf("   -h               display this help and exit\n");
	printf("   -c config_file   read configuration from config file\n");
	printf("   -d debug_level   use debug level [none|info|error|warning|debug]\n\n");
}

/**
 * \brief Verse server main function
 * \param	argc	The number of options and arguments.
 * \param	argv	The array of options and arguments.
 * \return Function returns 0, when it is ended successfully and non-zero
 * value, when there some error occurs. This function never reach the end,
 * because it is server.
 */
int main(int argc, char *argv[])
{
	VS_CTX vs_ctx;
	int opt;
	char *config_file=NULL;
	int debug_level_set = 0;

	/* Set up initial state */
	vs_ctx.state = SERVER_STATE_CONF;

	/* Default debug prints of verse server */
	v_init_print_log(VRS_PRINT_WARNING, stdout);

	/* When server received some arguments */
	if(argc>1) {
		while( (opt = getopt(argc, argv, "c:hd:l:")) != -1) {
			switch(opt) {
			case 'c':
				config_file = strdup(optarg);
				break;
			case 'd':
				debug_level_set = vs_set_debug_level(optarg);
				break;
			case 'h':
				vs_print_help(argv[0]);
				exit(EXIT_SUCCESS);
			case ':':
				exit(EXIT_FAILURE);
			case '?':
				exit(EXIT_FAILURE);
			}
		}
	}

	/* Load Verse server configuration file */
	vs_load_config_file(&vs_ctx, config_file);

	/* When debug level wasn't specified as option at command line, then use
	 * configuration from file */
	if(debug_level_set == 1) {
		uint8 log_level = v_log_level();
		v_init_print_log(log_level, vs_ctx.log_file);
	} else {
		v_init_print_log(vs_ctx.print_log_level, vs_ctx.log_file);
	}

	/* Load user accounts and save them in the linked list of verse server
	 * context */
	switch (vs_ctx.auth_type) {
		case AUTH_METHOD_CSV_FILE:
			if(vs_load_user_accounts(&vs_ctx) != 1)
				exit(EXIT_FAILURE);
			break;
		case AUTH_METHOD_PAM:
			/* TODO: read list of supported usernames and their uids somehow */
			exit(EXIT_FAILURE);
		case AUTH_METHOD_LDAP:
			if (vs_load_user_accounts(&vs_ctx) != 1)
				exit(EXIT_FAILURE);
			break;
		case AUTH_METHOD_LDAP_LOAD_AT_LOGIN:
			if (vs_load_user_accounts(&vs_ctx) != 1)
				exit(EXIT_FAILURE);
			break;
		default:
			/* Not supported method */
			exit(EXIT_FAILURE);
	}

	/* Add superuser account to the list of users */
	vs_add_superuser_account(&vs_ctx);

	/* Add fake account for other users to the list of users*/
	vs_add_other_users_account(&vs_ctx);

	/* Initialize data mutex */
	if( pthread_mutex_init(&vs_ctx.data.mutex, NULL) != 0) {
		exit(EXIT_FAILURE);
	}

	/* Create basic node structure of node tree */
	if(vs_nodes_init(&vs_ctx) == -1) {
		exit(EXIT_FAILURE);
	}

	if(vs_ctx.stream_protocol == TCP) {
		/* Initialize Verse server context */
		if(vs_init_stream_ctx(&vs_ctx) == -1) {
			exit(EXIT_FAILURE);
		}
	} else {
		exit(EXIT_FAILURE);
	}

	if(vs_ctx.flag & SERVER_DEBUG_MODE) {
		/* Set up signal handlers (only for debug mode, real server should ignore most of signals) */
		if(vs_config_signal_handling() == -1 ) {
			vs_destroy_ctx(&vs_ctx);
			exit(EXIT_FAILURE);
		}
	} else {
#if 0
		/* Make from verse server real verse application:
		 * - detach from standard file descriptors, terminals, PPID process etc. */
		if(vs_init_server(&vs_ctx) == -1) {
			vs_destroy_ctx(&vs_ctx);
			exit(EXIT_FAILURE);
		}
#endif
	}

	/* Initialize data semaphore */
	if( sem_init(&vs_ctx.data.sem, 0, 0) != 0) {
		exit(EXIT_FAILURE);
	}

	/* Try to create new data thread */
	if(pthread_create(&vs_ctx.data_thread, NULL, vs_data_loop, (void*)&vs_ctx) != 0) {
		v_print_log(VRS_PRINT_ERROR, "pthread_create(): %s\n", strerror(errno));
		vs_destroy_ctx(&vs_ctx);
		exit(EXIT_FAILURE);
	}

	/* Set up pointer to local server CTX -> server server could be terminated
	 * with signal now. */
	local_vs_ctx = &vs_ctx;

	vs_ctx.state = SERVER_STATE_READY;

	if(vs_ctx.stream_protocol == TCP) {
		if(vs_main_stream_loop(&vs_ctx) == -1) {
			vs_destroy_ctx(&vs_ctx);
			exit(EXIT_FAILURE);
		}
	} else {
		vs_destroy_ctx(&vs_ctx);
		exit(EXIT_FAILURE);
	}

	/* Free Verse server context */
	vs_destroy_ctx(&vs_ctx);

	return EXIT_SUCCESS;
}

