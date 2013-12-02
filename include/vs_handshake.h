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

int vs_user_auth(struct vContext *C, const char *username, const char *data);
int vs_TLS_handshake(struct vContext *C);
int vs_TLS_teardown(struct vContext *C);
int vs_kerberos_auth(struct vContext *C, char **u_name);
void vs_CLOSING(struct vContext *C);
int vs_STREAM_OPEN_tcp_loop(struct vContext *C);
int vs_NEGOTIATE_newhost_loop(struct vContext *C);
int vs_NEGOTIATE_cookie_ded_loop(struct vContext *C);
int vs_RESPOND_userauth_loop(struct vContext *C);
int vs_RESPOND_methods_loop(struct vContext *C);
int vs_handle_handshake(struct vContext *C, char *u_name);
