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

#ifndef VS_NODE_H_
#define VS_NODE_H_

#include "verse_types.h"

#include "v_session.h"

#include "vs_main.h"
#include "vs_user.h"
#include "vs_entity.h"

#define VS_NODE_SAVEABLE	1	/* This flag specify that node should be saved */

typedef struct VSNodeLock {
	struct VSession			*session;
} VSNodeLock;

typedef struct VSNodePermission {
	struct VSNodePermission		*prev, *next;
	struct VSUser				*user;
	uint8						permissions;
} VSNodePermission;

/* This structure store information about client, that is subscribed to this
 * node */
typedef struct VSNodeSubscriber {
	struct VSNodeSubscriber	*prev, *next;
	/* Pointer at session, that is subscribed in this entity */
	struct VSession			*session;
	/* Priority of this Verse client for this node */
	uint8					prio;
} VSNodeSubscriber;

typedef struct VSNode {
	uint32					id;				/* Unique identifier */
	uint16					custom_type;	/* Client defined type */
	/* Access control */
	struct VSUser			*owner;			/* Owner of this object */
	struct VListBase		permissions;	/* List of access permission */
	/* Links */
	struct VSLink			*parent_link;	/* One link to the parent node */
	struct VListBase		children_links;	/* List of links to the children nodes */
	/* TagGroups */
	struct VHashArrayBase	tag_groups;		/* List of tag groups */
	uint16					last_tg_id;		/* Last assigned tag group ID */
	/* Layers */
	struct VHashArrayBase	layers;			/* List of layers */
	uint16					first_free_layer_id;	/* Last assigned layer ID */
	/* Subscribing */
	struct VListBase		node_folls;		/* List of verse sessions that knows about this node */
	struct VListBase		node_subs;		/* List of verse sessions subscribed to data (child links, tag-groups, layers) of this node */
	/* Locking */
	struct VSNodeLock		lock;
	/* Internal staff */
	uint32					level;			/* Level in the node tree */
	uint8					state;			/* Node state */
	uint8					flags;			/* Several flags */
	pthread_mutex_t			mutex;			/* Internal locking of node */
	/* Versing */
	uint32					version;		/* Current version of node */
	uint32					saved_version;	/* Last saved version of node */
	uint32					crc32;			/* CRC32 of node (not supported yet) */
} VSNode;

struct VSNode *vs_node_create_linked(struct VS_CTX *vs_ctx,
		struct VSNode *parent_node,
		struct VSUser *owner,
		uint32 node_id,
		uint16 custom_type);

void vs_node_init(struct VSNode *node);

struct VSNode *vs_node_find(struct VS_CTX *vs_ctx, uint32 node_id);

int vs_node_send_data(struct VSNode *node,
		struct VSNodeSubscriber *node_subscriber);

int vs_node_send_create(struct VSNodeSubscriber *node_subscriber,
		struct VSNode *node,
		struct VSNode *avatar_node);

int vs_handle_node_prio(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_prio);

void vs_node_inc_version(struct VSNode *node);

int vs_node_is_created(struct VSNode *node);

int vs_handle_node_unsubscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_unsubscribe);
int vs_handle_node_subscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_subscribe);
int vs_node_destroy_branch(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		uint8 send);
int vs_handle_node_destroy_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd);
int vs_handle_node_destroy(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_destroy);
int vs_handle_node_create_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_create);
int vs_handle_node_create(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_create);

#endif /* VS_NODE_H_ */
