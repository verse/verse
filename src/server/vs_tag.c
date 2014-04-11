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

#include <assert.h>

#include "verse_types.h"
#include "v_list.h"
#include "v_common.h"
#include "v_tag_commands.h"
#include "v_fake_commands.h"

#include "vs_tag.h"
#include "vs_node.h"
#include "vs_node_access.h"
#include "vs_taggroup.h"

/**
 * \brief This function add any TagSet command to the queue of outgoing commands
 */
void vs_tag_send_set(struct VSession *vsession,
		uint8 prio,
		struct VSNode *node,
		struct VSTagGroup *tg,
		struct VSTag *tag)
{
	struct Generic_Cmd *tag_set_cmd = NULL;

	tag_set_cmd = v_tag_set_create(node->id, tg->id, tag->id, tag->data_type, tag->count, tag->value);

	v_out_queue_push_tail(vsession->out_queue, prio, tag_set_cmd);
}

/**
 * \brief This function tries to find tag in tag_group
 */
static struct VSTag *vs_tag_find(struct VSTagGroup *tg,
		uint16 tag_id)
{
	struct VSTag find_tag;
	struct VSTag *tag = NULL;
	struct VBucket *bucket;

	find_tag.id = tag_id;
	bucket = v_hash_array_find_item(&tg->tags, (void*)&find_tag);
	if(bucket != NULL) {
		tag = (struct VSTag*)bucket->data;
	}

	return tag;
}

/**
 * \brief This function add TagCreate command to the queue of the session
 */
int vs_tag_send_create(struct VSEntitySubscriber *tg_subscriber,
		struct VSNode *node,
		struct VSTagGroup *tg,
		struct VSTag *tag)
{
	struct Generic_Cmd *tag_create;
	struct VSEntityFollower	*tag_follower;

	/* Check if this command, has not been already sent */
	tag_follower = tag->tag_folls.first;
	while(tag_follower != NULL) {
		if(tag_follower->node_sub->session == tg_subscriber->node_sub->session) {
			return 0;
		}
		tag_follower = tag_follower->next;
	}

	/* Create new Tag_Create command */
	tag_create = v_tag_create_create(node->id, tg->id, tag->id, tag->data_type, tag->count, tag->type);

	/* Put command to the outgoing queue */
	if(tag_create != NULL &&
			(v_out_queue_push_tail(tg_subscriber->node_sub->session->out_queue,
					tg_subscriber->node_sub->prio,
					tag_create))==1)
	{
		tag_follower = (struct VSEntityFollower*)calloc(1, sizeof(struct VSEntityFollower));
		tag_follower->node_sub = tg_subscriber->node_sub;
		tag_follower->state = ENTITY_CREATING;
		v_list_add_tail(&tag->tag_folls, tag_follower);

		return 1;
	}

	return 0;
}

/**
 * \brief This function adds TagDestroy command to the queue of the session
 */
int vs_tag_send_destroy(struct VSNode *node,
		struct VSTagGroup *tg,
		struct VSTag *tag)
{
	struct VSEntityFollower *tag_follower;
	struct Generic_Cmd *tag_destroy_cmd;
	int ret = 0;

	tag_follower = tag->tag_folls.first;
	while(tag_follower != NULL) {
		/* Client can receive tag_destroy command only in situation, when
		 * this client already received tag_create command */
		if(tag_follower->state == ENTITY_CREATED) {
			tag_destroy_cmd = v_tag_destroy_create(node->id, tg->id, tag->id);

			if( tag_destroy_cmd != NULL &&
					(v_out_queue_push_tail(tag_follower->node_sub->session->out_queue,
							tag_follower->node_sub->prio,
							tag_destroy_cmd) == 1)) {
				tag_follower->state = ENTITY_DELETING;
				ret = 1;
			} else {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Tag_Destroy (node_id: %d, taggroup_id: %d, tag_id: %d) wasn't added to the queue\n",
						node->id, tg->id, tag->id);
			}
		}
		tag_follower = tag_follower->next;
	}

	tag->state = ENTITY_DELETING;

	return ret;
}

