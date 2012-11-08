/*
 * $Id: vs_layer.c 1348 2012-09-19 20:08:18Z jiri $
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


#include "v_common.h"
#include "v_layer_commands.h"
#include "v_fake_commands.h"

#include "vs_layer.h"
#include "vs_node_access.h"

/**
 * \brief This function creates new layer
 *
 * \param[in]	*node		The pointer at node, where layer will be created
 * \param[in]	*parent		The pointer at parent layer
 * \param[in]	data_type	The type of values stored in layer
 * \param[in]	count		The count of values stored in one layer item
 * \param[in]	type		The client defined type of layer
 *
 * \return This function returns pointer at new created layer
 */
struct VSLayer *vs_layer_create(struct VSNode *node,
		struct VSLayer *parent,
		uint8 data_type,
		uint8 count,
		uint16 type)
{
	struct VSLayer *layer = calloc(1, sizeof(struct VSLayer));
	struct VSLayerValue value;

	if(layer == NULL) {
		return NULL;
	}

	layer->prev = NULL;
	layer->next = NULL;
	layer->id = node->last_layer_id;
	node->last_layer_id++;
	layer->data_type = data_type;
	layer->type = type;
	layer->parent = parent;
	layer->num_vec_comp = count;

	v_hash_array_init(&layer->values,
				HASH_MOD_65536,
				(char*)&(value.id) - (char*)&(value),
				sizeof(uint32));

	/* When parent layer is not NULL, then add this layer to the linked list
	 * of child layers */
	if(parent != NULL) {
		v_list_add_tail(&parent->child_layers, layer);
	}
	/* Initialize linked list of child layers */
	layer->child_layers.first = layer->child_layers.last = NULL;

	layer->layer_folls.first = layer->layer_folls.last = NULL;
	layer->layer_subs.first = layer->layer_subs.last = NULL;

	layer->state = ENTITY_RESERVED;

	return layer;
}

/**
 * \brief This function destroys layer stored at verse server
 */
void vs_layer_destroy(struct VSNode *node, struct VSLayer *layer)
{
	struct VSLayer *child_layer;

	v_hash_array_destroy(&layer->values);

	/* Set references to parent layer in all child layers to NULL */
	child_layer = layer->child_layers.first;
	while(child_layer != NULL) {
		child_layer->parent = NULL;
		child_layer = child_layer->next;
	}
	layer->child_layers.first = NULL;
	layer->child_layers.last = NULL;

	/* If this layer has parent layer, then remove this layer from
	 * parent linked list of child layers */
	if(layer->parent != NULL) {
		v_list_rem_item(&layer->parent->child_layers, layer);
	}

	/* Free list of followers and subscribers */
	v_list_free(&layer->layer_folls);
	v_list_free(&layer->layer_subs);

	v_print_log(VRS_PRINT_DEBUG_MSG, "Layer: %d destroyed\n", layer->id);

	/* Destroy this layer itself */
	v_hash_array_remove_item(&node->layers, layer);
	free(layer);
}

/**
 * \brief This function removes all layers and its values from verse node.
 */
int vs_node_layers_destroy(struct VSNode *node)
{
	struct VBucket *layer_bucket, *layer_bucket_next;
	struct VSLayer *layer;

	layer_bucket = node->layers.lb.first;
	while(layer_bucket != NULL) {
		layer_bucket_next = layer_bucket->next;
		layer = (struct VSLayer*)layer_bucket->data;

		vs_layer_destroy(node, layer);

		layer_bucket = layer_bucket_next;
	}
	return 0;
}

static VSLayer *vs_layer_find(struct VSNode *node, uint16 layer_id)
{
	struct VSLayer find_layer;
	struct VBucket *bucket;
	struct VSLayer *layer = NULL;

	find_layer.id = layer_id;
	bucket = v_hash_array_find_item(&node->layers, &find_layer);
	if(bucket != NULL) {
		layer = (struct VSLayer*)bucket->data;
	}

	return layer;
}

int vs_layer_send_create(struct VSNodeSubscriber *node_subscriber,
		struct VSNode *node,
		struct VSLayer *layer)
{
	struct VSession			*vsession = node_subscriber->session;
	struct VSEntityFollower *layer_follower;
	struct Generic_Cmd		*layer_create_cmd;

