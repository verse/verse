/*
 * $Id: vs_node.c 1348 2012-09-19 20:08:18Z jiri $
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

#include "verse_types.h"

#include "v_list.h"
#include "v_common.h"
#include "v_node_commands.h"

#include "vs_main.h"
#include "vs_node.h"
#include "vs_node_access.h"
#include "vs_link.h"
#include "vs_entity.h"
#include "vs_taggroup.h"
#include "vs_tag.h"
#include "vs_layer.h"
#include "v_fake_commands.h"

static int vs_node_send_destroy(struct VSNode *node);

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
	node_subscriber = node->node_subs.first;
	while(node_subscriber != NULL) {
		if(node_subscriber->session == vsession) {
			break;
		}
		node_subscriber = node_subscriber->next;
	}

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
	struct VBucket *tg_bucket;
	struct VSTagGroup *tg;
	struct VSNodeSubscriber *_node_subscriber;
	struct VSEntityFollower *node_follower;
	struct VSEntityFollower	*taggroup_follower;

	/* Unsubscribe from all child nodes */
	link = node->children_links.first;
	while(link != NULL) {
		child_node = link->child;

		_node_subscriber = child_node->node_subs.first;
		while(_node_subscriber != NULL) {
			if(_node_subscriber->session == node_subscriber->session) {
				/* Unsubscribe from child node */
				vs_node_unsubscribe(child_node, _node_subscriber, level+1);
				break;
			}
			_node_subscriber = _node_subscriber->next;
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
		while(taggroup_follower!=NULL) {
			if(taggroup_follower->node_sub->session->session_id == node_subscriber->session->session_id) {
				v_list_free_item(&tg->tg_folls, taggroup_follower);
				break;
			}
			taggroup_follower = taggroup_follower->next;
		}

		tg_bucket = tg_bucket->next;
	}

	/* TODO: Unsubscribe from all layers */

	if(level > 0) {
		/* Remove this session from list of followers too */
		node_follower = node->node_folls.first;
		while(node_follower != NULL) {
			if(node_follower->node_sub->session == node_subscriber->session) {
				/* Remove client from list of clients, that knows about this node */
				printf("\t>>>Removing session: %d from list of node: %d followers<<<\n",
						node_subscriber->session->session_id, node->id);
				v_list_free_item(&node->node_folls, node_follower);
				break;
			}
			node_follower = node_follower->next;
		}
	}

	printf(">>>Removing session: %d from list of node: %d subscribers<<<\n",
			node_subscriber->session->session_id, node->id);
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
	 * between this version and current state, when versioning will be supported */
	if(version != 0) {
		v_print_log(VRS_PRINT_WARNING, "Version: %d != 0, versioning is not supported yet\n", version);
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
		v_print_log(VRS_PRINT_DEBUG_MSG, "Insufficient permission to read content of the node: %d\n", node->id);
		return 0;
	}

	/* Send node_create of all child nodes of this node and corresponding
	 * links */
	link = node->children_links.first;
	while(link!=NULL) {
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
		v_print_log(VRS_PRINT_DEBUG_MSG, "This node: %d is in %d state\n", node->id, node->state);
		return 0;
	}

	/* Check if this command, has not been already sent */
	node_follower = node->node_folls.first;
	while(node_follower!=NULL) {
		if(node_follower->node_sub->session->session_id == node_subscriber->session->session_id &&
				(node_follower->state==ENTITY_CREATING ||
				 node_follower->state==ENTITY_CREATED))
		{
			v_print_log(VRS_PRINT_DEBUG_MSG, "Client already knows about node: %d\n", node->id);
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
static void vs_node_init(struct VSNode *node)
{
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

}

/**
 * \brief This function creates new VSNode at Verse server
 */
struct VSNode *vs_node_create(struct VS_CTX *vs_ctx,
		struct VSNode *parent_node,
		struct VSUser *owner,
		uint16 custom_type)
{
	struct VSNode *node;
	struct VSLink *link;

	node = (struct VSNode*)calloc(1, sizeof(struct VSNode));
	if(node == NULL) {
		v_print_log(VRS_PRINT_ERROR, "Out of memory\n");
		return NULL;
	}

	vs_node_init(node);

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

	link = vs_link_create(parent_node, node);
	if(link == NULL) {
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
			v_print_log(VRS_PRINT_DEBUG_MSG, "%s(): node (id: %d) with child nodes can't be destroyed\n",
					__FUNCTION__, node->id);
			return 0;
		}
	} else {
		/* This should never happen */
		v_print_log(VRS_PRINT_WARNING, "%(): node (id: %d) with followers can't be destroyed\n",
				__FUNCTION__, node->id);
		return 0;
	}
}

/**
 * \brief This function "unsubscribe" avatar from all nodes (node subscription
 * and node data subscription). It removes avatar from all lists of followers.
 * It also remove all node locks.
 */
int vs_node_free_avatar_reference(struct VS_CTX *vs_ctx,
		struct VSession *session)
{
	struct VBucket *node_bucket, *tg_bucket, *tag_bucket, *layer_bucket;
	struct VSNode *node;
	struct VSTagGroup *tg;
	struct VSTag *tag;
	struct VSLayer *layer;
	struct VSNodeSubscriber *node_subscriber;
	struct VSEntitySubscriber *tg_subscriber, *layer_subscriber;
	struct VSEntityFollower *node_follower, *tg_follower, *tag_follower, *layer_follower;
	int was_locked;

	/* TODO: It is necessary to go through all nodes now. :-/
	 * Do this more efficient; e.g.: every session could have list of
	 * subscribed nodes. */
	node_bucket = vs_ctx->data.nodes.lb.first;
	while(node_bucket != NULL) {
		node = (struct VSNode*)node_bucket->data;
		was_locked = 0;

		/* Does client know about this node */
		node_follower = node->node_folls.first;
		while(node_follower != NULL) {
			if(node_follower->node_sub->session->session_id == session->session_id) {
				/* Remove client from list of clients, that knows about this node */
				v_list_free_item(&node->node_folls, node_follower);

				/* Continue with next node */
				break;

			}
			node_follower = node_follower->next;
		}

		/* Was node locked by this client? */
		if(node->lock.session == session) {
			was_locked = 1;
			node->lock.session = NULL;
		}

		/* Is client subscribed to this node? */
		node_subscriber = node->node_subs.first;
		while(node_subscriber != NULL) {
			if(node_subscriber->session->session_id == session->session_id) {
				v_list_free_item(&node->node_subs, node_subscriber);
				if(was_locked == 0) {
					/* No need to go through other subscribers */
					break;
				}
			} else {
				if(was_locked == 1) {
					vs_node_send_unlock(node_subscriber, session, node);
				}
			}
			node_subscriber = node_subscriber->next;
		}

		/* Remove client from list of tag_group subscribers */
		tg_bucket = node->tag_groups.lb.first;
		while(tg_bucket != NULL) {
			tg = (struct VSTagGroup*)tg_bucket->data;
			tg_follower = tg->tg_folls.first;
			while(tg_follower != NULL) {
				if(tg_follower->node_sub->session->session_id == session->session_id) {
					/* Remove client from list of clients, that knows about this node */
					v_list_free_item(&tg->tg_folls, tg_follower);

					/* When client knows to this tag_group, then it's possibly
					 * subscribed to this tag_group */
					tg_subscriber = tg->tg_subs.first;
					while(tg_subscriber != NULL) {
						if(tg_subscriber->node_sub->session->session_id == session->session_id) {
							/* Remove client from list of clients, that are subscribed to this tag_group */
							v_list_free_item(&tg->tg_subs, tg_subscriber);
							/* No need to go through other tag_groups */
							break;
						}
						tg_subscriber = tg_subscriber->next;
					}
					/* Continue with next tag_group */
					break;
				}
				tg_follower = tg_follower->next;
			}

			/* Go through all Tags and remove client from list of tag followers */
			tag_bucket = tg->tags.lb.first;
			while(tag_bucket != NULL) {
				tag = (struct VSTag*)tag_bucket->data;

				tag_follower = tag->tag_folls.first;
				while(tag_follower != NULL) {
					if(tag_follower->node_sub->session->session_id == session->session_id) {
						v_list_free_item(&tag->tag_folls, tag_follower);
						break;
					}
					tag_follower = tag_follower->next;
				}
				tag_bucket = tag_bucket->next;
			}

			tg_bucket = tg_bucket->next;
		}

		/* Remove client from layer followers and subscribers,
		 * when layers will be implemented */
		layer_bucket = node->layers.lb.first;
		while(layer_bucket != NULL) {
			layer = (struct VSLayer*)layer_bucket->data;

			/* Remove client from layer follower */
			layer_follower = layer->layer_folls.first;
			while(layer_follower != NULL) {
				if(layer_follower->node_sub->session->session_id == session->session_id) {
					v_list_free_item(&layer->layer_folls, layer_follower);
					break;
				}
				layer_follower = layer_follower->next;
			}

			/* Remove client from layer subscriber */
			layer_subscriber = layer->layer_subs.first;
			while(layer_subscriber != NULL) {
				if(layer_subscriber->node_sub->session->session_id == session->session_id) {
					v_list_free_item(&layer->layer_subs, layer_subscriber);
					break;
				}
				layer_subscriber = layer_subscriber->next;
			}

			layer_bucket = layer_bucket->next;
		}


		node_bucket = node_bucket->next;
	}

	return 1;
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

	v_list_free(&node->children_links);

	node_follower = node->node_folls.first;
	if(node_follower != NULL && send == 1) {
		/* Send node_destroy command to all subscribers of this node */
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
 * \brief This function sends Node_Destroy command of avatar node and all its
 * child nodes. Own avatar node could be removed from server, when there is
 * no subscriber to this node or verse server has received confirmation of
 * receiving node_destroy command from all subscribers.
 */
int vs_node_destroy_avatar_node(struct VS_CTX *vs_ctx,
		struct VSession *session)
{
	struct VSNode *avatar_node;

	avatar_node = vs_node_find(vs_ctx, session->avatar_id);
	if(avatar_node != NULL) {
		return vs_node_destroy_branch(vs_ctx, avatar_node, 1);
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s(): avatar node (id: %d) not found\n",
				__FUNCTION__, session->avatar_id);
	}
	session->avatar_id = -1;

	return 0;
}

/**
 * \brief This function creates new node representing avatar.
 */
long int vs_node_new_avatar_node(struct VS_CTX *vs_ctx, uint16 user_id)
{
	struct VSNode *avatar_parent, *node;
	struct VSUser *super_user, *other_users, *user;
	struct VSLink *link;
	struct VBucket *bucket;
	long int avatar_id = -1;
	uint32 count;

	if((count = v_hash_array_count_items(&vs_ctx->data.nodes)) > VRS_MAX_COMMON_NODE_COUNT) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Limit of nodes reached: %d\n", VRS_MAX_COMMON_NODE_COUNT);
		goto end;
	}

	/* Trie to find parent of avatar nodes */
	if((avatar_parent = vs_ctx->data.avatar_node) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Parent of node avatars: %d does not exist\n", VRS_AVATAR_PARENT_NODE_ID);
		goto end;
	}

	/* Try to find fake user for super user */
	if((super_user = vs_user_find(vs_ctx, VRS_SUPER_USER_UID)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "super_user: %d not found\n", VRS_SUPER_USER_UID);
		goto end;
	}

	/* Try to find fake user for other users */
	if((other_users = vs_user_find(vs_ctx, VRS_OTHER_USERS_UID)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "other_users: %d not found\n", VRS_SUPER_USER_UID);
		goto end;
	}

	/* Try to find user connected with this avatar */
	if((user = vs_user_find(vs_ctx, user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "User with this ID: %d not found\n", user_id);
		goto end;
	}

	node = (struct VSNode*)calloc(1, sizeof(struct VSNode));

	vs_node_init(node);

	/* Try to find first free node_id. It is fast and easy, When
	 * VRS_LAST_COMMON_NODE_ID did not reach 0xFFFFFFFF-1 value yet and not used
	 * node_id are not reused.
	 * TODO: implement faster finding of free node id */
	node->id = vs_ctx->data.last_common_node_id + 1;
	while( v_hash_array_find_item(&vs_ctx->data.nodes, &node) != NULL) {
		node->id++;
		/* Node id 0xFFFFFFFF has special purpose and node IDs in range <0, 65535>
		 * have special purposes too (skip them) */
		if(node->id > VRS_LAST_COMMON_NODE_ID)
			node->id = VRS_FIRST_COMMON_NODE_ID;
	}
	vs_ctx->data.last_common_node_id = node->id;

	link = vs_link_create(avatar_parent, node);

	if(link != NULL) {
		bucket = v_hash_array_add_item(&vs_ctx->data.nodes, node, sizeof(struct VSNode));
		if(bucket == NULL) {
			free(node);
			node = NULL;
		} else {
			struct VSNodePermission *perm;
			struct VSNodeSubscriber *node_subscriber;

			node->owner = super_user;

			/* Set access permissions for user connected with this avatar */
			perm = (struct VSNodePermission *)calloc(1, sizeof(struct VSNodePermission));
			perm->user = user;
			perm->permissions = VRS_PERM_NODE_READ | VRS_PERM_NODE_WRITE;
			v_list_add_tail(&node->permissions, perm);

			/* Set access permissions for other users */
			perm = (struct VSNodePermission *)calloc(1, sizeof(struct VSNodePermission));
			perm->user = other_users;
			perm->permissions = VRS_PERM_NODE_READ;
			v_list_add_tail(&node->permissions, perm);

			/* Send node_create to all subscribers of parent of avatar nodes */
			node_subscriber = avatar_parent->node_subs.first;

			if(node_subscriber != NULL) {
				/* When there is at least one client subscribed, then we have
				 * to mark this node as 'creating' and wait for confirmation
				 * of receiving node_create command from all clients */
				node->state = ENTITY_CREATING;
			} else {
				/* When there is no client subscribed, then this node could be
				 * marked as created directly */
				node->state = ENTITY_CREATED;
			}

			/* Send node_create to all subscribers of parent node */
			while(node_subscriber) {
				vs_node_send_create(node_subscriber, node, NULL);
				node_subscriber = node_subscriber->next;
			}

			avatar_id = node->id;
		}
	} else {
		free(node);
		node = NULL;
	}
end:

	return avatar_id;
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
				v_print_log(VRS_PRINT_DEBUG_MSG, "node_destroy (id: %d) wasn't added to the queue\n",
						node->id);
			}
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "Can't delete node %d, because it isn't in CREATED state\n");
		}
		node_follower = node_follower->next;
	}

	node->state = ENTITY_DELETING;

