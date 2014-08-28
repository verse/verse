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


#include "v_common.h"
#include "v_layer_commands.h"
#include "v_fake_commands.h"

#include "vs_layer.h"
#include "vs_node_access.h"

/**
 * \brief This function increments version of layer
 */
void vs_layer_inc_version(struct VSLayer *layer)
{
	/* TODO: compute CRC32 */
	if( (layer->version + 1 ) < UINT32_MAX ) {
		layer->version++;
	} else {
		layer->version = 1;
		layer->saved_version = 0;
	}
}

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
		uint16 layer_id,
		uint8 data_type,
		uint8 count,
		uint16 type)
{
#ifdef WITH_MONGODB
	int i;
#endif
	struct VSLayer *layer = calloc(1, sizeof(struct VSLayer));
	struct VBucket *vbucket;

	if(layer == NULL) {
		return NULL;
	}

	layer->prev = NULL;
	layer->next = NULL;
	layer->data_type = data_type;
	layer->custom_type = type;
	layer->parent = parent;
	layer->num_vec_comp = count;

	if(layer_id == VRS_RESERVED_LAYER_ID) {
		/* Try to find first free id for layer */
		layer->id = node->first_free_layer_id;
		while( v_hash_array_find_item(&node->layers, layer) != NULL ) {
			layer->id++;

			if(layer->id > LAST_LAYER_ID) {
				layer->id = FIRST_LAYER_ID;
			}
		}
	} else {
		layer->id = layer_id;
		if(v_hash_array_find_item(&node->layers, layer) != NULL) {
			v_print_log(VRS_PRINT_ERROR,
					"Layer with %d already exists in node %d\n",
					layer_id, node->id);
			free(layer);
			return NULL;
		}
	}
	node->first_free_layer_id = layer->id + 1;

	/* Try to add new layer to the hashed linked list of layers */
	vbucket = v_hash_array_add_item(&node->layers,
			(void*)layer, sizeof(struct VSLayer));

	if(vbucket == NULL) {
		free(layer);
		return NULL;
	}

	v_hash_array_init(&layer->values,
				HASH_MOD_65536,
				offsetof(VSLayerValue, id),
				sizeof(uint32));

	/* When parent layer is not NULL, then add this layer to the linked list
	 * of child layers */
	if(parent != NULL) {
		v_list_add_tail(&parent->child_layers, layer);
		vs_layer_inc_version(parent);
	}
	/* Initialize linked list of child layers */
	layer->child_layers.first = layer->child_layers.last = NULL;

	layer->layer_folls.first = layer->layer_folls.last = NULL;
	layer->layer_subs.first = layer->layer_subs.last = NULL;

	layer->state = ENTITY_RESERVED;

	layer->version = 0;
	layer->saved_version = -1;
	layer->crc32 = 0;

#ifdef WITH_MONGODB
	for(i=0; i<3; i++) {
		layer->oid.ints[i] = 0;
	}
#endif

	vs_node_inc_version(node);

	return layer;
}

/**
 * \brief This function destroys layer stored at verse server
 */
void vs_layer_destroy(struct VSNode *node, struct VSLayer *layer)
{
	struct VSLayer *child_layer;
	struct VSLayerValue *item;
	struct VBucket *vbucket;

	/* Free values in all items */
	vbucket = (struct VBucket*)layer->values.lb.first;
	while(vbucket != NULL) {
		item = (struct VSLayerValue*)vbucket->data;
		free(item->value);
		vbucket = vbucket->next;
	}

	/* Destroy hashed array with items */
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

	vs_node_inc_version(node);
}

/**
 * \brief This function unsubscribe client from the layer
 */
int vs_layer_unsubscribe(struct VSLayer *layer, struct VSession *vsession)
{
	struct VSEntitySubscriber	*layer_subscriber;

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
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() client not subscribed to the layer (id: %d)\n",
				__FUNCTION__, layer->id);
		return 0;
	}

	/* Remove client from the list of subscribers */
	v_list_free_item(&layer->layer_subs, layer_subscriber);

	return 1;
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

/**
 * \brief Try to find layer within node with specified layer
 */
