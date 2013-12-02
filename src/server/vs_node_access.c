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

#include "stdlib.h"

#include "verse_types.h"

#include "v_common.h"
#include "v_node_commands.h"

#include "vs_node.h"

/**
 * \brief This function checks if client can write to the node
 */
int vs_node_can_write(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct VSNode *node)
{
	struct VSUser *user;
	struct VSNodePermission	*perm;
	int user_can_write = 0;

	/* Is this node locked by other client? */
	if(node->lock.session != NULL && node->lock.session != vsession) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"node: %d locked by vsession->avatar_id: %d\n",
				node->id, vsession->avatar_id);
		return 0;
	}

	/* Try to find user of this session */
	if((user = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"vsession->user_id: %d not found\n",
				vsession->user_id);
		return 0;
	}

	/* Is user owner of this node or can user write to this node? */
	if(node->owner == user) {
		user_can_write = 1;
	} else {
		perm = node->permissions.first;
		while(perm != NULL) {
			/* Can this user or other users write to this node? */
			if(perm->user == user || perm->user == vs_ctx->other_users) {
				if(perm->permissions & VRS_PERM_NODE_WRITE) {
					user_can_write = 1;
				}
				break;
			}
			perm = perm->next;
		}
	}

	return user_can_write;
}

/**
 * \brief This function checks if client can read the node
 */
int vs_node_can_read(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct VSNode *node)
{
	struct VSUser *user;
	struct VSNodePermission	*perm;
	int	user_can_read = 0;

	/* Try to find user of this session */
	if((user = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"vsession->user_id: %d not found\n",
				vsession->user_id);
		return 0;
	}

	if(node->owner == user) {
		user_can_read = 1;
	} else {
		perm = node->permissions.first;
		while(perm!=NULL) {
			/* Can the user read this node? */
			if(perm->user == user) {
				if(perm->permissions & VRS_PERM_NODE_READ) {
					user_can_read = 1;
					break;
				}
			/* Can all other users read this node? */
			} else if(perm->user == vs_ctx->other_users) {
				if(perm->permissions & VRS_PERM_NODE_READ) {
					user_can_read = 1;
				}
				break;
			}
			perm = perm->next;
		}
	}

	return user_can_read;
}

/**
 * \brief This function send node_owner to the client
 */
int vs_node_send_owner(struct VSNodeSubscriber *node_subscriber,
		struct VSNode *node)
{
	struct Generic_Cmd *node_owner = NULL;

	node_owner = v_node_owner_create(node->id, node->owner->user_id);

	if(node_owner != NULL) {
		v_out_queue_push_tail(node_subscriber->session->out_queue,
							node_subscriber->prio,
							node_owner);
	} else {
		return 0;
	}

	return 1;
}

/*
 * \brief This function do changing/removing/adding of node permissions
 */
int vs_node_set_perm(struct VSNode *node, VSUser *user, uint8 permission)
{
	VSNodePermission *perm = node->permissions.first;
	int perm_found = 0;

	/* Go through all permissions and try to find permissions for user */
	while(perm != NULL) {
		/* Change permission for this user */
		if(perm->user == user) {
			if(permission == 0) {
				/* When client grant no permissions, then it is better to
				 * remove such permissions from list of permissions */
				v_list_free_item(&node->permissions, perm);
				perm = NULL;
			} else {
				perm->permissions = permission;
			}
			perm_found = 1;
			break;
		}
		perm = perm->next;
	}

	/* When no permissions were found, then add new permission to this user */
	if(perm_found == 0) {
		/* Create new permission */
		perm = (VSNodePermission*)calloc(1, sizeof(VSNodePermission));
		if(perm == NULL) {
			v_print_log(VRS_PRINT_ERROR, "Out of memory\n");
			return 0;
		}
		perm->user = user;
		perm->permissions = permission;
		/* Add permission to the end of list */
		v_list_add_tail(&node->permissions, perm);
	}

	return 1;
}

/**
 * \brief This function send node_perm to the client
 */
