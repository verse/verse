/*
 * $Id: vs_entity.h 1348 2012-09-19 20:08:18Z jiri $
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

#ifndef VS_ENTITY_H_
#define VS_ENTITY_H_

#include "verse_types.h"
#include "v_session.h"

#include "vs_node.h"

#define ENTITY_RESERVED	0
#define ENTITY_CREATING	1
#define ENTITY_CREATED	2
#define ENTITY_DELETING	3
#define ENTITY_DELETED	4

/* This structure store information about client, that is subscribed to this
 * entity (tag group, layer, etc.) (node uses own structure) */
typedef struct VSEntitySubscriber {
	struct VSEntitySubscriber	*prev, *next;
	/* Pointer at node subscriber */
	struct VSNodeSubscriber		*node_sub;
} VSEntitySubscriber;

typedef struct VSEntityFollower {
	struct VSEntityFollower		*prev, *next;
	/* Pointer at node subscriber containing session (information about client).
	 * This client received 'create' command of this entity and could receive
	 * 'destroy' command too. */
	struct VSNodeSubscriber		*node_sub;
	/* Per session state to avoid conflicts in data at clients and server */
	uint8						state;
} VSEntityFolower;

#endif /* VS_ENTITY_H_ */
