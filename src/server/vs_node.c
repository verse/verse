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
#include "v_node_commands.h"

#include "vs_main.h"
#include "vs_node.h"
#include "vs_sys_nodes.h"
#include "vs_node_access.h"
#include "vs_link.h"
#include "vs_entity.h"
#include "vs_taggroup.h"
#include "vs_tag.h"
#include "vs_layer.h"

#include "v_fake_commands.h"

static int vs_node_send_destroy(struct VSNode *node);

/**
 * \brief This function tries to find node subscriber corresponding to the
 * session. When subscriber is not foun, then return NULL
 */
static struct VSNodeSubscriber* vs_node_get_subscriber(struct VSNode *node,
		struct VSession *vsession)
{
	struct VSNodeSubscriber *node_subscriber = node->node_subs.first;

	while(node_subscriber != NULL) {
		if(node_subscriber->session->session_id == vsession->session_id) {
			break;
		}
		node_subscriber = node_subscriber->next;
	}

	return node_subscriber;
}

/**
 * \brief This function do recursive setting new node priority
 */
static int vs_node_prio(struct VSession *vsession,
		struct VSNode *node,
		uint8 prio)
{
	struct VSNodeSubscriber *node_subscriber;
	struct VSNode *child_node;
	struct VSLink *link;

	/* Is client subscribed to this node? */
	node_subscriber = vs_node_get_subscriber(node, vsession);

	/* If client is subscribed, then set up new priority */
	if(node_subscriber != NULL) {
		node_subscriber->prio = prio;

		/* Do recursive subscribing to child nodes */
		link = node->children_links.first;
		while(link != NULL) {
			child_node = link->child;
			vs_node_prio(vsession, child_node, prio);
			link = link->next;
		}
	} else {
		return 0;
	}

	return 1;
}

/**
 * \brief This function do recursive unsubscribing from node
 */
static int vs_node_unsubscribe(struct VSNode *node,
		struct VSNodeSubscriber *node_subscriber,
		int level)
{
	struct VSNode *child_node;
	struct VSLink *link;
	struct VBucket *tg_bucket, *layer_bucket;
	struct VSTagGroup *tg;
	struct VSLayer *layer;
	struct VSNodeSubscriber *_node_subscriber, *_next_node_subscriber;
	struct VSEntityFollower *node_follower;
	struct VSEntityFollower	*taggroup_follower;
	struct VSEntityFollower	*layer_follower;

	/* Unsubscribe from all child nodes */
	link = node->children_links.first;
	while(link != NULL) {
		child_node = link->child;

		_node_subscriber = child_node->node_subs.first;
		while(_node_subscriber != NULL) {
			_next_node_subscriber = _node_subscriber->next;
			if(_node_subscriber->session->session_id == node_subscriber->session->session_id) {
				/* Unsubscribe from child node */
				vs_node_unsubscribe(child_node, _node_subscriber, level+1);
				break;
			}
			_node_subscriber = _next_node_subscriber;
		}

		link = link->next;
	}

	/* Unsubscribe client from all tag groups */
	tg_bucket = node->tag_groups.lb.first;
	while(tg_bucket != NULL) {
		tg = (struct VSTagGroup*)tg_bucket->data;

		/* Remove client from the list of TagGroup subscribers */
		vs_taggroup_unsubscribe(tg, node_subscriber->session);

		/* Remove client from the list of TagGroup followers */
		taggroup_follower = tg->tg_folls.first;
		while(taggroup_follower != NULL) {
			if(taggroup_follower->node_sub->session->session_id == node_subscriber->session->session_id) {
				v_list_free_item(&tg->tg_folls, taggroup_follower);
				break;
			}
			taggroup_follower = taggroup_follower->next;
		}

		tg_bucket = tg_bucket->next;
	}

	/* Unsubscribe client from all layers */
	layer_bucket = node->layers.lb.first;
	while(layer_bucket != NULL) {
		layer = (struct VSLayer*)layer_bucket->data;

		/* Remove client from the list of Layer subscribers */
		vs_layer_unsubscribe(layer, node_subscriber->session);

		/* Remove client from the list of Layer followers */
		layer_follower = layer->layer_folls.first;
		while(layer_follower != NULL) {
			if(layer_follower->node_sub->session->session_id == node_subscriber->session->session_id) {
				v_list_free_item(&layer->layer_folls, layer_follower);
				break;
			}
			layer_follower = layer_follower->next;
		}
		layer_bucket = layer_bucket->next;
	}

	if(level > 0) {
		/* Remove this session from list of followers too */
		node_follower = node->node_folls.first;
		while(node_follower != NULL) {
			if(node_follower->node_sub->session->session_id == node_subscriber->session->session_id) {
				/* Remove client from list of clients, that knows about this node */
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"Removing session: %d from the list of node: %d followers\n",
						node_subscriber->session->session_id, node->id);
				v_list_free_item(&node->node_folls, node_follower);
				break;
			}
			node_follower = node_follower->next;
		}
	}

	/* Finally remove this session from list of node subscribers */
	v_list_free_item(&node->node_subs, node_subscriber);

	return 1;
}

