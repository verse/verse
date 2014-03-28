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
	mongo_cursor cursor;
	bson_iterator iterator;
	bson b;
	int ret, found_server_id = 0;

	/* Init cursor*/
	mongo_cursor_init(&cursor, vs_ctx->mongo_conn, vs_ctx->mongodb_db_name);

	/* Try to find existing "server_id" and update content of document,
	 * when it is different */
	while( mongo_cursor_next(&cursor) == MONGO_OK ) {

		if ( bson_find( &iterator, mongo_cursor_bson( &cursor ), "server_id" )) {
			const char *server_id = bson_iterator_string(&iterator);
			if(strcmp(server_id, vs_ctx->hostname) == 0) {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"Updating data in MongoDB\n");
				found_server_id = 1;
				break;
			}
		}
	}

	mongo_cursor_destroy(&cursor);

	/* Create first version o data at MongoDB */
	if(found_server_id == 0) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Creating first version data\n");

		bson_init(&b);
		bson_append_new_oid(&b, "_id");
		bson_append_string(&b, "server_id", vs_ctx->hostname);

		bson_append_start_array(&b, "nodes");
		/* Go through all nodes and save node to the database */
		bucket = vs_ctx->data.nodes.lb.first;
		while(bucket != NULL) {
			node = (struct VSNode*)bucket->data;
			vs_mongo_node_save(vs_ctx, node);
			bucket = bucket->next;
		}
		bson_append_finish_array(&b);

		bson_finish(&b);

		ret = mongo_insert(vs_ctx->mongo_conn, vs_ctx->mongodb_db_name, &b, NULL);

		if(ret == MONGO_OK) {
			return 1;
		} else {
			v_print_log(VRS_PRINT_ERROR,
					"Unable to write to to MongoDB: %s to collection: %s\n",
					vs_ctx->mongodb_server, vs_ctx->mongodb_db_name);
			return 0;
		}
	} else {
		/* Go through all nodes and save changed node to the database */
		bucket = vs_ctx->data.nodes.lb.first;
		while(bucket != NULL) {
			node = (struct VSNode*)bucket->data;
			vs_mongo_node_save(vs_ctx, node);
			bucket = bucket->next;
		}
	}

	return 1;
}


/**
 * \brief This function loads current context from Mongo database
 *
 * This function has to be called during start of Verse server, when no client
 * is connected yet. This function doesn't restore whole verse server context,
 * but it only loads nodes shared in parent node of scene nodes. Thus system
 * nodes like avatar nodes, user nodes are not loaded. Finaly there has to be
 * basic nodes structure (nodes: 1, 2, 3).
 *
 * \param[in] *vs_ctx The pointer at current verse server context
 */
int vs_mongo_context_load(struct VS_CTX *vs_ctx)
{
	(void)vs_ctx;
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

	/* Try to do  when authentication is configured */
	if(vs_ctx->mongodb_db_name != NULL &&
			vs_ctx->mongodb_user != NULL &&
			vs_ctx->mongodb_pass != NULL)
	{
#if 0
		status = mongo_cmd_add_user(vs_ctx->mongo_conn,
				vs_ctx->mongodb_db_name,
				vs_ctx->mongodb_user,
				vs_ctx->mongodb_pass);
#endif

		status = mongo_cmd_authenticate(vs_ctx->mongo_conn,
				vs_ctx->mongodb_db_name,
				vs_ctx->mongodb_user,
				vs_ctx->mongodb_pass);

		if(status != MONGO_OK) {
			mongo_dealloc(vs_ctx->mongo_conn);
			vs_ctx->mongo_conn = NULL;
			return 0;
		}

		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Authentication to %s database succeeded\n",
				vs_ctx->mongodb_db_name);
	}

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
