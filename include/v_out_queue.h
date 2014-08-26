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

#if !defined V_DATAQUEUE_H
#define V_DATAQUEUE_H

#include <pthread.h>

#include "v_list.h"
#include "v_commands.h"

#define INIT_QUEUE_LEN	1

/* Maximal number of key items in queue */
#define MAX_KEY_ITEMS		32

/* Maximal priority of command and queue */
#define MAX_PRIORITY		255

/* TODO: negotiate this value that set up priorities of queues */
#define REAL_PRIO_MUL		0.1
#define MAX_REAL_PRIO_VAL	(VRS_DEFAULT_PRIORITY*1000)
#define MIN_REAL_PRIO_VAL	1

#define OUT_QUEUE_ADD_TAIL	1
#define OUT_QUEUE_ADD_HEAD	2

/* Maximal size of outgoing queue is 1MB */
#define OUT_QUEUE_DEFAULT_MAX_SIZE 1048576

/**
 * Structure storing information about outgoing command waiting in the outgoing
 * queue
 */
typedef struct VOutQueueCommand {
	struct VOutQueueCommand	*prev, *next;	/**< To be able to add it to the linked list */
	struct VBucket			*vbucket;		/**< Own data of command stored in hashed linked list of commands */
	uint8					id;				/**< ID of command */
	uint8					prio;			/**< Current priority of the command */
	uint16					*counter;		/**< Pointer at number (allocated at heap) of following commands with same id */
	int8					*share;			/**< Pointer at size (allocated at heap) of address that could be shared between commands with same id */
	uint16					*len;			/**< Pointer at length of the compressed sequence of command with same ID */
} VOutQueueCommand;

/**
 * Queue for node commands of certain priority
 */
typedef struct VPrioOutQueue {
	struct VListBase	cmds;	/**< Linked list of commands */
	uint32				size;	/**< Size of stored commands (with this priority) in bytes */
	uint32				count;	/**< Count of stored commands (with this priority) */
	real32				r_prio;	/**< Real value of this priority queue */
} VPrioOutQueue;

/**
 * List of queues for incoming and outgoing commands. This is actually quueue
 * for all commands divided to the subqueues for all possible priorities of
 * nodes.
 */
typedef struct VOutQueue {
	pthread_mutex_t			lock;			/**< Mutex for locking queue (synchronization between threads) */
	struct VPrioOutQueue	*queues[MAX_PRIORITY+1];
	struct VCommandQueue	*cmds[MAX_CMD_ID+1];
	uint32					size;			/**< Size of stored commands in bytes */
	uint32					max_size;		/**< Maximal allowed size of commands stored in this queue */
	uint32					count;			/**< Count of stored commands */
	uint8					max_prio;		/**< Maximal used priority queue */
	uint8					min_prio;		/**< Minimal used priority queue */
	real32					r_prio_sum_high;/**< Summary of all real priorities <MAX_PRIO, DEFAULT_PRIO> */
	real32					r_prio_sum_low;	/**< Summary of all real priorities <DEFAULT_PRIO-1, MIN_PRIO> */
} VOutQueue;

int	v_out_queue_init(struct VOutQueue *out_queue, int max_size);
struct VOutQueue *v_out_queue_create(void);
void v_out_queue_destroy(struct VOutQueue **out_queue);

int v_out_queue_push_head(struct VOutQueue *out_queue, uint8 prio, struct Generic_Cmd *cmd);
int v_out_queue_push_tail(struct VOutQueue *out_queue, uint8 prio, struct Generic_Cmd *cmd);
struct Generic_Cmd * v_out_queue_pop(struct VOutQueue *out_queue, uint8 prio, uint16 *count, int8 *share, uint16 *len);
struct Generic_Cmd *v_out_queue_find_cmd(struct VOutQueue *out_queue, struct Generic_Cmd *cmd);

uint32 v_out_queue_get_count_prio(struct VOutQueue *out_queue, uint8 prio);
uint32 v_out_queue_get_size_prio(struct VOutQueue *out_queue, uint8 prio);
uint32 v_out_queue_get_count(struct VOutQueue *out_queue);
uint32 v_out_queue_get_size(struct VOutQueue *out_queue);
uint8 v_out_queue_get_max_prio(struct VOutQueue *out_queue);
uint8 v_out_queue_get_min_prio(struct VOutQueue *out_queue);
real32 v_out_queue_get_prio(struct VOutQueue *out_queue, uint8 prio);
real32 v_out_queue_get_prio_sum_high(struct VOutQueue *out_queue);
real32 v_out_queue_get_prio_sum_low(struct VOutQueue *out_queue);

#endif