	/* TODO: this could be more effective */
	/* Check if this command, has not been already sent */
	layer_follower = layer->layer_folls.first;
	while(layer_follower != NULL) {
		if(layer_follower->node_sub->session->session_id == node_subscriber->session->session_id) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "Client already knows about this Layer: %d\n",
					layer->id);
			return 0;
		}
		layer_follower = layer_follower->next;
	}

	if(layer->parent != NULL) {
		layer_create_cmd = v_layer_create_create(node->id, layer->parent->id,
				layer->id, layer->data_type, layer->num_vec_comp, layer->type);
	} else {
		layer_create_cmd = v_layer_create_create(node->id, RESERVED_LAYER_ID,
				layer->id, layer->data_type, layer->num_vec_comp, layer->type);
	}

	if(layer_create_cmd != NULL &&
			v_out_queue_push_tail(vsession->out_queue, node_subscriber->prio, layer_create_cmd) == 1)
	{
		/* Add this session to the list of session, that knows about this
		 * layer. Server could send them layer_destroy in the future. */
		layer_follower = (struct VSEntityFollower*)calloc(1, sizeof(struct VSEntityFollower));
		layer_follower->node_sub = node_subscriber;
		layer_follower->state = ENTITY_CREATING;
		v_list_add_tail(&layer->layer_folls, layer_follower);

		return 1;
	}

	return 0;
}

/**
 * \brief This function send command layer_destroy to all followers of this
 * layer (all subscribers to parent node of this layer)
 *
 * When this layer has any child layer, then verse server will automatically
 * and recursively sends layer_destroy to all child layers of this layer.
 *
 * \param[in]	*node	The pointer at verse node
 * \param[in]	*layer	The pointer at verse layer
 */
int vs_layer_send_destroy(struct VSNode *node,
		struct VSLayer *layer)
{
	struct VSLayer *child_layer;
	struct Generic_Cmd *layer_destroy_cmd;
	struct VSEntityFollower *layer_follower;
	int ret = 0;

	/* Send layer destroy to all child layers */
	child_layer = layer->child_layers.first;
	while(child_layer != NULL) {
		child_layer->state = ENTITY_DELETING;
		vs_layer_send_destroy(node, child_layer);
		child_layer = child_layer->next;
	}

	layer_follower = layer->layer_folls.first;
	while(layer_follower != NULL) {
		if(layer_follower->state == ENTITY_CREATED) {
			/* Create TagGroup_Destroy command */
			layer_destroy_cmd = v_layer_destroy_create(node->id, layer->id);

			/* Put this command to the outgoing queue */
			if( layer_destroy_cmd != NULL &&
					(v_out_queue_push_tail(layer_follower->node_sub->session->out_queue,
							layer_follower->node_sub->prio,
							layer_destroy_cmd) == 1))
			{
				layer_follower->state = ENTITY_DELETING;
				ret = 1;
			} else {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Layer_Destroy (node_id: %d, layer_id: %d) wasn't added to the queue\n",
						node->id, layer->id);
			}
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "node_id: %d, layer_id: %d, layer is not in CREATED state (current state: %d)\n",
					node->id, layer->id, layer_follower->state);
		}

		layer_follower = layer_follower->next;
	}

	return ret;
}

/**
 * \brief This function send unset layer value to the client
 */
int vs_layer_send_unset_value(struct VSEntitySubscriber *layer_subscriber,
		struct VSNode *node,
		struct VSLayer *layer,
		struct VSLayerValue *value)
{
	struct Generic_Cmd *unset_value_cmd;

	unset_value_cmd = v_layer_unset_value_create(node->id, layer->id, value->id);

	return v_out_queue_push_tail(layer_subscriber->node_sub->session->out_queue,
			layer_subscriber->node_sub->prio,
			unset_value_cmd);
}

/**
 * \brief This function send set layer value to the client
 */
int vs_layer_send_set_value(struct VSEntitySubscriber *layer_subscriber,
		struct VSNode *node,
		struct VSLayer *layer,
		struct VSLayerValue *value)
{
	struct Generic_Cmd *set_value_cmd;

	set_value_cmd = v_layer_set_value_create(node->id, layer->id, value->id,
			layer->data_type, layer->num_vec_comp, value->value);

	return v_out_queue_push_tail(layer_subscriber->node_sub->session->out_queue,
			layer_subscriber->node_sub->prio,
			set_value_cmd);
}

/**
 * \brief This function is called, when client acknowledge receiving of
 * layer_create command
 */
