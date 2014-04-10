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
 */
static void vs_mongo_layer_save_version(bson *bson_layer,
		struct VSLayer *layer)
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
		bson_append_int(&bson_version, str_num, child_layer->id);
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

	sprintf(str_num, "%d", layer->version);
	bson_append_bson(bson_layer, str_num, &bson_version);
}

/**
 * \brief This function tries to save layer into the MongoDB
 */
int vs_mongo_layer_save(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		struct VSLayer *layer)
{
	bson bson_layer;
	int ret;

	if((int)layer->saved_version == -1) {
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
		vs_mongo_layer_save_version(&bson_layer, layer);
		bson_append_finish_object(&bson_layer);

		bson_finish(&bson_layer);

		ret = mongo_insert(vs_ctx->mongo_conn, vs_ctx->mongo_layer_ns, &bson_layer, NULL);
		bson_destroy(&bson_layer);

		if(ret != MONGO_OK) {
			v_print_log(VRS_PRINT_ERROR,
					"Unable to write layer %d of node %d to MongoDB: %s\n",
					layer->id, node->id, vs_ctx->mongo_layer_ns);
			return 0;
		}
	} else {
		/* TODO: update item in database */
	}

	layer->saved_version = layer->version;

	return 1;
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
