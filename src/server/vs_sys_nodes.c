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

		/* Remove client from layer followers and subscribers */
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
 * \brief This function creates new node representing avatar.
 */
long int vs_node_new_avatar_node(struct VS_CTX *vs_ctx, uint16 user_id)
{
	struct VSNode *avatar_parent, *node;
	struct VSUser *super_user, *other_users, *user;
	struct VSNodePermission *perm;
	struct VSNodeSubscriber *node_subscriber;
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

	/* Try to create new node representing avatar */
	if( (node = vs_node_create(vs_ctx, avatar_parent, super_user, VRS_RESERVED_NODE_ID, 0)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Could not create avatar node for user: %d\n", user_id);
		goto end;
	}

	/* Set access permissions for user connected with this avatar. User can write data to its own
	 * avatar node */
	vs_node_set_perm(node, user, VRS_PERM_NODE_READ | VRS_PERM_NODE_WRITE);

	/* Set access permissions for other users */
	vs_node_set_perm(node, vs_ctx->other_users, VRS_PERM_NODE_READ);

	/* TODO: add child node of avatar node with information about connected verse client */

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

end:
	return avatar_id;
}


/**
 * \brief This function create node that is parent of all avatar nodes
 */
struct VSNode *vs_create_avatar_parent(struct VS_CTX *vs_ctx)
{
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

	/* Try to create new node representing parent of avatar nodes */
	if( (node = vs_node_create(vs_ctx, vs_ctx->data.root_node, super_user, VRS_AVATAR_PARENT_NODE_ID, 0)) == NULL) {
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
struct VSNode *vs_create_scene_parent(struct VS_CTX *vs_ctx)
{
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

	/* Try to create new node representing parent of scene nodes */
	if( (node = vs_node_create(vs_ctx, vs_ctx->data.root_node, super_user, VRS_SCENE_PARENT_NODE_ID, 0)) == NULL) {
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

	/* Try to create new node representing user nodes */
	if( (node = vs_node_create(vs_ctx, vs_ctx->data.user_node, super_user, user->user_id, 0)) == NULL) {
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
struct VSNode *vs_create_user_parent(struct VS_CTX *vs_ctx)
{
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

	/* Try to create new node representing parent of user nodes */
	if( (node = vs_node_create(vs_ctx, vs_ctx->data.root_node, super_user, VRS_USERS_PARENT_NODE_ID, 0)) == NULL) {
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
struct VSNode *vs_create_root_node(struct VS_CTX *vs_ctx)
{
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

	/* Try to create new node representing root node */
	if( (node = vs_node_create(vs_ctx, NULL, super_user, VRS_ROOT_NODE_ID, 0)) == NULL) {
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

	v_hash_array_init(&vs_ctx->data.nodes,
			HASH_MOD_65536,
			offsetof(VSNode, id),
			sizeof(uint32));

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