end:
	return ret;
}

/**
 * \brief This function creates new node at verse server
 */
static struct VSNode *vs_node_new(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		const uint16 type)
{
	struct VSNode *node = NULL;

	if(v_hash_array_count_items(&vs_ctx->data.nodes) < VRS_MAX_COMMON_NODE_COUNT) {
		struct VSNode find_node, *avatar_node;
		struct VSLink *link;
		struct VSUser *owner;
		struct VBucket *bucket;

		/* Try to find avatar node to be able to create initial link to
		 * avatar node (initial parent of new created node) */
		find_node.id = vsession->avatar_id;
		bucket = v_hash_array_find_item(&vs_ctx->data.nodes, &find_node);
		if(bucket != NULL) {
			avatar_node = (struct VSNode*)bucket->data;
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "vsession->avatar_id: %d not found\n", vsession->avatar_id);
			goto end;
		}

		/* Try to find owner of the new node */
		if((owner = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "vsession->user_id: %d not found\n", vsession->user_id);
			goto end;
		}

		node = (struct VSNode*)calloc(1, sizeof(struct VSNode));

		vs_node_init(node);

		/* Try to find first free node_id. It is fast and easy, When
		 * VRS_LAST_COMMON_NODE_ID did not reach 0xFFFFFFFF-1 value yet and not used
		 * node_id are not reused.
		 * TODO: implement faster finding of free node id */
		node->id = vs_ctx->data.last_common_node_id + 1;
		while( v_hash_array_find_item(&vs_ctx->data.nodes, node) != NULL) {
			node->id++;

			/* Node id 0xFFFFFFFF has special purpose and node IDs in range <0, 65535>
			 * have special purposes too (skip them) */
			if(node->id > VRS_LAST_COMMON_NODE_ID)
				node->id = VRS_FIRST_COMMON_NODE_ID;
		}
		vs_ctx->data.last_common_node_id = node->id;