/**
 * \brief This function add session (client) to the list of clients that are
 * subscribed this node.
 */
static int vs_node_subscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct VSNode *node,
		uint32 version)
{
	struct VSNode				*child_node;
	struct VSNodePermission		*perm;
	struct VSNodeSubscriber		*node_subscriber;
	struct VSLink				*link;
	struct VBucket				*bucket;
	struct VSTagGroup			*tg;
	struct VSLayer				*layer;
	int							user_can_read = 0;

	/* Can user subscribe to this node? */
	user_can_read = vs_node_can_read(vs_ctx, vsession, node);

	/* Add current session to the list of node subscribers */
	node_subscriber = (struct VSNodeSubscriber*)calloc(1, sizeof(struct VSNodeSubscriber));
	node_subscriber->session = vsession;
	node_subscriber->prio = VRS_DEFAULT_PRIORITY;
	v_list_add_tail(&node->node_subs, node_subscriber);

	/* TODO: send node_subscribe with version and commands with difference
	 * between this version and current state, when versing will be supported */
	if(version != 0) {
		v_print_log(VRS_PRINT_WARNING,
				"Version: %d != 0, versing is not supported yet\n", version);
	}

	/* Send node_perm commands to the new subscriber */
	perm = node->permissions.first;
	while(perm != NULL) {
		vs_node_send_perm(node_subscriber, node, perm->user, perm->permissions);
		perm = perm->next;
	}

	/* If the node is locked, then send node_lock to the subscriber */
	if(node->lock.session != NULL) {
		vs_node_send_lock(node_subscriber, node->lock.session, node);
	}

	/* If user doesn't have permission to subscribe to this node, then send
	 * only node_perm command explaining, why user can't rest of this
	 * node */
	if(user_can_read == 0) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Insufficient permission to read content of the node: %d\n",
				node->id);
		return 0;
	}

	/* Send node_create of all child nodes of this node and corresponding
	 * links */
	link = node->children_links.first;
	while(link != NULL) {
		child_node = link->child;
		vs_node_send_create(node_subscriber, child_node, NULL);
		link = link->next;
	}

	/* Send taggroup_create of all tag_groups in this node */
	bucket = node->tag_groups.lb.first;
	while(bucket != NULL) {
		tg = (struct VSTagGroup*)bucket->data;
		if(tg->state == ENTITY_CREATING || tg->state == ENTITY_CREATED) {
			vs_taggroup_send_create(node_subscriber, node, tg);
		}
		bucket = bucket->next;
	}

	/* Send layer_create for all layers in this node */
	bucket = node->layers.lb.first;
	while(bucket != NULL) {
		layer = (struct VSLayer*)bucket->data;
		if(layer->state == ENTITY_CREATING || layer->state == ENTITY_CREATED) {
			vs_layer_send_create(node_subscriber, node, layer);
		}
		bucket = bucket->next;
	}

	return 1;
}

/**
 * \brief This function increments version of node
 */
void vs_node_inc_version(struct VSNode *node)
{
	/* TODO: compute CRC32 */
	if( (node->version + 1) < UINT32_MAX) {
		node->version++;
	} else {
		node->version = 1;
		node->saved_version = 0;
	}
}

/**
 * \brief This function sends Node_Create command to the subscriber of parent
 * node.
 */
int vs_node_send_create(struct VSNodeSubscriber *node_subscriber,
		struct VSNode *node,
		struct VSNode *avatar_node)
{
	struct Generic_Cmd		*node_create_cmd = NULL;
	struct VSEntityFollower	*node_follower;

