/*
 * $Id: v_in_queue.h 1268 2012-07-24 08:14:52Z jiri $
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2011, Jiri Hnidek
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

#ifndef V_IN_QUEUE_H_
#define V_IN_QUEUE_H_

#include <pthread.h>

#include "verse_types.h"

#include "v_commands.h"
#include "v_list.h"

/**
 * Structure storing information about incoming command waiting in the incoming
 * queue
 */
typedef struct VInQueueCommand {
	struct VOutQueueCommand	*prev, *next;	/**< To be able to add it to the linked list */
	struct VBucket			*vbucket;		/**< Own data of command stored in hashed linked list of commands */
	uint8					id;				/**< ID of command */
} VInQueueCommand;

/**
 * This structure is used for storing incoming data
 */
typedef struct VInQueue {
	pthread_mutex_t			lock;	/**< Mutex for locking queue (synchronization between threads) */
	struct VCommandQueue	*cmds[MAX_CMD_ID+1];
	struct VListBase		queue;	/**< Linked list of commands */
	uint32					size;	/**< Size of stored commands in bytes */
	uint32					count;	/**< Count of stored commands */
} VInQueue;

uint32 v_in_queue_size(struct VInQueue *in_queue);
uint32 v_in_queue_cmd_count(struct VInQueue *in_queue);
struct Generic_Cmd *v_in_queue_pop(struct VInQueue *in_queue);
int v_in_queue_push(struct VInQueue *in_queue, struct Generic_Cmd *cmd);
int v_in_queue_init(struct VInQueue *in_queue);
struct VInQueue *v_in_queue_create(void);
void v_in_queue_destroy(struct VInQueue **in_queue);

#endif /* V_IN_QUEUE_H_ */