/**
 * \brief This function initialize structure storing tag
 */
static void vs_tag_init(struct VSTag *tag)
{
	tag->id = 0;
	tag->type = 0;
	tag->tag_folls.first = NULL;
	tag->tag_folls.last = NULL;
	tag->data_type = VRS_VALUE_TYPE_RESERVED;
	tag->count = 0;
	tag->flag = TAG_UNINITIALIZED;
	tag->value = NULL;
	tag->state = ENTITY_RESERVED;
}

/**
 * \brief This function tries to set data in tag
 */
void vs_tag_set_values(struct VSTag *tag, uint8 count, uint8 index, void *data)
{
	/* Set value in tag */
	switch(tag->data_type) {
	case VRS_VALUE_TYPE_UINT8:
		memcpy(&tag->value[index], data, UINT8_SIZE*count);
		break;
	case VRS_VALUE_TYPE_UINT16:
		memcpy(&tag->value[index], data, UINT16_SIZE*count);
		break;
	case VRS_VALUE_TYPE_UINT32:
		memcpy(&tag->value[index], data, UINT32_SIZE*count);
		break;
	case VRS_VALUE_TYPE_UINT64:
		memcpy(&tag->value[index], data, UINT64_SIZE*count);
		break;
	case VRS_VALUE_TYPE_REAL16:
		memcpy(&tag->value[index], data, REAL16_SIZE*count);
		break;
	case VRS_VALUE_TYPE_REAL32:
		memcpy(&tag->value[index], data, REAL32_SIZE*count);
		break;
	case VRS_VALUE_TYPE_REAL64:
		memcpy(&tag->value[index], data, REAL64_SIZE*count);
		break;
	case VRS_VALUE_TYPE_STRING8:
		if(tag->value == NULL) {
			tag->value = strdup((char*)data);
		} else {
			size_t new_str_len = strlen((char*)data);
			size_t old_str_len = strlen((char*)tag->value);
			/* Rewrite old string */
			if(new_str_len == old_str_len) {
				strcpy((char*)tag->value, (char*)data);
			} else {
				tag->value = (char*)realloc(tag->value, new_str_len*sizeof(char));
				strcpy((char*)tag->value, (char*)data);
			}
		}
		break;
	default:
		assert(0);
		break;
	}
}

/**
 * \brief This function creates new Verse Tag
 */
struct VSTag *vs_tag_create(struct VSTagGroup *tg,
		uint16 tag_id,
		uint8 data_type,
		uint8 count,
		uint16 custom_type)
{
	struct VSTag *tag = NULL;
	struct VBucket *tag_bucket;

	tag = (struct VSTag*)calloc(1, sizeof(struct VSTag));
	if(tag == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Out of memory.\n");
		return NULL;
	}

	/* Initialize new tag */
	vs_tag_init(tag);

	if(tag_id == RESERVED_TAG_ID) {
		/* Try to find first free id for tag */
		tag->id = tg->last_tag_id;
		while( v_hash_array_find_item(&tg->tags, tag) != NULL ) {
			tag->id++;

			if(tag->id > LAST_TAG_ID) {
				tag->id = FIRST_TAG_ID;
			}

			/* TODO: previous code could be more effective */
		}
	} else {
		tag->id = tag_id;
	}
	tg->last_tag_id = tag->id;

	/* Try to add new Tag to the hashed linked list of tags */
	tag_bucket = v_hash_array_add_item(&tg->tags, (void*)tag, sizeof(struct VSTag));

	if(tag_bucket==NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Tag could not be added to tag group: %d.\n",
				tg->id);
		free(tag);
		return NULL;
	}

	tag->data_type = data_type;
	tag->count = count;
	tag->type = custom_type;

	/* Allocate memory for value (not for type string8) */
	switch(data_type) {
		case VRS_VALUE_TYPE_UINT8:
			tag->value = (void*)calloc(count, sizeof(uint8));
			break;
		case VRS_VALUE_TYPE_UINT16:
			tag->value = (void*)calloc(count, sizeof(uint16));
			break;
		case VRS_VALUE_TYPE_UINT32:
			tag->value = (void*)calloc(count, sizeof(uint32));
			break;
		case VRS_VALUE_TYPE_UINT64:
			tag->value = (void*)calloc(count, sizeof(uint64));
			break;
		case VRS_VALUE_TYPE_REAL16:
			tag->value = (void*)calloc(count, sizeof(real16));
			break;
		case VRS_VALUE_TYPE_REAL32:
			tag->value = (void*)calloc(count, sizeof(real32));
			break;
		case VRS_VALUE_TYPE_REAL64:
			tag->value = (void*)calloc(count, sizeof(real64));
			break;
		case VRS_VALUE_TYPE_STRING8:
			/* Memory for this type of tag is allocated, when value of
			 * tag is set. */
			tag->value = NULL;
			break;
		default:
			break;
	}

	vs_taggroup_inc_version(tg);

	return tag;
}


