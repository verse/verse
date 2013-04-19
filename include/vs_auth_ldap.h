/*
 * $Id: vs_auth_cldap.h 1348 $
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

#ifndef VS_AUTH_LDAP_H_
#define VS_AUTH_LDAP_H_

int vs_ldap_auth_user(struct vContext *C, const char *username,
		const char *pass);
int vs_load_user_accounts_ldap_server(VS_CTX *vs_ctx, char *ldap_server_attrs);
int vs_load_new_user_accounts_ldap_server(VS_CTX *vs_ctx);

#endif /* VS_AUTH_LDAP_H_ */