		node->type = type;

		link = vs_link_create(avatar_node, node);

		if(link != NULL) {
			bucket = v_hash_array_add_item(&vs_ctx->data.nodes, node, sizeof(struct VSNode));
			if(bucket == NULL) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "link between nodes %d %d could not be created\n",
									avatar_node->id, node->id);
				free(node);
				node = NULL;
			} else {
				struct VSNodePermission *perm;
				struct VSNodeSubscriber *node_subscriber;

				node->owner = owner;

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
			}
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "link between nodes %d %d could not be created\n",
					avatar_node->id, node->id);
			free(node);
			node = NULL;
		}

	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "max number: %d of nodes reached\n", VRS_MAX_COMMON_NODE_COUNT);
	}

end:

	return node;
}

/**
 * \brief This function initialize list of all nodes shared at server
 */
static void vs_node_list_init(struct VS_CTX *vs_ctx)
{
	v_hash_array_init(&vs_ctx->data.nodes,
			HASH_MOD_65536,
			offsetof(VSNode, id),
			sizeof(uint32));
}

/**
 * \brief This function create parent node for all scenes
 */
static struct VSNode *vs_create_scene_parent(struct VS_CTX *vs_ctx)
{
	struct VSNodePermission *perm;
	struct VSUser *user, *super_user;
	struct VSNode *node = NULL;

