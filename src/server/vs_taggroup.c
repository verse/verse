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

#include "vs_taggroup.h"
#include "vs_tag.h"
#include "vs_node.h"
#include "vs_node_access.h"
#include "vs_entity.h"
#include "v_common.h"
#include "v_fake_commands.h"

/**
 * \brief This function increments version of tag group
 */
void vs_taggroup_inc_version(struct VSTagGroup *tg)
{
	/* TODO: Compute CRC32 of tag group */
	if( (tg->version + 1 ) < UINT32_MAX ) {
		tg->version++;
	} else {
		tg->version = 1;
		tg->saved_version = 0;
	}
}

/**
 * \brief This function finds tag group in node using tag group id
 *
 * \param[in] *node			The pointer at VSNode
 * \param[in] taggroup_id	The id of tag group
 *
 * \return This function return pointer at structure with tag group.
 */
struct VSTagGroup *vs_taggroup_find(struct VSNode *node,
		uint16 taggroup_id)
{
	struct VSTagGroup find_tg;
	struct VBucket *bucket;
	struct VSTagGroup *tg = NULL;

	find_tg.id = taggroup_id;
	bucket = v_hash_array_find_item(&node->tag_groups, (void*)&find_tg);
	if(bucket != NULL) {
		tg = (struct VSTagGroup*)bucket->data;
	}

	return tg;
}

/**
 * \brief This function sends TagGroupCreate command to the client
 *
 * \param[in] *node_subscriber
 * \param[in] *node
 * \param[in] *tg
 *
 * \return This function return 1, when command was successfully created and
 * pushed to outgoing queue. When some error occur (already subscribed to
 * tag group, out of memory, etc.) then 0 is returned. When outgoing queue is
 * empty, then -1 is returned.
 */
int vs_taggroup_send_create(struct VSNodeSubscriber *node_subscriber,
		struct VSNode *node,
		struct VSTagGroup *tg)
{
	struct VSession			*vsession = node_subscriber->session;
	struct Generic_Cmd		*taggroup_create_cmd;
	struct VSEntityFollower	*taggroup_follower;

	/* Check if this tag group is in created/creating state */
	if(!(tg->state == ENTITY_CREATING || tg->state == ENTITY_CREATED)) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"This tag group: %d in node: %d is in %d state\n",
				tg->id, node->id, tg->state);
		return 0;
	}

	/* Check if this command, has not been already sent */
	taggroup_follower = tg->tg_folls.first;
	while(taggroup_follower != NULL) {
		if(taggroup_follower->node_sub->session->session_id == vsession->session_id ) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Client already knows about this TagGroup: %d\n",
					tg->id);
			return 0;
		}
		taggroup_follower = taggroup_follower->next;
	}

	/* Create TagGroup create command */
	taggroup_create_cmd = v_taggroup_create_create(node->id, tg->id, tg->custom_type);

	/* Put this command to the outgoing queue */
	if(taggroup_create_cmd == NULL) {
		return 0;
	}

	if(v_out_queue_push_tail(vsession->out_queue, node_subscriber->prio, taggroup_create_cmd) == 0) {
		return -1;
	} else {
		/* Add this session to the list of session, that knows about this
		 * taggroup. Server could send them taggroup_destroy in the future. */
		taggroup_follower = (struct VSEntityFollower*)calloc(1, sizeof(struct VSEntityFollower));
		taggroup_follower->node_sub = node_subscriber;
		taggroup_follower->state = ENTITY_CREATING;
		v_list_add_tail(&tg->tg_folls, taggroup_follower);

		return 1;
	}

	return 0;
}

/**
 * \brief This function sends TagGroupDestroy command to the client
 */
int vs_taggroup_send_destroy(struct VSNode *node,
		struct VSTagGroup *tg)
{
	struct VSEntityFollower *tg_follower;
	struct Generic_Cmd *tg_destroy_cmd;
	int ret = 0;

	tg_follower = tg->tg_folls.first;
	while(tg_follower != NULL) {
		/* Send this command only in situation, when FAKE_CMD_TAGGROUP_CREATE_ACK
		 * was received	*/
		if(tg_follower->state == ENTITY_CREATED) {
			/* Create TagGroup_Destroy command */
			tg_destroy_cmd = v_taggroup_destroy_create(node->id, tg->id);

			/* Put this command to the outgoing queue */
			if( tg_destroy_cmd != NULL &&
					(v_out_queue_push_tail(tg_follower->node_sub->session->out_queue,
							tg_follower->node_sub->prio,
							tg_destroy_cmd) == 1))
			{
				tg_follower->state = ENTITY_DELETING;
				ret = 1;
			} else {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"TagGroup_Destroy (node_id: %d, taggroup_id: %d) wasn't added to the queue\n",
						node->id, tg->id);
			}
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"node_id: %d, taggroup_id: %d, tag_group is not in CREATED state (current state: %d)\n",
					node->id, tg->id, tg_follower->state);
		}
		tg_follower = tg_follower->next;
	}

	tg->state = ENTITY_DELETING;

	return ret;
}

