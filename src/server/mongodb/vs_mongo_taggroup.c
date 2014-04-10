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
#include "vs_mongo_taggroup.h"
#include "vs_node.h"
#include "vs_taggroup.h"
#include "vs_tag.h"

#include "v_common.h"


/**
 * \brief This function save current version o tag group to MongoDB
 */
static void vs_mongo_taggroup_save_version(bson *bson_tg,
		struct VSTagGroup *tg)
{
	bson bson_version;
	bson bson_tag;
	struct VBucket *bucket;
	struct VSTag *tag;
	char str_num[15];
	int item_id;

	bson_init(&bson_version);

	bson_append_int(&bson_version, "crc32", tg->crc32);

	bson_append_start_object(&bson_version, "tags");

	bucket = tg->tags.lb.first;
	while(bucket != NULL) {
		tag = (struct VSTag*)bucket->data;

		bson_init(&bson_tag);
		bson_append_int(&bson_tag, "data_type", tag->data_type);
		bson_append_int(&bson_tag, "count", tag->count);
		bson_append_int(&bson_tag, "custom_type", tag->type);

		bson_append_start_array(&bson_tag, "data");
		switch(tag->data_type) {
		case VRS_VALUE_TYPE_UINT8:
			for(item_id = 0; item_id < tag->count; item_id++) {
				sprintf(str_num, "%d", item_id);
				bson_append_int(&bson_tag, str_num, ((uint8*)tag->value)[item_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT16:
			for(item_id = 0; item_id < tag->count; item_id++) {
				sprintf(str_num, "%d", item_id);
				bson_append_int(&bson_tag, str_num, ((uint16*)tag->value)[item_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT32:
			for(item_id = 0; item_id < tag->count; item_id++) {
				sprintf(str_num, "%d", item_id);
				bson_append_int(&bson_tag, str_num, ((uint32*)tag->value)[item_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT64:
			for(item_id = 0; item_id < tag->count; item_id++) {
				sprintf(str_num, "%d", item_id);
				bson_append_int(&bson_tag, str_num, ((uint64*)tag->value)[item_id]);
			}
			break;
		case VRS_VALUE_TYPE_REAL16:
			/* TODO */
			break;
		case VRS_VALUE_TYPE_REAL32:
			for(item_id = 0; item_id < tag->count; item_id++) {
				sprintf(str_num, "%d", item_id);
				bson_append_double(&bson_tag, str_num, ((float*)tag->value)[item_id]);
			}
			break;
		case VRS_VALUE_TYPE_REAL64:
			for(item_id = 0; item_id < tag->count; item_id++) {
				sprintf(str_num, "%d", item_id);
				bson_append_double(&bson_tag, str_num, ((double*)tag->value)[item_id]);
			}
			break;
		case VRS_VALUE_TYPE_STRING8:
			bson_append_string(&bson_tag, "0", (char*)tag->value);
			break;
		}
		bson_append_finish_array(&bson_tag);

		bson_finish(&bson_tag);

		sprintf(str_num, "%d", tag->id);
		bson_append_bson(&bson_version, str_num, &bson_tag);

		bucket = bucket->next;
	}

	bson_append_finish_object(&bson_version);

	bson_finish(&bson_version);

	sprintf(str_num, "%d", tg->version);
	bson_append_bson(bson_tg, str_num, &bson_version);
}

/**
 * \brief This function saves tag group to the MongoDB
 *
 * \param[in] *vs_ctx	The verse server context
 * \param[in] *node		The node containing tag group
 * \param[in] *tg		The tag group that will be saved
 *
 * \return	This function returns 1 on success. Otherwise it returns 0;
 */
int vs_mongo_taggroup_save(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		struct VSTagGroup *tg)
{
	bson bson_tg;
	int ret;

	if((int)tg->saved_version == -1) {
		bson_init(&bson_tg);

		bson_oid_gen(&tg->oid);
		bson_append_oid(&bson_tg, "_id", &tg->oid);
		bson_append_int(&bson_tg, "node_id", node->id);
		bson_append_int(&bson_tg, "taggroup_id", tg->id);
		bson_append_int(&bson_tg, "custom_type", tg->type);
		bson_append_int(&bson_tg, "current_version", tg->version);

		bson_append_start_object(&bson_tg, "versions");
		vs_mongo_taggroup_save_version(&bson_tg, tg);
		bson_append_finish_object(&bson_tg);

		bson_finish(&bson_tg);

		ret = mongo_insert(vs_ctx->mongo_conn, vs_ctx->mongo_tg_ns, &bson_tg, 0);
		bson_destroy(&bson_tg);
		if(ret != MONGO_OK) {
			v_print_log(VRS_PRINT_ERROR,
					"Unable to write tag group %d of node %d to MongoDB: %s\n",
					tg->id, node->id, vs_ctx->mongo_tg_ns);
			return 0;
		}
	} else {
		/* TODO: update item in database */
	}

	tg->saved_version = tg->version;

	return 1;
}

/**
 * \brief This function tries to load tag group from MongoDB
 *
 * \param[in] *vs_ctx		The verse server context
 * \param[in] *oid			The pointer at ObjectID of tag group in MongoDB
 * \param[in] *node			The node containing tag group
 * \param[in] taggroup_id	The tag group ID that is requested from database
 * \param[in] version		The version of tag group id that is requested
 * 							from database. When version is equal to -1, then
 * 							current version is loaded from MongoDB.
 *
 * \return This function returns pointer at tag group, when tag group is found.
 * Otherwise it returns NULL.
 */
struct VSTagGroup *vs_mongo_taggroup_load_linked(struct VS_CTX *vs_ctx,
		bson_oid_t *oid,
		struct VSNode *node,
		uint16 taggroup_id,
		uint32 req_version)
{
	struct VSTagGroup *tg = NULL;
	bson query;
	mongo_cursor cursor;
	uint32 node_id, tg_id, found = 0, current_version, custom_type;
	bson_iterator tg_data_iter;
	const bson *bson_tg;

	bson_init(&query);
	bson_append_oid(&query, "_id", oid);
	bson_finish(&query);

	mongo_cursor_init(&cursor, vs_ctx->mongo_conn, vs_ctx->mongo_tg_ns);
	mongo_cursor_set_query(&cursor, &query);

	/* ObjectID should be unique */
	while( mongo_cursor_next(&cursor) == MONGO_OK ) {
		bson_tg = mongo_cursor_bson(&cursor);

		/* Try to get node id */
		if( bson_find(&tg_data_iter, bson_tg, "node_id") == BSON_INT ) {
			node_id = bson_iterator_int(&tg_data_iter);
		}

		/* Try to get tag group id */
		if( bson_find(&tg_data_iter, bson_tg, "taggroup_id") == BSON_INT ) {
			tg_id = bson_iterator_int(&tg_data_iter);
		}

		/* ObjectID is ALMOST unique. So it is check, if node id and
		 * tag group id matches */
		if(node_id == node->id && tg_id == taggroup_id) {
			found = 1;
			break;
		}
	}

	/* When tag group was found, then load required data from MongoDB */
	if(found == 1) {

		/* Try to get current version of tag group */
		if( bson_find(&tg_data_iter, bson_tg, "current_version") == BSON_INT ) {
			current_version = bson_iterator_int(&tg_data_iter);
		}

		/* Try to get custom type of tag group */
		if( bson_find(&tg_data_iter, bson_tg, "custom_type") == BSON_INT ) {
			custom_type = bson_iterator_int(&tg_data_iter);
		}

		/* Try to get versions of node */
		if( bson_find(&tg_data_iter, bson_tg, "versions") == BSON_OBJECT ) {
			bson bson_versions;
			bson_iterator version_iter;
			char str_num[15];
			uint32 version;

			/* Initialize sub-object of versions */
			bson_iterator_subobject_init(&tg_data_iter, &bson_versions, 0);

			if((int)req_version == -1) {
				version = current_version;
			} else {
				version = req_version;
			}
			sprintf(str_num, "%d", version);

			/* Try to find required version of tag group */
			if( bson_find(&version_iter, &bson_versions, str_num) == BSON_OBJECT ) {
				bson bson_version;
				bson_iterator version_data_iter;

				bson_iterator_subobject_init(&version_iter, &bson_version, 0);

				/* Try to get tags og tag group */
				if( bson_find(&version_data_iter, &bson_version, "tags") == BSON_OBJECT ) {
					/* TODO: get all tags from this bson */
				}
			}
		}
	}

	bson_destroy(&query);
	mongo_cursor_destroy(&cursor);

	return tg;
}