	/* Find superuser user */
	user = vs_ctx->users.first;
	while(user != NULL) {
		if(user->user_id == VRS_SUPER_USER_UID) {
			super_user = user;
			break;
		}
		user = user->next;
	}

	/* Create node, that is used as parent of scene nodes */
	node = (struct VSNode*)calloc(1, sizeof(struct VSNode));

	if(node != NULL) {
		struct VSLink *link;

		vs_node_init(node);

		node->id = VRS_SCENE_PARENT_NODE_ID;
		node->level = 1;
		node->owner = super_user;
		node->parent_link = NULL;
		node->state = ENTITY_CREATED;

		/* Create link to the root node */
		link = vs_link_create(vs_ctx->data.root_node, node);

		if(link != NULL) {
			node->parent_link = link;

			/* Create permission for other users. Other users can only read
			 * parent of scene nodes. It means, that they can subscribe to
			 * this node and get list of scenes shared at this server. */
			perm = (struct VSNodePermission *)calloc(1, sizeof(struct VSNodePermission));
			perm->user = vs_ctx->other_users;
			perm->permissions = VRS_PERM_NODE_READ | VRS_PERM_NODE_WRITE;
			v_list_add_tail(&node->permissions, perm);

			/* Add node to the hashed linked list of nodes */
			v_hash_array_add_item(&vs_ctx->data.nodes, (void*)node, sizeof(struct VSNode));
		} else {
			free(node);
			node = NULL;
		}
	}

