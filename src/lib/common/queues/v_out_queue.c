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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include "verse_types.h"

#include "v_out_queue.h"
#include "v_cmd_queue.h"
#include "v_common.h"
#include "v_commands.h"
#include "v_fake_commands.h"
#include "v_node_commands.h"

static struct VPrioOutQueue * _v_out_prio_queue_create(real32 r_prio);
static void _v_out_prio_queue_destroy(struct VPrioOutQueue *prio_queu);
static void _v_out_queue_command_add(struct VPrioOutQueue *prio_queue,
		uint8 flag,
		uint8 share_addr,
		struct VOutQueueCommand *queue_cmd,
		struct Generic_Cmd *cmd);
static struct VOutQueueCommand * _v_out_queue_command_create(struct VOutQueue *out_queue,
		uint8 flag, uint8 prio, struct Generic_Cmd *cmd);
static int _v_out_queue_push(struct VOutQueue *out_queue,
		uint8 flag, uint8 prio,	struct Generic_Cmd *cmd);

/**
 * \brief This function creates queue for commands with the priority
 */
static struct VPrioOutQueue * _v_out_prio_queue_create(real32 r_prio)
{
	struct VPrioOutQueue *prio_queue = (struct VPrioOutQueue *)calloc(1, sizeof(struct VPrioOutQueue));

	prio_queue->cmds.first = NULL;
	prio_queue->cmds.last = NULL;

	prio_queue->count = 0;
	prio_queue->size = 0;

	prio_queue->r_prio = r_prio;

	return prio_queue;
}

/**
 * \brief This function free all commands in linked list of priority queue
 */
static void _v_out_prio_queue_destroy(struct VPrioOutQueue *prio_queu)
{
	v_list_free(&prio_queu->cmds);
}

/**
 * \brief This function add VQueueCommand to the priority queue
 */
static void _v_out_queue_command_add(struct VPrioOutQueue *prio_queue,
		uint8 flag,
		uint8 share_addr,
		struct VOutQueueCommand *queue_cmd,
		struct Generic_Cmd *cmd)
{
	struct VOutQueueCommand *border_queue_cmd = NULL;

	/* Will be command added to then head or tail of queue? */
	if(flag & OUT_QUEUE_ADD_TAIL) {
		border_queue_cmd = prio_queue->cmds.last;
	} else if(flag & OUT_QUEUE_ADD_HEAD) {
		border_queue_cmd = prio_queue->cmds.first;
	}

	/* Add command to the end of new priority queue. If needed
	 * update counter of same commands in the queue */
	if(border_queue_cmd != NULL) {

		/* Is ID of this command same as ID of the last command
		 * in this priority queue? */
		if( (share_addr == 1) && (border_queue_cmd->id == queue_cmd->id)) {
			struct Generic_Cmd *border_cmd = (struct Generic_Cmd *)border_queue_cmd->vbucket->data;

			/* Is the last command in this priority queue and the
			 * first command with this ID? */
			if(border_queue_cmd->counter == NULL) {

				/* Set initial number of commands with same ID */
				border_queue_cmd->counter = (uint16*)malloc(sizeof(uint16));
				*border_queue_cmd->counter = 1;

				/* Compute size of address that could be shared */
				border_queue_cmd->share = (int8*)malloc(sizeof(int8));
				*border_queue_cmd->share = v_cmd_cmp_addr(border_cmd, cmd, 0xFF);

				/* Allocate memory for length of compressed commands */
				border_queue_cmd->len = (uint16*)malloc(sizeof(uint16));
				*border_queue_cmd->len = v_cmds_len(border_cmd, *border_queue_cmd->counter, *border_queue_cmd->share, 0);

			} else if(*border_queue_cmd->share > 0) {
				/* Try to update size of address that could be shared */
				*border_queue_cmd->share = v_cmd_cmp_addr(border_cmd, cmd, *border_queue_cmd->share);
			}

			/* Set up pointer */
			queue_cmd->counter = border_queue_cmd->counter;
			queue_cmd->share = border_queue_cmd->share;
			queue_cmd->len = border_queue_cmd->len;

			/* Update values */
			(*queue_cmd->counter)++;
			*queue_cmd->len = v_cmds_len(cmd, *queue_cmd->counter, *queue_cmd->share, *queue_cmd->len);
		} else {
			queue_cmd->counter = NULL;
			queue_cmd->share = NULL;
			queue_cmd->len = NULL;
		}
	}

