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

#include "verse_types.h"

#include "v_list.h"
#include "v_common.h"
#include "v_node_commands.h"
#include "v_fake_commands.h"

#include "vs_main.h"
#include "vs_node.h"
#include "vs_node_access.h"
#include "vs_node.h"
#include "vs_link.h"
#include "vs_entity.h"
#include "vs_taggroup.h"
#include "vs_tag.h"
#include "vs_layer.h"


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
 * Remove client from all tag followers in specific tag group
 */
static void vs_tag_free_avatar_reference(struct VSTagGroup *tg,
		struct VSession *session)
{
	struct VSTag *tag;
	struct VBucket *tag_bucket;
	struct VSEntityFollower *tag_follower;

	/* When client was subscribed to tag group, then
	 * it was following all tags in this tag group.
	 * Go through all Tags and remove client from list of tag followers */
	tag_bucket = tg->tags.lb.first;
	while(tag_bucket != NULL) {
		tag = (struct VSTag*)tag_bucket->data;

		tag_follower = tag->tag_folls.first;
		while(tag_follower != NULL) {
			if(tag_follower->node_sub->session->session_id == session->session_id) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Free follower: %d from tag: %d\n",
						session->avatar_id, tag->id);
				v_list_free_item(&tag->tag_folls, tag_follower);
				break;
			}
			tag_follower = tag_follower->next;
		}
		tag_bucket = tag_bucket->next;
	}
}


/**
 * \brief Remove client from subscribers and followers to tag groups and its
 * tags
 */
static void vs_tg_free_avatar_reference(struct VSNode *node,
		struct VSession *session)
{
	struct VSTagGroup *tg;
	struct VBucket *tg_bucket;
	struct VSEntitySubscriber *tg_subscriber;
	struct VSEntityFollower *tg_follower;

	/* Remove client from list of tag_group subscribers */
	tg_bucket = node->tag_groups.lb.first;
	while(tg_bucket != NULL) {
		tg = (struct VSTagGroup*)tg_bucket->data;

		tg_follower = tg->tg_folls.first;
		while(tg_follower != NULL) {
			if(tg_follower->node_sub->session->session_id == session->session_id) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Free follower: %d from tag group: %d\n",
						session->avatar_id, tg->id);
				/* Remove client from list of clients, that knows about this node */
				v_list_free_item(&tg->tg_folls, tg_follower);

				/* No need to go through other tag group followers*/
				break;
			}
			tg_follower = tg_follower->next;
		}

		/* Try to remove client from subscribers */
		tg_subscriber = tg->tg_subs.first;
		while(tg_subscriber != NULL) {
			if(tg_subscriber->node_sub->session->session_id == session->session_id) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Free subscriber: %d from tag group: %d\n",
						session->avatar_id, tg->id);
				/* Remove client from list of clients, that are subscribed to this tag_group */
				v_list_free_item(&tg->tg_subs, tg_subscriber);

				vs_tag_free_avatar_reference(tg, session);

				/* No need to go through other tag groups subscribers */
				break;
			}
			tg_subscriber = tg_subscriber->next;
		}

		tg_bucket = tg_bucket->next;
	}
}


/**
 * \brief Remove client from layer followers and subscribers
 */
static void vs_layer_free_avatar_reference(struct VSNode *node,
		struct VSession *session)
{
	struct VSLayer *layer;
	struct VBucket *layer_bucket;
	struct VSEntitySubscriber *layer_subscriber;
	struct VSEntityFollower *layer_follower;

	layer_bucket = node->layers.lb.first;
	while(layer_bucket != NULL) {
		layer = (struct VSLayer*)layer_bucket->data;

		/* Remove client from layer followers */
		layer_follower = layer->layer_folls.first;
		while(layer_follower != NULL) {
			if(layer_follower->node_sub->session->session_id == session->session_id) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Free follower: %d from layer: %d\n",
						session->avatar_id, layer->id);
				v_list_free_item(&layer->layer_folls, layer_follower);
				break;
			}
			layer_follower = layer_follower->next;
		}

		/* Remove client from layer subscribers */
		layer_subscriber = layer->layer_subs.first;
		while(layer_subscriber != NULL) {
			if(layer_subscriber->node_sub->session->session_id == session->session_id) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Free subscriber: %d from layer: %d\n",
						session->avatar_id, layer->id);
				v_list_free_item(&layer->layer_subs, layer_subscriber);
				break;
			}
			layer_subscriber = layer_subscriber->next;
		}

		layer_bucket = layer_bucket->next;
	}
}

