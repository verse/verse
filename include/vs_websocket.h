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

#ifndef VS_WEBSOCKET_H_
#define VS_WEBSOCKET_H_

/**
 *
 */
#define BASE64_ENCODE_RAW_LENGTH(length) ((((length) + 2)/3)*4)

/**
 * Constant from RFC 6455 that is used for concatenate to key
 * proposed by WebSocket client and result string is used for computing
 * SHA-1 hash. This hash is send as response to WebSocket client.
 */
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/**
 * The length of client key has to be 24 bytes
 */
#define WS_CLIENT_KEY_LEN 24

/**
 * The name of supported WebSocket protocol that is negotiated during
 * WebSocket handshake.
 */
#define WEB_SOCKET_PROTO_NAME "v1.verse.tul.cz"


struct vContext;

int vs_STREAM_OPEN_ws_loop(struct vContext *C);
void *vs_websocket_loop(void *arg);


#endif