/**
 * \brief This function initialize structure storing informations about
 * one tag group
 */
static void vs_taggroup_init(struct VSTagGroup *tg)
{
#ifdef WITH_MONGODB
	int i;
#endif

	tg->id = 0;
	tg->custom_type = 0;

	v_hash_array_init(&tg->tags,
				HASH_MOD_256,
				offsetof(VSTag, id),
				sizeof(uint16));
	tg->last_tag_id = 0;

	tg->tg_folls.first = NULL;
	tg->tg_folls.last = NULL;

	tg->tg_subs.first = NULL;
	tg->tg_subs.last = NULL;

	tg->state = ENTITY_RESERVED;

	tg->version = 0;
	tg->saved_version = -1;
	tg->crc32 = 0;

#ifdef WITH_MONGODB
	for(i=0; i<3; i++) {
		tg->oid.ints[i] = 0;
	}
#endif
}

/**
 * \brief This function create new tag group
 */
struct VSTagGroup *vs_taggroup_create(struct VSNode *node,
		uint16 tg_id,
		uint16 custom_type)
{
	struct VSTagGroup *tg = NULL;
	struct VBucket *tg_bucket;

	if ( !(v_hash_array_count_items(&node->tag_groups) < MAX_TAGGROUPS_COUNT) ) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Maximal number of tag groups in node: %d reached.\n",
				node->id);
		return NULL;
	}

	/* Try to create new tag group */
	tg = (struct VSTagGroup*)calloc(1, sizeof(VSTagGroup));
	if(tg == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Out of memory.\n");
		return NULL;
	}

	/* Initialize new tag group */
	vs_taggroup_init(tg);

	if(tg_id == VRS_RESERVED_TAGGROUP_ID) {
		/* Try to find first free taggroup_id */
		tg->id = node->last_tg_id;
		while( v_hash_array_find_item(&node->tag_groups, tg) != NULL) {
			/* When not found, then try higher value */
			tg->id++;

			/* Skip IDs with special purpose */
			if(tg->id > LAST_TAGGROUP_ID) {
				tg->id = FIRST_TAGGROUP_ID;
			}
		}
	} else {
		tg->id = tg_id;
	}
	node->last_tg_id = tg->id;

	/* Try to add TagGroup to the hashed linked list */
	tg_bucket = v_hash_array_add_item(&node->tag_groups, tg, sizeof(struct VSTagGroup));

	if(tg_bucket == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Tag group could not be added to node: %d.\n",
				node->id);
		free(tg);
		return NULL;
	}

	/* Copy type */
	tg->custom_type = custom_type;

	vs_node_inc_version(node);

	return tg;
}

/**
 * \brief This function unsubscribes client from the tag group
 */
int vs_taggroup_unsubscribe(struct VSTagGroup *tg,
		struct VSession *vsession)
{
	struct VSEntitySubscriber	*tg_subscriber;

	/* If client is subscribed to this tag group, then remove this client
	 * from list of subscribers */
	tg_subscriber = tg->tg_subs.first;
	while(tg_subscriber != NULL) {
		if(tg_subscriber->node_sub->session == vsession) {
			VSTag *tag;
			VBucket *bucket;
			struct VSEntityFollower	*tag_follower;

			/* Go through all tags in this tag group */
			bucket = tg->tags.lb.first;
			while(bucket != NULL) {
				tag = (struct VSTag*)bucket->data;
				tag_follower = tag->tag_folls.first;
				/* Remove client from list of tag followers */
				while(tag_follower != NULL) {
					if(tag_follower->node_sub->session == vsession) {
						v_list_free_item(&tag->tag_folls, tag_follower);
						break;
					}
					tag_follower = tag_follower->next;
				}
				bucket = bucket->next;
			}

			/* Remove client from list of tag group subscribers */
			v_list_free_item(&tg->tg_subs, tg_subscriber);

			return 1;
		}
		tg_subscriber = tg_subscriber->next;
	}

	return 0;
}

/**
 * \brief This function destroy tag group and all tags included in this tag
 * group. This function should be called only in situation, when all clients
 * received command 'TagGroup Destroy'.
 */
