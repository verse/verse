/*
 * $Id: v_history.c 1288 2012-08-05 20:34:43Z jiri $
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2010, Jiri Hnidek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END BSD LICENSE BLOCK *****
 *
 * Authors: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <assert.h>

#include "vs_main.h"

#include "v_history.h"
#include "v_sys_commands.h"
#include "v_node_commands.h"
#include "v_common.h"
#include "v_list.h"
#include "v_cmd_queue.h"
#include "v_out_queue.h"
#include "v_connection.h"
#include "v_in_queue.h"
#include "v_session.h"
#include "v_commands.h"
#include "v_fake_commands.h"

/**
 * \brief		This function prints history of sent packets
 * \param[in]	*history	The history of sent packets.
 */
void v_print_packet_history(struct VPacket_History *history)
{
	struct VSent_Packet *packet;
	struct VBucket *vbucket;
	int cmd_id;

	v_print_log(VRS_PRINT_DEBUG_MSG, "Packet history:\n\t");
	packet = history->packets.first;
	while(packet!=NULL) {
		v_print_log_simple(VRS_PRINT_DEBUG_MSG, "%d, ", packet->id);
		packet = packet->next;
	}
	v_print_log_simple(VRS_PRINT_DEBUG_MSG, "\n");

	v_print_log(VRS_PRINT_DEBUG_MSG, "Command history:\n");
	for(cmd_id=0; cmd_id<=MAX_CMD_ID; cmd_id++) {
		if(history->cmd_hist[cmd_id] != NULL) {
			vbucket = history->cmd_hist[cmd_id]->cmds.lb.first;
			while(vbucket != NULL) {
				v_cmd_print(VRS_PRINT_DEBUG_MSG, (struct Generic_Cmd*)vbucket->data);
				vbucket = vbucket->next;
			}
		}
	}
}

/**
 * \brief This function destroy history of sent packets. It free linked list of
 * sent packets and all hashed linked lists of commands.
 * \param[in]	*history	The history of sent packets.
 */
void v_packet_history_destroy(struct VPacket_History *history)
{
	struct VSent_Packet *sent_packet;
	int cmd_id;

	/* Free all pointer at commands in all sent packet */
	sent_packet = history->packets.first;
	while(sent_packet != NULL) {
		v_list_free(&sent_packet->cmds);
		sent_packet = sent_packet->next;
	}

	/* Free linked list of sent packets */
	v_list_free(&history->packets);

	/* Free commands in hashed linked lists */
	for(cmd_id=0; cmd_id<=MAX_CMD_ID; cmd_id++) {
		if(history->cmd_hist[cmd_id] != NULL) {
			v_cmd_queue_destroy(history->cmd_hist[cmd_id]);
			free(history->cmd_hist[cmd_id]);
			history->cmd_hist[cmd_id] = NULL;
		}
	}
}

/**
 * \brief This function initialize history of sent packets. It means, that
 * linked list of sent packet is initialized and hashed linked lists of
 * supported commands are initialized.
 * \param[in]	*history	The history of sent packets.
 */
void v_packet_history_init(struct VPacket_History *history)
{
	int cmd_id;

	history->packets.first = NULL;
	history->packets.last = NULL;

	for(cmd_id=0; cmd_id<=MAX_CMD_ID; cmd_id++) {
		history->cmd_hist[cmd_id] = v_cmd_queue_create(cmd_id, 0, 0);
	}
}

/**
 * \brief Find packet with id in the linked list of sent packet. When such
 * packet is not found, then NULL is returned.
 * \param[in]	*history	The history of sent packets.
 * \param[in]	id			The ID of packet.
 * \return		This function returns pointer at structure VSent_Packet.
 */
struct VSent_Packet *v_packet_history_find_packet(struct VPacket_History *history,
		uint32 id)
{
	struct VSent_Packet *packet = NULL;

	packet = history->packets.first;

	while(packet != NULL) {
		if(packet->id == id) break;
		packet = packet->next;
	}

	return packet;
}

/**
 * \brief		This functions add empty packet to the history of the send packets.
 * \param[in]	*history	The structure with history of all sent packets and
 * commands.
 * \param[in]	id			The ID of packet, that will be add to the history of sent packets.
 * \return		This function returns pointer at structure VSent_Packet.
 */