/**
 * Remove client from subscribers and followers of all entities
 */
static void vs_free_avatar_reference(struct VSNode *node,
		struct VSession *session)
{
	struct VSNodeSubscriber *node_subscriber, *next_node_subscriber;
	struct VSEntityFollower *node_follower;
	struct VSLink *link;
	int was_locked = 0;

	/* Remove client from subscribers and followers of tag groups and its tags */
	vs_tg_free_avatar_reference(node, session);

	/* Remove client from subscribers and followers of layer */
	vs_layer_free_avatar_reference(node, session);

	/* Remove client from followers of child nodes. When it is done, then
	 * it is possible and safe to remove client from node subscribers, because
	 * node follower relay on node subscriber of parent node. */
	link = node->children_links.first;
	while(link != NULL) {

		/* I say: recursive tree traversal */
		vs_free_avatar_reference(link->child, session);

		/* Remove client from followers of this node */
		node_follower = link->child->node_folls.first;
		while(node_follower != NULL) {
			if(node_follower->node_sub->session->session_id == session->session_id) {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Free follower: %d from node: %d\n",
						session->avatar_id, link->child->id);
				/* Remove client from list of clients, that knows about this node */
				v_list_free_item(&link->child->node_folls, node_follower);

				/* Continue with next node */
				break;
			}
			node_follower = node_follower->next;
		}

		link = link->next;
	}

	/* Was node locked by this client? */
	if(node->lock.session != NULL &&
			node->lock.session->session_id == session->session_id) {
		was_locked = 1;
		node->lock.session = NULL;
	}

	/* Is client subscribed to this node? */
	node_subscriber = node->node_subs.first;
	while(node_subscriber != NULL) {
		next_node_subscriber = node_subscriber->next;
		if(node_subscriber->session->session_id == session->session_id) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "Free subscriber: %d from node: %d\n",
					session->avatar_id, node->id);
			v_list_free_item(&node->node_subs, node_subscriber);
			if(was_locked == 0) {
				/* No need to go through other subscribers,
				 * because send_unlock needn't to be sent in this
				 * case */
				break;
			}
		} else {
			if(was_locked == 1) {
				vs_node_send_unlock(node_subscriber, session, node);
			}
		}
		node_subscriber = next_node_subscriber;
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
	struct VSNode *root_node;

	root_node = vs_node_find(vs_ctx, VRS_ROOT_NODE_ID);

	if(root_node != NULL) {
		vs_free_avatar_reference(root_node, session);
	}

	return 1;
}

/**
 * \brief This function creates node with information about connected
 * verse client
 */
