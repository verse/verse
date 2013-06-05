/*
 * $Id: vs_user.c 1348 2012-09-19 20:08:18Z jiri $
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

#include "verse_types.h"

#include "vs_main.h"
#include "vs_user.h"

/* TODO: use hashed linked list to store VSUsers to find them quickly */

/**
 * \brief This function will try to find user with user ID equal to user_id
 */
struct VSUser *vs_user_find(struct VS_CTX *vs_ctx, uint16 user_id)
{
	struct VSUser *user;

	/* Find VSUser */
	user = vs_ctx->users.first;

	while(user!=NULL) {
		if(user->user_id == user_id)
			break;
		user = user->next;
	}

	return user;
}

/**
 * \brief This function free allocated data from VSUser structure
 */
void vs_user_free(struct VSUser *user)
{
	free(user->username);
	free(user->realname);
	if(user->password) free(user->password);
}