struct VSent_Packet *v_packet_history_add_packet(struct VPacket_History *history,
		uint32 id)
{
	struct VSent_Packet *packet = NULL;

	packet = (struct VSent_Packet*)malloc(sizeof(struct VSent_Packet));

	/* Check if memory for sent packet was allocated */
	if(packet != NULL) {
		packet->cmds.first = NULL;
		packet->cmds.last = NULL;

		packet->id = id;

		v_print_log(VRS_PRINT_DEBUG_MSG, "Adding packet: %d to history\n", packet->id);

		v_list_add_tail(&history->packets, packet);
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "Unable to allocate enough memory for sent packet: %d\n", id);
	}

	return packet;
}

/**
 * \brief This function add command to the history of sent command
 *
 * When this new added command updates some recent command with
 * the same address, then this command is set as obsolete. Pointer at this
 * command is set to NULL in previous packet.
 *
 * \param[in]	*history		The structure containing history of sent packets
 * and commands
 * \param[in]	*sent_packet	The current packet
 * \param[in]	*cmd			The data of command
 *
 * \return This function returns 1, when command was added to history.
 * When it wasn't able to add command to history, then zero is returned.
 */
int v_packet_history_add_cmd(struct VPacket_History *history,
		struct VSent_Packet *sent_packet,
		struct Generic_Cmd *cmd,
		uint8 prio)
{
	struct VSent_Command *sent_cmd;
	struct VBucket *vbucket;
	void *_cmd = (void*)cmd;
	uint8 cmd_id = cmd->id;
	uint16 cmd_size = v_cmd_struct_size(cmd);
	int ret = 0;

	/* Are duplications allowed for this type of commands? */
	if(history->cmd_hist[cmd_id]->flag & REMOVE_HASH_DUPS) {
		/* Try to find command with the same address */
		vbucket = v_hash_array_find_item(&history->cmd_hist[cmd_id]->cmds, _cmd);
		if(vbucket != NULL) {
			struct Generic_Cmd *obsolete_cmd = (struct Generic_Cmd*)vbucket->data;

			/* Bucket has to include not NULL pointer */
			assert(vbucket->ptr!=NULL);
			assert(vbucket->data!=NULL);

			/* Debug print */
#if 0
			v_print_log(VRS_PRINT_INFO, "Replacing obsolete command\n");
			v_cmd_print(VRS_PRINT_INFO, obsolete_cmd);
			v_cmd_print(VRS_PRINT_INFO, cmd);
#endif

			/* When old data are obsolete, then set pointer at command in old
			 * command to the NULL (obsolete command would not be re-send) */
			((struct VSent_Command*)(vbucket->ptr))->vbucket = NULL;

			/* Remove data with the same key as cmd_data has from hashed linked
			 * list */
			ret = v_hash_array_remove_item(&history->cmd_hist[cmd_id]->cmds, vbucket->data);

			if(ret == 1) {
				/* Destroy original command */
				v_cmd_destroy(&obsolete_cmd);
			} else {
				v_print_log(VRS_PRINT_DEBUG_MSG, "Could not remove obsolete command (id: %d) from history\n", cmd_id);
				ret = 0;
			}
		}
	}

	/* Add own command data to the hashed linked list */
	vbucket = v_hash_array_add_item(&history->cmd_hist[cmd_id]->cmds, _cmd, cmd_size);

	if(vbucket != NULL) {
		/* Create new command */
		sent_cmd = (struct VSent_Command*)malloc(sizeof(struct VSent_Command));
		/* Check if it was possible to allocate enough memory for sent command */
		if(sent_cmd != NULL) {
			sent_cmd->id = cmd_id;
			/* Add command to the linked list of sent packet */
			v_list_add_tail(&sent_packet->cmds, sent_cmd);
			/* Set up pointer at command data */
			sent_cmd->vbucket = vbucket;
			/* Set up pointer at owner of this command item to be able to obsolete
			 * this command in future */
			vbucket->ptr = (void*)sent_cmd;
			/* Store information about command priority. Lost commands should
			 * be re-send with same priority*/
			sent_cmd->prio = prio;

			ret = 1;
		} else {
			/* When memory wasn't allocated, then free bucket from hashed
			 * linked list */
			v_print_log(VRS_PRINT_ERROR, "Unable allocate enough memory for sent command\n");
			ret = v_hash_array_remove_item(&history->cmd_hist[cmd_id]->cmds, _cmd);
			if(ret==1) {
				v_cmd_destroy(_cmd);
			}
			ret = 0;
		}
	} else {
		v_print_log(VRS_PRINT_ERROR, "Unable to add command (id: %d) to packet history\n", cmd_id);
		ret = 0;
	}

	return ret;
}