	/* Will be command added to then head or tail of queue? */
	if(flag & OUT_QUEUE_ADD_TAIL) {
		v_list_add_tail(&prio_queue->cmds, queue_cmd);
	} else if(flag & OUT_QUEUE_ADD_HEAD) {
		v_list_add_head(&prio_queue->cmds, queue_cmd);
	}

}

/**
 * \brief This function creates VQueueCommand to priority queue
 */
static struct VOutQueueCommand * _v_out_queue_command_create(struct VOutQueue *out_queue,
		uint8 flag,
		uint8 prio,
		struct Generic_Cmd *cmd)
{
	/* Create new command in queue */
	struct VOutQueueCommand *queue_cmd = NULL;

	/* Check if there is free space for adding new command to the queue */
	if( (flag & OUT_QUEUE_LIMITS) &&
			out_queue->max_size < out_queue->size + out_queue->cmds[cmd->id]->item_size) {
		v_print_log(VRS_PRINT_DEBUG_MSG,
				"No free space in outgoing queue for command: %d", cmd->id);
		return NULL;
	}

	queue_cmd = (struct VOutQueueCommand*)calloc(1, sizeof(struct VOutQueueCommand));

	if(queue_cmd != NULL) {
		/* Set up id and priority of command */
		queue_cmd->id = cmd->id;
		queue_cmd->prio = prio;
		queue_cmd->counter = NULL;
		queue_cmd->share = NULL;

		if(out_queue->cmds[cmd->id]->flag & NOT_SHARE_ADDR) {
			_v_out_queue_command_add(out_queue->queues[prio], flag, 0, queue_cmd, cmd);
		} else {
			_v_out_queue_command_add(out_queue->queues[prio], flag, 1, queue_cmd, cmd);
		}

		/* Add new command data */
		queue_cmd->vbucket = v_hash_array_add_item(&out_queue->cmds[cmd->id]->cmds,
				cmd, out_queue->cmds[cmd->id]->item_size);
		queue_cmd->vbucket->ptr = (void*)queue_cmd;

		assert(queue_cmd->vbucket->data != NULL);

		/* When this priority queue was empty, then update summary of real priorities */
		if(out_queue->queues[prio]->count == 0) {
			if(prio>=VRS_DEFAULT_PRIORITY)
				out_queue->r_prio_sum_high += out_queue->queues[prio]->r_prio;
			else
				out_queue->r_prio_sum_low += out_queue->queues[prio]->r_prio;
		}

		/* Update count and size of commands */
		out_queue->count++;
		out_queue->size += out_queue->cmds[cmd->id]->item_size;

		out_queue->queues[prio]->count++;
		out_queue->queues[prio]->size += out_queue->cmds[cmd->id]->item_size;

		/* If necessary update maximal or minimal priority */
		if(prio > out_queue->max_prio) {
			out_queue->max_prio = prio;
		} else if(prio < out_queue->min_prio) {
			out_queue->min_prio = prio;
		}

	}

	return queue_cmd;
}

/**
 * \brief This function adds command to the head or tail of the queue
 *
 * \param[in] *out_queue	The outgoing queue
 * \param[in] flag			The flag that specify place of adding command (tail/head)
 * \param[in] prio			The priority of command
 * \param[in] *cmd			The pointer at command
 *
 * \return This function returns 1, when command was added to the queue.
 * Otherwise it returns 0.
 */