	/* Check if this node is in created/creating state */
	if(!(node->state == ENTITY_CREATING || node->state == ENTITY_CREATED)) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"This node: %d is in %d state\n", node->id, node->state);
		return 0;
	}

	/* Check if this command, has not been already sent */
	node_follower = node->node_folls.first;
	while(node_follower != NULL) {
		if(node_follower->node_sub->session->session_id == node_subscriber->session->session_id &&
				(node_follower->state == ENTITY_CREATING ||
				 node_follower->state == ENTITY_CREATED))
		{
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Client already knows about node: %d\n", node->id);
			return 0;
		}
		node_follower = node_follower->next;
	}

	if(avatar_node != NULL){
		node_create_cmd = v_node_create_create(node->id, avatar_node->id, node->owner->user_id, node->type);
	} else {
		node_create_cmd = v_node_create_create(node->id, node->parent_link->parent->id, node->owner->user_id, node->type);
	}

	if ( node_create_cmd != NULL &&
			v_out_queue_push_tail(node_subscriber->session->out_queue,
					node_subscriber->prio,
					node_create_cmd) == 1)
	{
		/* Add this session to the list of session, that knows about this
		 * node. Server could send them node_destroy in the future. */
		node_follower = (struct VSEntityFollower*)calloc(1, sizeof(struct VSEntityFollower));
		node_follower->node_sub = node_subscriber;
		node_follower->state = ENTITY_CREATING;
		v_list_add_tail(&node->node_folls, node_follower);

		return 1;
	}

	return 0;
}

/**
 * \brief Try to find node using node_id
 *
 * \param[in] *vs_ctx	The pointer at server context
 * \param[in] node_id	The ID of node
 *
 * \return This function return pointer at VSNode, when node is found.
 * Otherwise, it returns NULL pointer.
 */
struct VSNode *vs_node_find(struct VS_CTX *vs_ctx, uint32 node_id)
{
	struct VSNode find_node;
	struct VBucket *bucket;
	struct VSNode *node = NULL;

	find_node.id = node_id;
	bucket = v_hash_array_find_item(&vs_ctx->data.nodes, &find_node);
	if(bucket != NULL) {
		node = (struct VSNode *)bucket->data;
	}

	return node;
}

/**
 * \brief This function initialize structure intended for storing node at server.
 */
void vs_node_init(struct VSNode *node)
{
	int res;

	node->id = 0xFFFFFFFF;
	node->type = 0;

	node->owner = NULL;
	node->permissions.first = NULL;
	node->permissions.last = NULL;

	node->parent_link = NULL;
	node->children_links.first = NULL;
	node->children_links.last = NULL;

	/* Hashed linked list of TagGroups */
	v_hash_array_init(&node->tag_groups,
			HASH_MOD_256,
			offsetof(VSTagGroup, id),
			sizeof(uint16));
	node->last_tg_id = 0;

	/* Hashed linked list of layers */
	v_hash_array_init(&node->layers,
			HASH_MOD_256,
			offsetof(VSLayer, id),
			sizeof(uint16));
	node->last_layer_id = 0;

	node->node_folls.first = NULL;
	node->node_folls.last = NULL;

	node->node_subs.first = NULL;
	node->node_subs.last = NULL;

	node->lock.session = NULL;

	node->state = ENTITY_RESERVED;
	node->flags = 0;

	/* Initialize mutex of this node */
	if((res = pthread_mutex_init(&node->mutex, NULL)) != 0) {
		/* This function always return 0 on Linux */
		if(is_log_level(VRS_PRINT_ERROR)) {
			v_print_log(VRS_PRINT_ERROR, "pthread_mutex_init(): %d\n", res);
		}
	}

	node->version = 0;
	node->saved_version = -1;
	node->crc32 = 0;

}

/**
 * \brief This function creates new VSNode at Verse server
 */
