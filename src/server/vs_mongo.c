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

#include <stdint.h>

#include "vs_main.h"
#include "vs_mongo.h"

#include "v_common.h"

/**
 * \brief This function tries to connect to MongoDB server
 */
int vs_init_mongo_conn(struct VS_CTX *vs_ctx)
{
	int status;

	vs_ctx->mongo_conn = mongo_alloc();
	mongo_init(vs_ctx->mongo_conn);

	/* TODO: replace hardcoded values with values from config file */
	mongo_set_op_timeout(vs_ctx->mongo_conn, 1000);
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
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Connection to MongoDB server %s:%d succeeded\n",
				vs_ctx->mongodb_server, vs_ctx->mongodb_port);
	}

	return 1;
}

/**
 * \brief This function tries to destroy connection with MongoDB server
 */
void vs_destroy_mongo_conn(struct VS_CTX *vs_ctx)
{
	if(vs_ctx->mongo_conn != NULL) {
		mongo_destroy(vs_ctx->mongo_conn);
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Connection to MongoDB server %s:%d destroyed\n",
				vs_ctx->mongodb_server, vs_ctx->mongodb_port);
	}
}