	return node;
}

/**
 * \brief This function creates user node for the user
 */
static struct VSNode *vs_create_user_node(struct VS_CTX *vs_ctx,
		struct VSUser *user)
{
	struct VSNode *node = NULL;
	struct VSNodePermission *perm;
	struct VSUser *tmp_user, *super_user;

	/* Find superuser user */
	tmp_user = vs_ctx->users.first;
	while(tmp_user != NULL) {
		if(tmp_user->user_id == VRS_SUPER_USER_UID) {
			super_user = tmp_user;
			break;
		}
		tmp_user = tmp_user->next;
	}

	if(vs_ctx->data.user_node != NULL) {
		/* Create node, that is used as user nodes */
		node = (struct VSNode*)calloc(1, sizeof(struct VSNode));

		if(node != NULL) {
			struct VSLink *link;
			struct VSTagGroup *tg;
			struct VSTag *tag;

			vs_node_init(node);

			node->id = user->user_id;
			node->level = 2;
			node->owner = super_user;
			node->parent_link = NULL;
			node->state = ENTITY_CREATED;

			/* Create link to the parent of user nodes */
			link = vs_link_create(vs_ctx->data.user_node, node);

			if(link != NULL) {
				node->parent_link = link;

				/* Create permission for other users. Other users can only read
				 * user nodes. It means, that they can subscribe to this node
				 * and get some basic information about this user. */
				perm = (struct VSNodePermission *)calloc(1, sizeof(struct VSNodePermission));
				perm->user = vs_ctx->other_users;
				perm->permissions = VRS_PERM_NODE_READ;
				v_list_add_tail(&node->permissions, perm);

				/* Add node to the hashed linked list of nodes */
				v_hash_array_add_item(&vs_ctx->data.nodes, (void*)node, sizeof(struct VSNode));
			} else {
				free(node);
				node = NULL;
			}

			/* Create tag group with user information */
			tg = vs_taggroup_create(node, 0);
			tg->state = ENTITY_CREATED;

			/* Create tag holding real name in the tag group */
			tag = vs_tag_create(tg, VRS_VALUE_TYPE_STRING8, 1, 0);
			tag->state = ENTITY_CREATED;
			tag->value = strdup(user->realname);
			tag->flag = TAG_INITIALIZED;
		}
	}