int vs_node_send_perm(struct VSNodeSubscriber *node_subscriber,
		struct VSNode *node,
		struct VSUser *user,
		uint8 perm)
{
	struct Generic_Cmd *node_perm = NULL;

	if(user != NULL) {
		node_perm = v_node_perm_create(node->id, user->user_id, perm);
	} else {
		node_perm = v_node_perm_create(node->id, VRS_OTHER_USERS_UID, perm);
	}

	if(node_perm != NULL) {
		v_out_queue_push_tail(node_subscriber->session->out_queue,
							node_subscriber->prio,
							node_perm);
	} else {
		return 0;
	}

	return 1;
}

/**
 * \brief This function sends node_unlock to the node subscriber
 */
int vs_node_send_unlock(struct VSNodeSubscriber *node_subscriber,
		struct VSession *vsession,
		struct VSNode *node)
{
	struct Generic_Cmd *cmd_node_unlock;

	cmd_node_unlock = v_node_unlock_create(node->id, vsession->avatar_id);

	if(cmd_node_unlock != NULL) {
		v_out_queue_push_tail(node_subscriber->session->out_queue,
				node_subscriber->prio,
				cmd_node_unlock);
	}

	return 0;
}

/**
 * \brief This function handle node_unlock command
 */
int vs_handle_node_unlock(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_unlock)
{
	struct VSNode *node;
	uint32 avatar_id = UINT32(node_unlock->data[0]);
	uint32 node_id = UINT32(node_unlock->data[UINT32_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Was this command sent with right avatar ID? */
	if(avatar_id != vsession->avatar_id) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"avatar_id: %d not equal to vsession->avatar_id: %d\n",
				avatar_id, vsession->avatar_id);
		return 0;
	}

	/* Can this avatar/user write to this node? */
	if(vs_node_can_write(vs_ctx, vsession, node) == 1) {
		/* Only client that locked this node can unlock this node */
		if(node->lock.session == NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Node: %d is not locked\n", node->id);
		} else if(node->lock.session == vsession) {
			struct VSNodeSubscriber *node_subscriber;

			node->lock.session = NULL;

			/* TODO: send node_unlock only in situation, when client received
			 * node_lock command */

			/* Send node_unlock to all node subscribers */
			for(node_subscriber = node->node_subs.first;
					node_subscriber != NULL;
					node_subscriber = node_subscriber->next)
			{
				vs_node_send_unlock(node_subscriber, vsession, node);
			}

		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Node: %d is not locked by avatar: %d\n",
					node->id, vsession->avatar_id);
		}
	}

	return 0;
}

/**
 * \brief This function sends node_lock to the subscriber of the node
 */
int vs_node_send_lock(struct VSNodeSubscriber *node_subscriber,
		struct VSession *vsession,
		struct VSNode *node)
{
	struct Generic_Cmd *cmd_node_lock;

	cmd_node_lock = v_node_lock_create(node->id, vsession->avatar_id);

	if(cmd_node_lock != NULL) {
		v_out_queue_push_tail(node_subscriber->session->out_queue,
				node_subscriber->prio,
				cmd_node_lock);
	}

	return 0;
}

/**
 * \brief This function handle node_lock command
 */
int vs_handle_node_lock(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_lock)
{
	struct VSNode *node;
	uint32 avatar_id = UINT32(node_lock->data[0]);
	uint32 node_id = UINT32(node_lock->data[UINT32_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Was this command sent with right avatar ID? */
	if(avatar_id != vsession->avatar_id) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"avatar_id: %d not equal to vsession->avatar_id: %d\n",
				avatar_id, vsession->avatar_id);
		return 0;
	}

	/* Can this avatar/user write to this node? */
	if(vs_node_can_write(vs_ctx, vsession, node) == 1) {
		struct timeval tv;

		/* Is this new lock */
		if(node->lock.session == NULL) {
			struct VSNodeSubscriber *node_subscriber;

			gettimeofday(&tv, NULL);
			node->lock.session = vsession;
			node->lock.tv.tv_sec = tv.tv_sec;
			node->lock.tv.tv_usec = tv.tv_usec;

			/* Send node_lock to all node subscribers */
			for(node_subscriber = node->node_subs.first;
					node_subscriber != NULL;
					node_subscriber = node_subscriber->next)
			{
				vs_node_send_lock(node_subscriber, vsession, node);
			}

		/* Is this keep alive lock */
		} else if (node->lock.session == vsession) {
			gettimeofday(&tv, NULL);
			node->lock.tv.tv_sec = tv.tv_sec;
			node->lock.tv.tv_usec = tv.tv_usec;
		/* Node can't be locked, when somebody already locked this node */
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Node: %d is already locked by avatar: %d\n",
					node->id, vsession->avatar_id);
		}
	}

	return 0;
}

