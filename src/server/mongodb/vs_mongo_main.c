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
#include <bson.h>
#include <bcon.h>

#include <stdint.h>

#include "vs_main.h"
#include "vs_mongo_main.h"
#include "vs_mongo_node.h"
#include "vs_node.h"
#include "vs_sys_nodes.h"

#include "v_common.h"


/**
 * \brief This function save current server context to Mongo database
 *
 * \param[in] *session	The pointer at verse
 *
 * \return This function returns 1, when context was save. Otherwise it
 * returns 0
 */
int vs_mongo_context_save(struct VS_CTX *vs_ctx)
{
	struct VSNode *node;
	struct VBucket *bucket;

	/* Go through all nodes and save changed node to the database */
	bucket = vs_ctx->data.nodes.lb.first;
	while(bucket != NULL) {
		node = (struct VSNode*)bucket->data;
		if(vs_mongo_node_save(vs_ctx, node) == 0) {
			v_print_log(VRS_PRINT_ERROR,
					"Saving data to MongoDB: %s failed\n",
					vs_ctx->mongodb_db_name);
			return 0;
		}
		bucket = bucket->next;
	}

	v_print_log(VRS_PRINT_DEBUG_MSG,
			"Data saved to MongoDB: %s\n",
			vs_ctx->mongodb_db_name);

	return 1;
}


/**
 * \brief This function loads current context from Mongo database
 *
 * This function has to be called during start of Verse server, when no client
 * is connected yet. This function doesn't restore whole verse server context,
 * but it only loads nodes shared in parent node of scene nodes. Thus system
 * nodes like avatar nodes, user nodes are not loaded. Finally, there has to be
 * basic nodes structure (nodes: 1, 2, 3). The node 3 will be removed with node
 * from MongoDB.
 *
 * \param[in] *vs_ctx The pointer at current verse server context
 */
int vs_mongo_context_load(struct VS_CTX *vs_ctx)
{
	/* Try to find parent of scene nodes in MongoDB */
	if(vs_mongo_node_node_exist(vs_ctx, VRS_SCENE_PARENT_NODE_ID) == 1) {

		/* When node exist, then destroy existing parent node of scene nodes */
		vs_node_destroy_branch(vs_ctx, vs_ctx->data.scene_node, 0);

		/* Try to load node from database and all child nodes */
		vs_ctx->data.scene_node = vs_mongo_node_load_linked(vs_ctx,
				vs_ctx->data.root_node,
				VRS_SCENE_PARENT_NODE_ID,
				-1);

		/* When loading of node failed, then recreate new default parent node
		 * of scene nodes */
		if(vs_ctx->data.scene_node == NULL) {
			v_print_log(VRS_PRINT_ERROR,
					"Restoring data from MongoDB: %s Failed\n",
					vs_ctx->mongodb_db_name);
			vs_ctx->data.scene_node = vs_node_create_scene_parent(vs_ctx);
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Data restored from MongoDB: %s\n",
					vs_ctx->mongodb_db_name);
		}
	}

	return 1;
}


/**
 * \brief This function tries to connect to MongoDB server
 */
