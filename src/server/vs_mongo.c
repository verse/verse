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

#include "vs_mongo.h"

#include <mongo.h>
#include <stdint.h>

#include "vs_main.h"
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
	status = mongo_client(vs_ctx->mongo_conn, "127.0.0.1", 27017);

	if( status != MONGO_OK ) {
		switch ( vs_ctx->mongo_conn->err ) {
		case MONGO_CONN_NO_SOCKET:
			v_print_log(VRS_PRINT_ERROR,
					"No MongoDB server socket\n");
			break;
		case MONGO_CONN_FAIL:
			v_print_log(VRS_PRINT_ERROR,
					"Connection to MongoDB server failed\n");
			break;
		case MONGO_CONN_NOT_MASTER:
			v_print_log(VRS_PRINT_ERROR,
					"MongoDB server is not master\n");
			break;
		default:
			break;
		}
		mongo_dealloc(vs_ctx->mongo_conn);
		vs_ctx->mongo_conn = NULL;
		return 0;
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Connection to MongoDB server succeeded\n");
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
				"Connection to MongoDB server destroyed\n");
	}
}