VSLayer *vs_layer_find(struct VSNode *node, uint16 layer_id)
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

	/* Check if this layer is in created/creating state */
	if(!(layer->state == ENTITY_CREATING || layer->state == ENTITY_CREATED)) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"This layer: %d in node: %d is in %d state\n",
				layer->id, node->id, layer->state);
		return 0;
	}

	/* Check if this command, has not been already sent */
	layer_follower = layer->layer_folls.first;
	while(layer_follower != NULL) {
		if(layer_follower->node_sub->session->session_id == node_subscriber->session->session_id) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"Client already knows about this Layer: %d\n",
					layer->id);
			return 0;
		}
		layer_follower = layer_follower->next;
	}

	if(layer->parent != NULL) {
		layer_create_cmd = v_layer_create_create(node->id, layer->parent->id,
				layer->id, layer->data_type, layer->num_vec_comp, layer->custom_type);
	} else {
		layer_create_cmd = v_layer_create_create(node->id, VRS_RESERVED_LAYER_ID,
				layer->id, layer->data_type, layer->num_vec_comp, layer->custom_type);
	}

	if(layer_create_cmd == NULL) {
		return 0;
	}

	if(v_out_queue_push_tail(vsession->out_queue, node_subscriber->prio, layer_create_cmd) == 0) {
		return -1;
	} else {
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
				v_print_log(VRS_PRINT_DEBUG_MSG,
						"Layer_Destroy (node_id: %d, layer_id: %d) wasn't added to the queue\n",
						node->id, layer->id);
			}
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"node_id: %d, layer_id: %d, layer is not in CREATED state (current state: %d)\n",
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

	if(set_value_cmd != NULL) {
		return v_out_queue_push_tail(layer_subscriber->node_sub->session->out_queue,
			layer_subscriber->node_sub->prio,
			set_value_cmd);
	} else {
		return 0;
	}
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
	int all_created = 1, ret = 0;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, layer_create_cmd_ack->node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, layer_create_cmd_ack->node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_create_cmd_ack->layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__,
				layer_create_cmd_ack->layer_id,
				layer_create_cmd_ack->node_id);
		goto end;
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

			ret = 1;

			/* If the layer is in the state DELETING, then it is possible
			 * now to sent layer_destroy command to the client, because
			 * the client knows about this layer now */
			if(layer->state == ENTITY_DELETING) {
				struct Generic_Cmd *layer_destroy_cmd = v_layer_destroy_create(node->id, layer->id);

				/* Push this command to the outgoing queue */
				if ( layer_destroy_cmd != NULL &&
						v_out_queue_push_tail(layer_foll->node_sub->session->out_queue,
								layer_foll->node_sub->prio,
								layer_destroy_cmd) == 1)
				{
					layer_foll->state = ENTITY_DELETING;
				} else {
					v_print_log(VRS_PRINT_DEBUG_MSG,
							"layer_destroy (node_id: %d, layer_id: %d) wasn't added to the queue\n",
							node->id, layer->id);
					ret = 0;
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

end:
	pthread_mutex_unlock(&node->mutex);

	return ret;
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
	int ret = 0;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* Node has to be created */
	if( vs_node_is_created(node) != 1 ) {
		return 0;
	}

	/* When parent layer id is not -1, then try to find this parent layer */
	if(parent_layer_id == VRS_RESERVED_LAYER_ID) {
		parent_layer = NULL;
	} else {
		parent_layer = vs_layer_find(node, parent_layer_id);
		if(parent_layer == NULL) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"%s() parent layer (id: %d) in node (id: %d) not found\n",
					__FUNCTION__, parent_layer_id, node_id);
			goto end;
		}
	}

	/* User has to have permission to write to the node */
	if(vs_node_can_write(vsession, node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s(): user: %s can't write to node: %d\n",
				__FUNCTION__,
				((struct VSUser *)(vsession->user))->username,
				node->id);
		goto end;
	}

	/* Client has to send layer_create command with layer_id equal to
	 * the value 0xFFFF */
	if(layer_id != VRS_RESERVED_LAYER_ID) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() layer_id: %d is not 0xFFFF\n",
				__FUNCTION__, layer_id);
		goto end;
	}

	/* Is type of Layer supported? */
	if(!(data_type>VRS_VALUE_TYPE_RESERVED && data_type<=VRS_VALUE_TYPE_REAL64)) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() data_type: %d is not supported\n",
				__FUNCTION__, data_type);
		goto end;
	}

	vbucket = node->layers.lb.first;
	/* Check, if there isn't tag with the same type */
	while(vbucket != NULL) {
		layer = vbucket->data;
		if(layer->custom_type == type) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"%s() layer type: %d is already used in layer: %d\n",
					__FUNCTION__, type, layer->id);
			goto end;
		}
		vbucket = vbucket->next;
	}

	if (v_hash_array_count_items(&node->layers) > MAX_LAYERS_COUNT) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() max number of layers in node: %d was reached\n",
				node->id);
		goto end;
	}

	layer = vs_layer_create(node, parent_layer, VRS_RESERVED_LAYER_ID,
			data_type, count, type);

	if(layer == NULL) {
		goto end;
	}

	layer->state = ENTITY_CREATING;

	ret = 1;

	/* Send layer_create to all node subscribers */
	node_subscriber = node->node_subs.first;
	while(node_subscriber != NULL) {
		if(vs_layer_send_create(node_subscriber, node, layer) != 1) {
			ret = 0;
		}
		node_subscriber = node_subscriber->next;
	}

