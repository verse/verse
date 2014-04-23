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
#ifndef VS_TAGGROUP_H_
#define VS_TAGGROUP_H_

#include "verse_types.h"

#include "v_list.h"

#include "vs_node.h"

#define FIRST_TAGGROUP_ID		0
#define LAST_TAGGROUP_ID		65534	/* 2^16 - 2 */

#define MAX_TAGGROUPS_COUNT		65534	/* Maximal number of tag groups in one node */

/**
 * Structure storing information about tag group
 */
typedef struct VSTagGroup {
	uint16					id;
	uint16					custom_type;
	/* Tags */
	struct VHashArrayBase	tags;
	uint16					last_tag_id;	/* Last used tag id */
	/* Subscribing */
	struct VListBase		tg_folls;		/* List of clients that know about this tag group */
	struct VListBase		tg_subs;		/* List of clients that are subscribed to this tag group */
	/* Internal stuff */
	uint8					state;
	/* Versing */
	uint32					version;
	uint32					saved_version;
	uint32					crc32;
#ifdef WITH_MONGODB
	bson_oid_t				oid;
#endif
} VSTagGroup;

void vs_taggroup_inc_version(struct VSTagGroup *tg);

struct VSTagGroup *vs_taggroup_find(struct VSNode *node,
		uint16 taggroup_id);

int vs_taggroup_send_create(struct VSNodeSubscriber *node_subscriber,
		struct VSNode *node,
		struct VSTagGroup *tg);
int vs_taggroup_send_destroy(struct VSNode *node,
		struct VSTagGroup *tg);

struct VSTagGroup *vs_taggroup_create(struct VSNode *node,
		uint16 tg_id,
		uint16 custom_type);
int vs_taggroup_destroy(struct VSNode *node,
		struct VSTagGroup *tg);

int vs_taggroup_unsubscribe(struct VSTagGroup *tg,
		struct VSession *vsession);

int vs_node_taggroups_destroy(struct VSNode *node);

int vs_handle_taggroup_create_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd);
int vs_handle_taggroup_destroy_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd);

int vs_handle_taggroup_create(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *taggroup_create_cmd);
int vs_handle_taggroup_destroy(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *taggroup_destroy_cmd);
int vs_handle_taggroup_subscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *taggroup_subscribe);
int vs_handle_taggroup_unsubscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *taggroup_unsubscribe);

#endif /* VS_TAG_H_ */