struct VSNode *vs_node_create_linked(struct VS_CTX *vs_ctx,
		struct VSNode *parent_node,
		struct VSUser *owner,
		uint32 node_id,
		uint16 custom_type)
{
	struct VSNode *node;
	struct VSLink *link;
	struct VBucket *bucket;

	if(! (v_hash_array_count_items(&vs_ctx->data.nodes) < VRS_MAX_COMMON_NODE_COUNT) ) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"max number: %d of nodes reached\n",
				VRS_MAX_COMMON_NODE_COUNT);
		return NULL;
	}

	node = (struct VSNode*)calloc(1, sizeof(struct VSNode));
	if(node == NULL) {
		v_print_log(VRS_PRINT_ERROR, "Out of memory\n");
		return NULL;
	}

	vs_node_init(node);

	if(node_id == VRS_RESERVED_NODE_ID) {
		/* Try to find first free node_id. It is fast and easy, When
		 * VRS_LAST_COMMON_NODE_ID did not reach 0xFFFFFFFF-1 value yet and not used
		 * node_id are not reused.*/
		node->id = vs_ctx->data.last_common_node_id + 1;
		while( v_hash_array_find_item(&vs_ctx->data.nodes, &node) != NULL) {
			node->id++;
			/* Node id 0xFFFFFFFF has special purpose and node IDs in range <0, 65535>
			 * have special purposes too (skip them) */
			if(node->id > VRS_LAST_COMMON_NODE_ID)
				node->id = VRS_FIRST_COMMON_NODE_ID;
			/* TODO: implement faster finding of free node id */
		}
		vs_ctx->data.last_common_node_id = node->id;
	} else {
		node->id = node_id;
	}

	/* Create link to the parent node */
	if(parent_node != NULL) {
		link = vs_link_create(parent_node, node);
		if(link == NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"link between nodes %d %d could not be created\n",
					parent_node->id, node->id);
			free(node);
			return NULL;
		}
	} else {
		/* This can happen only for root node */
		assert(node_id == VRS_ROOT_NODE_ID);
		node->parent_link = NULL;
		node->level = 0;
	}

	/* Add node to the hashed array of all verse nodes */
	bucket = v_hash_array_add_item(&vs_ctx->data.nodes, node, sizeof(struct VSNode));
	if(bucket == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"node %d could not be added to the hashed list of nodes\n",
				node->id);
		if(node->parent_link != NULL) {
			v_list_free_item(&parent_node->children_links, node->parent_link);
		}
		free(node);
		return NULL;
	}

	node->owner = owner;
	node->type = custom_type;

	return node;
}

/**
 * \brief This function will try to remove node from the server. The node can't
 * have any child node or subscriber.
 */
static int vs_node_destroy(struct VS_CTX *vs_ctx, struct VSNode *node)
{
	/* Node can't have any followers. The VSNode can be destroyed, when server
	 * receive ack of node_destroy command from all clients */
	if(node->node_folls.first == NULL) {
		/* Node can't have any child node */
		if(node->children_links.first == NULL) {

			/* Remove node permissions */
			if(node->permissions.first != NULL) {
				v_list_free(&node->permissions);
			}

			/* Remove link on this node from parent node */
			if(node->parent_link != NULL) {
				struct VSNode *parent_node = node->parent_link->parent;
				v_list_free_item(&parent_node->children_links, node->parent_link);
			}

			/* Remove all tag groups and tags */
			if(node->tag_groups.lb.first != NULL) {
				vs_node_taggroups_destroy(node);
			}
			v_hash_array_destroy(&node->tag_groups);

			/* Remove all layers */
			if(node->layers.lb.first != NULL) {
				vs_node_layers_destroy(node);
			}
			v_hash_array_destroy(&node->layers);

			v_print_log(VRS_PRINT_DEBUG_MSG, "Node: %d destroyed\n", node->id);

			/* Remove node from the hashed linked list of nodes */
			v_hash_array_remove_item(&vs_ctx->data.nodes, node);
			free(node);

			return 1;
		} else {
			/* This should never happen */
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"%s(): node (id: %d) with child nodes can't be destroyed\n",
					__FUNCTION__, node->id);
			return 0;
		}
	} else {
		/* This should never happen */
		v_print_log(VRS_PRINT_WARNING,
				"%(): node (id: %d) with followers can't be destroyed\n",
				__FUNCTION__, node->id);
		return 0;
	}
}


/**
 * \brief This function destroy branch of nodes
 */
int vs_node_destroy_branch(struct VS_CTX *vs_ctx,
		struct VSNode *node,
		uint8 send)
{
	struct VSNode *child_node;
	struct VSEntityFollower *node_follower;
	struct VSLink *link, *next_link;
	int ret = 0;

	link = node->children_links.first;

	/* Remove all sub-branches */
	while(link != NULL) {
		child_node = link->child;
		next_link = link->next;
		vs_node_destroy_branch(vs_ctx, child_node, send);
		link = next_link;
	}

	node_follower = node->node_folls.first;
	if(node_follower != NULL && send == 1) {
		/* Send node_destroy command to all clients that knows about this node */
		ret = vs_node_send_destroy(node);

		/* Own node will be destroyed, when verse server receive confirmation
		 * of receiving node_destroy command */
	} else {
		/* When all clients consider this node as destroyed, then it is
		 * possible to remove it from server */
		ret = vs_node_destroy(vs_ctx, node);
	}

	return ret;
}