/**
 * \brief This function destroy structure storing information about tag. This
 * function should be called only in situation, when all clients received
 * TagDestroy command.
 */
int vs_tag_destroy(struct VSTagGroup *tg, struct VSTag *tag)
{
	if(tag->tag_folls.first == NULL) {

		/* Free value */
		if(tag->value != NULL) {
			free(tag->value);
			tag->value = NULL;
		}

		/* Remove tag from tag group */
		v_hash_array_remove_item(&tg->tags, tag);

		free(tag);

		vs_taggroup_inc_version(tg);

		return 1;
	} else {
		/* This should never happen */
		v_print_log(VRS_PRINT_WARNING, "%s(): tag (id: %d) with followers can't be destroyed\n",
				__FUNCTION__, tag->id);
		return 0;
	}

	return 0;
}

/**
 * \brief This function tries to handle Tag_Create command.
 *
 * This function change state of follower from CREATING to CREATED
 * and it sends value to this client.
 */
int vs_handle_tag_create_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd)
{
	struct VSNode				*node;
	struct VSTagGroup			*tg;
	struct VSTag				*tag;
	struct VSEntityFollower		*tag_follower;
	struct Tag_Create_Ack_Cmd	*tag_create_ack = (struct Tag_Create_Ack_Cmd*)cmd;
	uint32						node_id = tag_create_ack->node_id;
	uint16						taggroup_id = tag_create_ack->taggroup_id;
	uint16						tag_id = tag_create_ack->tag_id;
	int							all_created = 1, tag_found = 0;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Try to find TagGroup */
	if( (tg = vs_taggroup_find(node, taggroup_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag_group (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, taggroup_id, node_id);
		return 0;
	}

	/* Try to find Tag */
	if ( (tag = vs_tag_find(tg, tag_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag (id: %d) in tag_group (id: %d), node (id: %d) not found\n",
				__FUNCTION__, tag_id, taggroup_id, node_id);
		return 0;
	}

	/* Try to find tag follower that generated this fake command */
	tag_follower = tag->tag_folls.first;
	while(tag_follower != NULL) {
		if(tag_follower->node_sub->session->session_id == vsession->session_id) {
			tag_found = 1;
			/* When tag contain value, then send this value to the client */
			if( (tag->flag & TAG_INITIALIZED) && (tag_follower->state != ENTITY_CREATED)) {
				vs_tag_send_set(tag_follower->node_sub->session, tag_follower->node_sub->prio, node, tg, tag);
			}
			tag_follower->state = ENTITY_CREATED;

			/* When this tag has been destroyed during sending tag_create
			 * command, then send tag_destroy command now */
			if(tag->state == ENTITY_DELETING) {
				struct Generic_Cmd *tag_destroy_cmd = v_tag_destroy_create(node->id, tg->id, tag->id);

				if( tag_destroy_cmd != NULL &&
						(v_out_queue_push_tail(tag_follower->node_sub->session->out_queue,
								tag_follower->node_sub->prio,
								tag_destroy_cmd) == 1)) {
					tag_follower->state = ENTITY_DELETING;
				} else {
					v_print_log(VRS_PRINT_DEBUG_MSG, "Tag_Destroy (node_id: %d, taggroup_id: %d, tag_id: %d) wasn't added to the queue\n",
							node->id, tg->id, tag->id);
				}
			}
		} else if(tag_follower->state != ENTITY_CREATED) {
			all_created = 0;
		}
		tag_follower = tag_follower->next;
	}

	/* When all clients knows about this tag, then switch tag to state CREATED */
	if(all_created == 1) {
		tag->state = ENTITY_CREATED;
	}

	if(tag_found == 0) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag_follower of tag (id: %d) in tag_group (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, tag_id, tg->id, node->id);
		return 0;
	} else {
		return 1;
	}

	return 0;
}

/**
 * \brief This function tries to handle Tag_Create command
 */
int vs_handle_tag_create(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *tag_create)
{
	struct VSNode			*node;
	struct VSTagGroup		*tg;
	struct VSTag			*tag;
	struct VSUser			*user;
	struct VBucket			*vbucket;
	struct VSEntitySubscriber	*tg_subscriber;
	uint32 					node_id = UINT32(tag_create->data[0]);
	uint16 					taggroup_id = UINT16(tag_create->data[UINT32_SIZE]);
	uint16					tag_id = UINT16(tag_create->data[UINT32_SIZE + UINT16_SIZE]);
	uint8					data_type = UINT8(tag_create->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]);
	uint8					count = UINT8(tag_create->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + UINT8_SIZE]);
	uint16 					type = UINT16(tag_create->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE + UINT8_SIZE + UINT8_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Node has to be created */
	if(!(node->state == ENTITY_CREATED || node->state == ENTITY_CREATING)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) is not in CREATING or CREATED state: %d\n",
				__FUNCTION__, node->id, node->state);
		return 0;
	}

	/* Try to find user */
	if((user = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() vsession->user_id: %d not found\n",
				__FUNCTION__, vsession->user_id);
		return 0;
	}

	/* User has to have permission to write to the node */
	if(vs_node_can_write(vs_ctx, vsession, node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s(): user: %s can't write to node: %d\n",
				__FUNCTION__, user->username, node->id);
		return 0;
	}

	/* Try to find TagGroup */
	if( (tg = vs_taggroup_find(node, taggroup_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag_group (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, taggroup_id, node_id);
		return 0;
	}

	/* Tag Group has to be created too (it can't be in DELETING or DELETED state ) */
	if(!(tg->state == ENTITY_CREATED || tg->state == ENTITY_CREATING)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag group (id: %d) from node (id: %d) is not in CREATING or CREATED state: %d\n",
				__FUNCTION__, tg->id, node->id, tg->state);
		return 0;
	}

	/* Client has to send tag_create command with tag_id equal to
	 * the value 0xFFFF */
	if(tag_id != RESERVED_TAG_ID) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag_id: %d is not 0xFFFF\n",
				__FUNCTION__, tag_id);
		return 0;
	}

	/* Is type of Tag supported? */
	if(!(data_type>VRS_VALUE_TYPE_RESERVED && data_type<=VRS_VALUE_TYPE_STRING8)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag_type: %d is not supported\n",
				__FUNCTION__, data_type);
		return 0;
	}

	vbucket = tg->tags.lb.first;
	/* Check, if there isn't tag with the same type */
	while(vbucket != NULL) {
		tag = vbucket->data;
		if(tag->type == type) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag type: %d is already used in taggroup: %d\n",
					__FUNCTION__, type, tg->id);
			return 0;
		}
		vbucket = vbucket->next;
	}

	if (v_hash_array_count_items(&tg->tags) > MAX_TAGS_COUNT) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() max number of tags in node: %d tag_group: %d was reached\n",
				tg->id);
		return 0;
	}

	/* Try to create new tag */
	tag = vs_tag_create(tg, RESERVED_TAG_ID, data_type, count, type);
	if(tag == NULL) {
		return 0;
	}

	tag->state = ENTITY_CREATING;

	/* Send TagCreate to all subscribers of tag group */
	tg_subscriber = tg->tg_subs.first;
	while(tg_subscriber != NULL) {
		vs_tag_send_create(tg_subscriber, node, tg, tag);
		tg_subscriber = tg_subscriber->next;
	}

	return 1;
}