/**
 * \brief This function removes packet with id from history of sent packets.
 * It removes all its commands from the command history too.
 *
 * \param[in]	*C		The verse context.
 * \param[in]	id		The ID of packet, the will be removed from the history.
 *
 * \return		This function returns 1, then packet with id was found in the history
 * and it returns 0 otherwise.
 */
int v_packet_history_rem_packet(struct vContext *C, uint32 id)
{
	struct VS_CTX *vs_ctx = CTX_server_ctx(C);
	struct VDgramConn *dgram_conn = CTX_current_dgram_conn(C);
	struct VPacket_History *history = &dgram_conn->packet_history;
	struct VPacket *r_packet = CTX_r_packet(C);
	struct VSent_Packet *sent_packet;
	int ret = 0, is_fake_cmd_received = 0;

	sent_packet = v_packet_history_find_packet(history, id);

	if(sent_packet != NULL) {
		struct VSent_Command *sent_cmd;

		v_print_log(VRS_PRINT_DEBUG_MSG, "Removing packet: %d from history\n", sent_packet->id);

		/* Go through the whole list of sent commands and free them from the
		 * hashed linked list */
		sent_cmd = sent_packet->cmds.first;
		while(sent_cmd != NULL) {
			/* Remove own command from hashed linked list if it wasn't already
			 * removed, when command was obsoleted by some newer packet */
			if(sent_cmd->vbucket!=NULL) {
				struct Generic_Cmd *cmd = (struct Generic_Cmd*)sent_cmd->vbucket->data;

				/* Bucket has to include some data */
				assert(sent_cmd->vbucket->data!=NULL);

				/* Decrease total size of commands that were sent and wasn't acknowladged yet*/
				dgram_conn->sent_size -= v_cmd_size(cmd);

				/* Put fake command for create/destroy commands at verse server */
				if(vs_ctx != NULL) {
					struct VSession *vsession = CTX_current_session(C);
					struct Generic_Cmd *fake_cmd = NULL;

					switch(cmd->id) {
					case CMD_NODE_CREATE:
						fake_cmd = v_fake_node_create_ack_create(UINT32(cmd->data[UINT16_SIZE + UINT32_SIZE]));
						break;
					case CMD_NODE_DESTROY:
						fake_cmd = v_fake_node_destroy_ack_create(UINT32(cmd->data[0]));
						break;
					case CMD_TAGGROUP_CREATE:
						fake_cmd = v_fake_taggroup_create_ack_create(UINT32(cmd->data[0]),
								UINT16(cmd->data[UINT32_SIZE]));
						break;
					case CMD_TAGGROUP_DESTROY:
						fake_cmd = v_fake_taggroup_destroy_ack_create(UINT32(cmd->data[0]),
								UINT16(cmd->data[UINT32_SIZE]));
						break;
					case CMD_TAG_CREATE:
						fake_cmd = v_tag_create_ack_create(UINT32(cmd->data[0]),
								UINT16(cmd->data[UINT32_SIZE]),
								UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]));
						break;
					case CMD_TAG_DESTROY:
						fake_cmd = v_tag_destroy_ack_create(UINT32(cmd->data[0]),
								UINT16(cmd->data[UINT32_SIZE]),
								UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]));
						break;
					case CMD_LAYER_CREATE:
						fake_cmd = v_fake_layer_create_ack_create(UINT32(cmd->data[0]),
								UINT16(cmd->data[UINT32_SIZE + UINT16_SIZE]));
						break;
					case CMD_LAYER_DESTROY:
						fake_cmd = v_fake_layer_destroy_ack_create(UINT32(cmd->data[0]),
								UINT16(cmd->data[UINT32_SIZE]));
						break;
					default:
						break;
					}

					if(fake_cmd != NULL) {
						is_fake_cmd_received = 1;
						/* Push command to the queue of incomming commands */
						v_in_queue_push(vsession->in_queue, fake_cmd);
						/* Print content of fake command */
						v_fake_cmd_print(VRS_PRINT_DEBUG_MSG, fake_cmd);
					}
				}

				/* Remove command from the history of sent commands */
				ret = v_hash_array_remove_item(&history->cmd_hist[sent_cmd->id]->cmds, sent_cmd->vbucket->data);

				if(ret == 1) {
					/* Destroy command */
					v_cmd_destroy(&cmd);
				} else {
					v_print_log(VRS_PRINT_ERROR, "Unable to remove command id: %d\n", sent_cmd->id);
					ret = 0;
				}
			}
			sent_cmd = sent_cmd->next;
		}

		/* Free linked list of sent commands */
		v_list_free(&sent_packet->cmds);

		/* Remove packet itself from the linked list of sent packet */
		v_list_rem_item(&history->packets, sent_packet);
		free(sent_packet);

		ret = 1;
	} else {
		/* When acknowledged payload packet is not found in history, then it
		 * is OK, because it is probably keep alive packet without any node
		 * commands */
		v_print_log(VRS_PRINT_DEBUG_MSG, "Packet with id: %d, not found in history\n", id);
	}

	/* When pure ack packet caused adding some fake commands to the queue, then
	 * poke server data thread */
	if( (vs_ctx != NULL) && (is_fake_cmd_received == 1) && (r_packet->data == NULL)) {
		sem_post(&vs_ctx->data.sem);
	}

	return ret;
}