int vs_mongo_conn_init(struct VS_CTX *vs_ctx)
{
	int status;

	vs_ctx->mongo_conn = mongo_alloc();
	mongo_init(vs_ctx->mongo_conn);

	/* Set connection timeout */
	mongo_set_op_timeout(vs_ctx->mongo_conn, 1000);

	/* Connect to MongoDB server */
	status = mongo_client(vs_ctx->mongo_conn,
			vs_ctx->mongodb_server, vs_ctx->mongodb_port);

	if( status != MONGO_OK ) {
		switch ( vs_ctx->mongo_conn->err ) {
		case MONGO_CONN_NO_SOCKET:
			v_print_log(VRS_PRINT_ERROR,
					"No MongoDB server %s:%d socket\n",
					vs_ctx->mongodb_server, vs_ctx->mongodb_port);
			break;
		case MONGO_CONN_FAIL:
			v_print_log(VRS_PRINT_ERROR,
					"Connection to MongoDB server %s:%d failed\n",
					vs_ctx->mongodb_server, vs_ctx->mongodb_port);
			break;
		case MONGO_CONN_NOT_MASTER:
			v_print_log(VRS_PRINT_ERROR,
					"MongoDB server %s:%d is not master\n",
					vs_ctx->mongodb_server, vs_ctx->mongodb_port);
			break;
		default:
			v_print_log(VRS_PRINT_ERROR,
					"Failed to connect to MongoDB server %s:%d , error: %d\n",
					vs_ctx->mongodb_server, vs_ctx->mongodb_port,
					vs_ctx->mongo_conn->err);
			break;
		}
		mongo_dealloc(vs_ctx->mongo_conn);
		vs_ctx->mongo_conn = NULL;
		return 0;
	}

	v_print_log(VRS_PRINT_DEBUG_MSG,
			"Connection to MongoDB server %s:%d succeeded\n",
			vs_ctx->mongodb_server, vs_ctx->mongodb_port);

	/* There has to be some db name defined */
	if(vs_ctx->mongodb_db_name == NULL) {
		v_print_log(VRS_PRINT_ERROR,
				"No database name defined\n");
		mongo_dealloc(vs_ctx->mongo_conn);
		vs_ctx->mongo_conn = NULL;
		return 0;
	}

	/* Try to do  when authentication is configured */
	if(vs_ctx->mongodb_user != NULL &&
			vs_ctx->mongodb_pass != NULL)
	{
		status = mongo_cmd_authenticate(vs_ctx->mongo_conn,
				vs_ctx->mongodb_db_name,
				vs_ctx->mongodb_user,
				vs_ctx->mongodb_pass);

		if(status != MONGO_OK) {
			v_print_log(VRS_PRINT_ERROR,
					"Authentication to %s database failed, error: %s\n",
					vs_ctx->mongodb_db_name,
					mongo_get_server_err_string(vs_ctx->mongo_conn));
			mongo_dealloc(vs_ctx->mongo_conn);
			vs_ctx->mongo_conn = NULL;
			return 0;
		}

		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Authentication to %s database succeeded\n",
				vs_ctx->mongodb_db_name);
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"No MongoDB authentication required\n");
	}

	/* Namespace used for storing nodes */
	vs_ctx->mongo_node_ns = (char*)malloc(sizeof(char) * (strlen(vs_ctx->mongodb_db_name) + 6 + 1));
	strcpy(vs_ctx->mongo_node_ns, vs_ctx->mongodb_db_name);
	strcat(vs_ctx->mongo_node_ns, ".nodes");

	/* Namespace used for storing tag groups and tags */
	vs_ctx->mongo_tg_ns = (char*)malloc(sizeof(char) * (strlen(vs_ctx->mongodb_db_name) + 11 + 1));
	strcpy(vs_ctx->mongo_tg_ns, vs_ctx->mongodb_db_name);
	strcat(vs_ctx->mongo_tg_ns, ".tag_groups");

	/* Namespace used for storing layers */
	vs_ctx->mongo_layer_ns = (char*)malloc(sizeof(char) * (strlen(vs_ctx->mongodb_db_name) + 7 + 1));
	strcpy(vs_ctx->mongo_layer_ns, vs_ctx->mongodb_db_name);
	strcat(vs_ctx->mongo_layer_ns, ".layers");

	return 1;
}

/**
 * \brief This function tries to destroy connection with MongoDB server
 */
void vs_mongo_conn_destroy(struct VS_CTX *vs_ctx)
{
	if(vs_ctx->mongo_conn != NULL) {
		mongo_destroy(vs_ctx->mongo_conn);
		vs_ctx->mongo_conn = NULL;
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Connection to MongoDB server %s:%d destroyed\n",
				vs_ctx->mongodb_server, vs_ctx->mongodb_port);
	}
}
