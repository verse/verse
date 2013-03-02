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
 * \brief This function tries to authenticate user. The username is compared
 * with records from LDAP server (this file is read after start of the server).
 * Send bind request to LDAP server. If bind is successful, then UID is returned
 * otherwise -1 is returned.
 * \param	vContext 				*C	Verse context
 * \param	const char *username	User name
 * \param	const char *pass		User password
 */
int vs_ldap_auth_user(struct vContext *C, const char *username,
		const char *pass)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VSUser *vsuser;
	int uid = -1;
	int ret;
	LDAP *ldap;

	vsuser = vs_ctx->users.first;

	while (vsuser) {
		/* Try to find record with this username. (the username has to be
		 * unique). The user could not be fake user (super user and other users) */
		if (vsuser->fake_user != 1) {
			if (strcmp(vsuser->username, username) == 0) {
				/* When record with username, then try LDAP bind with password */
				if ((ret = ldap_initialize(&ldap, vs_ctx->ldap_hostname))
						== LDAP_SUCCESS) {
					/* Initialization OK */
					int version = LDAP_VERSION3;
					/* Setting version to LDAP v3. */
					if ((ret = ldap_set_option(ldap, LDAP_OPT_PROTOCOL_VERSION,
							&version)) == LDAP_SUCCESS) {
						/* Bind to LDAP server */
						if ((ret = ldap_simple_bind_s(ldap, vsuser->ldap_dn,
								pass)) == LDAP_SUCCESS) {
							/* Login OK */
							uid = vsuser->user_id;
							/* Unbind */
							ldap_unbind_s(ldap);
							break;
						} else {
							/* Login failed */
							break;
						}
					} else {
						/* Setting version failed */
						v_print_log(VRS_PRINT_DEBUG_MSG,
								"ldap_set_option: %d: %s\n", ret,
								ldap_err2string(ret));
					}
				} else {
					/* Initialization failed */
					v_print_log(VRS_PRINT_DEBUG_MSG,
							"ldap_initialize: %d: %s\n", ret,
							ldap_err2string(ret));
				}
			}
		}
		vsuser = vsuser->next;
	}

	return uid;
}

/**
 * \brief Load user accounts from LdAP server
 * \param	VS_CTX *vs_ctx				The Verse server context.
 * \param	char *ldap_server_attrs 	LDAP attributes
 */