static void vs_create_client_info_node(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct VSNode *avatar_node)
{
	struct VSNode *client_info_node = NULL;
	struct VSTagGroup *tg;
	struct VSTag *tag;
	struct timeval tv;

	/* Try to create child node of avatar node with information about connected verse client */
	if( (client_info_node = vs_node_create(vs_ctx, avatar_node, vs_ctx->super_user, VRS_RESERVED_NODE_ID, VRS_AVATAR_INFO_NODE_CT)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Could not create avatar info node for user: %d\n", vsession->user_id);
		return;
	}

	/* Set access permissions only for other users. Every loged user can only read this
	 * node (including user binded with avatar node) */
	vs_node_set_perm(client_info_node, vs_ctx->other_users, VRS_PERM_NODE_READ);

	/* About avatar info node doesn't know anybody, then we can set state
	 * of node to created */
	client_info_node->state = ENTITY_CREATED;

	/* Create tag group with client information */
	tg = vs_taggroup_create(client_info_node, 0);
	if(tg != NULL) {
		tg->state = ENTITY_CREATED;

		/* Create tag holding real name in the tag group */
		tag = vs_tag_create(tg, VRS_VALUE_TYPE_STRING8, 1, 0);
		if(tag != NULL) {
			tag->state = ENTITY_CREATED;
			tag->value = strdup(vsession->peer_hostname);
			tag->flag = TAG_INITIALIZED;
		}

		/* Create tag holding time of login */
		gettimeofday(&tv, NULL);
		tag = vs_tag_create(tg, VRS_VALUE_TYPE_UINT64, 1, 1);
		if(tag != NULL) {
			uint64 sec = (uint64)tv.tv_sec;
			tag->state = ENTITY_CREATED;
			memcpy(tag->value, &sec, sizeof(uint64));
			tag->flag = TAG_INITIALIZED;
		}

		if(vsession->client_name != NULL) {
			/* Create tag holding client name */
			tag = vs_tag_create(tg, VRS_VALUE_TYPE_STRING8, 1, 2);
			if(tag != NULL) {
				tag->state = ENTITY_CREATED;
				tag->value = strdup(vsession->client_name);
				tag->flag = TAG_INITIALIZED;
			}
		}

		if(vsession->client_version != NULL) {
			/* Create tag holding client version */
			tag = vs_tag_create(tg, VRS_VALUE_TYPE_STRING8, 1, 3);
			if(tag != NULL) {
				tag->state = ENTITY_CREATED;
				tag->value = strdup(vsession->client_version);
				tag->flag = TAG_INITIALIZED;
			}
		}
	}
}


/**
 * \brief This function creates new node representing avatar.
 */
long int vs_create_avatar_node(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		uint16 user_id)
{
	struct VSNode *avatar_parent, *avatar_node;
	struct VSUser *user;
	struct VSNodeSubscriber *node_subscriber;

	/* Try to find parent of avatar nodes */
	if((avatar_parent = vs_ctx->data.avatar_node) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Parent of node avatars: %d does not exist\n", VRS_AVATAR_PARENT_NODE_ID);
		return -1;
	}

	/* Try to find user connected with this avatar */
	if((user = vs_user_find(vs_ctx, user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "User with this ID: %d not found\n", user_id);
		return -1;
	}

	/* Try to create new node representing avatar */
	if( (avatar_node = vs_node_create(vs_ctx, avatar_parent, vs_ctx->super_user, VRS_RESERVED_NODE_ID, VRS_AVATAR_NODE_CT)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Could not create avatar node for user: %d\n", user_id);
		return -1;
	}

	/* Set access permissions for user connected with this avatar. User can write data to its own
	 * avatar node */
	vs_node_set_perm(avatar_node, user, VRS_PERM_NODE_READ | VRS_PERM_NODE_WRITE);
	/* Set access permissions for other users */
	vs_node_set_perm(avatar_node, vs_ctx->other_users, VRS_PERM_NODE_READ);

	/* Create client info node */
	vs_create_client_info_node(vs_ctx, vsession, avatar_node);

	/* Send node_create to all subscribers of parent of avatar nodes */
	node_subscriber = avatar_parent->node_subs.first;

	if(node_subscriber != NULL) {
		/* When there is at least one client subscribed, then we have
		 * to mark this node as 'creating' and wait for confirmation
		 * of receiving node_create command from all clients */
		avatar_node->state = ENTITY_CREATING;
	} else {
		/* When there is no client subscribed, then this node could be
		 * marked as created directly */
		avatar_node->state = ENTITY_CREATED;
	}

	/* Send node_create to all subscribers of parent node */
	while(node_subscriber != NULL) {
		vs_node_send_create(node_subscriber, avatar_node, NULL);
		node_subscriber = node_subscriber->next;
	}

	return avatar_node->id;
}


/**
 * \brief This function create node that is parent of all avatar nodes
 */
static struct VSNode *vs_create_avatar_parent(struct VS_CTX *vs_ctx)
{
	struct VSNode *node = NULL;

	/* Try to create new node representing parent of avatar nodes */
	if( (node = vs_node_create(vs_ctx, vs_ctx->data.root_node, vs_ctx->super_user, VRS_AVATAR_PARENT_NODE_ID, VRS_AVATAR_PARENT_NODE_CT)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Could not create parent node of avatar nodes\n");
		return NULL;
	}

	node->state = ENTITY_CREATED;

	/* Create permission for other users. Other users can only read
	 * parent of avatar nodes. It means, that they can subscribe to
	 * this node and get list of logged-in verse clients. */
	vs_node_set_perm(node, vs_ctx->other_users, VRS_PERM_NODE_READ);

	return node;
}


/**
 * \brief This function create parent node for all scenes
 */
static struct VSNode *vs_create_scene_parent(struct VS_CTX *vs_ctx)
{
	struct VSNode *node = NULL;

	/* Try to create new node representing parent of scene nodes */
	if( (node = vs_node_create(vs_ctx, vs_ctx->data.root_node, vs_ctx->super_user, VRS_SCENE_PARENT_NODE_ID, VRS_SCENE_PARENT_NODE_CT)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Could not create parent node of scene nodes\n");
		return NULL;
	}

	/* Nobody is connected, then it is OK set this state */
	node->state = ENTITY_CREATED;

	/* Create permission for other users. Other users can only read
	 * parent of scene nodes. It means, that they can subscribe to
	 * this node and get list of scenes shared at this server. */
	vs_node_set_perm(node, vs_ctx->other_users, VRS_PERM_NODE_READ | VRS_PERM_NODE_WRITE);

	return node;
}


/**
 * \brief This function creates user node for the user
 */
struct VSNode *vs_create_user_node(struct VS_CTX *vs_ctx,
		struct VSUser *user)
{
	struct VSNode *node = NULL;
	struct VSTagGroup *tg;
	struct VSTag *tag;

	/* Try to create new node representing user nodes */
	if( (node = vs_node_create(vs_ctx, vs_ctx->data.user_node, vs_ctx->super_user, user->user_id, VRS_USER_NODE_CT)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Could not create user node\n");
		return NULL;
	}

	node->state = ENTITY_CREATED;

	/* Create permission for other users. Other users can only read
	 * user nodes. It means, that they can subscribe to this node
	 * and get some basic information about this user. */
	vs_node_set_perm(node, vs_ctx->other_users, VRS_PERM_NODE_READ);

	/* Create tag group with user information */
	tg = vs_taggroup_create(node, 0);
	if(tg != NULL) {
		tg->state = ENTITY_CREATED;

		/* Create tag holding real name in the tag group */
		tag = vs_tag_create(tg, VRS_VALUE_TYPE_STRING8, 1, 0);
		if(tag != NULL) {
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
	struct VSNode *node = NULL;

	/* Try to create new node representing parent of user nodes */
	if( (node = vs_node_create(vs_ctx, vs_ctx->data.root_node, vs_ctx->super_user, VRS_USERS_PARENT_NODE_ID, VRS_USERS_PARENT_NODE_CT)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Could not create parent node of user nodes\n");
		return NULL;
	}

	/* Create permission for other users. Other users can only read
	 * parent of user nodes. It means, that they can subscribe to
	 * this node and get list of users IDs. */
	vs_node_set_perm(node, vs_ctx->other_users, VRS_PERM_NODE_READ);

	node->state = ENTITY_CREATED;

	return node;
}


/**
 * \brief This function creates root node of node tree
 */
static struct VSNode *vs_create_root_node(struct VS_CTX *vs_ctx)
{
	struct VSNode *node = NULL;

	/* Try to create new node representing root node */
	if( (node = vs_node_create(vs_ctx, NULL, vs_ctx->super_user, VRS_ROOT_NODE_ID, VRS_ROOT_NODE_CT)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Could not create root node\n");
		return NULL;
	}

	node->state = ENTITY_CREATED;

	/* Create permission for other users. Other users can only read root node.
	 * It means, that they can subscribe to this node. */
	vs_node_set_perm(node, vs_ctx->other_users, VRS_PERM_NODE_READ);

	return node;
}

/**
 * \brief This function create basic structures for node and basic nodes at verse server.
 *
 * This function creates root node, parent of avarar nodes, parent of user nodes
 * and parent of scene nodes. It also initialize hashed array of all nodes at
 * Verse server.
 */
int vs_nodes_init(struct VS_CTX *vs_ctx)
{
	struct VSNode *node;
	struct VSUser *user;
	int ret = -1;

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
		/* Free not NULL nodes */
		if(vs_ctx->data.root_node != NULL) {
			vs_node_destroy_branch(vs_ctx, vs_ctx->data.root_node, 0);
			vs_ctx->data.root_node = NULL;
			vs_ctx->data.avatar_node = NULL;
			vs_ctx->data.user_node = NULL;
			vs_ctx->data.scene_node = NULL;
		}
	}

	return ret;
}
