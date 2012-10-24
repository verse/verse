/*
 * $Id: v_list.h 1268 2012-07-24 08:14:52Z jiri $
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

#ifndef V_LIST_H_
#define V_LIST_H_

#include <pthread.h>

#include "verse_types.h"

/* Property of command used in command history and command queue */
#define REMOVE_HASH_DUPS	1
#define NOT_SHARE_ADDR		2

#define INIT_ARRAY_LEN		1

#define HASH_MOD_256		1
#define HASH_MOD_65536		2
#define	HASH_COPY_BUCKET	4

typedef struct VItem {
	struct VItem	*prev, *next;
} VItem;

typedef struct VListBase {
	void			*first, *last;
} VListBase;

typedef struct VIndex {
	void			*item;
	uint32			next;
} VIndex;

typedef struct VArrayBase {
	struct VListBase	lb;
	struct VIndex		*array;
	uint32				length;
	uint32				count;
	uint32				first_free_index;
	uint32				last_free_index;
	pthread_mutex_t		mutex;
} VArrayBase;

typedef struct VBucket {
	struct VBucket		*prev, *next;
	void				*data;
	void				*ptr;
} VBucket;

typedef struct VBucketP {
	struct VBucket		*vbucket;	/* Pointer at item in linked list */
	struct VBucketP		*next;		/* Pointer at next item, when there is collision of hash function */
} VBucketP;

typedef struct VHashArrayBase {
	struct VListBase	lb;
	struct VBucketP		*buckets;	/* Array of buckets */
	uint32				length;		/* Length of the array of buckets (2^8 or 2^16)*/
	uint32				count;		/* Number of items in the linked list */
	/* Pointer at hash function returning pointer at item in linked list */
	int32				(*hash_func)(struct VHashArrayBase *hash_array, void *item);
	uint8				key_offset;	/* Offset of key from the begining of the key */
	uint8				key_size;	/* Size of key in bytes */
	pthread_mutex_t		mutex;
	uint32				flags;
} VHashArrayBase;

void v_list_add_head(struct VListBase *listbase, void *vitem);
void v_list_add_tail(struct VListBase *listbase, void *vitem);
void v_list_rem_item(struct VListBase *listbase, void *vitem);
void v_list_free_item(struct VListBase *listbase, void *vitem);
void v_insert_item(struct VListBase *listbase, void *vprevitem, void *vnewlink);
void v_list_insert_item_after(struct VListBase *listbase,
		void *vprev_item,
		void *vnew_item);
void v_list_insert_item_before(struct VListBase *listbase,
		void *vnext_item,
		void *vnew_item);
void v_list_free(struct VListBase *listbase);
int v_list_count_items(struct VListBase *listbase);
void *v_list_find_item(struct VListBase *listbase, int number);
int v_list_find_index(struct VListBase *listbase, void *vitem);

void *v_array_find_item(struct VArrayBase *arraybase, const uint32 index);
int v_array_remove_item(struct VArrayBase *arraybase, const uint32 index);
int v_array_add_item(struct VArrayBase *arraybase, void *vitem, uint32 *index);
int v_array_free(struct VArrayBase *arraybase);
int v_array_init(struct VArrayBase *arraybase);

struct VBucket *v_hash_array_find_item(struct VHashArrayBase *hash_array,
		void *item);
int v_hash_array_remove_item(struct VHashArrayBase *hash_array,
		void *item);
struct VBucket *v_hash_array_add_item(struct VHashArrayBase *hash_array,
		void *item,
		uint16 item_size);
uint32 v_hash_array_count_items(struct VHashArrayBase *hash_array);
int v_hash_array_destroy(struct VHashArrayBase *hash_array);
int v_hash_array_init(struct VHashArrayBase *hash_array,
		uint16 flag,
		uint8 key_offset,
		uint8 key_size);

#endif /* V_LIST_H_ */