int vs_handle_layer_create_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd)
{
	struct VSNode *node;
	struct VSLayer *layer;
	struct VSEntityFollower *layer_foll;
	struct Layer_Create_Ack_Cmd *layer_create_cmd_ack = (struct Layer_Create_Ack_Cmd*)cmd;
	int all_created = 1;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, layer_create_cmd_ack->node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, layer_create_cmd_ack->node_id);
		return 0;
	}

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_create_cmd_ack->layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_create_cmd_ack->layer_id, layer_create_cmd_ack->node_id);
		return 0;
	}

	for(layer_foll = layer->layer_folls.first;
			layer_foll != NULL;
			layer_foll = layer_foll->next)
	{
		if(layer_foll->node_sub->session->session_id == vsession->session_id) {
			/* Switch from state CREATING to state CREATED */
			if(layer_foll->state == ENTITY_CREATING) {
				layer_foll->state = ENTITY_CREATED;
			}

			/* If the layer is in the state DELETING, then it is possible
			 * now to sent layer_destroy command to the client, because
			 * the client knows about this layer now */
			if(layer->state == ENTITY_DELETING) {
				struct Generic_Cmd *layer_destroy_cmd = v_layer_destroy_create(node->id, layer->id);

				/* Push this command to the outgoing queue */
				if ( layer_destroy_cmd!= NULL &&
						v_out_queue_push_tail(layer_foll->node_sub->session->out_queue,
								layer_foll->node_sub->prio,
								layer_destroy_cmd) == 1) {
					layer_foll->state = ENTITY_DELETING;
				} else {
					v_print_log(VRS_PRINT_DEBUG_MSG, "layer_destroy (node_id: %d, layer_id: %d) wasn't added to the queue\n",
							node->id, layer->id);
				}
			} else {
				if(layer_foll->state != ENTITY_CREATED) {
					all_created = 0;
				}
			}
		}
	}

	if(all_created == 1) {
		layer->state = ENTITY_CREATED;
	}


	return 0;
}

/**
 * \brief This function is called, when server receives command layer_create
 *
 * \param[in]	*vs_ctx				The pointer at verse server context
 * \param[in]	*vsession			The pointer at session of client that sent this command
 * \param[in]	*layer_create_cmd	The pointer at received command
 *
 * \return This function returns 0, when parameters of received command are
 * wrong. When all parameters are right, then it returns 1.
 */
int vs_handle_layer_create(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_create_cmd)
{
	struct VSNode *node;
	struct VSUser *user;
	struct VSLayer *parent_layer;
	struct VSLayer *layer;
	struct VBucket *vbucket;
	struct VSNodeSubscriber *node_subscriber;

	uint32 node_id = UINT32(layer_create_cmd->data[0]);
	uint16 parent_layer_id = UINT16(layer_create_cmd->data[UINT32_SIZE]);
	uint16 layer_id = UINT16(layer_create_cmd->data[UINT32_SIZE+UINT16_SIZE]);
	uint8 data_type = UINT16(layer_create_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT16_SIZE]);
	uint8 count = UINT16(layer_create_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT16_SIZE+UINT8_SIZE]);
	uint16 type = UINT16(layer_create_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT16_SIZE+UINT8_SIZE+UINT8_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* When parent layer id is not -1, then try to find this parent layer */
	if(parent_layer_id == RESERVED_LAYER_ID) {
		parent_layer = NULL;
	} else {
		parent_layer = vs_layer_find(node, parent_layer_id);
		if(parent_layer == NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "%s() parent layer (id: %d) in node (id: %d) not found\n",
					__FUNCTION__, parent_layer_id, node_id);
			return 0;
		}
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

	/* Client has to send layer_create command with layer_id equal to
	 * the value 0xFFFF */
	if(layer_id != RESERVED_LAYER_ID) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer_id: %d is not 0xFFFF\n",
				__FUNCTION__, layer_id);
		return 0;
	}

	/* Is type of Layer supported? */
	if(!(data_type>VRS_VALUE_TYPE_RESERVED && data_type<=VRS_VALUE_TYPE_REAL64)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() data_type: %d is not supported\n",
				__FUNCTION__, data_type);
		return 0;
	}

	vbucket = node->layers.lb.first;
	/* Check, if there isn't tag with the same type */
	while(vbucket != NULL) {
		layer = vbucket->data;
		if(layer->type == type) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer type: %d is already used in layer: %d\n",
					__FUNCTION__, type, layer->id);
			return 0;
		}
		vbucket = vbucket->next;
	}

	if (v_hash_array_count_items(&node->layers) > MAX_LAYERS_COUNT) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() max number of layers in node: %d was reached\n",
				node->id);
		return 0;
	}

	layer = vs_layer_create(node, parent_layer, data_type, count, type);

	/* Try to find first free id for layer */
	/* TODO: this could be more effective */
	layer->id = node->last_layer_id;
	while( v_hash_array_find_item(&node->layers, layer) != NULL ) {
		layer->id++;

		if(layer->id > LAST_LAYER_ID) {
			layer->id = FIRST_LAYER_ID;
		}
	}
	node->last_layer_id = layer->id;

	/* Try to add new Tag to the hashed linked list of tags */
	vbucket = v_hash_array_add_item(&node->layers, (void*)layer, sizeof(struct VSLayer));

	if(vbucket==NULL) {
		free(layer);
		return 0;
	}

	layer->state = ENTITY_CREATING;

	/* Send layer_create to all node subscribers */
	node_subscriber = node->node_subs.first;
	while(node_subscriber != NULL) {
		vs_layer_send_create(node_subscriber, node, layer);
		node_subscriber = node_subscriber->next;
	}

	return 0;
}