	return node;
}

/**
 * \brief This function creates parent node for all user nodes
 */
static struct VSNode *vs_create_user_parent(struct VS_CTX *vs_ctx)
{
	struct VSUser *user, *super_user;
	struct VSNodePermission *perm;
	struct VSNode *node = NULL;

	/* Find superuser user */
	user = vs_ctx->users.first;
	while(user != NULL) {
		if(user->user_id == VRS_SUPER_USER_UID) {
			super_user = user;
			break;
		}
		user = user->next;
	}

	/* Create node, that is used as parent of user nodes */
	node = (struct VSNode*)calloc(1, sizeof(struct VSNode));

	if(node != NULL) {
		struct VSLink *link;

		vs_node_init(node);

		node->id = VRS_USERS_PARENT_NODE_ID;
		node->level = 1;
		node->owner = super_user;
		node->parent_link = NULL;
		node->state = ENTITY_CREATED;

		/* Create link to the root node */
		link = vs_link_create(vs_ctx->data.root_node, node);

		if(link != NULL) {
			node->parent_link = link;

			/* Create permission for other users. Other users can only read
			 * parent of user nodes. It means, that they can subscribe to
			 * this node and get list of users IDs. */
			perm = (struct VSNodePermission *)calloc(1, sizeof(struct VSNodePermission));
			perm->user = vs_ctx->other_users;
			perm->permissions = VRS_PERM_NODE_READ;
			v_list_add_tail(&node->permissions, perm);

			/* Add node to the hashed linked list of nodes */
			v_hash_array_add_item(&vs_ctx->data.nodes, (void*)node, sizeof(struct VSNode));
		} else {
			free(node);
			node = NULL;
		}
	}

	return node;
}

/**
 * \brief This function create node that is parent of all avatar nodes
 */
static struct VSNode *vs_create_avatar_parent(struct VS_CTX *vs_ctx)
{
	struct VSNodePermission *perm;
	struct VSUser *user, *super_user;
	struct VSNode *node = NULL;

	/* Find superuser user */
	user = vs_ctx->users.first;
	while(user != NULL) {
		if(user->user_id == VRS_SUPER_USER_UID) {
			super_user = user;
			break;
		}
		user = user->next;
	}

	/* Create node, that is used as parent of avatar nodes */
	node = (struct VSNode*)calloc(1, sizeof(struct VSNode));

	if(node != NULL) {
		struct VSLink *link;

		vs_node_init(node);

		node->id = VRS_AVATAR_PARENT_NODE_ID;
		node->level = 1;
		node->owner = super_user;
		node->parent_link = NULL;
		node->state = ENTITY_CREATED;

		/* Create link to the root node */
		link = vs_link_create(vs_ctx->data.root_node, node);

		if(link != NULL) {
			node->parent_link = link;

			/* Create permission for other users. Other users can only read
			 * parent of avatar nodes. It means, that they can subscribe to
			 * this node and get list of logged in verse clients. */
			perm = (struct VSNodePermission *)calloc(1, sizeof(struct VSNodePermission));
			perm->user = vs_ctx->other_users;
			perm->permissions = VRS_PERM_NODE_READ;
			v_list_add_tail(&node->permissions, perm);

			/* Add node to the hashed linked list of nodes */
			v_hash_array_add_item(&vs_ctx->data.nodes, (void*)node, sizeof(struct VSNode));
		} else {
			free(node);
			node = NULL;
		}
	}

