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

#ifndef VS_TASK_H_
#define VS_TASK_H_

#include "verse_types.h"

/**
 * Type of tasks define type of data to be sent
 */
typedef enum VSTaskType {
	VS_TT_NODE_PERMS,
	VS_TT_CHILD_NODES,
	VS_TT_TAG_GROUPS,
	VS_TT_LAYERS,
	VS_TT_TAGS,
	VS_TT_LAYER_ITEMS
} VSTaskType;

/**
 * This structure holds information about shared data that has to be sent
 * to the client, when there will be free space in outgoing queue. Task are
 * queued in simple two way linked list.
 */
typedef struct VSTask {
	struct VSTask	*prev, *next;
	enum VSTaskType	type;
	uint32			node_id;
	uint16			entity_id;
	void			*next_item;
} VSTask;

struct VSTask *vs_task_create(enum VSTaskType type,
		uint32 node_id,
		uint16 entity_id,
		void *next_item);

#endif /* VS_TASK_H_ */