/**
 * \brief This function send Destroy_Node command to the all clients that
 * know about this node.
 *
 * This function only send Node_Destroy command. Own node destruction is done,
 * when all clients confirmed receiving of Node_Destroy command.
 */
static int vs_node_send_destroy(struct VSNode *node)
{
	struct VSEntityFollower *node_follower;
	struct Generic_Cmd *node_destroy_cmd=NULL;
	int ret = 0;

	/* Has node any child? */
	if(node->children_links.first != NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "node (id: %d) has children\n",
				node->id);
		goto end;
	}

	/* Send node_destroy to all clients, that knows about this node. These
	 * clients received node_create command. */
	node_follower = node->node_folls.first;
	while(node_follower!=NULL) {
		/* Node has to be in CREATED state */
		if(node_follower->state == ENTITY_CREATED) {
			/* Create Destroy_Node command */
			node_destroy_cmd = v_node_destroy_create(node->id);

			/* Push this command to the outgoing queue */
			if ( node_destroy_cmd!= NULL &&
					v_out_queue_push_tail(node_follower->node_sub->session->out_queue,
							node_follower->node_sub->prio,
							node_destroy_cmd) == 1) {
				node_follower->state = ENTITY_DELETING;
				ret = 1;
			} else {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"node_destroy (id: %d) wasn't added to the queue\n",
						node->id);
			}
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Can't delete node %d, because it isn't in CREATED state\n");
		}
		node_follower = node_follower->next;
	}

	node->state = ENTITY_DELETING;

end:
	return ret;
}

/**
 * \brief This function creates new node at verse server, when client sent
 * node_create command.
 */
static struct VSNode *vs_node_new(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		const uint16 type)
{
	struct VSNode *node = NULL;
	struct VSNode find_node, *avatar_node;
	struct VSUser *owner;
	struct VBucket *bucket;
	struct VSNodePermission *perm;
	struct VSNodeSubscriber *node_subscriber;

	/* Try to find avatar node to be able to create initial link to
	 * avatar node (initial parent of new created node) */
	find_node.id = vsession->avatar_id;
	bucket = v_hash_array_find_item(&vs_ctx->data.nodes, &find_node);
	if(bucket != NULL) {
		avatar_node = (struct VSNode*)bucket->data;
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"vsession->avatar_id: %d not found\n", vsession->avatar_id);
		goto end;
	}

	/* Try to find owner of the new node */
	if((owner = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"vsession->user_id: %d not found\n", vsession->user_id);
		goto end;
	}

	/* Try to create new verse node */
	if( (node = vs_node_create_linked(vs_ctx, avatar_node, owner, VRS_RESERVED_NODE_ID, type)) == NULL) {
		goto end;
	}

	/* Set initial state of this node */
	node->state = ENTITY_CREATING;

	/* Find node representing fake user other_users */
	if( vs_ctx->other_users != NULL) {
		/* Set access permissions for other users */
		perm = (struct VSNodePermission *)calloc(1, sizeof(struct VSNodePermission));
		perm->user = vs_ctx->other_users;

		/* TODO: implement default session permissions and use them,
		 * when are available */

		perm->permissions = vs_ctx->default_perm;
		v_list_add_tail(&node->permissions, perm);
	}

	/* Send node_create to all subscribers of avatar node data */
	node_subscriber = avatar_node->node_subs.first;
	while(node_subscriber) {
		vs_node_send_create(node_subscriber, node, avatar_node);
		node_subscriber = node_subscriber->next;
	}


end:

	return node;
}

/**
 * \brief This function checks if node is in created/creating state and could
 * be used.
 */
int vs_node_is_created(struct VSNode *node)
{
	/* Node has to be created */
	if(! (node->state == ENTITY_CREATED || node->state == ENTITY_CREATING)) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"node, id: %d is not in CREATED/CREATING state: %d\n",
				__FUNCTION__, node->id, node->state);
		return 0;
	}

	return 1;
}

/**
 * \brief This function handle Node_Priority command
 */