end:
	pthread_mutex_unlock(&node->mutex);

	return ret;
}

int vs_handle_layer_destroy_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *cmd)
{
	struct VSNode *node;
	struct VSLayer *layer;
	struct VSEntityFollower *layer_foll, *next_layer_foll;
	struct Layer_Destroy_Ack_Cmd *layer_destroy_cmd = (struct Layer_Destroy_Ack_Cmd*)cmd;
	int ret = 0;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, layer_destroy_cmd->node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, layer_destroy_cmd->node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_destroy_cmd->layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__,
				layer_destroy_cmd->layer_id,
				layer_destroy_cmd->node_id);
		goto end;
	}

	ret = 1;

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

end:
	pthread_mutex_unlock(&node->mutex);

	return ret;
}

int vs_handle_layer_destroy(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_destroy_cmd)
{
	struct VSNode *node;
	struct VSLayer *layer;
	uint32 node_id = UINT32(layer_destroy_cmd->data[0]);
	uint16 layer_id = UINT16(layer_destroy_cmd->data[UINT32_SIZE]);
	int ret = 0;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* Node has to be created */
	if( vs_node_is_created(node) != 1 ) {
		goto end;
	}

	/* User has to have permission to write to the node */
	if(vs_node_can_write(vsession, node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s(): user: %s can't write to node: %d\n",
				__FUNCTION__,
				((struct VSUser *)(vsession->user))->username,
				node->id);
		goto end;
	}

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_id, node_id);
		goto end;
	}

	/* Layer has to be created */
	if(! (layer->state == ENTITY_CREATING || layer->state == ENTITY_CREATED)) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() layer (id: %u) in node (id: %d) is not in CREATED state: %d\n",
				__FUNCTION__, layer->id, node->id, node->state);
		goto end;
	}

	layer->state = ENTITY_DELETING;

	ret = vs_layer_send_destroy(node, layer);

end:
	pthread_mutex_unlock(&node->mutex);

	return ret;
}

/**
 * \brief This function tries to push set_value to outgoing queue.
 *
 * When there is not enough free space in outgoing queue, then pushing more
 * command is stopped.
 */
static void vs_layer_send_values(struct VSEntitySubscriber *layer_subscriber,
		struct VSNode *node,
		struct VSLayer *layer,
		struct VBucket **next_item)
{
	struct VSLayerValue *value;
	struct VBucket *vbucket = NULL;

	/* When this sending is not invoked by sending task, but directly by
	 * subscribe command, then begin with first bucket. */
	if(next_item == NULL) {
		vbucket = layer->values.lb.first;
	} else {
		vbucket = *next_item;
	}

	/* Send value_set cmd for all possible items in this layer */
	while(vbucket != NULL) {
		value = (struct VSLayerValue*)vbucket->data;
		/* Try to push command to outgoing queue, when there is enough free
		 * space */
		if(vs_layer_send_set_value(layer_subscriber, node, layer, value) == -1) {
			/* TODO: create sending task, when next_item is NULL.
			 * Otherwise update *next_item */
			break;
		}
		vbucket = vbucket->next;
	}
}

