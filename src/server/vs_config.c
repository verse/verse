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

		/* Try to load section [Security] */
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