int vs_handle_node_prio(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_prio)
{
	struct VSNode *node;
	struct VSNodeSubscriber *node_subscriber;
	uint8 prio = UINT8(node_prio->data[0]);
	uint32 node_id = UINT32(node_prio->data[UINT8_SIZE]);
	int ret = 1;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		ret = 0;
	}

	if(node != NULL) {
		/* Node has to be created */
		if(vs_node_is_created(node) == 1) {

			/* Is client subscribed to this node? */
			node_subscriber = vs_node_get_subscriber(node, vsession);

			/* When client is subscribed to this node, then change node priority */
			if(node_subscriber != NULL) {
				node_subscriber->prio = prio;
				/* Change priority for this node and all child nodes */
				vs_node_prio(vsession, node, prio);
			} else {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"%s() client not subscribed to this node (id: %d)\n",
						__FUNCTION__, node_id);
				ret = 0;
			}
		}
	}

	return ret;
}

/**
 * \brief This function handle node_unsubscribe command
 */
int vs_handle_node_unsubscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_unsubscribe)
{
	struct VSNode *node;
	struct VSNodeSubscriber *node_subscriber;
	uint32 node_id = UINT32(node_unsubscribe->data[0]);
	uint32 version = UINT32(node_unsubscribe->data[UINT32_SIZE]);
	/*uint32 crc32 = UINT32(node_unsubscribe->data[UINT32_SIZE + UINT32_SIZE]);*/

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* TODO: when versing will be supported, then compute crc32, save data to
	 * the disk and send node_unsubscribe command to the client with version number
	 * and crc32 */
	if(version != 0) {
		v_print_log(VRS_PRINT_WARNING,
				"Version: %d != 0, versing is not supported yet\n", version);
	}

	/* Node has to be created */
	if(vs_node_is_created(node) == 1) {
		/* Is client subscribed to this node? */
		node_subscriber = vs_node_get_subscriber(node, vsession);
		if(node_subscriber != NULL) {
			return vs_node_unsubscribe(node, node_subscriber, 0);
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"%s() client not subscribed to this node (id: %d)\n",
					__FUNCTION__, node_id);
			return 0;
		}
	} else {
		return 0;
	}

	return 1;
}


/**
 * \brief This function handle node_subscribe command
 */
int vs_handle_node_subscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_subscribe)
{
	struct VSNode *node;
	struct VSNodeSubscriber *node_subscriber;
	uint32 node_id = UINT32(node_subscribe->data[0]);
	uint32 version = UINT32(node_subscribe->data[UINT32_SIZE]);
	/*uint32 crc32 = UINT32(node_subscribe->data[UINT32_SIZE + UINT32_SIZE]);*/

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Node has to be created */
	if( vs_node_is_created(node) == 1) {

		/* Isn't client already subscribed to this node? */
		node_subscriber = vs_node_get_subscriber(node, vsession);

		if(node_subscriber != NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"%s() client %d is already subscribed to the node (id: %d)\n",
					__FUNCTION__, vsession->session_id, node->id);
		} else {
			return vs_node_subscribe(vs_ctx, vsession, node, version);
		}
	} else {
		return 0;
	}

	return 1;
}

/**
 * \brief This function is called, when server receive ack command of packet
 * that contained node_destroy command that was sent to the client
 *
 * When this function is called, then we can be sure, that client knows, that
 * this node was destroyed. When the last one of follower acknowledge receiving
 * of this command, then this node could be deleted
 */
int vs_handle_node_destroy_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd)
{
	struct VSNode *node;
	struct VSEntityFollower *node_follower, *next_node_follower;
	struct Node_Destroy_Ack_Cmd *node_destroy_ack = (struct Node_Destroy_Ack_Cmd*)cmd;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_destroy_ack->node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_destroy_ack->node_id);
		return 0;
	}

	/* Remove corresponding follower from the list of followers */
	node_follower = node->node_folls.first;
	while(node_follower != NULL) {
		next_node_follower = node_follower->next;
		if(node_follower->node_sub->session->session_id == vsession->session_id) {
			node_follower->state = ENTITY_DELETED;
			v_list_free_item(&node->node_folls, node_follower);
			break;
		}
		node_follower = next_node_follower;
	}

	/* When node doesn't have any follower, then it is possible to destroy this node*/
	if(node->node_folls.first == NULL) {
		node->state = ENTITY_DELETED;
		vs_node_destroy(vs_ctx, node);
	}

	return 1;
}