int vs_handle_layer_subscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_subscribe_cmd)
{
	struct VSNode *node;
	struct VSLayer *layer;
	struct VSNodeSubscriber *node_subscriber;
	struct VSEntitySubscriber *layer_subscriber;
	uint32 node_id = UINT32(layer_subscribe_cmd->data[0]);
	uint16 layer_id = UINT16(layer_subscribe_cmd->data[UINT32_SIZE]);
/*	uint32 version = UINT32(layer_subscribe_cmd->data[UINT32_SIZE+UINT16_SIZE]);
	uint32 crc32 = UINT32(layer_subscribe_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE]); */
	int ret = 0;

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

	/* User has to have permission to read the node */
	if(vs_node_can_read(vsession, node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s(): user: %s can't read the node: %d\n",
				__FUNCTION__,
				((struct VSUser *)(vsession->user))->username,
				node->id);
		goto end;
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
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s(): client has to be subscribed to the node: %d before subscribing to the layer: %d\n",
				__FUNCTION__, node_id, layer_id);
		goto end;
	}

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_id, node_id);
		goto end;
	}

	/* Layer has to be created */
	if(! (layer->state == ENTITY_CREATING || layer->state == ENTITY_CREATED)) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() layer (id: %u) in node (id: %d) is not in CREATED state: %d\n",
				__FUNCTION__, layer->id, node->id, node->state);
		goto end;
	}

	/* Try to find layer subscriber (client can't be subscribed twice) */
	layer_subscriber = layer->layer_subs.first;
	while(layer_subscriber != NULL) {
		if(layer_subscriber->node_sub->session->session_id == vsession->session_id) {
			v_print_log(VRS_PRINT_DEBUG_MSG,
					"%s() client already subscribed to the layer (id: %d) in node (id: %d)\n",
					__FUNCTION__, layer_id, node_id);
			goto end;
		}
		layer_subscriber = layer_subscriber->next;
	}

	/* Add new subscriber to the list of layer subscribers */
	layer_subscriber = (struct VSEntitySubscriber*)malloc(sizeof(struct VSEntitySubscriber));
	layer_subscriber->node_sub = node_subscriber;
	v_list_add_tail(&layer->layer_subs, layer_subscriber);
	ret = 1;

	vs_layer_send_values(layer_subscriber, node, layer, NULL);

end:
	pthread_mutex_unlock(&node->mutex);

	return ret;
}


int vs_handle_layer_unsubscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_unsubscribe_cmd)
{
	struct VSNode *node;
	struct VSLayer *layer;

	uint32 node_id = UINT32(layer_unsubscribe_cmd->data[0]);
	uint16 layer_id = UINT16(layer_unsubscribe_cmd->data[UINT32_SIZE]);
/*	uint32 version = UINT32(layer_subscribe_cmd->data[UINT32_SIZE+UINT16_SIZE]);
	uint32 crc32 = UINT32(layer_subscribe_cmd->data[UINT32_SIZE+UINT16_SIZE+UINT32_SIZE]); */
	int ret = 0;

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_id, node_id);
		goto end;
	}

	ret = vs_layer_unsubscribe(layer, vsession);

end:
	pthread_mutex_unlock(&node->mutex);

	return ret;
}

/**
 * \brief This function returns size of data type stored in layer item
 */
int vs_layer_data_size(struct VSLayer *layer)
{
	int item_data_size = 0;

	switch(layer->data_type) {
		case VRS_VALUE_TYPE_UINT8:
			item_data_size = UINT8_SIZE;
			break;
		case VRS_VALUE_TYPE_UINT16:
			item_data_size = UINT16_SIZE;
			break;
		case VRS_VALUE_TYPE_UINT32:
			item_data_size = UINT32_SIZE;
			break;
		case VRS_VALUE_TYPE_UINT64:
			item_data_size = UINT64_SIZE;
			break;
		case VRS_VALUE_TYPE_REAL16:
			item_data_size = REAL16_SIZE;
			break;
		case VRS_VALUE_TYPE_REAL32:
			item_data_size = REAL32_SIZE;
			break;
		case VRS_VALUE_TYPE_REAL64:
			item_data_size = REAL64_SIZE;
			break;
		default:
			break;
	}

	return item_data_size;
}

