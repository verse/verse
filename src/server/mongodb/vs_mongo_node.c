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

#define MONGO_HAVE_STDINT 1

#include <mongo.h>

#include "vs_main.h"
#include "vs_mongo_main.h"
#include "vs_mongo_node.h"
#include "vs_mongo_taggroup.h"
#include "vs_mongo_layer.h"
#include "vs_node.h"
#include "vs_link.h"
#include "vs_taggroup.h"
#include "vs_layer.h"

#include "v_common.h"

/**
 * \brief This function writes new version of node to Mongo database
 */
static void vs_mongo_node_save_version(struct VS_CTX *vs_ctx,
		bson *bson_node,
		struct VSNode *node)
{
	VSNode *child_node;
	VSLink *link;
	VBucket *bucket;
	VSTagGroup *tg;
	VSLayer *layer;
	VSNodePermission *perm;
	bson bson_version, bson_item;
	char str_num[15];
	int item_id;

	bson_init(&bson_version);
	bson_append_int(&bson_version, "crc32", node->crc32);
	bson_append_int(&bson_version, "owner_id", node->owner->user_id);

	/* Save permissions */
	bson_append_start_array(&bson_version, "permissions");
	perm = node->permissions.first;
	item_id = 0;
	while(perm != NULL) {
		sprintf(str_num, "%d", item_id);
		bson_init(&bson_item);
		bson_append_int(&bson_item, "user_id", perm->user->user_id);
		bson_append_int(&bson_item, "perm", perm->permissions);
		bson_finish(&bson_item);
		bson_append_bson(&bson_version, str_num, &bson_item);
		item_id++;
		perm = perm->next;
	}
	bson_append_finish_array(&bson_version);

	/* Save child nodes */
	bson_append_start_array(&bson_version, "child_nodes");
	link = node->children_links.first;
	item_id = 0;
	while(link != NULL) {
		child_node = link->child;
		sprintf(str_num, "%d", item_id);
		/* TODO: try to save ObjectId and not node ID */
		bson_append_int(&bson_version, str_num, child_node->id);
		item_id++;
		link = link->next;
	}
	bson_append_finish_array(&bson_version);

	/* Save all tag groups */
	bson_append_start_object(&bson_version, "tag_groups");
	bucket = node->tag_groups.lb.first;
	item_id = 0;
	while(bucket != NULL) {
		tg = (struct VSTagGroup*)bucket->data;
		/* Save own tag group to mongo db */
		vs_mongo_taggroup_save(vs_ctx, node, tg);
		sprintf(str_num, "%d", item_id);
		/* TODO: try to save ObjectId and not tag group ID */
		bson_append_int(&bson_version, str_num, tg->id);
		item_id++;
		bucket = bucket->next;
	}
	bson_append_finish_object(&bson_version);

	/* Save all layers */
	bson_append_start_object(&bson_version, "layers");
	bucket = node->layers.lb.first;
	item_id = 0;
	while(bucket != NULL) {
		layer = (struct VSLayer*)bucket->data;
		vs_mongo_layer_save(vs_ctx, node, layer);
		sprintf(str_num, "%d", item_id);
		/* TODO: try to save ObjectId and not layer ID */
		bson_append_int(&bson_version, str_num, tg->id);
		item_id++;
		bucket = bucket->next;
	}
	bson_append_finish_object(&bson_version);

	bson_finish(&bson_version);

	sprintf(str_num, "%d", item_id);
	bson_append_bson(bson_node, str_num, &bson_version);
}

/**
 * \brief This function writes content of node to Mongo database
 *
 * \param vs_ctx	The Verse server context
 * \param node		The node that will be saved to the database
 *
 * \return The function return 1 on success. Otherwise it returns 0.
 */