static int _v_out_queue_push(struct VOutQueue *out_queue,
		uint8 flag,
		uint8 prio,
		struct Generic_Cmd *cmd)
{
	struct VOutQueueCommand *queue_cmd;
	struct VBucket *vbucket;
	int ret = 1;

	assert(cmd != NULL);

	/* Try to find command with the same address, when duplications are not
	 * allowed in command queue */
	if(out_queue->cmds[cmd->id]->flag & REMOVE_HASH_DUPS) {
		vbucket = v_hash_array_find_item(&out_queue->cmds[cmd->id]->cmds, (void*)cmd);
		if(vbucket != NULL) {
			/* Bucket has to include not NULL pointer */
			assert(vbucket->ptr != NULL);

			queue_cmd = (struct VOutQueueCommand*)vbucket->ptr;

			/* Remove old command from the queue if the priority is different
			 * and add command to the end of new priority queue */
			if(queue_cmd->prio != prio) {

				/* Remove old obsolete data */
				v_hash_array_remove_item(&out_queue->cmds[cmd->id]->cmds, vbucket->data);
				/* Remove old  command */
				v_list_rem_item(&out_queue->queues[queue_cmd->prio]->cmds, queue_cmd);

				/* Update size and count in old priority queue */
				out_queue->queues[queue_cmd->prio]->count--;
				out_queue->queues[queue_cmd->prio]->size -= out_queue->cmds[cmd->id]->item_size;

				/* If needed, then update counter of commands with the same id in the queue */
				if(queue_cmd->counter != NULL) {
					*queue_cmd->counter -= 1;
					if(*queue_cmd->counter == 0) {
						free(queue_cmd->counter);
						queue_cmd->counter = NULL;
						free(queue_cmd->share);
						queue_cmd->share = NULL;
						free(queue_cmd->len);
						queue_cmd->len = NULL;
					}
				}

				/* When this priority queue is empty now, then update summary of real priorities */
				if(out_queue->queues[queue_cmd->prio]->count == 0) {
					if(prio>=VRS_DEFAULT_PRIORITY)
						out_queue->r_prio_sum_high -= out_queue->queues[queue_cmd->prio]->r_prio;
					else
						out_queue->r_prio_sum_low -= out_queue->queues[queue_cmd->prio]->r_prio;
				}

				/* Update new priority command */
				queue_cmd->id = cmd->id;
				queue_cmd->prio = prio;

				if(out_queue->cmds[cmd->id]->flag & NOT_SHARE_ADDR) {
					_v_out_queue_command_add(out_queue->queues[prio], flag, 0, queue_cmd, cmd);
				} else {
					_v_out_queue_command_add(out_queue->queues[prio], flag, 1, queue_cmd, cmd);
				}

				/* Add data of the command to the hashed linked list */
				queue_cmd->vbucket = v_hash_array_add_item(&out_queue->cmds[cmd->id]->cmds, cmd, out_queue->cmds[cmd->id]->item_size);
				queue_cmd->vbucket->ptr = (void*)queue_cmd;

				assert(queue_cmd->vbucket->data != NULL);

				/* When this priority queue was empty, then update summary of real priorities */
				if(out_queue->queues[prio]->count == 0) {
					if(prio>=VRS_DEFAULT_PRIORITY)
						out_queue->r_prio_sum_high += out_queue->queues[prio]->r_prio;
					else
						out_queue->r_prio_sum_low += out_queue->queues[prio]->r_prio;
				}

				/* Update count and size in new priority queue */
				out_queue->queues[prio]->count++;
				out_queue->queues[prio]->size += out_queue->cmds[cmd->id]->item_size;

				/* If necessary update maximal or minimal priority */
				if(prio > out_queue->max_prio) {
					out_queue->max_prio = prio;
				} else if(prio < out_queue->min_prio) {
					out_queue->min_prio = prio;
				}

			} else {
				/* Debug print */
#if 0
				v_print_log(VRS_PRINT_DEBUG_MSG, "Replacing obsolete command with new\n");
				v_cmd_print(VRS_PRINT_DEBUG_MSG, (struct Generic_Cmd*)vbucket->data);
				v_cmd_print(VRS_PRINT_DEBUG_MSG, cmd);
#endif

				/* Destroy original command */
				v_cmd_destroy((struct Generic_Cmd**)&vbucket->data);

				/* Replace data of current queue command with new data */
				vbucket->data = (void*)cmd;
			}
		} else {
			/* Add new command in queue */
			queue_cmd = _v_out_queue_command_create(out_queue, flag, prio, cmd);

			if(queue_cmd == NULL) {
				/* Destroy command, when it can not be added to the queue */
				v_cmd_destroy(&cmd);
				ret = 0;
			}
		}
	} else {
		/* Add new command in queue */
		queue_cmd = _v_out_queue_command_create(out_queue, flag, prio, cmd);

		if(queue_cmd == NULL) {
			/* Destroy command, when it can not be added to the queue */
			v_cmd_destroy(&cmd);
			ret = 0;
		}
	}

