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

#ifndef VS_USER_H_
#define VS_USER_H_

#include "verse_types.h"

struct VS_CTX;

#define MIN_USER_ID		1000
#define MAX_USER_ID		(VRS_OTHER_USERS_UID - 1)

/**
 * Structure holding information about verse user.
 */
typedef struct VSUser {
	struct VSUser	*prev, *next;
	uint16			user_id;		/* Unique ID of the user */
	char			*username;		/* Username used for login */
	char			*password;		/* Raw password (unsecure) */
	char			*password_hash;	/* SHA1 hash of password */
	char			*realname;		/* Name displayed to other users */
	char			*ldap_dn;
	uint8			fake_user;		/* Fake user for server and other_users */
} VSUser;

struct VSUser *vs_user_find(struct VS_CTX *vs_ctx, uint16 user_id);
void vs_user_free(struct VSUser *user);
int vs_add_other_users_account(struct VS_CTX *vs_ctx);
int vs_add_superuser_account(struct VS_CTX *vs_ctx);

#endif /* V_USER_H_ */