int vs_taggroup_destroy(struct VSNode *node, struct VSTagGroup *tg)
{
	struct VBucket *bucket;
	struct VSTag *tag;

	/* All clients had to received TagGroup_Destroy command */
	assert(tg->tg_folls.first == NULL);

	/* Free all data allocated in tags at the first time */
	bucket = tg->tags.lb.first;
	while(bucket != NULL) {
		tag = (struct VSTag*)bucket->data;

		if(tag->value != NULL) {
			free(tag->value);
			tag->value = NULL;
		}

		free(tag);

		bucket = bucket->next;
	}

	/* Destroy all tags in this taggroup */
	v_hash_array_destroy(&tg->tags);

	/* Free list of followers and subscribers */
	v_list_free(&tg->tg_folls);
	v_list_free(&tg->tg_subs);

	v_print_log(VRS_PRINT_DEBUG_MSG, "TagGroup: %d destroyed\n", tg->id);

	/* Destroy this tag group itself */
	v_hash_array_remove_item(&node->tag_groups, tg);
	free(tg);

	vs_node_inc_version(node);

	return 1;
}

/**
 * \brief This function removes all tag groups and tags from the VSNode. This
 * function doesn't check, if any client is subscribed to tag groups or not.
 * This function should be called only in situation, when node is destroyed.
 */
int vs_node_taggroups_destroy(struct VSNode *node)
{
	struct VBucket *tg_bucket, *tg_bucket_next, *bucket;
	struct VSTagGroup *tg;
	struct VSTag *tag;

	tg_bucket = node->tag_groups.lb.first;
	while(tg_bucket != NULL) {
		tg_bucket_next = tg_bucket->next;
		tg = (struct VSTagGroup*)tg_bucket->data;

		/* Free all data allocated in tags at the first time */
		bucket = tg->tags.lb.first;
		while(bucket != NULL) {
			tag = (struct VSTag*)bucket->data;

			if(tag->value) {
				free(tag->value);
				tag->value = NULL;
			}

			free(tag);

			bucket = bucket->next;
		}

		/* Destroy all tags in this taggroup */
		v_hash_array_destroy(&tg->tags);

		/* Free list of followers and subscribers */
		v_list_free(&tg->tg_folls);
		v_list_free(&tg->tg_subs);

		/* Destroy this tag group itself */
		v_hash_array_remove_item(&node->tag_groups, tg);
		free(tg);

		tg_bucket = tg_bucket_next;
	}

	return 1;
}

int vs_handle_taggroup_destroy_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd)
{
	struct VSNode *node;
	struct VSTagGroup *tg;
	struct VSEntityFollower *tg_foll, *next_tg_foll;
	struct TagGroup_Destroy_Ack_Cmd *cmd_tg_destroy_ack = (struct TagGroup_Destroy_Ack_Cmd*)cmd;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, cmd_tg_destroy_ack->node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, cmd_tg_destroy_ack->node_id);
		return 0;
	}

	if( (tg = vs_taggroup_find(node, cmd_tg_destroy_ack->taggroup_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() tag_group (id: %d) in node (id: %d) not found\n",
				__FUNCTION__,
				cmd_tg_destroy_ack->taggroup_id,
				cmd_tg_destroy_ack->node_id);
		return 0;
	}

	tg_foll = tg->tg_folls.first;
	while(tg_foll != NULL) {
		next_tg_foll = tg_foll->next;
		if(tg_foll->node_sub->session->session_id == vsession->session_id) {
			tg_foll->state = ENTITY_DELETED;
			v_list_free_item(&tg->tg_folls, tg_foll);
		}
		tg_foll = next_tg_foll;
	}

	/* When taggroup doesn't have any follower, then it is possible to destroy
	 * this taggroup */
	if(tg->tg_folls.first == NULL) {
		tg->state = ENTITY_DELETED;
		vs_taggroup_destroy(node, tg);
	}

	return 1;
}

