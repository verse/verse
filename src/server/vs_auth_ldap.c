/*
 * $Id: vs_auth_ldap.c 1360 $
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

#include <string.h>
#include <ldap.h>

#include "vs_main.h"
#include "v_context.h"
#include "v_common.h"

/**
 * \brief Load user accounts from LDAP server
 */
int vs_load_user_accounts_ldap_server(VS_CTX *vs_ctx,
		char *ldap_server_hostname, char *verse_ldap_passwd,
		char *verse_ldap_dn, char *ldap_search_base)
{
	LDAP *ldap;
	int ret = 0;

	/* Initialization of LDAP structure */
	if ((ldap = (LDAP *) ldap_init(ldap_server_hostname, LDAP_PORT)) != NULL ) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "LDAP initialized\n");
		/* Bind to LDAP server */
		if ((ret = ldap_simple_bind_s(ldap, verse_ldap_dn, verse_ldap_passwd))
				> 0) {
			char *search_filter = NULL;
			char **user_cn = NULL;
			char **user_pass = NULL;
			char **user_given_name = NULL;
			char **user_sn = NULL;
			char *user_real_name = NULL;
			char **uid_str = NULL;
			struct VSUser *new_user, *user;
			LDAPMessage *ldap_message = NULL, *ldap_entry = NULL;
			/*int ldap_search_result = 0;*/
			int count = 0;

			v_print_log(VRS_PRINT_DEBUG_MSG, "LDAP binded\n");
			/* Setup search filter */
			search_filter = strdup("objectClass=inetOrgPerson");
			/* Sear for users in LDAP */
			ret = ldap_search_ext_s(ldap, ldap_search_base, LDAP_SCOPE_SUBTREE,
					search_filter, NULL, 0, NULL, NULL, NULL, 0, &ldap_message);
			if (ret != LDAP_SUCCESS) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "error: %d:%s\n", ret,
						ldap_err2string(ret));
			} else {
				v_print_log(VRS_PRINT_DEBUG_MSG, "LDAP search successful\n");
				/* Fetching user info from LDAP message */
				for (ldap_entry = ldap_first_entry(ldap, ldap_message);
						ldap_entry != NULL ;
						ldap_entry = ldap_next_entry(ldap, ldap_entry)) {
					/* username */
					new_user = (struct VSUser*) calloc(1,
							sizeof(struct VSUser));
					user_cn = (char **) ldap_get_values(ldap, ldap_entry, "cn");
					new_user->username = *user_cn;
					/* password */
					user_pass = (char **) ldap_get_values(ldap, ldap_entry,
							"userPassword");
					new_user->password = *user_pass;
					/* realname */
					user_given_name = (char **) ldap_get_values(ldap,
							ldap_entry, "givenName");
					user_sn = (char **) ldap_get_values(ldap, ldap_entry, "sn");
					user_real_name = malloc(
							(strlen(*user_given_name) + strlen(*user_sn) + 2)
									* sizeof(char));
					snprintf(user_real_name,
							strlen(*user_given_name) + strlen(*user_sn) + 2,
							"%s %s", *user_given_name, *user_sn);
					new_user->realname = user_real_name;
					/* uid */
					uid_str = (char **) ldap_get_values(ldap, ldap_entry,
							"uid");
					sscanf(*uid_str, "%d", (int *) &new_user->user_id);
					/* This is real user and can login */
					new_user->fake_user = 0;
					/* Check uniqueness of username and user id */
					user = vs_ctx->users.first;
					while (user != NULL ) {
						if (user->user_id == new_user->user_id) {
							v_print_log(VRS_PRINT_WARNING,
									"User %s could not be added to list of user, because user %s has same user ID: %d\n",
									new_user->username, user->username,
									user->user_id);
							break;
						}
						if (strcmp(user->username, new_user->username) == 0) {
							v_print_log(VRS_PRINT_WARNING,
									"User %s could not be added to list of user, because user ID: %d has the same name\n",
									new_user->username, user->user_id);
							break;
						}
						user = user->next;
					}

					/* Add user or free memory */
					if (user == NULL ) {
						v_list_add_tail(&vs_ctx->users, (void*) new_user);
						v_print_log(VRS_PRINT_DEBUG_MSG,
								"Added: username: %s, ID: %d, realname: %s\n",
								new_user->username, new_user->user_id,
								new_user->realname);
						count++;
					} else {
						free(new_user);
					}
					/* Free memory */
					free(*user_given_name);
					*user_given_name = NULL;
					free(user_given_name);
					user_given_name = NULL;
					free(*user_sn);
					*user_sn = NULL;
					free(user_sn);
					user_sn = NULL;
					free(user_pass);
					user_pass = NULL;
					free(*uid_str);
					*uid_str = NULL;
					free(uid_str);
					uid_str = NULL;
					free(user_cn);
					user_cn = NULL;
				}
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"%d user account loaded from LDAP server: %s, search base: %s\n",
						count, ldap_server_hostname, ldap_search_base);
				/* Free memory */
				ldap_memfree(ldap_entry);
				ldap_entry = NULL;
				ldap_memfree(ldap_message);
				ldap_message = NULL;
				ret = 1;
			}
			/* Free memory */
			free(search_filter);
			search_filter = NULL;
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "error: %d:%s\n", ret,
					ldap_err2string(ret));
		}
	}
	return ret;
}
