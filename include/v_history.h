/*
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

#if !defined V_HISTORY_H
#define V_HISTORY_H

#include "verse_types.h"

#include "v_list.h"
#include "v_out_queue.h"
#include "v_context.h"

#define INIT_ACK_NAK_HISTORY_SIZE	2

/**
 * This structure does not contain own command, but it only contain pointer
 * at command in the queue.
 */
typedef struct VSent_Command {
	struct VSent_Command	*prev, *next;	/* Item of linked list */
	struct VBucket			*vbucket;		/* Pointer at item of command data in bucket list */
	uint8					id;				/* ID of command */
	uint8					prio;			/* The priority that was used for sending this command */
} VSent_Command;

/**
 * Structure of sent packet. This structure contains the linked list of
 * pointers at commands. Commands are stored in queue. If any command with
 * the same address is sent to the peer in packet with higher packet ID,
 * then content of current command is obsolete and pointer at command is set
 * as NULL. Then it would be necessary to re-send only not NULL commands.
 */
typedef struct VSent_Packet {
	struct VSent_Packet		*prev, *next;	/* Item of linked list */
	uint32					id;				/* ID of packet */
	struct VListBase		cmds;			/* Linked list of pointers at commands */
	struct timeval			tv;				/* The time, when packet was sent */
} VSent_Packet;

/**
 * Structure containing history of sent packets. The packets contains linked
 * list of sent commands. The command lists contains sent hashed commands.
 */
typedef struct VPacket_History {
	/* Linked list of sent packets */
	struct VListBase		packets;
	/* Own sent commands are stored in separated structure. Each type of command
	 * has own slots */
	struct VCommandQueue	*cmd_hist[MAX_CMD_ID+1];
} VPacket_History;

/**
 *  Array of compressed ACK and NAK commands, that are sent in ACK packet
 */
typedef struct AckNakHistory {
	struct Ack_Nak_Cmd		*cmds;		/* Compressed array of ACK and NAK commands */
	int						len;		/* Length of array */
	int						count;		/* Count of items in the array */
} AckNakHistory;

void v_print_packet_history(struct VPacket_History *history);
void v_packet_history_destroy(struct VPacket_History *history);
void v_packet_history_init(struct VPacket_History *history);
int v_packet_history_add_cmd(struct VPacket_History *history,
		struct VSent_Packet *sent_packet,
		struct Generic_Cmd *cmd,
		uint8 prio);
struct VSent_Packet *v_packet_history_find_packet(struct VPacket_History *history,
		uint32 id);
struct VSent_Packet *v_packet_history_add_packet(struct VPacket_History *history,
		uint32 id);
int v_packet_history_rem_packet(struct vContext *C, uint32 id);

void v_ack_nak_history_print(struct AckNakHistory *ack_nak_history);
int v_ack_nak_history_remove_cmds(struct AckNakHistory *history,
		unsigned int ank_id);
int v_ack_nak_history_add_cmd(struct AckNakHistory *ack_nak_history,
		struct Ack_Nak_Cmd *cmd);
int v_ack_nak_history_init(struct AckNakHistory *ack_nak_history);
void v_ack_nak_history_clear(struct AckNakHistory *history);

#endif
