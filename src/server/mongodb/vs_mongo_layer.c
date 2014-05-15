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
static void vs_mongo_layer_save_version(struct VSLayer *layer,
		bson *bson_layer,
		uint32 version)
{
	bson bson_version;
	VSLayerValue *layer_value;
	VBucket *bucket;
	char str_num[15];
	int data_id;

	bson_init(&bson_version);

	bson_append_int(&bson_version, "crc32", layer->crc32);

	bson_append_start_object(&bson_version, "values");
	bucket = layer->values.lb.first;
	while(bucket != NULL) {
		layer_value = (struct VSLayerValue*)bucket->data;
		sprintf(str_num, "%d", layer_value->id);
		bson_append_start_array(&bson_version, str_num);
		switch(layer->data_type) {
		case VRS_VALUE_TYPE_UINT8:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_version, str_num, ((uint8*)layer_value->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT16:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_version, str_num, ((uint16*)layer_value->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT32:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_version, str_num, ((uint32*)layer_value->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT64:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_version, str_num, ((uint64*)layer_value->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_REAL16:
			/* TODO */
			break;
		case VRS_VALUE_TYPE_REAL32:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_double(&bson_version, str_num, ((float*)layer_value->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_REAL64:
			for(data_id = 0; data_id < layer->num_vec_comp; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_double(&bson_version, str_num, ((double*)layer_value->value)[data_id]);
			}
			break;
		}
		bson_append_finish_array(&bson_version);

		bucket = bucket->next;
	}
	bson_append_finish_object(&bson_version);

	bson_finish(&bson_version);

	sprintf(str_num, "%u", version);
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
				vs_mongo_layer_save_version(layer, &bson_version, UINT32_MAX);
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
	bson_append_int(&bson_layer, "custom_type", layer->custom_type);
	bson_append_int(&bson_layer, "data_type", layer->data_type);
	bson_append_int(&bson_layer, "vec_size", layer->num_vec_comp);
	bson_append_int(&bson_layer, "current_version", layer->version);

	if(layer->parent != NULL) {
		bson_append_int(&bson_layer, "parent_layer_id", layer->parent->id);
	}

	bson_append_start_object(&bson_layer, "versions");
	vs_mongo_layer_save_version(layer, &bson_layer, UINT32_MAX);
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
 * \brief This function tries to load data of layer from MongoDB
 */
static void vs_mongo_layer_load_data(struct VSNode *node,
		struct VSLayer *layer,
		bson *bson_version)
{
	bson_iterator version_data_iter;

	(void)node;
	(void)layer;

	/* Try to get values of layer */
	if( bson_find(&version_data_iter, bson_version, "values") == BSON_OBJECT ) {
		struct VSLayerValue *item;
		bson_iterator items_iter, values_iter;
		const char *key;
		uint8 val_uint8;
/*		uint16 val_uint16;
		uint32 val_uint32;
		uint64 val_uint64;
		real32 val_real32;
		real64 val_real64;*/
		uint32 item_id;
		int item_data_size = vs_layer_data_size(layer);
		int value_id;

		bson_iterator_subiterator(&version_data_iter, &items_iter);

		while( bson_iterator_next(&items_iter) == BSON_ARRAY ) {
			key = bson_iterator_key(&items_iter);
			sscanf(key, "%u", &item_id);

			bson_iterator_subiterator(&items_iter, &values_iter);

			item = (struct VSLayerValue*)calloc(1, sizeof(struct VSLayerValue));
			item->value = (void*)calloc(layer->num_vec_comp, item_data_size);

			value_id = 0;

			while( bson_iterator_next(&values_iter) != BSON_EOO &&
					value_id < layer->num_vec_comp)
			{
				switch(layer->data_type) {
				case VRS_VALUE_TYPE_UINT8:
					val_uint8 = (uint8)bson_iterator_int(&values_iter);
					((uint8*)item->value)[value_id] = val_uint8;
					break;
				/* TODO: load other types of items */
				default:
					break;
				}
				value_id++;
			}
		}
	}
}

/**
 * \brief This function tries to load layer from the MongoDB
 */
struct VSLayer *vs_mongo_layer_load_linked(struct VS_CTX *vs_ctx,
		bson_oid_t *oid,
		struct VSNode *node,
		uint16 layer_id,
		uint32 req_version)
{
	struct VSLayer *layer = NULL;
	struct VSLayer *parent_layer = NULL;
	bson query;
	mongo_cursor cursor;
	uint32 node_id = -1, tmp_layer_id = -1, current_version = -1,
			custom_type = -1, data_type = -1, vec_size = -1,
			parent_layer_id = -1;
	int found = 0;
	bson_iterator layer_data_iter;
	const bson *bson_layer;

	bson_init(&query);
	bson_append_oid(&query, "_id", oid);
	bson_finish(&query);

	mongo_cursor_init(&cursor, vs_ctx->mongo_conn, vs_ctx->mongo_layer_ns);
	mongo_cursor_set_query(&cursor, &query);

	/* ObjectID should be unique */
	while( mongo_cursor_next(&cursor) == MONGO_OK ) {
		bson_layer = mongo_cursor_bson(&cursor);

		/* Try to get node id */
		if( bson_find(&layer_data_iter, bson_layer, "node_id") == BSON_INT ) {
			node_id = bson_iterator_int(&layer_data_iter);
		}

		/* Try to get layer id */
		if( bson_find(&layer_data_iter, bson_layer, "layer_id") == BSON_INT ) {
			tmp_layer_id = bson_iterator_int(&layer_data_iter);
		}

		/* ObjectID is ALMOST unique. So it is checked, if node id and
		 * layer id matches */
		if(node_id == node->id && tmp_layer_id == layer_id) {
			found = 1;
			break;
		}
	}

	/* When layer was found in MongoDB, then try to create new layer */
	if(found == 1) {
		/* Try to get current version of layer */
		if( bson_find(&layer_data_iter, bson_layer, "current_version") == BSON_INT ) {
			current_version = bson_iterator_int(&layer_data_iter);
		}

		/* Try to get data type of layer */
		if( bson_find(&layer_data_iter, bson_layer, "data_type") == BSON_INT ) {
			data_type = bson_iterator_int(&layer_data_iter);
		}

		/* Try to get vector size of data stored in layer */
		if( bson_find(&layer_data_iter, bson_layer, "vec_size") == BSON_INT ) {
			vec_size = bson_iterator_int(&layer_data_iter);
		}

		/* Try to get custom type of layer */
		if( bson_find(&layer_data_iter, bson_layer, "custom_type") == BSON_INT ) {
			custom_type = bson_iterator_int(&layer_data_iter);
		}

		/* Try to get parent layer. It is OK, when this item does not exist. */
		if( bson_find(&layer_data_iter, bson_layer, "parent_layer_id") == BSON_INT ) {
			parent_layer_id = bson_iterator_int(&layer_data_iter);
			/* Try to find parent layer. It should be already created. */
			parent_layer = vs_layer_find(node, parent_layer_id);
		}

		/* Create layer with specific ID */
		if((int)current_version != -1 &&
				(int)custom_type != -1 &&
				(int)data_type != -1 &&
				(int)vec_size != -1)
		{
			layer = vs_layer_create(node, parent_layer, layer_id, data_type,
					vec_size, custom_type);

			if(layer != NULL) {
				layer->state = ENTITY_CREATED;

				/* Save ObjectID to layer */
				memcpy(&layer->oid, oid, sizeof(bson_oid_t));

				/* Try to get versions of layer */
				if( bson_find(&layer_data_iter, bson_layer, "versions") == BSON_OBJECT ) {
					bson bson_versions;
					bson_iterator version_iter;
					char str_num[15];

					/* Initialize sub-object of versions */
					bson_iterator_subobject_init(&layer_data_iter, &bson_versions, 0);

					sprintf(str_num, "%u", req_version);

					/* Try to find required version of layer */
					if( bson_find(&version_iter, &bson_versions, str_num) == BSON_OBJECT ) {
						bson bson_version;

						/* Set version of layer */
						layer->version = layer->saved_version = current_version;

						bson_iterator_subobject_init(&version_iter, &bson_version, 0);

						/* Try to load data of layer */
						vs_mongo_layer_load_data(node, layer, &bson_version);
					}
				}
			}
		}
	}

	bson_destroy(&query);
	mongo_cursor_destroy(&cursor);

	return layer;
}
