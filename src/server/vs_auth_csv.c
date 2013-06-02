/*
 * $Id: vs_auth_csv.c 1360 2012-10-18 20:25:04Z jiri $
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

#include "vs_main.h"
#include "v_context.h"
#include "v_common.h"

/**
 * \brief This function tries to authenticate user. The username is compared
 * with records from CSV file (this file is read after start of the server).
 * When username and password match, then 1 is returned otherwise 0 is
 * returned.
 */
int vs_csv_auth_user(struct vContext *C, const char *username, const char *pass)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VSUser *vsuser;
	int uid = -1;

	vsuser = vs_ctx->users.first;

	while(vsuser) {
		/* Try to find record with this username. (the username has to be
		 * unique). The user could not be fake user (super user and other users) */
		if( vsuser->fake_user != 1) {
			if( strcmp(vsuser->username, username) == 0 ) {
				/* When record with username, then passwords has to be same,
				 * otherwise return 0 */
				if( strcmp(vsuser->password, pass) == 0 ) {
					uid = vsuser->user_id;
					break;
				} else {
					break;
				}
			}
		}
		vsuser = vsuser->next;
	}

	return uid;
}

/**
 * \brief Load user accounts from CSV file
 */
int vs_load_user_accounts_csv_file(VS_CTX *vs_ctx)
{
	int raw = 0, col, prev_comma, next_comma;
	int ret = 0;
	FILE *file;

	if(vs_ctx->csv_user_file != NULL) {
		file = fopen(vs_ctx->csv_user_file, "rt");
		if(file != NULL) {
			struct VSUser *new_user, *user;
			char line[LINE_LEN], *tmp;

			vs_ctx->users.first = NULL;
			vs_ctx->users.last = NULL;

			raw = 0;
			while( fgets(line, LINE_LEN-2, file) != NULL) {
				/* First line has to have following structure:
				 * username,password,UID,real name */
				if(raw==0) {
					if(strncmp(&line[0], "username", 8) != 0) break;
					if(strncmp(&line[9], "password", 8) != 0) break;
					if(strncmp(&line[18], "UID", 3) != 0) break;
					if(strncmp(&line[22], "real name", 9) != 0) break;
				} else {
					new_user = (struct VSUser*)calloc(1, sizeof(struct VSUser));

					/* username */
					prev_comma = next_comma = 0;
					for(col=0; col<LINE_LEN && line[col]!=','; col++) {}
					next_comma = col;
					new_user->username = strndup(&line[prev_comma], next_comma-prev_comma);
					prev_comma = next_comma+1;

					/* password */
					for(col=prev_comma; col<LINE_LEN && line[col]!=','; col++) {}
					next_comma = col;
					new_user->password = strndup(&line[prev_comma], next_comma-prev_comma);
					prev_comma = next_comma+1;

					/* user id */
					for(col=prev_comma; col<LINE_LEN && line[col]!=','; col++) {}
					next_comma = col;
					tmp = strndup(&line[prev_comma], next_comma-prev_comma);
					sscanf(tmp, "%d", (int*)&new_user->user_id);
					free(tmp);
					prev_comma = next_comma+1;

					/* real name */
					for(col=prev_comma; col<LINE_LEN && line[col]!='\n'; col++) {}
					next_comma = col;
					new_user->realname = strndup(&line[prev_comma], next_comma-prev_comma);

					/* This is real user and can login */
					new_user->fake_user = 0;

					/* Check uniqueness of username and user id */
					user = vs_ctx->users.first;
					while(user != NULL) {
						if(user->user_id == new_user->user_id) {
							v_print_log(VRS_PRINT_WARNING, "User %s could not be added to list of user, because user %s has same user ID: %d\n",
									new_user->username, user->username, user->user_id);
							break;
						}
						if( strcmp(user->username, new_user->username) == 0) {
							v_print_log(VRS_PRINT_WARNING, "User %s could not be added to list of user, because user ID: %d has the same name\n",
									new_user->username, user->user_id);
							break;
						}
						user = user->next;
					}

					if(user == NULL) {
						v_list_add_tail(&vs_ctx->users, (void*)new_user);
						v_print_log(VRS_PRINT_DEBUG_MSG, "Added: username: %s, ID: %d, realname: %s\n",
								new_user->username, new_user->user_id, new_user->realname);
					} else {
						free(new_user);
					}

				}
				raw++;
			}
			ret = 1;
			fclose(file);
		}
	}

	v_print_log(VRS_PRINT_DEBUG_MSG, "%d user account loaded from file: %s\n",
			raw-1, vs_ctx->csv_user_file);

	return ret;
}
