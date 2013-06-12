/*
 * $Id$
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

#ifndef VS_TAG_H_
#define VS_TAG_H_

#include "verse_types.h"
#include "v_list.h"

#include "vs_node.h"
#include "vs_taggroup.h"

#define FIRST_TAG_ID			0
#define LAST_TAG_ID				65534	/* 2^16 - 2 */

#define RESERVED_TAG_ID			0xFFFF	/* This ID sends client in TagCreate command */

#define MAX_TAGS_COUNT			65534	/* Maximal number of tags in one tag group */

#define TAG_INITIALIZED			1
#define TAG_UNINITIALIZED		2

/**
 * Structure storing information about one tag
 */
typedef struct VSTag {
	uint16				id;
	uint8				data_type;	/* (u)int(8/16/32/64), real(16/32/64), string8 */
	uint8				count;		/* The count of values */
	uint8				flag;		/* Initialized/unitialized */
	uint16				type;		/* Client specified type */
	void				*value;		/* Pointer at own value */
	/* Subscribing */
	struct VListBase	tag_folls;	/* List of clients that knows about
								   	   this node and are auto-subscribed
									   to this tag */
	uint8				state;
} VSTag;

void vs_tag_send_set(struct VSession *vsession,
		uint8 prio,
		struct VSNode *node,
		struct VSTagGroup *tg,
		struct VSTag *tag);

int vs_tag_send_destroy(struct VSNode *node,
		struct VSTagGroup *tg,
		struct VSTag *tag);

void vs_tag_init(struct VSTag *tag);

struct VSTag *vs_tag_create(struct VSNode *node,
		struct VSTagGroup *tg,
		char *name,
		uint8 type);
int vs_tag_destroy(struct VSTagGroup *tg,
		struct VSTag *tag);

int vs_handle_tag_create_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd);

int vs_handle_tag_create(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *tag_create);

int vs_tag_send_create(struct VSEntitySubscriber *tg_subscriber,
		struct VSNode *node,
		struct VSTagGroup *tg,
		struct VSTag *tag);

int vs_handle_tag_destroy_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *tag_destroy);

int vs_handle_tag_destroy(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *tag_destroy);

int vs_handle_tag_set(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *tag_set_int8,
		uint8 data_type,
		uint8 count);

#endif /* VS_TAG_H_ */
