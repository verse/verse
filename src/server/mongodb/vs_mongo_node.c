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

#include "v_common.h"

/**
 * \brief This function writes new version of node to Mongo database
 */
static void vs_mongo_node_save_version(bson *b, struct VSNode *node)
{
	bson version;
	bson_init(&version);
	bson_append_int(&version, "version", node->version);
	bson_append_int(&version, "crc32", node->crc32);
	bson_append_int(&version, "owner_id", node->owner->user_id);
	/* TODO: save permissions, child_nodes, tag_groups, layers */
	bson_finish(&version);
	/* TODO: create index from node->version */
	bson_append_bson(b, "0", &version);
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
	bson b;

	/* TODO: save only nodes that are in subtree of scene node */

	if((int)node->saved_version == -1) {
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
		mongo_insert(vs_ctx->mongo_conn, vs_ctx->mongo_node_ns, &b, 0);
		bson_destroy(&b);
	} else if(node->saved_version < node->version) {
		/* TODO: find document and add new version */
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
