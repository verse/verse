/*
 * $Id: vs_config.c 1348 2012-09-19 20:08:18Z jiri $
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
#include <iniparser.h>
#include <string.h>

#include "vs_main.h"
#include "v_common.h"

/**
 * \brief Load configuration from file to the Verse server context.
 *
 * \param	vs_ctx	The Verse server context.
 * \param	file	The pointer at FILE structure;
 */
void vs_read_config_file(struct VS_CTX *vs_ctx, const char *ini_file_name)
{
	dictionary *ini_dict;

	ini_dict = iniparser_load((char*) ini_file_name);

	if(ini_dict != NULL) {
		char *user_auth_method;
		char *certificate_file_name;
		char *ca_certificate_file_name;
		char *private_key;
		char *fc_type;
		int fc_win_scale;
		int in_queue_max_size;
		int out_queue_max_size;
		int tcp_port_number;
		int ws_port_number;
		int udp_low_port_number;
		int udp_high_port_number;
		int max_session_count;
#ifdef WITH_KERBEROS
		char *use_kerberos;
#endif

#ifdef WITH_OPENSSL
		/* Try to get TLS port number */
		tcp_port_number = iniparser_getint(ini_dict, "Global:TLS_port", -1);
		if(tcp_port_number != -1) {
			if(tcp_port_number >= 1024 && tcp_port_number <= 65535) {
				vs_ctx->tcp_port = tcp_port_number;
			} else {
				v_print_log(VRS_PRINT_WARNING, "TLS port: %d out of range: 1024-65535\n",
						tcp_port_number);
			}
		}
#else
		/* Try to get TCP port number */
		tcp_port_number = iniparser_getint(ini_dict, "Global:TCP_port", -1);
		if(tcp_port_number != -1) {
			if(tcp_port_number >= 1024 && tcp_port_number <= 65535) {
				vs_ctx->tcp_port = tcp_port_number;
			} else {
				v_print_log(VRS_PRINT_WARNING, "TCP port: %d out of range: 1024-65535\n",
						tcp_port_number);
			}
		}
#endif

		/* Try to get WebSocket port number */
		ws_port_number = iniparser_getint(ini_dict, "Global:WS_port", -1);
		if(ws_port_number != -1) {
			if(ws_port_number >= 1024 && ws_port_number <= 65535) {
				vs_ctx->ws_port = ws_port_number;
			} else {
				v_print_log(VRS_PRINT_WARNING, "WebSocket port: %d out of range: 1024-65535\n",
						ws_port_number);
			}
		}

		/* Try to get lowest UDP port */
		udp_low_port_number = iniparser_getint(ini_dict, "Global:UDP_port_low", -1);
		if(udp_low_port_number != -1) {
			if(udp_low_port_number >= 49152 && udp_low_port_number <= 65535) {
				vs_ctx->port_low = udp_low_port_number;
			} else {
				v_print_log(VRS_PRINT_WARNING, "UDP port: %d out of range: 49152-65535\n",
						udp_low_port_number);
			}
		}

		udp_high_port_number = iniparser_getint(ini_dict, "Global:UDP_port_high", -1);
		if(udp_high_port_number != -1) {
			if(udp_high_port_number >= 49152 && udp_high_port_number <= 65535) {
				vs_ctx->port_high = udp_high_port_number;
			} else {
				v_print_log(VRS_PRINT_WARNING, "UDP port: %d out of range: 49152-65535\n",
						udp_high_port_number);
			}
		}

		max_session_count = iniparser_getint(ini_dict, "Global:MaxSessionCount", -1);
		if(max_session_count != -1) {
			vs_ctx->max_sessions = max_session_count;
		}

		/* Try to load section [Users] */
		user_auth_method = iniparser_getstring(ini_dict, "Users:Method", NULL);
		if(user_auth_method != NULL &&
				strcmp(user_auth_method, "file") == 0)
		{
			char *file_type;

			v_print_log(VRS_PRINT_DEBUG_MSG, "user_auth_method: %s\n", user_auth_method);

			file_type = iniparser_getstring(ini_dict, "Users:FileType", NULL);

			if(file_type != NULL &&
					strcmp(file_type, "csv") == 0)
			{
				char *csv_file_name;

				v_print_log(VRS_PRINT_DEBUG_MSG, "file_type: %s\n", file_type);

				csv_file_name = iniparser_getstring(ini_dict, "Users:File", NULL);

				if(csv_file_name !=NULL) {
					vs_ctx->auth_type = AUTH_METHOD_CSV_FILE;
					vs_ctx->csv_user_file = strdup(csv_file_name);
					printf("csv_file_name: %s\n", csv_file_name);
				}
			}
		}

		/* Try to load section [Users] for LDAP */
		if (user_auth_method != NULL && strcmp(user_auth_method, "ldap") == 0) {
			int ldap_version = 0;

			printf("user_auth_method: %s\n", user_auth_method);

			ldap_version = iniparser_getint(ini_dict, "Users:Version", 0 );

			if (ldap_version > 0 && ldap_version <= 3) {
				char *ldap_hostname, *ldap_DN, *ldap_pass, *ldap_base,
						*ldap_LOS;

				printf("LDAP version %d\n", ldap_version);
				vs_ctx->ldap_version = ldap_version;

				ldap_hostname = iniparser_getstring(ini_dict, "Users:Hostname",
						NULL );
				ldap_DN = iniparser_getstring(ini_dict, "Users:UserDN",
										NULL );
				ldap_pass = iniparser_getstring(ini_dict, "Users:Pass",
										NULL );
				ldap_base = iniparser_getstring(ini_dict, "Users:Base",
										NULL );
				ldap_LOS = iniparser_getstring(ini_dict, "Users:LoadOnStart",
						NULL );

				if (ldap_hostname != NULL && ldap_DN != NULL
						&& ldap_pass != NULL && ldap_base != NULL ) {
					if (strcmp(ldap_LOS, "yes") == 0) {
						vs_ctx->auth_type = AUTH_METHOD_LDAP;
						printf("Users will be loaded on startup.\n");
					} else {
						char *cache_file;

						cache_file = iniparser_getstring(ini_dict, "Users:File",
								NULL );
						vs_ctx->auth_type = AUTH_METHOD_LDAP_LOAD_AT_LOGIN;
						vs_ctx->created_user_file = strdup(cache_file);
						printf("Users will be loaded at login.\n");
						printf("File for saving user accounts: %s\n",
								cache_file);
					}

					vs_ctx->ldap_hostname = strdup(ldap_hostname);
					vs_ctx->ldap_user = strdup(ldap_DN);
					vs_ctx->ldap_passwd = strdup(ldap_pass);
					vs_ctx->ldap_search_base = strdup(ldap_base);
					printf("LDAP server hostname: %s\n", ldap_hostname);
					printf("LDAP user DN: %s\n", ldap_DN);
					printf("LDAP search base: %s\n", ldap_base);
				}
			}
		}

		/* Try to load section [Security] */
#ifdef WITH_KERBEROS
		use_kerberos = iniparser_getstring(ini_dict, "Security:UseKerberos", NULL);
		if(use_kerberos != NULL && strcmp(use_kerberos, "yes") == 0) {
			char *service_name, *domain_name;
			service_name = iniparser_getstring(ini_dict, "Security:Service", NULL);
			domain_name = iniparser_getstring(ini_dict, "Security:Domain", NULL);
			vs_ctx->use_krb5 = USE_KERBEROS;
			if(domain_name != NULL)
			vs_ctx->domain_name = strdup(domain_name);
			if(service_name != NULL)
			vs_ctx->service_name = strdup(service_name);
			printf("Kerberos will be used\n");
		}
#endif
		certificate_file_name = iniparser_getstring(ini_dict, "Security:Certificate", NULL);
		if(certificate_file_name != NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"certificate_file_name: %s\n", certificate_file_name);
			vs_ctx->public_cert_file = strdup(certificate_file_name);
		}

		/* Certificate of certificate authority */
		ca_certificate_file_name = iniparser_getstring(ini_dict, "Security:CACertificate", NULL);
		if(ca_certificate_file_name != NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"ca_certificate_file_name: %s\n", ca_certificate_file_name);
			vs_ctx->ca_cert_file = strdup(ca_certificate_file_name);
		}

		/* Server private key */
		private_key = iniparser_getstring(ini_dict, "Security:PrivateKey", NULL);
		if(private_key != NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"private_key: %s\n", private_key);
			vs_ctx->private_cert_file = strdup(private_key);
		}

		/* Type of Flow Control */
		fc_type = iniparser_getstring(ini_dict, "FlowControl:Type", NULL);
		if(fc_type != NULL) {
			if(strcmp(fc_type, "tcp_like")==0) {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"flow_control: %s\n", fc_type);
				vs_ctx->fc_meth = FC_TCP_LIKE;
			} else if(strcmp(fc_type, "none")==0) {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"flow_control type: %s\n", fc_type);
				vs_ctx->fc_meth = FC_NONE;
			}
		}

		/* Scale of Flow Control window */
		fc_win_scale = iniparser_getint(ini_dict, "FlowControl:WinScale", -1);
		if(fc_win_scale != -1) {
			if(fc_win_scale >= 0 && fc_win_scale <= 255) {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"flow_control scale: %d\n", fc_win_scale);
				vs_ctx->rwin_scale = fc_win_scale;
			}
		}

		/* Maximal size of incoming queue */
		in_queue_max_size = iniparser_getint(ini_dict, "InQueue:MaxSize", -1);
		if(in_queue_max_size != -1) {
			if(in_queue_max_size > 0 && in_queue_max_size <= INT_MAX) {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"in_queue max size: %d\n", in_queue_max_size);
				vs_ctx->in_queue_max_size = in_queue_max_size;
			}
		}

		/* Maximal size of outgoing queue */
		out_queue_max_size = iniparser_getint(ini_dict, "OutQueue:MaxSize", -1);
		if(out_queue_max_size != -1) {
			if(out_queue_max_size > 0 && out_queue_max_size <= INT_MAX) {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"in_queue max size: %d\n", out_queue_max_size);
				vs_ctx->in_queue_max_size = out_queue_max_size;
			}
		}
		iniparser_freedict(ini_dict);
	}
}
