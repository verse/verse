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

#include "v_common.h"

#include "vs_node.h"
#include "vs_node_access.h"
#include "vs_link.h"

/**
 * \brief This function test two nodes, if parent node could be parent of child
 * node.
 *
 * The parent node could not be any child of the child node. If the condition was
 * not satisfied, then node tree would be split into the two separated parts.
 */
int vs_link_test_nodes(struct VSNode *parent, struct VSNode *child)
{
	int ret = 0;

	/* Test subordination of parent */
	if(parent->level < child->level) {
		ret = 1;
	} else {
		struct VSNode *node;

		node = parent;

		/* Go to the root of the tree and test if child is not in this
		 * branch */
		ret = 1;
		while(node!=NULL) {
			if(node == child) {
				ret = 0;
				break;
			}
			if(node->parent_link != NULL)
				node = node->parent_link->parent;
			else
				node = NULL;
		}
	}

	return ret;
}

/**
 * \brief This function tries to create link between parent and child. This
 * function does not check, if child had permission to be linked with parent
 * node. It has to be checked before this link. If link is successfully created,
 * then level of child node is updated. The link is not copied to the bucket!
 */
struct VSLink *vs_link_create(struct VSNode *parent, struct VSNode *child)
{
	struct VSLink *link = NULL;

	assert(parent != NULL);
	assert(child != NULL);

	link = (struct VSLink*)malloc(sizeof(struct VSLink));

	if( link != NULL ) {
		link->parent = parent;
		link->child = child;
		v_list_add_tail(&parent->children_links, link);
		child->parent_link = link;
		child->level = parent->level + 1;
	} else {
		v_print_log(VRS_PRINT_WARNING, "Could not create link between %d and %d, not enough memory\n",
				parent->id, child->id);
	}

	return link;
}

/**
 * \brief This function send Node_Link command
 */
static int vs_link_change_send(struct VSNodeSubscriber *node_subscriber,
		struct VSLink *link)
{
	struct Generic_Cmd *node_link_cmd;

	node_link_cmd = v_node_link_create(link->parent->id, link->child->id);

	return v_out_queue_push_tail(node_subscriber->session->out_queue,
			node_subscriber->prio,
			node_link_cmd);
}

/**
 * \brief This function handle changing link between nodes
 */
int vs_handle_link_change(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *node_link)
{
	struct VSUser			*user;
	struct VSNode			*old_parent_node, *parent_node, *child_node;
	struct VSLink			*link;
	struct VSNodeSubscriber	*node_subscriber;
	struct VSEntityFollower *node_follower;
	uint32					parent_node_id = UINT32(node_link->data[0]);
	uint32					child_node_id = UINT32(node_link->data[UINT32_SIZE]);
	int						i;

	/* Try to find child node */
	if((child_node = vs_node_find(vs_ctx, child_node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s():%d node (id: %d) not found\n",
				__FUNCTION__, __LINE__, child_node_id);
		return 0;
	}

	/* Child node has to be created */
	if(! (child_node->state == ENTITY_CREATED || child_node->state == ENTITY_CREATING)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s():%d node id: %d is not in NODE_CREATED state: %d\n",
				__FUNCTION__, __LINE__, child_node->id, child_node->state);
		return 0;
	}

	/* Try to find user */
	if((user = vs_user_find(vs_ctx, vsession->user_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "vsession->user_id: %d not found\n", vsession->user_id);
		return 0;
	}

	/* Is user owner of child node or can user write to child node? */
	if(vs_node_can_write(vs_ctx, vsession, child_node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s():%d user: %s can't write to child node: %d\n",
				__FUNCTION__, __LINE__, user->username, child_node->id);
		return 0;
	}

	/* Old link */
	link = child_node->parent_link;

	/* Old parent node */
	old_parent_node = link->parent;

	/* Is user owner of old parent node or can user write to old parent node? */
	if(vs_node_can_write(vs_ctx, vsession, old_parent_node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s():%d user: %s can't write to old parent node: %d\n",
				__FUNCTION__, __LINE__, user->username, old_parent_node->id);
		return 0;
	}

	/* Try to find new parent node */
	if((parent_node = vs_node_find(vs_ctx, parent_node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s():%d node (id: %d) not found\n",
				__FUNCTION__, __LINE__, parent_node_id);
		return 0;
	}

	/* Parent node has to be created */
	if(! (parent_node->state == ENTITY_CREATED || parent_node->state == ENTITY_CREATING)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s():%d node (id: %d) is not in NODE_CREATED state: %d\n",
				__FUNCTION__, __LINE__, parent_node->id, parent_node->state);
		return 0;
	}

	/* Test if client doesn't want to recreate existing link */
	if( parent_node == old_parent_node) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s():%d link between nodes (parent_node_id: %d) (child_node_id: %d) already exists\n",
				__FUNCTION__, __LINE__, parent_node->id, child_node->id);
		return 0;
	}

	/* Is user owner of parent node or can user write to parent node? */
	if(vs_node_can_write(vs_ctx, vsession, parent_node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s():%d user: %s can't write to parent node: %d\n",
				__FUNCTION__, __LINE__, user->username, parent_node->id);
		return 0;
	}

	/* Test, if new link could be created */
	if(vs_link_test_nodes(parent_node, child_node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s():%d node: %d can't be child of node: %d\n",
				__FUNCTION__, __LINE__, child_node->id, parent_node->id);
		return 0;
	}

	/* Remove link from old parent node */
	v_list_rem_item(&old_parent_node->children_links, link);

	/* Add link to new parent node */
	v_list_add_tail(&parent_node->children_links, link);
	link->parent = parent_node;

	/* Subscribers of old and new parent node will receive information about
	 * changing link between nodes. Prevent double sending command Node_Link,
	 * when clients are subscribed to both nodes. */

	/* Set temporary value in all sessions */
	for(i=0; i<vs_ctx->max_sessions; i++) {
		vs_ctx->vsessions[i]->tmp = 0;
	}

	/* Send Node_Link command to subscribers of old parent node and set
	 * session temporary value */
	node_subscriber = old_parent_node->node_subs.first;
	while(node_subscriber != NULL) {
		if(vs_node_can_read(vs_ctx, node_subscriber->session, old_parent_node) == 1) {
			node_subscriber->session->tmp = 1;
			vs_link_change_send(node_subscriber, link);
		}
		node_subscriber = node_subscriber->next;
	}

	/* When client is subscribed to the new parent node and aware of child
	 * node, then send to the client only node_link */
	node_follower = child_node->node_folls.first;
	while(node_follower != NULL) {
		if(node_follower->node_sub->session->tmp != 1) {
			vs_link_change_send(node_follower->node_sub, link);
			node_follower->node_sub->session->tmp = 1;
		}
		node_follower = node_follower->next;
	}

	/* Send Node_Create command to subscribers of new parent node, when
	 * subscribers were not subscribed to child node */
	node_subscriber = parent_node->node_subs.first;
	while(node_subscriber != NULL) {
		if(node_subscriber->session->tmp != 1) {
			if(vs_node_can_read(vs_ctx, node_subscriber->session, parent_node) == 1) {
				vs_node_send_create(node_subscriber, child_node, NULL);
			}
		}
		node_subscriber = node_subscriber->next;
	}

	return 1;
}