	return ret;
}

/**
 * \brief This function add command to the head of the queue
 */
int v_out_queue_push_head(struct VOutQueue *out_queue,
		uint8 prio,
		struct Generic_Cmd *cmd)
{
	struct VBucket *vbucket;
	int ret = 0;

	/* Lock mutex */
	pthread_mutex_lock(&out_queue->lock);

	/* Try to find command with the same address, when duplicities are not
	 * allowed in command queue */
	if(out_queue->cmds[cmd->id]->flag & REMOVE_HASH_DUPS) {
		vbucket = v_hash_array_find_item(&out_queue->cmds[cmd->id]->cmds, (void*)cmd);
		/* Add command from history of sent command to the outgoing queue
		 * only in situation, when there isn't already newer command in the
		 * queue. */
		if(vbucket == NULL) {
			v_print_log_simple(VRS_PRINT_DEBUG_MSG,
					"\tRe-sending command: %d\n", cmd->id);
			v_cmd_print(VRS_PRINT_DEBUG_MSG, (struct Generic_Cmd*)cmd);

			ret = _v_out_queue_push(out_queue, OUT_QUEUE_ADD_HEAD, prio, cmd);
		}
	} else {
		v_print_log_simple(VRS_PRINT_DEBUG_MSG,
				"\tRe-sending command: %d\n", cmd->id);
		v_cmd_print(VRS_PRINT_DEBUG_MSG, (struct Generic_Cmd*)cmd);

		ret = _v_out_queue_push(out_queue, OUT_QUEUE_ADD_HEAD, prio, cmd);
	}

	pthread_mutex_unlock(&out_queue->lock);

	return ret;
}

/**
 * \brief This function add command to the tail of the queue
 */
int v_out_queue_push_tail(struct VOutQueue *out_queue,
		uint8 flag,
		uint8 prio,
		struct Generic_Cmd *cmd)
{
	int ret = 0;

	/* Lock mutex */
	pthread_mutex_lock(&out_queue->lock);

	assert(flag == 0 || flag == OUT_QUEUE_LIMITS);

	ret = _v_out_queue_push(out_queue, (flag | OUT_QUEUE_ADD_TAIL), prio, cmd);

	pthread_mutex_unlock(&out_queue->lock);

	return ret;
}

/*
 * \brief This function pop command from queue with specific priority.
 *
 * Returned command is allocated on the heap and have to be free from calling
 * function. When *count is not NULL, then this function will write number of
 * commands with same ID that could be compressed on this address. When *len
 * is not NULL, then this function will write length of compressed commands
 * at this address. When *len value is not 0, then this value represents maximal
 * size of commands that could be removed from this priority queue.
 *
 * \param[in]	*out_queue	The pointer at list of priority queues
 * \param[in]	prio		The value of priority
 * \param[out]	*count		The pointer at count of commands with same id that
 * could be compressed.
 * \param[out]	*share		The size of address that could be shared
 * \param[out]	*len		The length of compressed commands.
 *
 * \return This function returns pointer at command. This command is allocated
 * at the heap.
 */
