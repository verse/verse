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

#ifndef VS_LAYER_H_
#define VS_LAYER_H_

#include "verse_types.h"

#include "v_list.h"

#include "vs_node.h"

#define FIRST_LAYER_ID				0
#define LAST_LAYER_ID				65534	/* 2^16 - 2 */

#define RESERVED_LAYER_ID			0xFFFF	/* This ID sends client in LayerCreate command */

#define MAX_LAYERS_COUNT			65534	/* Max number of layer that could be stored inside one node */

/**
 * \brief The structure storing own value(s)
 */
typedef struct VSLayerValue {
	uint32	id;
	void	*value;
} VSLayerValue;

/**
 * \brief The structure storing information about layer
 */
typedef struct VSLayer {
	struct VSLayer			*prev, *next;	/**< Pointers at layer at linked list */
	uint16					id;				/**< The ID of this layer */
	uint8					data_type;		/**< The type of values stored in this layer */
	uint8					num_vec_comp;	/**< The number of vector components (1, 2, 3, 4) */
	uint16					custom_type;	/**< The type of layer defined by client */
	struct VHashArrayBase	values;			/**< The hashed linked list of values */
	/* Parent-Child */
	struct VSLayer			*parent;		/**< The parent layer */
	struct VListBase		child_layers;	/**< The list of child layers */
	/* Subscribing */
	struct VListBase		layer_folls;	/**< The list of clients that know about this layer */
	struct VListBase		layer_subs;		/**< The list of clients that are subscribed to this layer */
	uint8					state;			/**< The layer state */
	/* Versing */
	uint32					version;		/**< Current version of layer */
	uint32					saved_version;	/**< last saved version of layer */
	uint32					crc32;			/**< CRC32 of current layer version */
#ifdef WITH_MONGODB
	bson_oid_t				oid;
#endif
} VSLayer;

void vs_layer_inc_version(struct VSLayer *layer);

int vs_layer_data_size(struct VSLayer *layer);

VSLayer *vs_layer_find(struct VSNode *node, uint16 layer_id);

int vs_layer_send_create(struct VSNodeSubscriber *node_subscriber,
		struct VSNode *node,
		struct VSLayer *layer);

int vs_layer_send_destroy(struct VSNode *node,
		struct VSLayer *layer);

int vs_layer_unsubscribe(struct VSLayer *layer,
		struct VSession *vsession);

int vs_handle_layer_create(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_create_cmd);

int vs_handle_layer_create_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_create_cmd_ack);

int vs_handle_layer_destroy(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_destroy_cmd);

int vs_handle_layer_destroy_ack(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_destroy_cmd);

int vs_handle_layer_subscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_subscribe_cmd);

int vs_handle_layer_unsubscribe(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_unsubscribe_cmd);

int vs_handle_layer_set_value(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_set_value_cmd,
		uint8 data_type,
		uint8 count);

int vs_handle_layer_unset_value(struct VS_CTX *vs_ctx,
		struct VSession *vsession,
		struct Generic_Cmd *layer_unset_value_cmd);

struct VSLayer *vs_layer_create(struct VSNode *node,
		struct VSLayer *parent,
		uint8 data_type,
		uint8 count,
		uint16 type);

void vs_layer_destroy(struct VSNode *node,
		struct VSLayer *layer);

int vs_node_layers_destroy(struct VSNode *node);

#endif /* VS_LAYER_H_ */
