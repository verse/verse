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

#include "verse.h"
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
					int version = vs_ctx->ldap_version;
					/* Setting LDAP version */
					if ((ret = ldap_set_option(ldap, LDAP_OPT_PROTOCOL_VERSION,
							&version)) == LDAP_SUCCESS) {
						struct berval bv;

						/* Open TLS connection */
						ret = ldap_start_tls_s(ldap, NULL, NULL);
						if (ret != LDAP_SUCCESS) {
							v_print_log(VRS_PRINT_DEBUG_MSG,
									"ldap_start_tls_s: %d: %s\n", ret,
									ldap_err2string(ret));
						}

						/* Bind to LDAP server */
						bv.bv_len = strlen(pass);
						bv.bv_val = malloc((bv.bv_len + 1) * sizeof(char));
						strcpy(bv.bv_val, pass);
						ret = ldap_sasl_bind_s(ldap, vsuser->ldap_dn,
								LDAP_SASL_SIMPLE, &bv, NULL,
								NULL, NULL);
						if (ret == LDAP_SUCCESS) {
							/* Login OK */
							uid = vsuser->user_id;
							/* Unbind */
							ldap_unbind_ext(ldap, NULL, NULL);
							break;
						} else {
							/* Login failed */
							v_print_log(VRS_PRINT_DEBUG_MSG,
									"ldap_sasl_bind_s: %d: %s\n", ret,
									ldap_err2string(ret));
							break;
						}
						free(bv.bv_val);
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
 * \brief Save given user's username and UID to csv file
 * \param VS_CTX *vs_ctx	The Verse server context
 * \param VSUser *user		VSUser *user
 */
int vs_save_ldap_user_to_file(VS_CTX *vs_ctx, VSUser *user)
{
	int ret = 0;

	if (vs_ctx->created_user_file != NULL ) {
		FILE *file;
		char line[LINE_LEN], record[LINE_LEN];

		/* Try to open file */
		file = fopen(vs_ctx->created_user_file, "a+");
		if (fgets(line, LINE_LEN - 2, file) == NULL ) {
			/* If file was created now add header */
			fprintf(file, "username,UID\n");
			fprintf(file, "%s,%d\n", user->username, user->user_id);
			v_print_log(VRS_PRINT_DEBUG_MSG, "Added to cache: %s,%d\n",
					user->username, user->user_id);
			ret = 1;
		} else {
			sprintf(record, "%s,%d\n", user->username, user->user_id);
			rewind(file);
			/* Check if user is not already in file */
			while ((fgets(line, LINE_LEN - 2, file) != NULL )&& (ret != 1)){
			if(strncmp(&line[0], &record[0], strlen(&record[0])) == 0) {
				ret = 1;
				break;
			}
		}
			if (ret == 0) {
				/* Add user to file */
				fprintf(file, "%s,%d\n", user->username, user->user_id);
				v_print_log(VRS_PRINT_DEBUG_MSG, "Added to cache: %s,%d\n",
						user->username, user->user_id);
				ret = 1;
			}
		}
		ret = 1;

		fclose(file);
	}

	return ret;
}

/**
 * \brief Parse user data from ldap message.
 * \param VS_CTX *vs_ctx			The Verse server context
 * \param LDAP *ldap				LDAP context
 * \param LDAPMessage *ldap_message	Message with search results
 */
int vs_add_users_from_ldap_message(struct VS_CTX *vs_ctx, LDAP *ldap,
		LDAPMessage *ldap_message)
{
	int count = 0;
	struct berval *user_cn = NULL;
	char *user_dn = NULL;
	struct berval *user_given_name = NULL;
	struct berval *user_sn = NULL;
	char *user_real_name = NULL;
	struct berval *uid_str = NULL;
	struct VSUser *new_user, *user;
	LDAPMessage *ldap_entry = NULL;

	for (ldap_entry = ldap_first_entry(ldap, ldap_message); ldap_entry != NULL ;
			ldap_entry = ldap_next_entry(ldap, ldap_entry)) {
		/* username */
		new_user = (struct VSUser*) calloc(1, sizeof(struct VSUser));
		user_cn = *ldap_get_values_len(ldap, ldap_entry, "uid");
		new_user->username = user_cn->bv_val;
		/* DN */
		user_dn = (char *) ldap_get_dn(ldap, ldap_entry);
		new_user->ldap_dn = user_dn;
		/* realname */
		user_given_name = *ldap_get_values_len(ldap, ldap_entry,
				"givenName");
		user_sn = *ldap_get_values_len(ldap, ldap_entry, "sn");
		user_real_name = malloc(
				(user_given_name->bv_len + user_sn->bv_len + 2)
						* sizeof(char));
		snprintf(user_real_name,
				user_given_name->bv_len + user_sn->bv_len + 2, "%s %s",
				user_given_name->bv_val, user_sn->bv_val);
		new_user->realname = user_real_name;
		/* uid */
		uid_str = *ldap_get_values_len(ldap, ldap_entry, "employeeNumber");
		sscanf(uid_str->bv_val, "%d", (int *) &new_user->user_id);
		/* This is real user and can login */
		new_user->fake_user = 0;
		/* Check uniqueness of username and user id */
		user = vs_ctx->users.first;
		while (user != NULL ) {
			if (user->user_id == new_user->user_id) {
				v_print_log(VRS_PRINT_WARNING,
						"User %s could not be added to list of user, because user %s has same user ID: %d\n",
						new_user->username, user->username, user->user_id);
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
					new_user->username, new_user->user_id, new_user->realname);
			if (vs_ctx->auth_type == AUTH_METHOD_LDAP_LOAD_AT_LOGIN) {
				if (vs_save_ldap_user_to_file(vs_ctx, new_user) != 1) {
					v_print_log(VRS_PRINT_WARNING,
							"Unsuccessful adding to cache file\n");
				}
			}
			count++;
		} else {
			free(new_user);
		}
		/* Free memory */
		free(user_given_name);
		user_given_name = NULL;
		free(user_sn);
		user_sn = NULL;
		free(uid_str);
		uid_str = NULL;
		free(user_cn);
		user_cn = NULL;
	}
	v_print_log(VRS_PRINT_DEBUG_MSG,
			"%d user account loaded from LDAP server: %s, search base: %s\n",
			count, vs_ctx->ldap_hostname, vs_ctx->ldap_search_base);
	/* Free memory */
	ldap_memfree(ldap_entry);
	ldap_entry = NULL;

	return count;
}

/**
 * \brief This function tries to add concrete (by uid) LDAP user to verse.
 * \param VS_CTX *vs_ctx			The Verse server context
 * \param const char *username	User name (uid)
 * \param const char *search_by	Attribute used for search ("uid=")
 */
int vs_ldap_add_concrete_user(struct VS_CTX *vs_ctx, const char *user_name,
		const char *search_by)
{
	int ret = 0;
	LDAP *ldap;

	/* Initialize LDAP */
	if ((ret = ldap_initialize(&ldap, vs_ctx->ldap_hostname)) == LDAP_SUCCESS) {
		/* Initialization OK */
		int version = vs_ctx->ldap_version;
		/* Setting LDAP version. */
		if ((ret = ldap_set_option(ldap, LDAP_OPT_PROTOCOL_VERSION, &version))
				== LDAP_SUCCESS) {
			struct berval bv;

			/* Open TLS connection */
			ret = ldap_start_tls_s(ldap, NULL, NULL);
			if (ret != LDAP_SUCCESS) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "ldap_start_tls_s: %d: %s\n",
						ret, ldap_err2string(ret));
			}

			/* Bind to LDAP server */
			bv.bv_len = strlen(vs_ctx->ldap_passwd);
			bv.bv_val = vs_ctx->ldap_passwd;
			ret = ldap_sasl_bind_s(ldap, vs_ctx->ldap_user, LDAP_SASL_SIMPLE, &bv,
					NULL, NULL, NULL);
			if (ret == LDAP_SUCCESS) {
				/* bind OK */
				char *search_filter;
				int length;
				LDAPMessage *ldap_message = NULL;

				length = strlen(user_name);
				length += strlen(search_by);
				length += 1;
				search_filter = malloc(length * sizeof(char));
				search_filter = strcpy(search_filter, search_by);
				search_filter = strcat(search_filter, user_name);
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
					/* Successful search */

					/* Fetching user info from LDAP message */
					if (vs_add_users_from_ldap_message(vs_ctx, ldap,
							ldap_message) > 0) {
						ret = 1;
					}

					ldap_memfree(ldap_message);
					ldap_message = NULL;
				}
				/* Free memory */
				free(search_filter);
				search_filter = NULL;
				/* Unbind */
				ldap_unbind_ext(ldap, NULL, NULL);
			} else {
				/* Unsuccessful bind */
				v_print_log(VRS_PRINT_DEBUG_MSG, "ldap_sasl_bind_s: %d: %s\n",
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

/**
 * \brief This function tries to authenticate user. The username is compared
 * with records from LDAP server (this file is read after start of the server).
 * Send bind request to LDAP server. If bind is successful, then UID is returned
 * otherwise -1 is returned. If user exists in LDAP but not in verse then creates
 * verse user.
 * \param	vContext 				*C	Verse context
 * \param	const char *username	User name
 * \param	const char *pass		User password
 */
int vs_ldap_auth_and_add_user(struct vContext *C, const char *username,
		const char *pass)
{
	int uid;

	/* Try to do traditional authentication */
	if ((uid = vs_ldap_auth_user(C, username, pass)) == -1) {
		/* User does not exist in Verse, LDAP or bad password */
		if (vs_ldap_add_concrete_user(CTX_server_ctx(C), username, "uid=")
				== 1) {
			uid = vs_ldap_auth_user(C, username, pass);
		}
	}

	return uid;
}

/**
 * \brief Load user accounts from LDAP server
 * \param	VS_CTX *vs_ctx	The Verse server context.
 */
int vs_load_user_accounts_ldap_server(VS_CTX *vs_ctx)
{
	LDAP *ldap;
	int ret = 0;

	/* Initialization of LDAP structure */
	if ((ret = ldap_initialize(&ldap, vs_ctx->ldap_hostname)) == LDAP_SUCCESS) {
		int version;

		version = vs_ctx->ldap_version;
		/* Setting LDAP version. */
		if ((ret = ldap_set_option(ldap, LDAP_OPT_PROTOCOL_VERSION, &version))
				== LDAP_SUCCESS) {
			struct berval bv;

			/* Open TLS connection */
			ret = ldap_start_tls_s(ldap, NULL, NULL);
			if (ret != LDAP_SUCCESS){
				v_print_log(VRS_PRINT_DEBUG_MSG, "ldap_start_tls_s: %d: %s\n", ret,
								ldap_err2string(ret));
			}

			/* Bind to LDAP server */
			bv.bv_len = strlen(vs_ctx->ldap_passwd);
			bv.bv_val = vs_ctx->ldap_passwd;
			ret = ldap_sasl_bind_s(ldap, vs_ctx->ldap_user, LDAP_SASL_SIMPLE,
					&bv, NULL, NULL, NULL);
			if (ret == LDAP_SUCCESS) {
				LDAPMessage *ldap_message = NULL;
				char *search_filter = NULL;

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
					/* Successful search */

					/* Fetching user info from LDAP message */
					vs_add_users_from_ldap_message(vs_ctx, ldap, ldap_message);

					ldap_memfree(ldap_message);
					ldap_message = NULL;
					ret = 1;
				}
				/* Free memory */
				free(search_filter);
				search_filter = NULL;
				/* Unbind */
				ldap_unbind_ext(ldap, NULL, NULL);
			} else {
				/* Unsuccessful bind */
				v_print_log(VRS_PRINT_DEBUG_MSG, "ldap_sasl_bind_s: %d: %s\n",
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

/**
 * \brief Load LDAP user accounts saved in csv file
 * \param VS_CTX *vs_ctx	The Verse server context.
 */
int vs_load_saved_ldap_users(VS_CTX *vs_ctx)
{
	int ret = 1;

	/*vs_ctx->users.first = NULL;
	vs_ctx->users.last = NULL;*/

	v_print_log(VRS_PRINT_DEBUG_MSG, "Loading cached user accounts.\n");

	if (vs_ctx->created_user_file != NULL ) {
		FILE *file;

		/* Try to open cache file */
		file = fopen(vs_ctx->created_user_file, "rt");
		if(file != NULL){
			char line[LINE_LEN], *name;
			int col;

			/* Go trough file */
			while (fgets(line, LINE_LEN - 2, file) != NULL ) {
				if (!((strncmp(&line[0], "username", 8) == 0)
						&& (strncmp(&line[9], "UID", 3) == 0))) {
					/* username */
					for (col = 0; col < LINE_LEN && line[col] != ','; col++) {
					}
					name = strndup(&line[0], col);
					/* Search for user with given username */
					ret = vs_ldap_add_concrete_user(vs_ctx, name, "uid=");
				}

			}
			fclose(file);
		}
	}

	return ret;
}
