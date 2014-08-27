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

/**
 * \file	vs_task.c
 * \brief	This file include implementation of tasks at verse server.
 *
 * Tasks are used for sending huge data to client, that could not be simple
 * pushed at once to outgoing queue. Huge data has to be sent to client in
 * several bursts. Size of burst is influenced by free space in outgoing
 * queue and free space in outgoing queue is influenced by flow and congestion
 * control.
 */

#include <stdio.h>
#include <stdlib.h>

#include "vs_task.h"

/**
 * \brief This function tries to create new task
 *
 * \param type			The type o task
 * \param node_id		The ID of node with not yet sent data
 * \param entity_id		Optional argument (ID of TagGroup or Layer)
 * \param next_item		The pointer at next not yet sent item. It could be
 * 						child node, tag group, layer, tag or layer item.
 *
 * \return When there is enough free memory, then pointer at task is returned
 */
struct VSTask *vs_task_create(enum VSTaskType type,
		uint32 node_id,
		uint16 entity_id,
		void *next_item)
{
	struct VSTask *task = (struct VSTask*)calloc(1, sizeof(struct VSTask));

	if(task != NULL) {
		task->type = type;
		task->node_id = node_id;
		task->entity_id = entity_id;
		task->next_item = next_item;
	}

	return task;
}