int vs_load_user_accounts_ldap_server(VS_CTX *vs_ctx, char *ldap_server_attrs,
		int len)
{
	LDAP *ldap;
	int ret = 0;
	char *ldap_server_hostname = NULL;
	char *verse_ldap_dn = NULL;
	char *verse_ldap_passwd = NULL;
	char *ldap_search_base = NULL;
	int col, prev_colon, next_colon;

	/* Parse LDAP server properties */
	prev_colon = next_colon = 0;
	for (col = 0; col < len && ldap_server_attrs[col] != '%'; col++)
		;
	next_colon = col;
	ldap_server_hostname = strndup(&ldap_server_attrs[prev_colon],
			next_colon - prev_colon);
	vs_ctx->ldap_hostname = ldap_server_hostname;
	prev_colon = next_colon + 1;

	for (col = prev_colon; col < len && ldap_server_attrs[col] != '%'; col++)
		;
	next_colon = col;
	verse_ldap_dn = strndup(&ldap_server_attrs[prev_colon],
			next_colon - prev_colon);
	vs_ctx->ldap_user = verse_ldap_dn;
	prev_colon = next_colon + 1;

	for (col = prev_colon; col < len && ldap_server_attrs[col] != '%'; col++)
		;
	next_colon = col;
	verse_ldap_passwd = strndup(&ldap_server_attrs[prev_colon],
			next_colon - prev_colon);
	vs_ctx->ldap_passwd = verse_ldap_passwd;
	prev_colon = next_colon + 1;

	for (col = prev_colon; col < len && ldap_server_attrs[col] != '%'; col++)
		;
	next_colon = col;
	ldap_search_base = strndup(&ldap_server_attrs[prev_colon],
			next_colon - prev_colon);
	vs_ctx->ldap_search_base = ldap_search_base;
	v_print_log(VRS_PRINT_DEBUG_MSG, "Initialiying LDAP\n");

	/* Initialization of LDAP structure */
	if ((ret = ldap_initialize(&ldap, vs_ctx->ldap_hostname)) == LDAP_SUCCESS) {
		int version;

		v_print_log(VRS_PRINT_DEBUG_MSG, "LDAP initialized\n");
		version = LDAP_VERSION3;
		/* Setting version to LDAP v3. */
		if ((ret = ldap_set_option(ldap, LDAP_OPT_PROTOCOL_VERSION, &version))
				== LDAP_SUCCESS) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "LDAP version %d\n", version);
			/* Bind to LDAP server */
			if ((ret = ldap_simple_bind_s(ldap, vs_ctx->ldap_user,
					vs_ctx->ldap_passwd)) == LDAP_SUCCESS) {
				char *search_filter = NULL;
				char **user_cn = NULL;
				char *user_dn = NULL;
				char **user_given_name = NULL;
				char **user_sn = NULL;
				char *user_real_name = NULL;
				char **uid_str = NULL;
				struct VSUser *new_user, *user;
				LDAPMessage *ldap_message = NULL, *ldap_entry = NULL;
				int count = 0;

				v_print_log(VRS_PRINT_DEBUG_MSG, "LDAP binded\n");
				/* Setup search filter */
				search_filter = strdup("objectClass=inetOrgPerson");
				/* Sear for users in LDAP */
				ret = ldap_search_ext_s(ldap, vs_ctx->ldap_search_base,
						LDAP_SCOPE_SUBTREE, search_filter, NULL, 0, NULL, NULL,
						NULL, 0, &ldap_message);
				/* Unsuccessful search */
				if (ret != LDAP_SUCCESS) {
					v_print_log(VRS_PRINT_DEBUG_MSG,
							"ldap_search_ext_s: %d: %s\n", ret,
							ldap_err2string(ret));
				} else {
					/* Successsful search */
					v_print_log(VRS_PRINT_DEBUG_MSG,
							"LDAP search successful\n");
					vs_ctx->users.first = NULL;
					vs_ctx->users.last = NULL;
					/* Fetching user info from LDAP message */
					for (ldap_entry = ldap_first_entry(ldap, ldap_message);
							ldap_entry != NULL ;
							ldap_entry = ldap_next_entry(ldap, ldap_entry)) {
						/* username */
						new_user = (struct VSUser*) calloc(1,
								sizeof(struct VSUser));
						user_cn = (char **) ldap_get_values(ldap, ldap_entry,
								"cn");
						new_user->username = *user_cn;
						/* DN */
						user_dn = (char *) ldap_get_dn(ldap, ldap_entry);
						new_user->ldap_dn = user_dn;
						/* realname */
						user_given_name = (char **) ldap_get_values(ldap,
								ldap_entry, "givenName");
						user_sn = (char **) ldap_get_values(ldap, ldap_entry,
								"sn");
						user_real_name =
								malloc(
										(strlen(*user_given_name)
												+ strlen(*user_sn) + 2)
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
							if (strcmp(user->username, new_user->username)
									== 0) {
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
				/* Unbind */
				ldap_unbind_s(ldap);
			} else {
				/* Unsuccessful bind */
				v_print_log(VRS_PRINT_DEBUG_MSG, "ldap_simple_bind_s: %d: %s\n",
						ret, ldap_err2string(ret));
			}
		} else {
			/* Setting version failed */
			v_print_log(VRS_PRINT_DEBUG_MSG, "ldap_set_option: %d: %s\n", ret,
					ldap_err2string(ret));
		}
	} else {
		/* Initialization failed */
		v_print_log(VRS_PRINT_DEBUG_MSG, "ldap_initialize: %d: %s\n", ret,
				ldap_err2string(ret));
	}
	return ret;
}