/**
 * \brief This function tries to handle received command layer_set_value
 */
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
	struct VSEntitySubscriber *layer_subscriber;
	int ret = 0;
	int item_data_size;

	uint32 node_id = UINT32(layer_set_value_cmd->data[0]);
	uint16 layer_id = UINT16(layer_set_value_cmd->data[UINT32_SIZE]);
	uint32 item_id = UINT32(layer_set_value_cmd->data[UINT32_SIZE + UINT16_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* User has to have permission to write to the node */
	if(vs_node_can_write(vsession, node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s(): user: %s can't write to the node: %d\n",
				__FUNCTION__,
				((struct VSUser *)(vsession->user))->username,
				node->id);
		goto end;
	}

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_id, node_id);
		goto end;
	}

	/* Check type of value */
	if( data_type != layer->data_type ) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() data type (%d) of layer (id: %d) in node (id: %d) does not match data type of received command (%d)\n",
				__FUNCTION__, layer->data_type, layer_id, node_id, data_type);
		goto end;
	}

	/* Check count of value */
	if( count != layer->num_vec_comp ) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() count of values (%d) of layer (id: %d) in node (id: %d) does not match count of values of received command (%d)\n",
				__FUNCTION__, layer->num_vec_comp, layer_id, node_id, count);
		goto end;
	}

	/* Set item value */

	item_data_size = vs_layer_data_size(layer);
	if(item_data_size > 0) {
		/* Try to find item value first */
		_item.id = item_id;
		vbucket = v_hash_array_find_item(&layer->values, &_item);
		if(vbucket == NULL) {
			/* When this item doesn't exist yet, then allocate memory for this item
			 * and add it to the hashed array */
			item = calloc(1, sizeof(struct VSLayerValue));

			if(item == NULL) {
				v_print_log(VRS_PRINT_ERROR, "Out of memory\n");
				goto end;
			}

			item->id = item_id;

			/* Allocate memory for values and copy data to this memory */
			item->value = (void*)calloc(layer->num_vec_comp, item_data_size);
			if(item->value != NULL) {
				v_hash_array_add_item(&layer->values, item, sizeof(struct VSLayerValue));
				memcpy(item->value,
						&layer_set_value_cmd->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE],
						layer->num_vec_comp * item_data_size);
			} else {
				free(item);
				item = NULL;
				v_print_log(VRS_PRINT_ERROR, "Out of memory\n");
				goto end;
			}
		} else {
			/* When data exist, then only change values */
			item = (struct VSLayerValue*)vbucket->data;
			memcpy(item->value,
					&layer_set_value_cmd->data[UINT32_SIZE + UINT16_SIZE + UINT32_SIZE],
					layer->num_vec_comp * item_data_size);
		}
	} else {
		v_print_log(VRS_PRINT_ERROR, "Unsupported data type: %d\n",
				layer->data_type);
		goto end;
	}

	vs_layer_inc_version(layer);

	ret = 1;

	/* Send command layer_set_value to all layer subscribers */
	layer_subscriber = layer->layer_subs.first;
	while(layer_subscriber != NULL) {
		if(vs_layer_send_set_value(layer_subscriber, node, layer, item) != 1) {
			ret = 0;
		}
		layer_subscriber = layer_subscriber->next;
	}

end:
	pthread_mutex_unlock(&node->mutex);

	return ret;
}

/**
 * \brief This function tries to unset value in the layer and all child layers
 *
 * This function is called for parent layer and it is called recursively to
 * unset values in all child layers
 *
 * \param[in]	*node	The pointer at node
 * \param[in]	*layer	The pointer at layer
 * \param[in]	item_id	The ID of item which should be unset (deleted)
 * \param[in]	send_command	If this argument is equal to 1, then unset_value
 * command is sent to all subscribers (should be equal to1 only for parent layer)
 *
 * \return This function returns 1, when it was able to unset value. Otherwise
 * it returns 0.
 */
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
		/* Remove value from hashed array */
		v_hash_array_remove_item(&layer->values, item);
		/* Free item */
		free(item);
	} else {
		return 0;
	}

	vs_layer_inc_version(layer);

	/* Try to unset values in all child values, but don't send unset_value command
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
	int ret = 0;

	uint32 node_id = UINT32(layer_unset_value_cmd->data[0]);
	uint16 layer_id = UINT16(layer_unset_value_cmd->data[UINT32_SIZE]);
	uint32 item_id = UINT32(layer_unset_value_cmd->data[UINT32_SIZE + UINT16_SIZE]);

	/* Try to find node */
	if((node = vs_node_find(vs_ctx, node_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s() node (id: %d) not found\n",
				__FUNCTION__, node_id);
		return 0;
	}

	pthread_mutex_lock(&node->mutex);

	/* User has to have permission to write to the node */
	if(vs_node_can_write(vsession, node) != 1) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s(): user: %s can't write to the node: %d\n",
				__FUNCTION__,
				((struct VSUser *)(vsession->user))->username,
				node->id);
		goto end;
	}

	/* Try to find layer */
	if( (layer = vs_layer_find(node, layer_id)) == NULL) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"%s() layer (id: %d) in node (id: %d) not found\n",
				__FUNCTION__, layer_id, node_id);
		goto end;
	}

	ret = vs_layer_unset_value(node, layer, item_id, 1);

end:
	pthread_mutex_unlock(&node->mutex);

	return ret;
}