/**
 * \brief This function handle node_perm command
 */
int vs_handle_node_owner(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_perm)
{
	struct VSNode *node;
	struct VSUser *user, *new_owner;
	struct VSEntityFollower *node_follower;
	uint32 node_id = UINT32(node_perm->data[UINT16_SIZE]);
	uint16 user_id = UINT16(node_perm->data[0]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Try to find user of this session */
	if((user = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"vsession->user_id: %d not found\n", vsession->user_id);
		return 0;
	}

	/* Check if user is owner of this node*/
	if(user != node->owner) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"user->id: %d is not owner: %d of node: %d\n",
				user_id, node->owner->user_id, node->id);
		return 0;
	}

	/* Try to find user that should be new owner */
	if((new_owner = vs_user_find(vs_ctx, user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "user_id: %d not found\n", user_id);
		return 0;
	}

	/* New Owner can't be fake user */
	if(new_owner->fake_user != 1) {

		/* Change owner of the node */
		node->owner = new_owner;

		/* Send node_owner to all node followers */
		for(node_follower = node->node_folls.first;
				node_follower != NULL;
				node_follower = node_follower->next)
		{
			vs_node_send_owner(node_follower->node_sub, node);
		}
	}

	return 0;
}

/**
 * \brief This function handle node_perm command
 */
int vs_handle_node_perm(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_perm)
{
	struct VSNode *node;
	struct VSNodeSubscriber *node_subscriber;
	struct VSUser *user, *avatar_user;
	uint32 node_id = UINT32(node_perm->data[UINT16_SIZE+UINT8_SIZE]);
	uint16 user_id = UINT16(node_perm->data[0]);
	uint8 permissions = UINT8(node_perm->data[UINT16_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Try to find user of current avatar */
	if((avatar_user = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"vsession->user_id: %d not found\n", vsession->user_id);
		return 0;
	}

	/* Try to find user, that is object of permission changing/setting/adding */
	if((user = vs_user_find(vs_ctx, user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"vsession->user_id: %d not found\n", user_id);
		return 0;
	}

	/* Node has to be created */
	if(!(node->state == ENTITY_CREATED || node->state == ENTITY_CREATING)) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() node (id: %d) is not in NODE_CREATED state: %d\n",
				__FUNCTION__, node->id, node->state);
		return 0;
	}

	/* Check if permission contains valid permission flags */
	if( ! ((permissions & VRS_PERM_NODE_READ) ||
			(permissions & VRS_PERM_NODE_WRITE) ||
			(permissions == 0)) )
	{
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"Not valid permission flags: %d", permissions);
		return 0;
	}

	/* User has to be owner of the node, to be able to change permission of
	 * this node */
	if(node->owner == avatar_user) {
		/* Set permission for this node */
		vs_node_set_perm(node, user, permissions);

		/* Set this command to all subscribers */
		node_subscriber = node->node_subs.first;
		while(node_subscriber != NULL) {
			vs_node_send_perm(node_subscriber, node, user, permissions);

			/* TODO: send create commands to the client, when at least read
			 * access was granted for the user */

			/* TODO: unlock locked node, when locker of the node lost permission
			 * to lock this node */

			node_subscriber = node_subscriber->next;
		}
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s(): user: %s can't change permissions of node: %d\n",
				__FUNCTION__, user->username, node->id);
		return 0;
	}

	return 0;
}