int vs_handle_taggroup_create_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd)
{
	struct VSNode *node;
	struct VSTagGroup *tg;
	struct VSEntityFollower *tg_foll;
	struct TagGroup_Create_Ack_Cmd *cmd_tg_create_ack = (struct TagGroup_Create_Ack_Cmd*)cmd;
	int all_created = 1, ret = 0;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, cmd_tg_create_ack->node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, cmd_tg_create_ack->node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	if( (tg = vs_taggroup_find(node, cmd_tg_create_ack->taggroup_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() tag_group (id: %d) in node (id: %d) not found\n",
				__FUNCTION__,
				cmd_tg_create_ack->taggroup_id,
				cmd_tg_create_ack->node_id);
	} else {

		ret = 1;

		for(tg_foll = tg->tg_folls.first;
				tg_foll != NULL;
				tg_foll = tg_foll->next)
		{
			/* Try to find follower of this tag group */
			if(tg_foll->node_sub->session->session_id == vsession->session_id) {
				/* Switch from state CREATING to state CREATED */
				if(tg_foll->state == ENTITY_CREATING) {
					tg_foll->state = ENTITY_CREATED;
				}

				/* If the tag group is in the state DELETING, then it is possible
				 * now to sent tag_group_destroy command to the client, because
				 * the client knows about this tag group now */
				if(tg->state == ENTITY_DELETING) {
					struct Generic_Cmd *taggroup_destroy_cmd = v_taggroup_destroy_create(node->id, tg->id);

					/* Push this command to the outgoing queue */
					if ( taggroup_destroy_cmd!= NULL &&
							v_out_queue_push_tail(tg_foll->node_sub->session->out_queue,
									tg_foll->node_sub->prio,
									taggroup_destroy_cmd) == 1) {
						tg_foll->state = ENTITY_DELETING;
					} else {
						v_print_log(VRS_PRINT_DEBUG_MSG,
								"taggroup_destroy (node_id: %d, tg_id: %d) wasn't added to the queue\n",
								node->id, tg->id);
						ret = 0;
					}
				}
			} else {
				if(tg_foll->state != ENTITY_CREATED) {
					all_created = 0;
				}
			}
		}

		if(all_created == 1) {
			tg->state = ENTITY_CREATED;
		}
	}

	pthread_mutex_unlock(&node->mutex);

	return ret;
}

/**
 * \brief This function tries to handle node_create command
 */
int vs_handle_taggroup_create(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *taggroup_create_cmd)
{
	struct VSNode			*node;
	struct VSTagGroup		*tg;
	struct VBucket			*vbucket;
	uint32 					node_id = UINT32(taggroup_create_cmd->data[0]);
	uint16 					taggroup_id = UINT16(taggroup_create_cmd->data[UINT32_SIZE]);
	uint16 					type = UINT16(taggroup_create_cmd->data[UINT32_SIZE+UINT16_SIZE]);
	int						ret = 0;

	/* Client has to send taggroup_create command with taggroup_id equal to
	 * the value 0xFFFF */
	if(taggroup_id != VRS_RESERVED_TAGGROUP_ID) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() taggroup_id: %d is not 0xFFFF\n",
				__FUNCTION__, taggroup_id);
		return 0;
	}

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* Node has to be created */
	if( vs_node_is_created(node) != 1 ) {
		goto end;
	}

	vbucket = node->tag_groups.lb.first;
	/* Check, if there isn't tag group with the same type */
	while(vbucket != NULL) {
		tg = vbucket->data;
		if(tg->custom_type == type) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"%s() taggroup type: %d is already used in node: %d\n",
					__FUNCTION__, type, node->id);
			goto end;
		}
		vbucket = vbucket->next;
	}

	/* Is user owner of this node or can user write to this node? */
	if(vs_node_can_write(vsession, node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s(): user: %s can't write to node: %d\n",
				__FUNCTION__,
				((struct VSUser *)vsession->user)->username,
				node->id);
		goto end;
	}

	/* Try to create new tag group */
	tg = vs_taggroup_create(node, VRS_RESERVED_TAGGROUP_ID, type);
	if(tg == NULL) {
		goto end;
	} else {
		struct VSNodeSubscriber *node_subscriber;

		/* Set state for this entity */
		tg->state = ENTITY_CREATING;

		ret = 1;

		/* Send tag group create command to all subscribers to the node
		 * that can read this node */
		for(node_subscriber = node->node_subs.first;
				node_subscriber != NULL;
				node_subscriber = node_subscriber->next)
		{
			if( vs_node_can_read(node_subscriber->session, node) == 1) {
				if(vs_taggroup_send_create(node_subscriber, node, tg) != 1) {
					ret = 0;
				}
			}
		}
	}

end:

	pthread_mutex_unlock(&node->mutex);

	return ret;
}

/**
 * \brief This function tries to handle node_create command
 */