/**
 * \brief This function print all commands in history of ACK and NAK commands.
 * \param[in]	*history	The structure storing history of ACK and NAK
 * commands.
 */
void v_ack_nak_history_print(struct AckNakHistory *history)
{
	int i;

	printf("Ack Nak History: count: %d, len: %d\n", history->count, history->len);
	for(i=0; i<history->count; i++) {
		v_print_ack_nak_cmd(VRS_PRINT_DEBUG_MSG, &history->cmds[i]);
	}

}

/**
 * Example of list of ACK and NAK command in history, that is compressed
 *
 *      +-----+-----+-----+-----+-----+-----+
 *      | ACK | NAK | ACK | NAK | ACK | ACK |
 *      +-----+-----+-----+-----+-----+-----+
 *      | 107 | 115 | 146 | 157 | 179 | 183 |
 *      +-----+-----+-----+-----+-----+-----+
 *
 * ack: +-----+     +-----+     +-----------+
 * id:  |107  |115  |146  |157  |179   183  |
 * nak: +     +-----+     +-----+           +
 *
 */

/**
 * \brief This function removes ACK and NAK commands from the history of
 * commands. ANK ID is used as rule for removing. It means, that all commands
 * with PAY ID less or equal to the ANK ID will be removed from the history.
 * \param[in]	*history	The structure storing history of ack and nak
 * commands
 * \param[in]	ank_id		The ANK_ID is the ID of last acknowledged Payload ID.
 * \returns 	This function returns 1, when array was successfully reduced
 * and it returns 0, when reducing was not possible, because function was
 * called with some strange values. */
int v_ack_nak_history_remove_cmds(struct AckNakHistory *history, unsigned int ank_id)
{
	int i,j;

	/* If history is already empty, then it is not neccessary to remove any
	 * command */
	if( history->count == 0) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "History of ACK and NAK messages is empty, ANK_ID: %d dropped\n", ank_id);
		return 0;
	}

	/* ANK ID could not be bigger then last acknowledged Payload ID, because
	 * it was not sent yet */
	if( ank_id > history->cmds[history->count-1].pay_id ) {
		v_print_log(VRS_PRINT_WARNING, "ANK_ID: %d is bigger then last ACK(PAY_ID): %d\n",
				ank_id, history->cmds[history->count-1].pay_id);
		return 0;
	}

	/* Find place, where we cut off array of ACK NAK history */
	for(i=0; i<history->count; i++) {
		if(history->cmds[i].pay_id >= ank_id)
			break;
	}

	/* Is ACK command directly on the index? */
	if(history->cmds[i].pay_id == ank_id) {
		/* Will it be cut somewhere in the middle of the list? */
		if((i+1) < history->count) {
			/* Could be first ACK command from subsequence removed? */
			if(history->cmds[i+1].pay_id==ank_id+1) {
				/* Move ACK and NAK commands to the beginning of the array */
				for(j=0, i=i+1; i<history->count; j++, i++) {
					history->cmds[j].id = history->cmds[i].id;
					history->cmds[j].pay_id = history->cmds[i].pay_id;
				}
				history->count = j;
			} else {
				/* Update first item in list of ACK and NAK commands */
				history->cmds[0].id = history->cmds[i].id;
				history->cmds[0].pay_id = ank_id+1;
				/* Move ACK and NAK commands to the beginning of the array */
				for(j=1, i=i+1; i<history->count; j++, i++) {
					history->cmds[j].id = history->cmds[i].id;
					history->cmds[j].pay_id = history->cmds[i].pay_id;
				}
				history->count = j;
			}
		} else {
			/* If last PAY ID of ANK NAK history is equal to the ANK ID,
			 * then consider history as empty. */
			history->count = 0;
		}
	} else if(history->cmds[i].pay_id > ank_id) {
		if(i>0) {
			history->cmds[0].id = history->cmds[i-1].id;
			history->cmds[0].pay_id = ank_id;
			/* Move ACK and NAK commands to the beginning of the array */
			for(j=1; i<history->count; j++, i++) {
				history->cmds[j].id = history->cmds[i].id;
				history->cmds[j].pay_id = history->cmds[i].pay_id;
			}
			history->count = j;
		} else {
			/* This should not happen. */
			return 0;
		}
	}

	return 1;
}

