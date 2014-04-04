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
#include "vs_node.h"
#include "vs_link.h"

#include "v_common.h"

/**
 * \brief This function writes new version of node to Mongo database
 */
static void vs_mongo_node_save_version(bson *b, struct VSNode *node)
{
	VSNode *child_node;
	VSLink *link;
	VSNodePermission *perm;
	bson bson_version, bson_item;
	char str_num[15];
	int item_id;

	bson_init(&bson_version);
	bson_append_int(&bson_version, "version", node->version);
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
		/* TODO: try to save ObjectId and not node_id */
		bson_append_int(&bson_version, str_num, child_node->id);
		item_id++;
		link = link->next;
	}
	bson_append_finish_array(&bson_version);

	/* TODO: save tag_groups, layers */
	bson_finish(&bson_version);

	bson_append_bson(b, "0", &bson_version);
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
			bson b;
			int ret;

			bson_init(&b);
			bson_append_new_oid(&b, "_id");
			/* Save Node ID and Custom Type of node */
			bson_append_int(&b, "node_id", node->id);
			bson_append_int(&b, "custom_type", node->type);
			/* Create array of versions and save first version */
			bson_append_start_array(&b, "versions");
			vs_mongo_node_save_version(&b, node);
			bson_append_finish_array(&b);
			bson_finish(&b);
			ret = mongo_insert(vs_ctx->mongo_conn, vs_ctx->mongo_node_ns, &b, 0);
			bson_destroy(&b);
			if(ret != MONGO_OK) {
				v_print_log(VRS_PRINT_ERROR,
						"Unable to write node %d to mongo db: %s\n",
						node->id, vs_ctx->mongo_node_ns);
				return 0;
			}
		} else if(node->saved_version < node->version) {
			/* TODO: find document and add new version */
		}
	}

	return 1;
}


/**
 * \brief This function read concrete version of node from Mongo database
 *
 * \param *vs_ctx	The Verse server context
 * \param node_id	The ID of node that is requested from database
 * \param version	The number of node version that is requested from database
 *
 * \return The function returns pointer at found node. When node with requested
 * version or id is not found in databse, then NULL is returned.
 */
struct VSNode *vs_mongo_node_load(struct VS_CTX *vs_ctx,
		uint32 node_id,
		uint32 version)
{
	/*
	 * TODO:
	 *  - Try to find bson object representing node with node_id
	 *  - Try to find version of node
	 *  - Create new VSNode from corresponding bson object and return it
	 */
	(void)vs_ctx;
	(void)node_id;
	(void)version;
	return NULL;
}