	return node;
}

/**
 * \brief This function creates root node of node tree
 */
static struct VSNode *vs_create_root_node(struct VS_CTX *vs_ctx)
{
	struct VSNodePermission *perm;
	struct VSUser *user, *super_user;
	struct VSNode *node = NULL;

	/* Find superuser user */
	user = vs_ctx->users.first;
	while(user != NULL) {
		if(user->user_id == VRS_SUPER_USER_UID) {
			super_user = user;
			break;
		}
		user = user->next;
	}

	/* Create root node */
	node = (struct VSNode*)calloc(1, sizeof(struct VSNode));

	if(node != NULL) {
		vs_node_init(node);

		node->id = VRS_ROOT_NODE_ID;
		node->level = 0;
		node->parent_link = NULL;
		node->owner = super_user;
		node->state = ENTITY_CREATED;

		/* Create permission for other users. Other users can only read root node.
		 * It means, that they can subscribe to this node. */
		perm = (struct VSNodePermission *)calloc(1, sizeof(struct VSNodePermission));
		perm->user = vs_ctx->other_users;
		perm->permissions = VRS_PERM_NODE_READ;

		v_list_add_tail(&node->permissions, perm);

		/* Add node to the hashed linked list of nodes */
		v_hash_array_add_item(&vs_ctx->data.nodes, (void*)node, sizeof(struct VSNode));
	}

	return node;
}

/**
 * \brief This function create basic nodes at verse server.
 *
 * This function creates root node, parent of avarar nodes, parent of user nodes
 * and parent of scene nodes.
 */
