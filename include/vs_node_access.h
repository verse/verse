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

#ifndef VS_NODE_ACCESS_H_
#define VS_NODE_ACCESS_H_

int vs_node_set_perm(struct VSNode *node,
		VSUser *user,
		uint8 permission);

int vs_node_can_write(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct VSNode *node);
int vs_node_can_read(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct VSNode *node);

/* Senders of commands */
int vs_node_send_unlock(struct VSNodeSubscriber *node_subscriber,
		struct VSession *vsession,
		struct VSNode *node);
int vs_node_send_lock(struct VSNodeSubscriber *node_subscriber,
		struct VSession *vsession,
		struct VSNode *node);
int vs_node_send_owner(struct VSNodeSubscriber *node_subscriber,
		struct VSNode *node);
int vs_node_send_perm(struct VSNodeSubscriber *node_subscriber,
		struct VSNode *node,
		struct VSUser *user,
		uint8 perm);

/* Handlers of received commands */
int vs_handle_node_unlock(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_unlock);
int vs_handle_node_lock(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_lock);
int vs_handle_node_owner(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_perm);
int vs_handle_node_perm(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_perm);

#endif /* VS_NODE_ACCESS_H_ */