int vs_handle_layer_destroy_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd)
{
	struct VSNode *node;
	struct VSLayer *layer;
	struct VSEntityFollower *layer_foll, *next_layer_foll;
	struct Layer_Destroy_Ack_Cmd *layer_destroy_cmd = (struct Layer_Destroy_Ack_Cmd*)cmd;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, layer_destroy_cmd->node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, layer_destroy_cmd->node_id);
		return 0;
	}

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_destroy_cmd->layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_destroy_cmd->layer_id, layer_destroy_cmd->node_id);
		return 0;
	}

	/* Mark the layer in this session as DELETED and remove this follower from
	 * the list of layer followers */
	layer_foll = layer->layer_folls.first;
	while(layer_foll != NULL) {
		next_layer_foll = layer_foll->next;
		if(layer_foll->node_sub->session->session_id == vsession->session_id) {
			layer_foll->state = ENTITY_DELETED;
			v_list_free_item(&layer->layer_folls, layer_foll);
		}
		layer_foll = next_layer_foll;
	}

	/* When layer doesn't have any follower, then it is possible to destroy
	 * this layer */
	if(layer->layer_folls.first == NULL) {
		layer->state = ENTITY_DELETED;
		vs_layer_destroy(node, layer);
	}

	return 0;
}

int vs_handle_layer_destroy(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_destroy_cmd)
{
	struct VSNode *node;
	struct VSLayer *layer;
	struct VSUser *user;

	uint32 node_id = UINT32(layer_destroy_cmd->data[0]);
	uint16 layer_id = UINT16(layer_destroy_cmd->data[UINT32_SIZE]);

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

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_id, node_id);
		return 0;
	}

	/* Layer has to be created */
	if(! (layer->state == ENTITY_CREATING || layer->state == ENTITY_CREATED)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer (id: %u) in node (id: %d) is not in CREATED state: %d\n",
				__FUNCTION__, layer->id, node->id, node->state);
		return 0;
	}

	layer->state = ENTITY_DELETING;

	return vs_layer_send_destroy(node, layer);
}


int vs_handle_layer_subscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_subscribe_cmd)
{
	struct VSNode *node;
	struct VSUser *user;
	struct VSLayer *layer;
	struct VSNodeSubscriber *node_subscriber;
	struct VSEntitySubscriber *layer_subscriber;
	struct VBucket *vbucket;
	struct VSLayerValue *value;

	uint32 node_id = UINT32(layer_subscribe_cmd->data[0]);
	uint16 layer_id = UINT16(layer_subscribe_cmd->data[UINT32_SIZE]);
/*
	uint32 version = UINT32(layer_subscribe_cmd->data[UINT32_SIZE+UINT16_SIZE]);
	uint32 crc32 = UINT32(layer_subscribe_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE]);
*/

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

	/* User has to have permission to read the node */
	if(vs_node_can_read(vs_ctx, vsession, node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s(): user: %s can't read the node: %d\n",
				__FUNCTION__, user->username, node->id);
		return 0;
	}

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
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s(): client has to be subscribed to the node: %d before subscribing to the layer: %d\n",
				__FUNCTION__, node_id, layer_id);
		return 0;
	}

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_id, node_id);
		return 0;
	}

	/* Layer has to be created */
	if(! (layer->state == ENTITY_CREATING || layer->state == ENTITY_CREATED)) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer (id: %u) in node (id: %d) is not in CREATED state: %d\n",
				__FUNCTION__, layer->id, node->id, node->state);
		return 0;
	}

	/* Try to find layer subscriber (client can't be subscribed twice) */
	layer_subscriber = layer->layer_subs.first;
	while(layer_subscriber != NULL) {
		if(layer_subscriber->node_sub->session->session_id == vsession->session_id) {
			v_print_log(VRS_PRINT_DEBUG_MSG, "%s() client already subscribed to the layer (id: %d) in node (id: %d)\n",
									__FUNCTION__, layer_id, node_id);
			return 0;
		}
		layer_subscriber = layer_subscriber->next;
	}

	/* Add new subscriber to the list of layer subscribers */
	layer_subscriber = (struct VSEntitySubscriber*)malloc(sizeof(struct VSEntitySubscriber));
	layer_subscriber->node_sub = node_subscriber;
	v_list_add_tail(&layer->layer_subs, layer_subscriber);

	/* Send value set for all items in this layer
	 * TODO: do not push all values to outgoing queue at once, when there is lot
	 * of values in this layer. Implement this, when queue limits will be
	 * finished. */
	vbucket = layer->values.lb.first;
	while(vbucket != NULL) {
		value = (struct VSLayerValue*)vbucket->data;
		vs_layer_send_set_value(layer_subscriber, node, layer, value);
		vbucket = vbucket->next;
	}

	return 1;
}


