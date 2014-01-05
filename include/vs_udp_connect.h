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

#ifndef VS_UDP_CONNECT_H
#define VS_UDP_CONNECT_H

#ifdef WITH_OPENSSL
#include <openssl/ssl.h>
#endif

#include "vs_main.h"

#include "v_connection.h"
#include "v_context.h"

#define COOKIE_SECRET_LENGTH	16

#if OPENSSL_VERSION_NUMBER>=0x10000000
int vs_dtls_generate_cookie(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len);
int vs_dtls_verify_cookie(SSL *ssl, unsigned char *cookie, unsigned int cookie_len);
#endif
void vs_init_dgram_conn(struct VDgramConn *dgram_conn);
void vs_destroy_vconn(struct VDgramConn *dgram_conn);
int vs_handle_packet(struct vContext *C, int vs_STATE_handle_packet(struct vContext*C));
int vs_send_packet(struct vContext *C);
void *vs_main_dgram_loop(void *arg);
void vs_close_dgram_conn(struct VDgramConn *dgram_conn);

#endif /* VS_UDP_CONNECT_H */