int vs_mongo_node_save(struct VS_CTX *vs_ctx, struct VSNode *node)
{
	/* Save only node that is in subtree of scene node */
	if(node->flags & VS_NODE_SAVEABLE) {
		/* Create new mongo document for unsaved node */
		if((int)node->saved_version == -1) {
			bson bson_node;
			int ret;

			bson_init(&bson_node);
			bson_append_new_oid(&bson_node, "_id");
			/* Save Node ID and Custom Type of node */
			bson_append_int(&bson_node, "node_id", node->id);
			bson_append_int(&bson_node, "custom_type", node->type);
			bson_append_int(&bson_node, "current_version", node->version);

			/* Create object of versions and save first version */
			bson_append_start_object(&bson_node, "versions");
			vs_mongo_node_save_version(vs_ctx, &bson_node, node);
			bson_append_finish_object(&bson_node);

			bson_finish(&bson_node);
			ret = mongo_insert(vs_ctx->mongo_conn, vs_ctx->mongo_node_ns, &bson_node, 0);
			bson_destroy(&bson_node);
			if(ret != MONGO_OK) {
				v_print_log(VRS_PRINT_ERROR,
						"Unable to write node %d to mongo db: %s\n",
						node->id, vs_ctx->mongo_node_ns);
				return 0;
			}
		} else if(node->saved_version < node->version) {
			/* TODO: find document and add new version */
		}

		node->saved_version = node->version;
	}

	return 1;
}

/**
 * \brief This function returns 1, when node with node_id exist in mongo db.
 * Otherwise it returns 0.
 */
int vs_mongo_node_node_exist(struct VS_CTX *vs_ctx, uint32 node_id)
{
	bson query;
	mongo_cursor cursor;
	int ret = 0;

	bson_init(&query);
	bson_append_int(&query, "node_id", node_id);
	bson_finish(&query);

	mongo_cursor_init(&cursor, vs_ctx->mongo_conn, vs_ctx->mongo_node_ns);
	mongo_cursor_set_query(&cursor, &query);

	/* Node ID should be unique */
	if( mongo_cursor_next(&cursor) == MONGO_OK ) {
		ret = 1;
	}

	bson_destroy(&query);
	mongo_cursor_destroy(&cursor);

	return ret;
}


/**
 * \brief This function read concrete version of node from Mongo database
 *
 * \param *vs_ctx	The Verse server context
 * \param node_id	The ID of node that is requested from database
 * \param version	The number of node version that is requested from database
 *
 * \return The function returns pointer at found node. When node with requested
 * version or id is not found in database, then NULL is returned.
 */
struct VSNode *vs_mongo_node_load(struct VS_CTX *vs_ctx,
		struct VSNode *parent_node,
		uint32 node_id,
		uint32 version)
{
	struct VSNode *node = NULL;
	bson query, bson_versions;
	bson_iterator iterator, version_iter;
	mongo_cursor cursor;
	uint32 custom_type, current_version;
	char str_num[15];
	int ret;

	bson_init(&query);
	bson_append_int(&query, "node_id", node_id);
	bson_finish(&query);

	mongo_cursor_init(&cursor, vs_ctx->mongo_conn, vs_ctx->mongo_node_ns);
	mongo_cursor_set_query(&cursor, &query);

	/* Node ID should be unique */
	if( mongo_cursor_next(&cursor) == MONGO_OK ) {
		const bson *bson_node = mongo_cursor_bson(&cursor);

		/* Try to get custom type of node */
		if( bson_find(&iterator, bson_node, "custom_type") == BSON_INT) {
			custom_type = bson_iterator_int(&iterator);
		}

		/* Try to get current version of node */
		if( bson_find(&iterator, bson_node, "current_version") == BSON_INT) {
			current_version = bson_iterator_int(&iterator);
		}

		/* Try to get versions of node */
		if( bson_find(&iterator, bson_node, "versions") == BSON_ARRAY) {

			/* Try to find required version of node */
			bson_iterator_subiterator(&iterator, &version_iter);
			bson_iterator_subobject_init(&iterator, &bson_versions, 0);

			if((int)version == -1) {
				sprintf(str_num, "%d", current_version);
			} else {
				sprintf(str_num, "%d", version);
			}

			ret = bson_find(&version_iter, &bson_versions, str_num);
			if(ret == BSON_OBJECT) {

			}
		}

		printf(" >>>> Node: %d, custom_type: %d, current_ version: %d\n",
				node_id, custom_type, current_version);

		/* TODO: Create new VSNode from corresponding bson object */
		(void)parent_node;
		(void)custom_type;
	}

	bson_destroy(&query);
	mongo_cursor_destroy(&cursor);

	return node;
}
