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
#include "vs_node_access.h"

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
		/* Save own tag group to MongoDB */
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

	sprintf(str_num, "%d", node->version);
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
						"Unable to write node %d to MongoDB: %s\n",
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
 * \brief This function returns 1, when node with node_id exist in MongoDB.
 * Otherwise it returns 0.
 */
int vs_mongo_node_node_exist(struct VS_CTX *vs_ctx,
		uint32 node_id)
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
struct VSNode *vs_mongo_node_load_linked(struct VS_CTX *vs_ctx,
		struct VSNode *parent_node,
		uint32 node_id,
		uint32 req_version)
{
	struct VSNode *node = NULL;
	bson query;
	mongo_cursor cursor;
	char str_num[15];

	bson_init(&query);
	bson_append_int(&query, "node_id", node_id);
	bson_finish(&query);

	mongo_cursor_init(&cursor, vs_ctx->mongo_conn, vs_ctx->mongo_node_ns);
	mongo_cursor_set_query(&cursor, &query);

	/* Node ID should be unique */
	if( mongo_cursor_next(&cursor) == MONGO_OK ) {
		const bson *bson_node = mongo_cursor_bson(&cursor);
		bson_iterator node_data_iter;
		uint32 custom_type = -1, current_version = -1;

		/* Try to get custom type of node */
		if( bson_find(&node_data_iter, bson_node, "custom_type") == BSON_INT ) {
			custom_type = bson_iterator_int(&node_data_iter);
		}

		/* Try to get current version of node */
		if( bson_find(&node_data_iter, bson_node, "current_version") == BSON_INT ) {
			current_version = bson_iterator_int(&node_data_iter);
		}

		printf(" >>>> Node: %d, custom_type: %d, current_ version: %d\n",
				node_id, custom_type, current_version);

		/* Try to get versions of node */
		if( bson_find(&node_data_iter, bson_node, "versions") == BSON_OBJECT ) {
			bson bson_versions;
			bson_iterator version_iter;
			uint32 version;

			printf(" >>>> Versions item FOUND\n");

			/* Initialize sub-object of versions */
			bson_iterator_subobject_init(&node_data_iter, &bson_versions, 0);

			if((int)req_version == -1) {
				version = current_version;
			} else {
				version = req_version;
			}

			sprintf(str_num, "%d", version);

			/* Try to find required version of node */
			if( bson_find(&version_iter, &bson_versions, str_num) == BSON_OBJECT ) {
				bson bson_version;
				bson_iterator version_data_iter;
				VSUser *owner;
				uint32 owner_id = -1;

				printf(" >>>> Version: %s FOUND\n", str_num);

				bson_iterator_subobject_init(&version_iter, &bson_version, 0);

				/* Try to get owner of node */
				if( bson_find(&version_data_iter, &bson_version, "owner_id") == BSON_INT ) {
					owner_id = bson_iterator_int(&version_data_iter);
				}

				printf(" >>>> Owner_ID: %d\n", owner_id);

				owner = vs_user_find(vs_ctx, owner_id);

				/* Try to find owner object */
				if(owner != NULL) {

					/* Create node */
					node = vs_node_create_linked(vs_ctx, parent_node, owner, node_id, custom_type);

					if(node != NULL) {

						node->version = version;
						node->saved_version = version;
						node->flags |= VS_NODE_SAVEABLE;

						/* Try to get permission of node */
						if( bson_find(&version_data_iter, &bson_version, "permissions") == BSON_ARRAY ) {
							bson bson_perm;
							bson_iterator perms_iter, perm_data_iter;
							uint32 user_id, perm;
							VSUser *user;

							printf(" >>>> Permissions: %s FOUND\n", str_num);

							bson_iterator_subiterator(&version_data_iter, &perms_iter);

							/* Go through all permissions */
							while( bson_iterator_next(&perms_iter) == BSON_OBJECT ) {
								bson_iterator_subobject_init(&perms_iter, &bson_perm, 0);
								user_id = -1;
								perm = 0;

								if( bson_find(&perm_data_iter, &bson_perm, "user_id") == BSON_INT) {
									user_id = bson_iterator_int(&perm_data_iter);
								}

								if( bson_find(&perm_data_iter, &bson_perm, "perm") == BSON_INT) {
									perm = bson_iterator_int(&perm_data_iter);
								}

								printf(" >>>> Perm, user_id: %d, perm: %d\n",
										user_id, perm);

								/* Try to find object of user */
								user = vs_user_find(vs_ctx, user_id);

								/* Set permission for the user */
								if(user != NULL) {
									vs_node_set_perm(node, user, perm);
								} else {
									v_print_log(VRS_PRINT_WARNING,
											"Verse user %d does not exist\n",
											user_id);
								}

								bson_destroy(&bson_perm);
							}
						}

						/* TODO: load child nodes, tag groups and layers */
					}
				} else {
					v_print_log(VRS_PRINT_WARNING,
							"Verse owner %d does not exist\n",
							owner_id);
				}
				bson_destroy(&bson_version);
			}
			bson_destroy(&bson_versions);
		}
	}

	bson_destroy(&query);
	mongo_cursor_destroy(&cursor);

	return node;
}
