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

#ifndef VS_LINK_H_
#define VS_LINK_H_

#include <pthread.h>

#include "verse_types.h"

#include "vs_node.h"

/**
 * Link between nodes
 */
typedef struct VSLink {
	struct VSLink *prev, *next;
	struct VSNode *parent;
	struct VSNode *child;
} VSLink;

struct VSLink *vs_link_create(struct VSNode *parent, struct VSNode *child);
int vs_handle_link_change(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_link);

#endif /* VS_LINK_H_ */