/**
 * \brief This function adds ACK or NAK command to the end of the list of
 * commands and implement compression of commands.
 * \param[in]	*history	The structure storing history of ACK and NAK
 * commands
 * \param[in]	*cmd		The structure sroting ACK or NAK command
 * \returns		This function returns 1, when command was successfully added
 * at the end of list of command and it returns 0, when some error occurs. */
int v_ack_nak_history_add_cmd(struct AckNakHistory *history, struct Ack_Nak_Cmd *cmd)
{
	/* If there is not enough space for new item, them realloc array */
	if(history->count+1 > history->len) {
		history->len *= 2;
		if( (history->cmds = (struct Ack_Nak_Cmd*)realloc(history->cmds, sizeof(struct Ack_Nak_Cmd)*history->len)) == NULL) {
			history->len = 0;
			history->count = 0;
			return 0;
		}
	}

	/* When array is empty, then simply add command at the first position of the list */
	if(history->count==0) {
		history->cmds[0].id = cmd->id;
		history->cmds[0].pay_id = cmd->pay_id;
		history->count++;
	} else {
		if(cmd->id == CMD_ACK_ID) {
			/* When we want to add ACK command at the end of the of the list, then we have to check several things */
			if(history->cmds[history->count-1].id == CMD_ACK_ID) {
				/* Add last ACK command in the last sequence of ACK commands */
				if(history->count==1 ||
						(history->count>1 && history->cmds[history->count-2].id == CMD_NAK_ID)) {
					history->cmds[history->count].id = cmd->id;
					history->cmds[history->count].pay_id = cmd->pay_id;
					history->count++;
				/* Only update last ACK command in the last sequence of ACK commands */
				} else {
					history->cmds[history->count-1].id = cmd->id;
					history->cmds[history->count-1].pay_id = cmd->pay_id;
				}
			/* When last command in array is NAK command, then we have to add this ACK command */
			} else if(history->cmds[history->count-1].id == CMD_NAK_ID) {
				history->cmds[history->count].id = cmd->id;
				history->cmds[history->count].pay_id = cmd->pay_id;
				history->count++;
			}
		/* Adding NAK command to the array */
		} else if(cmd->id == CMD_NAK_ID) {
			/* When last command in the array is ACK command, then we have to add this command to the array */
			if(history->cmds[history->count-1].id == CMD_ACK_ID) {
				/* When, there is trailing sequence of two ACK command, then rewrite last ACK command*/
				if(history->count>1 && history->cmds[history->count-2].id == CMD_ACK_ID) {
					history->cmds[history->count-1].id = cmd->id;
					history->cmds[history->count-1].pay_id = cmd->pay_id;
				} else {
					history->cmds[history->count].id = cmd->id;
					history->cmds[history->count].pay_id = cmd->pay_id;
					history->count++;
				}
			}
		}
	}

	return 1;
}

/* Initialize history of ACK and NAK commands */
int v_ack_nak_history_init(struct AckNakHistory *history)
{
	if(history->cmds == NULL) {
		if( (history->cmds = (struct Ack_Nak_Cmd*)malloc(sizeof(struct Ack_Nak_Cmd)*INIT_ACK_NAK_HISTORY_SIZE)) != NULL) {
			history->count = 0;
			history->len = INIT_ACK_NAK_HISTORY_SIZE;
		} else {
			history->count = 0;
			history->len = 0;
			return 0;
		}
	}
	return 1;
}

/* Free allocated space for history of ack nak commands */
void v_ack_nak_history_clear(struct AckNakHistory *history)
{
	if(history->cmds != NULL) {
		free(history->cmds);
		history->cmds = NULL;
		history->count = 0;
		history->len = 0;
	}
}