/**
 * \brief This function tries to handle Tag_Destroy command
 */
int vs_handle_tag_destroy_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd)
{

	struct VSNode				*node;
	struct VSTagGroup			*tg;
	struct VSTag				*tag;
	struct VSEntityFollower		*tag_follower;
	struct Tag_Destroy_Ack_Cmd	*tag_destroy_ack = (struct Tag_Destroy_Ack_Cmd*)cmd;
	uint32						node_id = tag_destroy_ack->node_id;
	uint16						taggroup_id = tag_destroy_ack->taggroup_id;
	uint16						tag_id = tag_destroy_ack->tag_id;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Try to find TagGroup */
	if( (tg = vs_taggroup_find(node, taggroup_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag_group (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, taggroup_id, node_id);
		return 0;
	}

	/* Try to find Tag */
	if ( (tag = vs_tag_find(tg, tag_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag (id: %d) in tag_group (id: %d), node (id: %d) not found\n",
				__FUNCTION__, tag_id, taggroup_id, node_id);
		return 0;
	}

	/* Try to find tag follower that generated this fake command */
	tag_follower = tag->tag_folls.first;
	while(tag_follower != NULL) {
		if(tag_follower->node_sub->session->session_id == vsession->session_id) {
			tag_follower->state = ENTITY_DELETED;
			v_list_free_item(&tag->tag_folls, tag_follower);
			break;
		}
		tag_follower = tag_follower->next;
	}

	/* When tag doesn't have any follower, then it is possible to destroy
	 * this tag and remove it from list of tags from the tag group */
	if(tag->tag_folls.first == NULL) {
		tag->state = ENTITY_DELETED;
		vs_tag_destroy(tg, tag);
	}

	return 1;
}

/**
 * \brief This function tries to handle Tag_Destroy command
 */
int vs_handle_tag_destroy(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *tag_destroy)
{
	struct VSNode			*node;
	struct VSTagGroup		*tg;
	struct VSTag			*tag;
	struct VSUser			*user;
	uint32 					node_id = UINT32(tag_destroy->data[0]);
	uint16 					taggroup_id = UINT16(tag_destroy->data[UINT32_SIZE]);
	uint16					tag_id = UINT16(tag_destroy->data[UINT32_SIZE + UINT16_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Node has to be created */
	if(!(node->state == ENTITY_CREATED || node->state == ENTITY_CREATING)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) is not in NODE_CREATED state: %d\n",
				__FUNCTION__, node->id, node->state);
		return 0;
	}

	/* Try to find user */
	if((user = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() vsession->user_id: %d not found\n",
				__FUNCTION__, vsession->user_id);
		return 0;
	}

	/* Is user owner of this node or can user write to this node? */
	if(vs_node_can_write(vs_ctx, vsession, node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s(): user: %s can't write to node: %d\n",
				__FUNCTION__, user->username, node->id);
		return 0;
	}

	/* Try to find TagGroup */
	if( (tg = vs_taggroup_find(node, taggroup_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag_group (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, taggroup_id, node_id);
		return 0;
	}

	/* Try to find Tag */
	if ( (tag = vs_tag_find(tg, tag_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag (id: %d) in tag_group (id: %d), node (id: %d) not found\n",
				__FUNCTION__, tag_id, taggroup_id, node_id);
		return 0;
	}

	return vs_tag_send_destroy(node, tg, tag);
}


/**
 * \brief This function tries to handle Tag_Set_Int8 command
 */
int vs_handle_tag_set(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *tag_set,
		uint8 data_type,
		uint8 count)
{
	struct VSNode				*node;
	struct VSTagGroup			*tg;
	struct VSTag				*tag;
	struct VSUser				*user;
	struct VSEntitySubscriber	*tg_subscriber;
	uint32 						node_id;
	uint16 						taggroup_id;
	uint16						tag_id;

	node_id = UINT32(tag_set->data[0]);
	taggroup_id = UINT16(tag_set->data[UINT32_SIZE]);
	tag_id = UINT16(tag_set->data[UINT32_SIZE + UINT16_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Node has to be created */
	if(!(node->state == ENTITY_CREATED || node->state == ENTITY_CREATING)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) is not in NODE_CREATED state: %d\n",
				__FUNCTION__, node->id, node->state);
		return 0;
	}

	/* Try to find user */
	if((user = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() vsession->user_id: %d not found\n",
				__FUNCTION__, vsession->user_id);
		return 0;
	}

	/* Is user owner of this node or can user write to this node? */
	if(vs_node_can_write(vs_ctx, vsession, node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s(): user: %s can't write to node: %d\n",
				__FUNCTION__, user->username, node->id);
		return 0;
	}

	/* Try to find TagGroup */
	if( (tg = vs_taggroup_find(node, taggroup_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag_group (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, taggroup_id, node_id);
		return 0;
	}

	/* Try to find Tag */
	if ( (tag = vs_tag_find(tg, tag_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag (id: %d) in tag_group (id: %d), node (id: %d) not found\n",
				__FUNCTION__, tag_id, taggroup_id, node_id);
		return 0;
	}

	/* Data type has to match */
	if(data_type != tag->data_type) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() data type (%d) of tag (id: %d) in tg (id: %d) in node (id: %d) does not match data type of received command (%d)\n",
						__FUNCTION__, tag->data_type, tag_id, taggroup_id, node_id, data_type);
		return 0;
	}

	/* Count of values has to match */
	if(count != tag->count) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() count of values (%d) of tag (id: %d) in tg (id: %d) in node (id: %d) does not match count of values of received command (%d)\n",
						__FUNCTION__, tag->count, tag_id, taggroup_id, node_id, count);
		return 0;
	}

	/* Set value in tag */
	switch(tag->data_type) {
	case VRS_VALUE_TYPE_UINT8:
		memcpy(tag->value, &tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE], UINT8_SIZE*tag->count);
		break;
	case VRS_VALUE_TYPE_UINT16:
		memcpy(tag->value, &tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE], UINT16_SIZE*tag->count);
		break;
	case VRS_VALUE_TYPE_UINT32:
		memcpy(tag->value, &tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE], UINT32_SIZE*tag->count);
		break;
	case VRS_VALUE_TYPE_UINT64:
		memcpy(tag->value, &tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE], UINT64_SIZE*tag->count);
		break;
	case VRS_VALUE_TYPE_REAL16:
		memcpy(tag->value, &tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE], REAL16_SIZE*tag->count);
		break;
	case VRS_VALUE_TYPE_REAL32:
		memcpy(tag->value, &tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE], REAL32_SIZE*tag->count);
		break;
	case VRS_VALUE_TYPE_REAL64:
		memcpy(tag->value, &tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE], REAL64_SIZE*tag->count);
		break;
	case VRS_VALUE_TYPE_STRING8:
		if(tag->value == NULL) {
			tag->value = strdup(PTR(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]));
		} else {
			size_t new_str_len = strlen(PTR(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]));
			size_t old_str_len = strlen((char*)tag->value);
			/* Rewrite old string */
			if(new_str_len == old_str_len) {
				strcpy((char*)tag->value, PTR(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]));
			} else {
				tag->value = (char*)realloc(tag->value, new_str_len*sizeof(char));
				strcpy((char*)tag->value, PTR(tag_set->data[UINT32_SIZE + UINT16_SIZE + UINT16_SIZE]));
			}
		}
		break;
	default:
		assert(0);
		break;
	}

	/* Set this tag as initialized, because value of this tag was set. */
	tag->flag = TAG_INITIALIZED;

	vs_taggroup_inc_version(tg);

	/* Send this tag to all client subscribed to the TagGroup */
	tg_subscriber = tg->tg_subs.first;
	while(tg_subscriber != NULL) {
		vs_tag_send_set(tg_subscriber->node_sub->session, tg_subscriber->node_sub->prio, node, tg, tag);
		tg_subscriber = tg_subscriber->next;
	}

	return 0;
}

