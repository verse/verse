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

#ifndef VS_SYS_NODES_H_
#define VS_SYS_NODES_H_

int vs_node_destroy_avatar_node(struct VS_CTX *vs_ctx,
		struct VSession *session);

int vs_node_free_avatar_reference(struct VS_CTX *vs_ctx,
		struct VSession *session);

long int vs_create_avatar_node(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		uint16 user_id);

struct VSNode *vs_create_user_node(struct VS_CTX *vs_ctx,
		struct VSUser *user);

int vs_nodes_init(struct VS_CTX *vs_ctx);

#endif /* VS_SYS_NODES_H_ */