struct Generic_Cmd *v_out_queue_pop(struct VOutQueue *out_queue,
		uint8 prio,
		uint16 *count,
		int8 *share,
		uint16 *len)
{
	struct VPrioOutQueue *prio_queue = out_queue->queues[prio];
	struct VOutQueueCommand *queue_cmd;
	struct Generic_Cmd *cmd=NULL;
	int i, can_pop_cmd = 1;

	/* Lock mutex */
	pthread_mutex_lock(&out_queue->lock);

	/*printf("\tcount: %u, share: %u, len: %u\n", *count, *share, *len);*/

	queue_cmd = prio_queue->cmds.first;

	if(queue_cmd != NULL ) {
		cmd = (struct Generic_Cmd *)queue_cmd->vbucket->data;

		assert(cmd != NULL);

		/* Return value of count and length of compressed commands */
		if(queue_cmd->counter != NULL) {
			if(count != NULL) {
				*count = *queue_cmd->counter;
			}
			if(share != NULL) {
				*share = *queue_cmd->share;
			}
			if(len != NULL) {
				/* When *len value is not 0, then this value represents maximal
				 * size of buffer that could be filled with this priority queue. This
				 * function should return value, that is smaller or equal than this
				 * maximum. */
				if(*len != 0) {
					if(*len < *queue_cmd->len) {
						if (count != NULL) {
							*count = v_cmd_count(cmd, *len, *queue_cmd->share);
							/* FIXME: compute length and count correctly, when command is added to the queue */
							*count = ((*count) > (*queue_cmd->counter)) ? *queue_cmd->counter : *count;
							/* Is enough space in buffer to unpack this command? */
							if(*count == 0) {
								can_pop_cmd = 0;
							}
							*len = v_cmds_len(cmd, *count, *queue_cmd->share, 0);
						} else {
							can_pop_cmd = 0;
						}
					} else {
						*len = *queue_cmd->len;
					}
				} else {
					*len = *queue_cmd->len;
				}
			}
		} else {
			if(count != NULL) {
				*count = 0;
			}
			if(share != NULL) {
				*share = 0;
			}
			if(len != NULL) {
				if(v_cmd_size(cmd) > *len) {
					can_pop_cmd = 0;
				}
				*len = 0;
			}
		}

		/* Is it possible to pop command from the queue */
		if(can_pop_cmd == 1) {
			/* Remove command from hashed linked list */
			v_hash_array_remove_item(&out_queue->cmds[cmd->id]->cmds, (void*)cmd);

			/* Remove command from priority queue */
			v_list_rem_item(&prio_queue->cmds, queue_cmd);

			/* Update total count and size of commands */
			out_queue->count--;
			out_queue->size -= out_queue->cmds[cmd->id]->item_size;

			/* Update count and size of commands in subqueue with priority prio */
			out_queue->queues[prio]->count--;
			out_queue->queues[prio]->size -= out_queue->cmds[cmd->id]->item_size;

			/* If needed, then update counter of commands with the same id in the queue */
			if(queue_cmd->counter != NULL) {
				*queue_cmd->counter -= 1;
				/* Free values, when it's last command in the queue */
				if(*queue_cmd->counter == 0) {
					free(queue_cmd->counter);
					queue_cmd->counter = NULL;
					free(queue_cmd->share);
					queue_cmd->share = NULL;
					free(queue_cmd->len);
					queue_cmd->len = NULL;
				}
			}

			/* When this priority queue is empty now, then update summary of
			 * real priorities and it is possibly necessary to change maximal
			 * or minimal priority queue*/
			if(out_queue->queues[prio]->count == 0) {
				if(prio>=VRS_DEFAULT_PRIORITY) {
					out_queue->r_prio_sum_high -= out_queue->queues[prio]->r_prio;
				} else {
					out_queue->r_prio_sum_low -= out_queue->queues[prio]->r_prio;
				}

				if(out_queue->max_prio == prio && out_queue->min_prio != prio) {
					for(i=prio; i>=out_queue->min_prio; i--) {
						if(out_queue->queues[i]->count != 0) {
							out_queue->max_prio = i;
							break;
						}
					}
				} else if(out_queue->min_prio == prio && out_queue->max_prio != prio) {
					for(i=prio; i<=out_queue->max_prio; i++) {
						if(out_queue->queues[i]->count != 0) {
							out_queue->min_prio = i;
							break;
						}
					}
				} else if(out_queue->min_prio == prio && out_queue->max_prio == prio) {
					out_queue->min_prio = out_queue->max_prio = VRS_DEFAULT_PRIORITY;
				}
			}

			/* Free queue command */
			free(queue_cmd);
		} else {
			cmd = NULL;
		}
	}

	pthread_mutex_unlock(&out_queue->lock);

	return cmd;
}