int vs_nodes_init(struct VS_CTX *vs_ctx)
{
	struct VSNode *node;
	struct VSUser *user;
	int ret = -1;

	vs_node_list_init(vs_ctx);

	vs_ctx->data.last_common_node_id = VRS_FIRST_COMMON_NODE_ID;

	node = vs_create_root_node(vs_ctx);
	if(node != NULL) {
		vs_ctx->data.root_node = node;

		node = vs_create_avatar_parent(vs_ctx);
		if(node != NULL) {
			vs_ctx->data.avatar_node = node;
		}

		node = vs_create_user_parent(vs_ctx);
		if(node != NULL) {
			vs_ctx->data.user_node = node;
		}

		node = vs_create_scene_parent(vs_ctx);
		if(node != NULL) {
			vs_ctx->data.scene_node = node;
		}
	}

	if( vs_ctx->data.root_node != NULL &&
			vs_ctx->data.avatar_node != NULL &&
			vs_ctx->data.user_node != NULL &&
			vs_ctx->data.scene_node != NULL)
	{
		ret = 1;

		/* Create user nodes */
		user = vs_ctx->users.first;
		while(user != NULL) {
			vs_create_user_node(vs_ctx, user);
			user = user->next;
		}
	} else {
		/* TODO: free not NULL nodes */
	}

	return ret;
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

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Node has to be created */
	if(! (node->state == ENTITY_CREATED || node->state == ENTITY_CREATING)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) is not in NODE_CREATED state: %d\n",
				__FUNCTION__, node->id, node->state);
		return 0;
	}

	/* Is client subscribed to this node? */
	node_subscriber = node->node_subs.first;
	while(node_subscriber != NULL) {
		if(node_subscriber->session == vsession) {
			break;
		}
		node_subscriber = node_subscriber->next;
	}

	/* When client is subscribed to this node, then change node priority */
	if(node_subscriber!= NULL) {
		/* TODO: change priority for all child nodes */
		node_subscriber->prio = prio;
		vs_node_prio(vsession, node, prio);
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() client not subscribed to this node (id: %d)\n",
						__FUNCTION__, node_id);
		return 0;
	}

	return 0;
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

	/* TODO: when versioning will be supported, then compute crc32, save data to
	 * the disk and send node_unsubscribe command to the client with version number
	 * and crc32 */
	if(version != 0) {
		v_print_log(VRS_PRINT_WARNING, "Version: %d != 0, versioning is not supported yet\n", version);
	}

	/* Node has to be created */
	if(node->state == ENTITY_CREATED || node->state == ENTITY_CREATING) {
		/* Is client subscribed to this node? */
		node_subscriber = node->node_subs.first;
		while(node_subscriber != NULL) {
			if(node_subscriber->session == vsession) {
				break;
			}
			node_subscriber = node_subscriber->next;
		}
		if(node_subscriber!= NULL) {
			return vs_node_unsubscribe(node, node_subscriber, 0);
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "%s() client not subscribed to this node (id: %d)\n",
							__FUNCTION__, node_id);
			return 0;
		}
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) is not in NODE_CREATED state: %d\n",
				__FUNCTION__, node->id, node->state);
		return 0;
	}

	return 0;
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
	if(node->state == ENTITY_CREATED || node->state == ENTITY_CREATING) {
		/* Isn't client already subscribed to this node? */
		node_subscriber = node->node_subs.first;
		while(node_subscriber != NULL) {
			if(node_subscriber->session->session_id == vsession->session_id) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "%s() client %d is already subscribed to the node (id: %d)\n",
						__FUNCTION__, vsession->session_id, node->id);
				return 0;
			}
			node_subscriber = node_subscriber->next;
		}

		return vs_node_subscribe(vs_ctx, vsession, node, version);
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) is not in NODE_CREATED state: %d\n",
				__FUNCTION__, node->id, node->state);
		return 0;
	}

	return 0;
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
	if(! (node->state == ENTITY_CREATED || node->state == ENTITY_CREATING)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) is not in NODE_CREATED state: %d\n",
				__FUNCTION__, node->id, node->state);
		return 0;
	}

	/* Try to find user */
	if((user = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "vsession->user_id: %d not found\n", vsession->user_id);
		return 0;
	}

	/* Is this user owner of this node? */
	if(user != node->owner) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "vsession->user_id: %d is not owner of the node (id: %d) and can't delete this node.\n",
				vsession->user_id, node->id);
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

			if(node_follower->state==ENTITY_CREATING) {
				node_follower->state = ENTITY_CREATED;

				/* If the node is in state DELETING, then send to the client command
				 * node_delete. The client knows about this node now and can receive
				 * node_destroy command */
				if(node->state == ENTITY_DELETING) {
					/* Create Destroy_Node command */
					struct Generic_Cmd *node_destroy_cmd = v_node_destroy_create(node->id);

					/* Push this command to the outgoing queue */
					if ( node_destroy_cmd!= NULL &&
							v_out_queue_push_tail(node_follower->node_sub->session->out_queue,
									node_follower->node_sub->prio,
									node_destroy_cmd) == 1) {
						node_follower->state = ENTITY_DELETING;
					} else {
						v_print_log(VRS_PRINT_DEBUG_MSG, "node_destroy (id: %d) wasn't added to the queue\n",
								node->id);
					}
				}
			} else {
				v_print_log(VRS_PRINT_DEBUG_MSG, "node %d isn't in CREATING state\n");
			}
		} else {
			if(node_follower->state!=ENTITY_CREATED) {
				all_created = 0;
			}
		}
		node_follower = node_follower->next;
	}

	/* When all followers know about this node, then change state of this node */
	if(all_created == 1) {
		node->state = ENTITY_CREATED;
	}

	return 0;
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
	uint16 type = UINT16(node_create->data[UINT16_SIZE+UINT32_SIZE+UINT32_SIZE]);

	/* Client has to send node_create command with node_id equal to
	 * the value 0xFFFFFFFF */
	if(node_id != VRS_RESERVED_NODE_ID) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node_id is 0xFFFFFFFF\n",
				__FUNCTION__);
		return 0;
	}

	/* Client has to send node_create with parent_id equal to its avatar_id */
	if(parent_id != vsession->avatar_id) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() parent_id: %d is not equal to session avatar id %d\n",
				__FUNCTION__, parent_id, vsession->avatar_id);
		return 0;
	}

	/* Client has to send node_create command with his user_id */
	if(user_id != vsession->user_id) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() user_id: %d is not equal to session user id %d\n",
				__FUNCTION__, user_id, vsession->user_id);
		return 0;
	}

	if(vs_node_new(vs_ctx, vsession, type) != NULL) {
		return 1;
	} else {
		return 0;
	}

	return 0;
}