/**
 * \brief This function tries to handle node_destroy command
 */
int vs_handle_node_destroy(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_destroy)
{
	struct VSUser *user;
	struct VSNode *node;
	uint32 node_id = UINT32(node_destroy->data[0]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Node has to be created */
	if(vs_node_is_created(node) == 1) {

		/* Try to find user */
		if((user = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"vsession->user_id: %d not found\n", vsession->user_id);
			return 0;
		}

		/* Is this user owner of this node? */
		if(user != node->owner) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"user_id: %d is not owner of the node (id: %d) and can't delete this node.\n",
					vsession->user_id, node->id);
			return 0;
		}
	} else {
		return 0;
	}

	return vs_node_send_destroy(node);
}

/**
 * \brief This function is called, when server receive ack command of packet
 * that contained node_create command sent to the client
 *
 * When this function is called, then we can be sure, that client knows about
 * the node. This node can be switched to the NODE_CREATED state for the
 * follower of this node
 */
int vs_handle_node_create_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd)
{
	VSNode *node;
	struct VSEntityFollower *node_follower;
	struct Node_Create_Ack_Cmd *node_create_ack = (struct Node_Create_Ack_Cmd*)cmd;
	int all_created = 1;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_create_ack->node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_create_ack->node_id);
		return 0;
	}

	node_follower = node->node_folls.first;
	while(node_follower != NULL) {
		if(node_follower->node_sub->session->session_id == vsession->session_id) {

			if(node_follower->state == ENTITY_CREATING) {
				node_follower->state = ENTITY_CREATED;

				/* If the node is in state DELETING, then send to the client command
				 * node_delete. The client knows about this node now and can receive
				 * node_destroy command */
				if(node->state == ENTITY_DELETING) {
					/* Create Destroy_Node command */
					struct Generic_Cmd *node_destroy_cmd = v_node_destroy_create(node->id);

					/* Push this command to the outgoing queue */
					if ( node_destroy_cmd != NULL &&
							v_out_queue_push_tail(node_follower->node_sub->session->out_queue,
									node_follower->node_sub->prio,
									node_destroy_cmd) == 1)
					{
						node_follower->state = ENTITY_DELETING;
					} else {
						v_print_log(VRS_PRINT_DEBUG_MSG,
								"node_destroy (id: %d) wasn't added to the queue\n",
								node->id);
					}
				}
			} else {
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"node %d isn't in CREATING state\n");
			}
		} else {
			if(node_follower->state != ENTITY_CREATED) {
				all_created = 0;
			}
		}
		node_follower = node_follower->next;
	}

	/* When all followers know about this node, then change state of this node */
	if(all_created == 1) {
		node->state = ENTITY_CREATED;
	}

	return 1;
}

/**
 * \brief This function tries to handle node_create command
 */
int vs_handle_node_create(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_create)
{
	uint16 user_id = UINT16(node_create->data[0]);
	uint32 parent_id = UINT32(node_create->data[UINT16_SIZE]);
	uint32 node_id = UINT32(node_create->data[UINT16_SIZE+UINT32_SIZE]);
	uint16 custom_type = UINT16(node_create->data[UINT16_SIZE+UINT32_SIZE+UINT32_SIZE]);

	/* Client has to send node_create command with node_id equal to
	 * the value 0xFFFFFFFF */
	if(node_id != VRS_RESERVED_NODE_ID) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node_id is 0xFFFFFFFF\n",
				__FUNCTION__);
		return 0;
	}

	/* Client has to send node_create with parent_id equal to its avatar_id */
	if(parent_id != vsession->avatar_id) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() parent_id: %d is not equal to session avatar id %d\n",
				__FUNCTION__, parent_id, vsession->avatar_id);
		return 0;
	}

	/* Client has to send node_create command with his user_id */
	if(user_id != vsession->user_id) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() user_id: %d is not equal to session user id %d\n",
				__FUNCTION__, user_id, vsession->user_id);
		return 0;
	}

	/* Client has to send node_create command with custom_type bigger or
	 * equal 32, because lower values are reserved for special nodes */
	if( custom_type < 32 ) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() custom_type: %d is smaller then 32\n",
				__FUNCTION__, custom_type);
		return 0;
	}

	if(vs_node_new(vs_ctx, vsession, custom_type) != NULL) {
		return 1;
	} else {
		return 0;
	}

	return 0;
}