int vs_handle_layer_unsubscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_unsubscribe_cmd)
{
	struct VSNode *node;
	struct VSLayer *layer;
	struct VSEntitySubscriber *layer_subscriber;

	uint32 node_id = UINT32(layer_unsubscribe_cmd->data[0]);
	uint16 layer_id = UINT16(layer_unsubscribe_cmd->data[UINT32_SIZE]);
/*
	uint32 version = UINT32(layer_subscribe_cmd->data[UINT32_SIZE+UINT16_SIZE]);
	uint32 crc32 = UINT32(layer_subscribe_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE]);
*/

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_id, node_id);
		return 0;
	}

	/* Try to find layer subscriber */
	layer_subscriber = layer->layer_subs.first;
	while(layer_subscriber != NULL) {
		if(layer_subscriber->node_sub->session->session_id == vsession->session_id) {
			break;
		}
		layer_subscriber = layer_subscriber->next;
	}

	/* Client has to be subscribed to the layer */
	if(layer_subscriber == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() client not subscribed to the layer (id: %d) in node (id: %d)\n",
								__FUNCTION__, layer_id, node_id);
		return 0;
	}

	/* Remove client from the list of subscribers */
	v_list_free_item(&layer->layer_subs, layer_subscriber);

	return 1;
}


