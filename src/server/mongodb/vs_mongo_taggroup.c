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
 * \brief This function save current version o tag group to mongo db
 */
static void vs_mongo_taggroup_save_version(bson *bson_tg,
		struct VSTagGroup *tg)
{
	bson bson_version;
	bson bson_tag;
	struct VBucket *bucket;
	struct VSTag *tag;
	char str_num[15];
	int item_id, data_id;

	bson_init(&bson_version);

	bson_append_int(&bson_version, "version", tg->version);
	bson_append_int(&bson_version, "crc32", tg->crc32);

	bson_append_start_array(&bson_version, "tags");

	bucket = tg->tags.lb.first;
	item_id = 0;
	while(bucket != NULL) {
		tag = (struct VSTag*)bucket->data;

		bson_init(&bson_tag);
		bson_append_int(&bson_tag, "tag_id", tag->id);
		bson_append_int(&bson_tag, "data_type", tag->data_type);
		bson_append_int(&bson_tag, "count", tag->count);
		bson_append_int(&bson_tag, "custom_type", tag->type);

		bson_append_start_array(&bson_tag, "data");
		switch(tag->data_type) {
		case VRS_VALUE_TYPE_UINT8:
			for(data_id = 0; data_id < tag->count; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_tag, str_num, ((uint8*)tag->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT16:
			for(data_id = 0; data_id < tag->count; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_tag, str_num, ((uint16*)tag->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT32:
			for(data_id = 0; data_id < tag->count; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_tag, str_num, ((uint32*)tag->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_UINT64:
			for(data_id = 0; data_id < tag->count; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_int(&bson_tag, str_num, ((uint64*)tag->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_REAL16:
			/* TODO */
			break;
		case VRS_VALUE_TYPE_REAL32:
			for(data_id = 0; data_id < tag->count; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_double(&bson_tag, str_num, ((float*)tag->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_REAL64:
			for(data_id = 0; data_id < tag->count; data_id++) {
				sprintf(str_num, "%d", data_id);
				bson_append_double(&bson_tag, str_num, ((double*)tag->value)[data_id]);
			}
			break;
		case VRS_VALUE_TYPE_STRING8:
			bson_append_string(&bson_tag, "0", (char*)tag->value);
			break;
		}
		bson_append_finish_array(&bson_tag);

		bson_finish(&bson_tag);

		sprintf(str_num, "%d", item_id);
		bson_append_bson(&bson_version, str_num, &bson_tag);

		item_id++;
		bucket = bucket->next;
	}

	bson_append_finish_array(&bson_version);

	bson_finish(&bson_version);

	bson_append_bson(bson_tg, "0", &bson_version);
}

/**
 * \brief This function saves tag group to the mongo database
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
	bson_oid_t oid;
	int ret;

	bson_init(&bson_tg);

	bson_oid_gen(&oid);
	bson_append_oid(&bson_tg, "_id", &oid);
	bson_append_int(&bson_tg, "node_id", node->id);
	bson_append_int(&bson_tg, "taggroup_id", tg->id);
	bson_append_int(&bson_tg, "custom_type", tg->type);
	bson_append_start_array(&bson_tg, "versions");
	vs_mongo_taggroup_save_version(&bson_tg, tg);
	bson_append_finish_array(&bson_tg);

	bson_finish(&bson_tg);

	ret = mongo_insert(vs_ctx->mongo_conn, vs_ctx->mongo_tg_ns, &bson_tg, 0);
	bson_destroy(&bson_tg);
	if(ret != MONGO_OK) {
		v_print_log(VRS_PRINT_ERROR,
				"Unable to write tag group %d of node %d to mongo db: %s\n",
				tg->id, node->id, vs_ctx->mongo_node_ns);
		return 0;
	}

	return 1;
}

/**
 * \brief This function tries to load tag group from mongo database
 *
 * \param[in] *vs_ctx		The verse server context
 * \param[in] *node			The node containing tag group
 * \param[in] taggroup_id	The tag group ID that is requested from database
 * \param[in] version		The version of tag group id that is requested
 * 							from database
 * \return This function returns pointer at tag group, when tag group is found.
 * Otherwise it returns NULL.
 */
struct VSTagGroup *vs_mongo_taggroup_load(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		uint16 taggroup_id,
		uint32 version)
{
	(void)vs_ctx;
	(void)node;
	(void)taggroup_id;
	(void)version;

	return NULL;
}
