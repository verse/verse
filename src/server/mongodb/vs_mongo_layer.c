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
#include "vs_mongo_layer.h"
#include "vs_node.h"
#include "vs_layer.h"

#include "v_common.h"


/**
 * \brief This function tries to save current version of layer to the database
 *
 * TODO: use GridFS for storing large layers
 */
static void vs_mongo_layer_save_version(bson *bson_layer,
		struct VSLayer *layer,
		uint32 version)
{
	bson bson_version;
	bson bson_value;
	VSLayer *child_layer;
	VSLayerValue *layer_value;
	VBucket *bucket;
	char str_num[15];
	int item_id, data_id;

	bson_init(&bson_version);

	bson_append_int(&bson_version, "crc32", layer->crc32);

	bson_append_start_array(&bson_version, "child_layers");
	child_layer = layer->child_layers.first;
	item_id = 0;
	while(child_layer != NULL) {
		sprintf(str_num, "%d", item_id);
		bson_append_oid(&bson_version, str_num, &child_layer->oid);
		child_layer = child_layer->next;
	}
	bson_append_finish_array(&bson_version);

	bson_append_start_object(&bson_version, "values");
	bucket = layer->values.lb.first;
	while(bucket != NULL) {
		layer_value = (struct VSLayerValue*)bucket->data;
		bson_init(&bson_value);
		bson_append_start_array(&bson_value, "value");
		switch(layer->data_type) {
		case VRS_VALUE_TYPE_UINT8:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_value, str_num, ((uint8*)layer_value->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT16:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_value, str_num, ((uint16*)layer_value->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT32:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_value, str_num, ((uint32*)layer_value->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT64:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_value, str_num, ((uint64*)layer_value->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_REAL16:
			/* TODO */
			break;
		case VRS_VALUE_TYPE_REAL32:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_double(&bson_value, str_num, ((float*)layer_value->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_REAL64:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_double(&bson_value, str_num, ((double*)layer_value->value)[data_id]);
			}
			break;
		}
		bson_append_finish_array(&bson_value);
		bson_finish(&bson_value);
		sprintf(str_num, "%d", layer_value->id);
		bson_append_bson(&bson_version, str_num, &bson_value);
		bucket = bucket->next;
	}
	bson_append_finish_object(&bson_version);

	bson_finish(&bson_version);

	sprintf(str_num, "%d", version);
	bson_append_bson(bson_layer, str_num, &bson_version);
}

/**
 * \brief This function tries to update layer stored in MongoDB
 */
int vs_mongo_layer_update(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		struct VSLayer *layer)
{
	bson cond, op;
	bson bson_version;
	int ret;

	/* TODO: delete old version, when there is too much versions:
	int old_saved_version = layer->saved_version;
	*/

	bson_init(&cond);
	{
		bson_append_oid(&cond, "_id", &layer->oid);
		/* To be sure that right layer will be updated */
		bson_append_int(&cond, "node_id", node->id);
		bson_append_int(&cond, "layer_id", layer->id);
	}
	bson_finish(&cond);

	bson_init(&op);
	{
		/* Update item current_version in document */
		bson_append_start_object(&op, "$set");
		{
			bson_append_int(&op, "current_version", layer->version);
		}
		bson_append_finish_object(&op);
		/* Create new bson object representing current version and add it to
		 * the object versions */
		bson_append_start_object(&op, "$set");
		{
			bson_init(&bson_version);
			{
				vs_mongo_layer_save_version(&bson_version, layer, UINT32_MAX);
			}
			bson_finish(&bson_version);
			bson_append_bson(&op, "versions", &bson_version);
		}
		bson_append_finish_object(&op);
	}
	bson_finish(&op);

	ret = mongo_update(vs_ctx->mongo_conn, vs_ctx->mongo_tg_ns, &cond, &op,
			MONGO_UPDATE_BASIC, 0);

	bson_destroy(&bson_version);
	bson_destroy(&cond);
	bson_destroy(&op);

	if(ret != MONGO_OK) {
		v_print_log(VRS_PRINT_ERROR,
				"Unable to update layer %d to MongoDB: %s, error: %s\n",
				layer->id, vs_ctx->mongo_tg_ns,
				mongo_get_server_err_string(vs_ctx->mongo_conn));
		return 0;
	}

	return 1;
}

/**
 * \brief This function tries to save new layer to MongoDB
 */
int vs_mongo_layer_add_new(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		struct VSLayer *layer)
{
	bson bson_layer;
	int ret;

	bson_init(&bson_layer);

	bson_oid_gen(&layer->oid);
	bson_append_oid(&bson_layer, "_id", &layer->oid);
	bson_append_int(&bson_layer, "node_id", node->id);
	bson_append_int(&bson_layer, "layer_id", layer->id);
	bson_append_int(&bson_layer, "custom_type", layer->type);
	bson_append_int(&bson_layer, "data_type", layer->data_type);
	bson_append_int(&bson_layer, "vec_size", layer->num_vec_comp);
	bson_append_int(&bson_layer, "current_version", layer->version);

	bson_append_start_object(&bson_layer, "versions");
	vs_mongo_layer_save_version(&bson_layer, layer, UINT32_MAX);
	bson_append_finish_object(&bson_layer);

	bson_finish(&bson_layer);

	ret = mongo_insert(vs_ctx->mongo_conn, vs_ctx->mongo_layer_ns, &bson_layer, NULL);
	bson_destroy(&bson_layer);

	if(ret != MONGO_OK) {
		v_print_log(VRS_PRINT_ERROR,
				"Unable to write layer %d of node %d to MongoDB: %s, error: %s\n",
				layer->id, node->id, vs_ctx->mongo_layer_ns,
				mongo_get_server_err_string(vs_ctx->mongo_conn));
		return 0;
	}

	return 1;
}

/**
 * \brief This function tries to save layer into the MongoDB
 */
int vs_mongo_layer_save(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		struct VSLayer *layer)
{
	int ret = 1;

	if((int)layer->saved_version == -1) {
		/* Save new layer to MongoDB */
		ret = vs_mongo_layer_add_new(vs_ctx, node, layer);
	} else {
		/* Update item in database */
		ret = vs_mongo_layer_update(vs_ctx, node, layer);
	}

	if(ret == 1) {
		layer->saved_version = layer->version;
	}

	return ret;
}

/**
 * \brief This function tries to load layer from the MongoDB
 */
struct VSLayer *vs_mongo_layer_load(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		uint16 layer_id,
		uint32 version)
{
	(void)vs_ctx;
	(void)node;
	(void)layer_id;
	(void)version;

	return NULL;
}