int vs_handle_layer_set_value(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_set_value_cmd,
		uint8 data_type,
		uint8 count)
{
	struct VSNode *node;
	struct VSLayer *layer;
	struct VSLayerValue *item, _item;
	struct VBucket *vbucket;
	struct VSUser *user;
	struct VSEntitySubscriber *layer_subscriber;

	uint32 node_id = UINT32(layer_set_value_cmd->data[0]);
	uint16 layer_id = UINT16(layer_set_value_cmd->data[UINT32_SIZE]);
	uint32 item_id = UINT32(layer_set_value_cmd->data[UINT32_SIZE + UINT16_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
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
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s(): user: %s can't write to the node: %d\n",
				__FUNCTION__, user->username, node->id);
		return 0;
	}

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_id, node_id);
		return 0;
	}

	/* Check type of value */
	if( data_type != layer->data_type ) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() data type (%d) of layer (id: %d) in node (id: %d) does not match data type of received command (%d)\n",
				__FUNCTION__, layer->data_type, layer_id, node_id, data_type);
		return 0;
	}

	/* Check count of value */
	if( count != layer->num_vec_comp ) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() count of values (%d) of layer (id: %d) in node (id: %d) does not match count of values of received command (%d)\n",
				__FUNCTION__, layer->num_vec_comp, layer_id, node_id, count);
		return 0;
	}

	/* Set item value */

	/* Try to find item value first */
	_item.id = item_id;
	vbucket = v_hash_array_find_item(&layer->values, &_item);
	if(vbucket == NULL) {
		item = calloc(1, sizeof(struct VSLayerValue));
		item->id = item_id;
		v_hash_array_add_item(&layer->values, item, sizeof(struct VSLayerValue));

		/* Allocate memory for values and copy data to this memory */
		switch(layer->data_type) {
			case VRS_VALUE_TYPE_UINT8:
				item->value = (void*)calloc(layer->num_vec_comp, sizeof(uint8));
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*UINT8_SIZE);
				break;
			case VRS_VALUE_TYPE_UINT16:
				item->value = (void*)calloc(layer->num_vec_comp, sizeof(uint16));
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*UINT16_SIZE);
				break;
			case VRS_VALUE_TYPE_UINT32:
				item->value = (void*)calloc(layer->num_vec_comp, sizeof(uint32));
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*UINT32_SIZE);
				break;
			case VRS_VALUE_TYPE_UINT64:
				item->value = (void*)calloc(layer->num_vec_comp, sizeof(uint64));
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*UINT64_SIZE);
				break;
			case VRS_VALUE_TYPE_REAL16:
				item->value = (void*)calloc(layer->num_vec_comp, sizeof(real16));
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*REAL16_SIZE);
				break;
			case VRS_VALUE_TYPE_REAL32:
				item->value = (void*)calloc(layer->num_vec_comp, sizeof(real32));
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*REAL32_SIZE);
				break;
			case VRS_VALUE_TYPE_REAL64:
				item->value = (void*)calloc(layer->num_vec_comp, sizeof(real64));
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*REAL64_SIZE);
				break;
			default:
				return 0;
		}
	} else {
		item = (struct VSLayerValue*)vbucket->data;
		switch(layer->data_type) {
			case VRS_VALUE_TYPE_UINT8:
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*UINT8_SIZE);
				break;
			case VRS_VALUE_TYPE_UINT16:
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*UINT16_SIZE);
				break;
			case VRS_VALUE_TYPE_UINT32:
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*UINT32_SIZE);
				break;
			case VRS_VALUE_TYPE_UINT64:
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*UINT64_SIZE);
				break;
			case VRS_VALUE_TYPE_REAL16:
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*REAL16_SIZE);
				break;
			case VRS_VALUE_TYPE_REAL32:
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*REAL32_SIZE);
				break;
			case VRS_VALUE_TYPE_REAL64:
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE],
						layer->num_vec_comp*REAL64_SIZE);
				break;
			default:
				return 0;
		}
	}

	/* Send item value set to all layer subscribers */
	layer_subscriber = layer->layer_subs.first;
	while(layer_subscriber != NULL) {
		vs_layer_send_set_value(layer_subscriber, node, layer, item);
		layer_subscriber = layer_subscriber->next;
	}

	return 1;
}

static int vs_layer_unset_value(struct VSNode *node,
		struct VSLayer *layer,
		uint32 item_id,
		uint8 send_command)
{
	struct VSLayer *child_layer;
	struct VSLayerValue *item = NULL, _item;
	struct VBucket *vbucket;
	struct VSEntitySubscriber *layer_subscriber;

	/* Try to find item value first */
	_item.id = item_id;
	vbucket = v_hash_array_find_item(&layer->values, &_item);
	if(vbucket != NULL) {
		item = (struct VSLayerValue*)vbucket->data;

		/* Send unset command only for parent layer */
		if(send_command == 1) {
			/* Send item value unset to all layer subscribers */

			layer_subscriber = layer->layer_subs.first;
			while(layer_subscriber != NULL) {
				vs_layer_send_unset_value(layer_subscriber, node, layer, item);
				layer_subscriber = layer_subscriber->next;
			}
		}

		/* Free value */
		free(item->value);
		v_hash_array_remove_item(&layer->values, item);
		/* Free item */
		free(item);
	}

	/* Unset values in all child values, but don't send unset_value command
	 * about this unsetting, because client will receive layer_unset of parent
	 * layer*/
	child_layer = layer->child_layers.first;
	while(child_layer != NULL) {
		vs_layer_unset_value(node, child_layer, item_id, 0);
		child_layer = child_layer->next;
	}

	return 1;
}

/**
 * \brief This function handle command layer_unset_value
 */
int vs_handle_layer_unset_value(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_unset_value_cmd)
{
	struct VSNode *node;
	struct VSLayer *layer;
	struct VSUser *user;

	uint32 node_id = UINT32(layer_unset_value_cmd->data[0]);
	uint16 layer_id = UINT16(layer_unset_value_cmd->data[UINT32_SIZE]);
	uint32 item_id = UINT32(layer_unset_value_cmd->data[UINT32_SIZE + UINT16_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
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
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s(): user: %s can't write to the node: %d\n",
				__FUNCTION__, user->username, node->id);
		return 0;
	}

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_id, node_id);
		return 0;
	}

	return vs_layer_unset_value(node, layer, item_id, 1);
}