/**
 * \brief This function initialize queue for outgoing commands
 */
int v_out_queue_init(struct VOutQueue *out_queue, int max_size)
{
	int id, prio, res;
	real32 r_prio;

	/* Initialize mutex of this queue */
	if((res=pthread_mutex_init(&out_queue->lock, NULL))!=0) {
		/* This function always return 0 on Linux */
		if(is_log_level(VRS_PRINT_ERROR)) v_print_log(VRS_PRINT_ERROR, "pthread_mutex_init(): %d\n", res);
		return 0;
	}

	out_queue->count = 0;
	out_queue->size = 0;

	out_queue->max_size = max_size;

	out_queue->max_prio = VRS_DEFAULT_PRIORITY;
	out_queue->min_prio = VRS_DEFAULT_PRIORITY;

	out_queue->r_prio_sum_high = 0.0;
	out_queue->r_prio_sum_low = 0.0;

	/* Set up high priorities */
	r_prio = VRS_DEFAULT_PRIORITY;
	for(prio=VRS_DEFAULT_PRIORITY; prio<=MAX_PRIORITY; prio++) {
		out_queue->queues[prio] = _v_out_prio_queue_create((r_prio<MAX_REAL_PRIO_VAL)?r_prio:MAX_REAL_PRIO_VAL);
		r_prio = r_prio + r_prio*REAL_PRIO_MUL;
	}

	/* Set up low priorities */
	r_prio = VRS_DEFAULT_PRIORITY - 1;
	for(prio=VRS_DEFAULT_PRIORITY-1; prio>=0; prio--) {
		out_queue->queues[prio] = _v_out_prio_queue_create((r_prio>MIN_REAL_PRIO_VAL)?r_prio:MIN_REAL_PRIO_VAL);
		r_prio = r_prio - r_prio*REAL_PRIO_MUL;
	}

	/* Set command queues */
	for(id=0; id<=MAX_CMD_ID; id++) {
		out_queue->cmds[id] = v_cmd_queue_create(id, 0, 1);
	}

	return 1;
}

/**
 * \brief This function creates new queue for outgoing commands
 */
struct VOutQueue *v_out_queue_create(void)
{
	struct VOutQueue *out_queue = (struct VOutQueue *)malloc(sizeof(struct VOutQueue));

	if(out_queue!=NULL) {
		if( v_out_queue_init(out_queue, OUT_QUEUE_DEFAULT_MAX_SIZE) != 1) {
			free(out_queue);
			out_queue = NULL;
		}
	}

	return out_queue;
}

/**
 * \brief This function destroy queue for outgoing commands
 */
