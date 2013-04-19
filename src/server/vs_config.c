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

		/* Try to load section [Users] */
		user_auth_method = iniparser_getstring(ini_dict, "Users:Method", NULL);
		if(user_auth_method != NULL &&
				strcmp(user_auth_method, "file") == 0)
		{
			char *file_type;

			printf("user_auth_method: %s\n", user_auth_method);

			file_type = iniparser_getstring(ini_dict, "Users:FileType", NULL);

			if(file_type != NULL &&
					strcmp(file_type, "csv") == 0)
			{
				char *csv_file_name;

				printf("file_type: %s\n", file_type);

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

			ldap_version = iniparser_getint(ini_dict, "Users:Version", NULL );

			if (ldap_version != NULL && ldap_version > 0 && ldap_version <= 3) {
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
						vs_ctx->auth_type = AUTH_METHOD_LDAP_LOAD_AT_LOGIN;
						printf("Users will be loaded at login.\n");
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
		certificate_file_name = iniparser_getstring(ini_dict, "Security:Certificate", NULL);
		if(certificate_file_name != NULL) {
			printf("certificate_file_name: %s\n", certificate_file_name);
			vs_ctx->public_cert_file = strdup(certificate_file_name);
		}

		ca_certificate_file_name = iniparser_getstring(ini_dict, "Security:CACertificate", NULL);
		if(ca_certificate_file_name != NULL) {
			printf("ca_certificate_file_name: %s\n", ca_certificate_file_name);
			vs_ctx->ca_cert_file = strdup(ca_certificate_file_name);
		}

		private_key = iniparser_getstring(ini_dict, "Security:PrivateKey", NULL);
		if(private_key != NULL) {
			printf("private_key: %s\n", private_key);
			vs_ctx->private_cert_file = strdup(private_key);
		}

		iniparser_freedict(ini_dict);
	}
}
