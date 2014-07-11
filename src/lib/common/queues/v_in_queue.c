/*
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "v_in_queue.h"
#include "v_cmd_queue.h"
#include "v_common.h"

/**
 * \brief This function pop command from the queue for incoming commands
 */
struct Generic_Cmd *v_in_queue_pop(struct VInQueue *in_queue)
{
	struct Generic_Cmd *cmd=NULL;
	struct VInQueueCommand *queue_cmd;

	pthread_mutex_lock(&in_queue->lock);

	queue_cmd = in_queue->queue.first;

	if(queue_cmd != NULL) {
		cmd = (struct Generic_Cmd *)queue_cmd->vbucket->data;

		/* There should be some data */
		assert(cmd != NULL);

		/* Remove command from hashed linked list */
		v_hash_array_remove_item(&in_queue->cmds[cmd->id]->cmds, (void*)cmd);

		/* Remove command from queue */
		v_list_free_item(&in_queue->queue, queue_cmd);

		/* Update total count and size of commands */
		in_queue->count--;
		in_queue->size -= in_queue->cmds[cmd->id]->item_size;
	}

	pthread_mutex_unlock(&in_queue->lock);

	return cmd;
}

/**
 * \brief This function push incoming command to the tail of queue for
 * incoming commands
 */
int v_in_queue_push(struct VInQueue *in_queue, struct Generic_Cmd *cmd)
{
	struct VBucket *vbucket = NULL;
	int ret = 1;

	/* Lock mutex */
	pthread_mutex_lock(&in_queue->lock);

	/* Command queue has to exist for this type of command */
	assert(in_queue->cmds[cmd->id] != NULL);

	/* Try to find command with the same address, when duplicities are not
	 * allowed in command queue */
	if( (in_queue->cmds[cmd->id]->flag & REMOVE_HASH_DUPS) &&
		( vbucket = v_hash_array_find_item(&in_queue->cmds[cmd->id]->cmds, (void*)cmd) ) != NULL )
	{
		/* Bucket has to include not NULL pointer */
		assert(vbucket->ptr!=NULL);

		/* Debug print */
#if 0
		v_print_log(VRS_PRINT_DEBUG_MSG, "Replacing obsolete command with new\n");
		v_cmd_print(VRS_PRINT_DEBUG_MSG, (struct Generic_Cmd*)vbucket->data);
		v_cmd_print(VRS_PRINT_DEBUG_MSG, cmd);
#endif

		/* Destroy original command */
		v_cmd_destroy((struct Generic_Cmd**)&vbucket->data);

		/* Replace current command with new data */
		vbucket->data = (void*)cmd;
	} else {
		/* Create new command in queue */
		struct VInQueueCommand *queue_cmd = (struct VInQueueCommand*)calloc(1, sizeof(struct VInQueueCommand));

		/* Add new command data */
		queue_cmd->vbucket = v_hash_array_add_item(&in_queue->cmds[cmd->id]->cmds, cmd, in_queue->cmds[cmd->id]->item_size);
		queue_cmd->vbucket->ptr = (void*)queue_cmd;

		/* Update count and size of queue */
		in_queue->count++;
		in_queue->size += in_queue->cmds[cmd->id]->item_size;

		/* Add own command to the tail of the queue */
		v_list_add_tail(&in_queue->queue, queue_cmd);
	}
	/* Unlock mutex */
	pthread_mutex_unlock(&in_queue->lock);

	return ret;
}

/**
 * \brief This function initialize queue for incoming commands
 */
int v_in_queue_init(struct VInQueue *in_queue, int max_size)
{
	int id, res;

	/* Initialize mutex of this queue */
	if((res = pthread_mutex_init(&in_queue->lock, NULL)) != 0) {
		/* This function always return 0 on Linux */
		if(is_log_level(VRS_PRINT_ERROR)) {
			v_print_log(VRS_PRINT_ERROR, "pthread_mutex_init(): %d\n", res);
		}
		return 0;
	}

	in_queue->count = 0;
	in_queue->size = 0;

	in_queue->max_size = max_size;

	in_queue->queue.first = NULL;
	in_queue->queue.last = NULL;

	for(id=0; id<=MAX_CMD_ID; id++) {
		in_queue->cmds[id] = v_cmd_queue_create(id, 0, 1);
	}

	return 1;
}

/**
 * \brief This function creates new queue for incoming commnds
 */
struct VInQueue *v_in_queue_create(void)
{
	struct VInQueue *in_queue = (struct VInQueue *)malloc(sizeof(struct VInQueue));

	if(in_queue!=NULL) {
		if( v_in_queue_init(in_queue, IN_QUEUE_DEFAULT_MAX_SIZE) != 1) {
			free(in_queue);
			in_queue = NULL;
		}
	}

	return in_queue;
}

/**
 * \brief This function destroy queue for incoming commands
 */
void v_in_queue_destroy(struct VInQueue **in_queue)
{
	int id;

	pthread_mutex_lock(&(*in_queue)->lock);

	(*in_queue)->count = 0;
	(*in_queue)->size = 0;

	v_list_free(&(*in_queue)->queue);

	for(id=0; id<=MAX_CMD_ID; id++) {
		if((*in_queue)->cmds[id] != NULL) {
			v_cmd_queue_destroy((*in_queue)->cmds[id]);
			free((*in_queue)->cmds[id]);
			(*in_queue)->cmds[id] = NULL;
		}
	}

	pthread_mutex_unlock(&(*in_queue)->lock);

	pthread_mutex_destroy(&(*in_queue)->lock);

	free(*in_queue);
	*in_queue = NULL;
}

/**
 * \brief This function return size of commands in queue
 */
uint32 v_in_queue_size(struct VInQueue *in_queue)
{
	uint32 size;

	pthread_mutex_lock(&in_queue->lock);
	size = in_queue->size;
	pthread_mutex_unlock(&in_queue->lock);

	return size;
}

/**
 * \brief This function returns number of commands in queue
 */
uint32 v_in_queue_cmd_count(struct VInQueue *in_queue)
{
	uint32 count;

	pthread_mutex_lock(&in_queue->lock);
	count = in_queue->count;
	pthread_mutex_unlock(&in_queue->lock);

	return count;
}