void v_out_queue_destroy(struct VOutQueue **out_queue)
{
	int id;

	pthread_mutex_lock(&(*out_queue)->lock);

	(*out_queue)->count = 0;
	(*out_queue)->size = 0;

	(*out_queue)->max_prio = VRS_DEFAULT_PRIORITY;
	(*out_queue)->min_prio = VRS_DEFAULT_PRIORITY;

	(*out_queue)->r_prio_sum_high = 0.0;
	(*out_queue)->r_prio_sum_low = 0.0;

	for(id=0; id<=MAX_PRIORITY; id++) {
		if((*out_queue)->queues[id] != NULL) {
			_v_out_prio_queue_destroy((*out_queue)->queues[id]);
			free((*out_queue)->queues[id]);
			(*out_queue)->queues[id] = NULL;
		}
	}

	for(id=0; id<=MAX_CMD_ID; id++) {
		if((*out_queue)->cmds[id] != NULL) {
			v_cmd_queue_destroy((*out_queue)->cmds[id]);
			free((*out_queue)->cmds[id]);
			(*out_queue)->cmds[id] = NULL;
		}
	}

	pthread_mutex_unlock(&(*out_queue)->lock);

	pthread_mutex_destroy(&(*out_queue)->lock);

	free(*out_queue);
	*out_queue = NULL;
}

/**
 * \brief This function returns number of commands in queue with priority that
 * is equal to value prio.
 */
uint32 v_out_queue_get_count_prio(struct VOutQueue *out_queue, uint8 prio)
{
	uint32 count;

	pthread_mutex_lock(&out_queue->lock);
	count = out_queue->queues[prio]->count;
	pthread_mutex_unlock(&out_queue->lock);

	return count;
}

uint32 v_out_queue_get_size_prio(struct VOutQueue *out_queue, uint8 prio)
{
	uint32 size;

	pthread_mutex_lock(&out_queue->lock);
	size = out_queue->queues[prio]->size;
	pthread_mutex_unlock(&out_queue->lock);

	return size;
}

uint32 v_out_queue_get_count(struct VOutQueue *out_queue)
{
	uint32 count;

	pthread_mutex_lock(&out_queue->lock);
	count = out_queue->count;
	pthread_mutex_unlock(&out_queue->lock);

	return count;
}

uint32 v_out_queue_get_size(struct VOutQueue *out_queue)
{
	uint32 size;

	pthread_mutex_lock(&out_queue->lock);
	size = out_queue->size;
	pthread_mutex_unlock(&out_queue->lock);

	return size;
}

uint8 v_out_queue_get_max_prio(struct VOutQueue *out_queue)
{
	uint8 max;

	pthread_mutex_lock(&out_queue->lock);
	max = out_queue->max_prio;
	pthread_mutex_unlock(&out_queue->lock);

	return max;
}

uint8 v_out_queue_get_min_prio(struct VOutQueue *out_queue)
{
	uint8 min;

	pthread_mutex_lock(&out_queue->lock);
	min = out_queue->min_prio;
	pthread_mutex_unlock(&out_queue->lock);

	return min;
}

real32 v_out_queue_get_prio(struct VOutQueue *out_queue, uint8 prio)
{
	return out_queue->queues[prio]->r_prio;
}

real32 v_out_queue_get_prio_sum_high(struct VOutQueue *out_queue)
{
	real32 r_prio;

	pthread_mutex_lock(&out_queue->lock);
	r_prio = out_queue->r_prio_sum_high;
	pthread_mutex_unlock(&out_queue->lock);

	return r_prio;
}

real32 v_out_queue_get_prio_sum_low(struct VOutQueue *out_queue)
{
	real32 r_prio;

	pthread_mutex_lock(&out_queue->lock);
	r_prio = out_queue->r_prio_sum_low;
	pthread_mutex_unlock(&out_queue->lock);

	return r_prio;
}