int vs_handle_taggroup_destroy(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *taggroup_destroy_cmd)
{
	struct VSNode			*node;
	struct VSTagGroup		*tg;
	uint32 					node_id = UINT32(taggroup_destroy_cmd->data[0]);
	uint16 					taggroup_id = UINT16(taggroup_destroy_cmd->data[UINT32_SIZE]);
	int						ret = 0;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* Node has to be created */
	if( vs_node_is_created(node) != 1 ) {
		goto end;
	}

	/* Is user owner of this node? */
	if(vs_node_can_write(vsession, node) == 1) {
		/* Try to find TagGroup */
		if( (tg = vs_taggroup_find(node, taggroup_id)) == NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"%s() tag_group (id: %d) in node (id: %d) not found\n",
					__FUNCTION__, taggroup_id, node_id);
			goto end;
		}

		ret = vs_taggroup_send_destroy(node, tg);
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s(): user: %s can't write to node: %d\n",
				__FUNCTION__,
				((struct VSUser *)vsession->user)->username,
				node->id);
	}

end:

	pthread_mutex_unlock(&node->mutex);

	return ret;
}

/**
 * \brief This function handle node_subscribe command
 */
int vs_handle_taggroup_subscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *taggroup_subscribe)
{
	struct VSNode	*node;
	uint32			node_id = UINT32(taggroup_subscribe->data[0]);
	uint16			taggroup_id = UINT16(taggroup_subscribe->data[UINT32_SIZE]);
	int				ret = 0;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* Node has to be created */
	if( vs_node_is_created(node) != 1 ) {
		goto end;
	}

	/* Is user owner of the node or can user read the node? */
	if(vs_node_can_read(vsession, node) == 1) {
		struct VSNodeSubscriber		*node_subscriber;
		struct VSTagGroup			*tg;
		struct VSTag				*tag;
		struct VSEntitySubscriber	*tg_subscriber;
		struct VBucket				*bucket;

		/* Try to find node subscriber */
		node_subscriber = node->node_subs.first;
		while(node_subscriber != NULL) {
			if(node_subscriber->session == vsession) {
				break;
			}
			node_subscriber = node_subscriber->next;
		}

		/* Client has to be subscribed to the node first */
		if(node_subscriber == NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"%s(): client has to be subscribed to the node: %d before subscribing to the tag_group: %d\n",
					__FUNCTION__, node_id, taggroup_id);
			goto end;
		}

		/* Try to find TagGroup */
		if( (tg = vs_taggroup_find(node, taggroup_id)) == NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"%s() tag_group (id: %d) in node (id: %d) not found\n",
					__FUNCTION__, taggroup_id, node_id);
			goto end;
		}

		/* Is Client already subscribed to this tag group? */
		tg_subscriber = tg->tg_subs.first;
		while(tg_subscriber != NULL) {
			if(tg_subscriber->node_sub->session->session_id == vsession->session_id) {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"%s() client already subscribed to the tag_group (id: %d) in node (id: %d)\n",
						__FUNCTION__, taggroup_id, node_id);
				goto end;
			}
			tg_subscriber = tg_subscriber->next;
		}

		ret = 1;

		/* Add new subscriber to the list of tag group subscribers */
		tg_subscriber = (struct VSEntitySubscriber*)malloc(sizeof(struct VSEntitySubscriber));
		tg_subscriber->node_sub = node_subscriber;
		v_list_add_tail(&tg->tg_subs, tg_subscriber);

		/* Try to send tag_create for all tags in this tag group */
		bucket = tg->tags.lb.first;
		while(bucket != NULL) {
			tag = (struct VSTag*)bucket->data;
			if(vs_tag_send_create(tg_subscriber, node, tg, tag) == -1) {
				/* TODO: create sending task */
				break;
			}
			bucket = bucket->next;
		}
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s(): user: %s doesn't have permissions to subscribe to taggroup: %d in node: %d\n",
				__FUNCTION__,
				((struct VSUser *)vsession->user)->username,
				taggroup_id, node->id);
	}

end:

	pthread_mutex_unlock(&node->mutex);

	return ret;
}

/**
 * \brief This function handle taggroup_unsubscribe command
 */
int vs_handle_taggroup_unsubscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *taggroup_unsubscribe)
{
	struct VSNode				*node;
	struct VSTagGroup			*tg;
	uint32						node_id = UINT32(taggroup_unsubscribe->data[0]);
	uint16						taggroup_id = UINT16(taggroup_unsubscribe->data[UINT32_SIZE]);
	int							ret = 0;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* Node has to be created */
	if( vs_node_is_created(node) != 1 ) {
		ret = 0;
	} else {
		/* Try to find TagGroup */
		if( (tg = vs_taggroup_find(node, taggroup_id)) == NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "%s() tag_group (id: %d) in node (id: %d) not found\n",
					__FUNCTION__, taggroup_id, node_id);
			ret = 0;
		} else {
			ret = vs_taggroup_unsubscribe(tg, vsession);
		}
	}

	pthread_mutex_unlock(&node->mutex);

	return ret;
}
