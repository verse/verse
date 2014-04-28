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
 * Most of the source code was copied from Blender (util.c)
 *
 */


#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

#include "v_list.h"
#include "v_common.h"

void v_list_add_head(struct VListBase *listbase, void *vitem)
{
	VItem *item = vitem;

	if (item == NULL) return;
	if (listbase == NULL) return;

	item->next = listbase->first;
	item->prev = NULL;

	if (listbase->first) ((VItem *)listbase->first)->prev = item;
	if (listbase->last == NULL) listbase->last = item;
	listbase->first = item;
}

void v_list_add_tail(struct VListBase *listbase, void *vitem)
{
	VItem *item = vitem;

	if (item == NULL) return;
	if (listbase == NULL) return;

	item->next = NULL;
	item->prev = listbase->last;

	if (listbase->last) ((VItem *)listbase->last)->next = item;
	if (listbase->first == NULL) listbase->first = item;
	listbase->last = item;
}

void v_list_rem_item(struct VListBase *listbase, void *vitem)
{
	VItem *item = vitem;

	if (item == NULL) return;
	if (listbase == NULL) return;

	if (item->next) item->next->prev = item->prev;
	if (item->prev) item->prev->next = item->next;

	if (listbase->last == item) listbase->last = item->prev;
	if (listbase->first == item) listbase->first = item->next;
}

void v_list_free_item(struct VListBase *listbase, void *vitem)
{
	VItem *item = vitem;

	if (item == NULL) return;
	if (listbase == NULL) return;

	v_list_rem_item(listbase, item);
	free(item);
}

void v_insert_item(struct VListBase *listbase,
		void *vprev_item,
		void *vnew_item)
{
	VItem *prev_item= vprev_item;
	VItem *new_item= vnew_item;

	/* newlink comes after previtem */
	if (new_item == NULL) return;
	if (listbase == NULL) return;

	/* empty list */
	if(listbase->first == NULL) {
		listbase->first = new_item;
		listbase->last = new_item;
		return;
	}

	/* insert before first element */
	if (prev_item == NULL) {
		new_item->next = listbase->first;
		new_item->prev = NULL;
		new_item->next->prev = new_item;
		listbase->first = new_item;
		return;
	}

	/* at end of list */
	if(listbase->last == prev_item)
		listbase->last = new_item;

	new_item->next = prev_item->next;
	prev_item->next = new_item;
	if(new_item->next)
		new_item->next->prev = new_item;
	new_item->prev = prev_item;
}

void v_list_insert_item_after(struct VListBase *listbase,
		void *vprev_item,
		void *vnew_item)
{
	VItem *prev_item= vprev_item;
	VItem *new_item= vnew_item;

	/* newlink before nextlink */
	if (new_item == NULL) return;
	if (listbase == NULL) return;

	/* empty list */
	if (listbase->first == NULL) {
		listbase->first = new_item;
		listbase->last = new_item;
		return;
	}

	/* insert at head of list */
	if (prev_item == NULL) {
		new_item->prev = NULL;
		new_item->next = listbase->first;
		((VItem *)listbase->first)->prev = new_item;
		listbase->first = new_item;
		return;
	}

	/* at end of list */
	if (listbase->last == prev_item)
		listbase->last = new_item;

	new_item->next = prev_item->next;
	new_item->prev = prev_item;
	prev_item->next = new_item;
	if (new_item->next) new_item->next->prev = new_item;
}

void v_list_insert_item_before(struct VListBase *listbase,
		void *vnext_item,
		void *vnew_item)
{
	VItem *next_item = vnext_item;
	VItem *new_item = vnew_item;

	/* newlink before nextlink */
	if (new_item == NULL) return;
	if (listbase == NULL) return;

	/* empty list */
	if (listbase->first == NULL) {
		listbase->first = new_item;
		listbase->last = new_item;
		return;
	}

	/* insert at end of list */
	if (next_item == NULL) {
		new_item->prev = listbase->last;
		new_item->next = NULL;
		((VItem *)listbase->last)->next= new_item;
		listbase->last = new_item;
		return;
	}

	/* at beginning of list */
	if (listbase->first == next_item)
		listbase->first = new_item;

	new_item->next = next_item;
	new_item->prev = next_item->prev;
	next_item->prev = new_item;
	if (new_item->prev) new_item->prev->next = new_item;
}

void v_list_free(struct VListBase *listbase)
{
	VItem *item, *next;

	if(listbase == NULL)
		return;

	item = listbase->first;
	while(item) {
		next = item->next;
		free(item);
		item = next;
	}

	listbase->first = NULL;
	listbase->last = NULL;
}

int v_list_count_items(struct VListBase *listbase)
{
	VItem *item;
	int count = 0;

	if (listbase) {
		item = listbase->first;
		while (item) {
			count++;
			item = item->next;
		}
	}

	return count;
}

void *v_list_find_item(struct VListBase *listbase, int number)
{
	VItem *item = NULL;

	if(number >= 0) {
		item = listbase->first;
		while(item != NULL && number != 0) {
			number--;
			item = item->next;
		}
	}

	return item;
}

int v_list_find_index(struct VListBase *listbase, void *vitem)
{
	struct VItem *item = NULL;
	int number = 0;

	if (listbase == NULL) return -1;
	if (vitem == NULL) return -1;

	item = listbase->first;
	while(item) {
		if(item == vitem)
			return number;

		number++;
		item= item->next;
	}

	return -1;
}

/* -------------- Two way linked list with access array -------------- */

void *v_array_find_item(struct VArrayBase *arraybase, const uint32 index)
{
	struct VItem *item;

	pthread_mutex_lock(&arraybase->mutex);

	if(index < arraybase->length) {
		item = arraybase->array[index].item;
	} else {
		item = NULL;
	}

	pthread_mutex_unlock(&arraybase->mutex);

	return item;
}

int v_array_remove_item(struct VArrayBase *arraybase, const uint32 index)
{
	int ret = 1;

	pthread_mutex_lock(&arraybase->mutex);

	if(index < arraybase->length) {
		if(arraybase->array[index].item != NULL) {
			/* Remove item from linked list */
			v_list_rem_item(&arraybase->lb, arraybase->array[index].item);
			/* Update access array */
			arraybase->array[index].item = NULL;
			arraybase->array[index].next = 0xFFFFFFFF;
			arraybase->last_free_index = index;
			arraybase->count--;
		} else {
			ret = 0;
			goto end;
		}
	} else {
		ret = 0;
		goto end;
	}

end:
	pthread_mutex_unlock(&arraybase->mutex);

	return ret;
}

int v_array_add_item(struct VArrayBase *arraybase, void *vitem, uint32 *index)
{
	int ret = 1;

	pthread_mutex_lock(&arraybase->mutex);

	/* When access array is too small, then it is necessary to make it bigger
	 * (twice bigger) */
	if(arraybase->count+1 > arraybase->length) {
		struct VIndex *new_array;
		uint32 i, length = arraybase->length*2;

		/* Reallocate access array */
		new_array = realloc(arraybase->array, sizeof(struct VIndex)*length);
		if(new_array != NULL) {
			arraybase->array = new_array;
			arraybase->length = length;
		} else {
			/* It wasn't able to reallocate memory, exit with error */
			ret = 0;
			*index = 0xFFFFFFFF;
			goto end;
		}

		/* Generate new indexes in new part of access array */
		for(i=(length/2); i<length; i++) {
			arraybase->array[i].item = NULL;
			arraybase->array[i].next = i+1;
		}
		arraybase->array[i-1].next = 0xFFFFFFFF;

		arraybase->first_free_index = (length/2);
		arraybase->last_free_index = length-1;
	}

	/* Add item to the linked list */
	v_list_add_tail(&arraybase->lb, vitem);

	/* Set up used index in access array */
	*index = arraybase->first_free_index;

	/* Update access array */
	arraybase->array[arraybase->first_free_index].item = vitem;
	arraybase->first_free_index = arraybase->array[arraybase->first_free_index].next;
	arraybase->count++;

end:
	pthread_mutex_unlock(&arraybase->mutex);

	return ret;
}

int v_array_free(struct VArrayBase *arraybase)
{
	pthread_mutex_lock(&arraybase->mutex);

	v_list_free(&arraybase->lb);

	if(arraybase->array != NULL) {
		free(arraybase->array);
		arraybase->array = NULL;
	}

	arraybase->count = 0;
	arraybase->first_free_index = 0;
	arraybase->last_free_index = 0;
	arraybase->length = 0;

	pthread_mutex_unlock(&arraybase->mutex);

	return 1;
}

int v_array_init(struct VArrayBase *arraybase)
{
	int i, res;

	/* Initialize mutex of this queue */
	if((res=pthread_mutex_init(&arraybase->mutex, NULL))!=0) {
		/* This function always return 0 on Linux */
		v_print_log(VRS_PRINT_ERROR, "pthread_mutex_init(): %d\n", res);
		return 0;
	}

	arraybase->lb.first = NULL;
	arraybase->lb.last = NULL;

	arraybase->array = (struct VIndex*)calloc(INIT_ARRAY_LEN, sizeof(struct VIndex));
	for(i=0; i<INIT_ARRAY_LEN; i++) {
		arraybase->array[i].item = NULL;
		arraybase->array[i].next = i+1;
	}
	arraybase->first_free_index = 0;
	arraybase->last_free_index = INIT_ARRAY_LEN-1;

	arraybase->length = INIT_ARRAY_LEN;
	arraybase->count = 0;

	return 1;
}

/* -------------- Two way linked list with hashed access array -------------- */

/**
 * \brief This function is hash function, that compute simple hash: modulo 256
 * from key of the item. The key is specified in has_array structure as
 * key_offset and key_size. This function doesn't include lock of the mutex,
 * because this function is always called, when has_array is already locked.
 * \param[in]	*hash_array	The pointer at structure storing linked list and
 * own array of access array
 * \param[in]	*item		The pointer at item containing key for computing
 * hash function.
 * \return This function returns index to the array of hashes, when
 * hash_array->key_size is bigger then 0, otherwise it returns -1, because it
 * is not possible to compute hash function from key with zero size.
 */
static int32 v_hash_func_uint16(struct VHashArrayBase *hash_array, void *item)
{
	uint8 *data = NULL;
	int32 hash_value = -1;
	int16 i = 0;
	long unsigned int tmp = 0;

	if(hash_array->key_size == 0) return -1;

	/* Compute index from address */
	if(hash_array->key_size % 2 == 0) {
		for(tmp=0, i=0, data=(uint8*)item + hash_array->key_offset;
				i < hash_array->key_size;
				i+=2, data+=2)
		{
			tmp += *(uint16*)data;
		}
	} else {
		for(tmp=0, i=0, data=(uint8*)item + hash_array->key_offset;
				i < hash_array->key_size-1;
				i+=2, data+=2)
		{
			tmp += *(uint16*)data;
		}
		/* Add not padded data to the tmp variable */
		tmp += *(uint8*)data;
	}

	hash_value = tmp % hash_array->length;

	return hash_value;
}

/**
 * \brief The hash function, that implement simple hash function: modulo 65536.
 * The size of array of hashes is 65536*sizeof(VBucketP). This function doesn't
 * include lock of the mutex, because this function is always called, when
 * has_array is already locked.
 * \param[in]	*hash_array	The pointer at hashed linked list
 * \param[in[	*item		The pointer at item containing key to be used as
 * input to hash function
 * \return	This function returns index to the array of hashes, when
 * hash_array->key_size is bigger then 0, otherwise it returns -1, because it
 * is not possible to compute hash function from key with zero size.
 */
static int32 v_hash_func_uint32(struct VHashArrayBase *hash_array, void *item)
{
	uint8 *data = NULL;
	int16 i = 0;
	int32 hash_value = -1;
	uint32 modulo = 0;
	long unsigned int tmp = 0;

	if(hash_array->key_size == 0) return -1;

	modulo = hash_array->key_size % 4;
	/* Compute index from address */
	switch(modulo) {
		case 0:
			for(tmp=0, i=0, data=(uint8*)item + hash_array->key_offset;
					i < hash_array->key_size;
					i+=4, data+=4)
			{
				tmp += *(uint32*)data;
			}
			break;
		case 1:
			for(tmp=0, i=0, data=(uint8*)item + hash_array->key_offset;
					i < hash_array->key_size-1;
					i+=4, data+=4)
			{
				tmp += *(uint32*)data;
			}
			tmp += *(uint16*)data;
			data += 2;
			tmp += *(uint8*)data;
			break;
		case 2:
			for(tmp=0, i=0, data=(uint8*)item + hash_array->key_offset;
					i < hash_array->key_size-1;
					i+=4, data+=4)
			{
				tmp += *(uint32*)data;
			}
			tmp += *(uint16*)data;
			break;
		case 3:
			for(tmp=0, i=0, data=(uint8*)item + hash_array->key_offset;
					i < hash_array->key_size-1;
					i+=4, data+=4)
			{
				tmp += *(uint32*)data;
			}
			tmp += *(uint8*)data;
			break;
	}

	hash_value = tmp % hash_array->length;

	return hash_value;
}

/*static void print_mem(void *mem, uint32 size)
{
	int i;
	for(i=0; i<size; i++) {
		printf("%X", (int)*((char*)mem+i));
	}
	printf("\n");
}*/

/**
 * \brief		This function tries to find item in hashed linked list.
 * \param[in]	*hash_array	The pointer at hashed linked list
 * \param[in]	*item		The pointer at item containing key for hash
 * function. The relative position of key in item is stored in hash_array.
 * \return		This function returns pointer at bucket, when item was found
 * in hashed linked list and it returns NULL, when item was not found.
 */
struct VBucket *v_hash_array_find_item(struct VHashArrayBase *hash_array,
		void *item)
{
	struct VBucket *vbucket = NULL;
	int64 index = -1;

	pthread_mutex_lock(&hash_array->mutex);

	/* Hashed linked list has to include array of buckets */
	assert(hash_array->buckets != NULL);

	/* Hashed linked list has to have own hash function */
	assert(hash_array->hash_func != NULL);

	index = hash_array->hash_func(hash_array, item);

	/* Found index has to be in  valid range? */
	assert(index >= 0 && index < hash_array->length);

	/* Is there any collision at this index? */
	if(hash_array->buckets[index].next == NULL) {
		/* If there is no collision, then return pointer at item */
		vbucket = hash_array->buckets[index].vbucket;

		/* Is there any bucket with data? */
		if(vbucket != NULL) {
			/* Bucket has to include some data */
			assert(vbucket->data != NULL);

			/* And check if the key of found bucket has the same as key
			 * as the item has. */
			if( !(memcmp((uint8*)item + hash_array->key_offset,
					(uint8*)vbucket->data + hash_array->key_offset,
					hash_array->key_size) == 0) )
			{
				/* This happens, when item is in the collision with the
				 * bucket in the hashed linked list, but item and found bucket
				 * has different key */
				vbucket = NULL;
			}
		}
	} else {
		struct VBucketP *bucket_p;

		/* When collision is at this index, then go through the list
		 * and compare addresses of each item and key */
		for( bucket_p = &hash_array->buckets[index];
				bucket_p != NULL;
				bucket_p = bucket_p->next)
		{
			/* All pointers at buckets have to include some bucket */
			assert(bucket_p->vbucket != NULL);

			/* All buckets have to include pointer at data */
			assert(bucket_p->vbucket->data != NULL);

			if( memcmp((uint8*)item + hash_array->key_offset,
					(uint8*)bucket_p->vbucket->data + hash_array->key_offset,
					hash_array->key_size) == 0 )
			{
				vbucket = bucket_p->vbucket;
				break;
			}
		}
	}

	pthread_mutex_unlock(&hash_array->mutex);

	return vbucket;
}

/**
 * \brief This function adds new item to the linked list and add new record
 * to the array of hashes. The item_size has to be bigger then
 * (key_offset + key_size).
 */
struct VBucket *v_hash_array_add_item(struct VHashArrayBase *hash_array,
		void *item,
		uint16 item_size)
{
	struct VBucket *vbucket = NULL;
	int64 index = -1;

	pthread_mutex_lock(&hash_array->mutex);

	/* Hashed linked list has to have own hash function */
	assert(hash_array->hash_func != NULL);

	/* Hashed linked list has to have buckets */
	assert(hash_array->buckets != NULL);

	/* The item_size has to be bigger then (key_offset + key_size).*/
	assert( item_size >= (hash_array->key_offset + hash_array->key_size) );

	index = hash_array->hash_func(hash_array, item);

	/* Is computed index in allowed range */
	assert(index >= 0 && index < hash_array->length);

	/* Create new bucket */
	vbucket = (struct VBucket*)calloc(1, sizeof(struct VBucket));

	/* Check if it was possible to allocate memory for bucket */
	if(vbucket == NULL) {
		v_print_log(VRS_PRINT_ERROR, "Not enough memory for new bucket\n");
		vbucket = NULL;
		goto end;
	}

	v_list_add_tail(&hash_array->lb, vbucket);

	if(hash_array->flags & HASH_COPY_BUCKET) {
		/* Allocate memory for item */
		vbucket->data = malloc(item_size);
		/* Check if it was possible to allocate memory for data */
		if(vbucket->data == NULL) {
			v_print_log(VRS_PRINT_ERROR,
					"Not enough memory for new data of bucket\n");
			v_list_rem_item(&hash_array->lb, vbucket);
			free(vbucket);
			vbucket = NULL;
			goto end;
		}
		/* Copy item to the bucket */
		memcpy(vbucket->data, item, item_size);
	} else {
		vbucket->data = item;
	}

	if(hash_array->buckets[index].vbucket == NULL) {
		/* It is first item at this index */
		hash_array->buckets[index].vbucket = vbucket;
		hash_array->buckets[index].next = NULL;
	} else {
		/* There is collision at this index */
		struct VBucketP *vbucket_p, *new_vbucket_p;

		/* Create new bucket pointer */
		new_vbucket_p = (struct VBucketP *)calloc(1, sizeof(struct VBucketP));

		/* Check if it was possible to allocate memory for pointer at
		 * bucket */
		if(new_vbucket_p == NULL) {
			v_print_log(VRS_PRINT_ERROR,
					"Not enough memory for new pointer at bucket\n");
			v_list_rem_item(&hash_array->lb, vbucket);
			if(hash_array->flags & HASH_COPY_BUCKET) {
				free(vbucket->data);
			}
			free(vbucket);
			vbucket = NULL;
			goto end;
		}

		/* Go to the end of the list */
		for(vbucket_p = &hash_array->buckets[index];
				vbucket_p->next != NULL;
				vbucket_p = vbucket_p->next) {}

		/* Paste new bucket pointer at the end of the linked list */
		vbucket_p->next = new_vbucket_p;

		new_vbucket_p->vbucket = vbucket;
		new_vbucket_p->next = NULL;
	}

	hash_array->count++;

end:
	pthread_mutex_unlock(&hash_array->mutex);

	return vbucket;
}

/**
 * \brief This function tries to remove item from hashed linked list. The size
 * of item has to be bigger then key_offset and key_size, because the key in
 * this item is used as input for hash function. When corresponding bucket is
 * found, then it is removed from hashed linked list. If flag of hash_array is
 * set to HASH_COPY_BUCKET, then data of bucket are freed.
 * \param[in]	*hash_array	The pointer at hashed linked list
 * \param		*item		The pointer at item to try to removed from hashed
 * linked list
 * \return	This function returns 1, when item was removed and it returns 0,
 * when item was not found in hashed linked list and could not be removed.
 */
int v_hash_array_remove_item(struct VHashArrayBase *hash_array, void *item)
{
	int ret = 0;
	int64 index = -1;

	pthread_mutex_lock(&hash_array->mutex);

	if(hash_array->buckets != NULL) {

		/* Each hash array has to have own hash function */
		assert(hash_array->hash_func != NULL);

		index = hash_array->hash_func(hash_array, item);

		/* Hash function has to return index in allowed range */
		assert(index >=0 && index < hash_array->length);

		/* Was any bucket found at this index? */
		if(hash_array->buckets[index].vbucket != NULL) {

			/* Is there any collision at this index? */
			if(hash_array->buckets[index].next == NULL) {
				/* If there is no collision, then it is simple */

				/* Bucket has to include some data */
				assert(hash_array->buckets[index].vbucket->data != NULL);

				/* Is this really item, that we try to remove */
				if( (memcmp((uint8*)item + hash_array->key_offset,
						(uint8*)hash_array->buckets[index].vbucket->data + hash_array->key_offset,
						hash_array->key_size) == 0))
				{
					/* Free data of the item, when the item was copied */
					if(hash_array->flags & HASH_COPY_BUCKET) {
						free(hash_array->buckets[index].vbucket->data);
						hash_array->buckets[index].vbucket->data = NULL;
					}
					hash_array->buckets[index].vbucket->ptr = NULL;

					/* Remove bucket from the linked list of buckets */
					v_list_rem_item(&hash_array->lb, hash_array->buckets[index].vbucket);

					/* Free bucket */
					free(hash_array->buckets[index].vbucket);
					/* Update VbucketP pointer */
					hash_array->buckets[index].vbucket = NULL;

					hash_array->count--;

					ret = 1;
				} else {
					v_print_log(VRS_PRINT_DEBUG_MSG, "Item wasn't removed, key of item doesn't match at index: %d\n", index);
				}
			} else {
				/* When collision is at this index, then it is necessary to find
				 * right bucket. Go through the list and compare key of each item */
				struct VBucketP *vbucket_p, *prev_vbucket_p;

				for( vbucket_p = &hash_array->buckets[index], prev_vbucket_p = NULL;
						vbucket_p != NULL;
						prev_vbucket_p = vbucket_p, vbucket_p = vbucket_p->next)
				{
					/* Pointer at bucket has to include valid pointer */
					assert(vbucket_p->vbucket != NULL);

					/* Bucket has to include pointer at data*/
					assert(vbucket_p->vbucket->data != NULL);

					if( memcmp((uint8*)item + hash_array->key_offset,
							(uint8*)vbucket_p->vbucket->data + hash_array->key_offset,
							hash_array->key_size) == 0 )
					{
						/* Free data of the item, when the item was copied */
						if(hash_array->flags & HASH_COPY_BUCKET) {
							free(vbucket_p->vbucket->data);
							vbucket_p->vbucket->data = NULL;
						}
						vbucket_p->vbucket->ptr = NULL;

						/* Remove bucket from the linked list */
						v_list_rem_item(&hash_array->lb, vbucket_p->vbucket);

						/* Free bucket */
						free(vbucket_p->vbucket);
						vbucket_p->vbucket = NULL;

						/* Update VbucketP linked list */
						if(prev_vbucket_p == NULL) {
							struct VBucketP *next_vbucket_p = vbucket_p->next;
							/* In this case first VBucketP is removed from
							 * the list and second VBucketP will be removed.
							 * Second VBucketP has to exist, because we are
							 * handling collision */
							vbucket_p->vbucket = vbucket_p->next->vbucket;
							vbucket_p->next = vbucket_p->next->next;
							free(next_vbucket_p);
						} else {
							if(vbucket_p->next==NULL) {
								/* In this case last VBucketP is removed from the list */
								prev_vbucket_p->next = NULL;
								free(vbucket_p);
							} else {
								/* VBucketP between other VBucketPs is removed */
								prev_vbucket_p->next = vbucket_p->next;
								free(vbucket_p);
							}
						}

						hash_array->count--;

						ret = 1;
						break;
					}
				}

				if(ret != 1) {
					v_print_log(VRS_PRINT_DEBUG_MSG, "Item wasn't removed, no item with the same key at index: %d\n", index);
				}
			}
		} else {
			v_print_log(VRS_PRINT_DEBUG_MSG, "Item wasn't removed, no item at index: %d\n", index);
		}
	}

	pthread_mutex_unlock(&hash_array->mutex);

	return ret;
}

/**
 * \brief This function returns number of items in hashed linked list
 * \param[in]	*hash_array	The pointer at hashed linked list
 * \return It returns number of items in hashed linked list
 */
uint32 v_hash_array_count_items(struct VHashArrayBase *hash_array)
{
	uint32 count;

	pthread_mutex_lock(&hash_array->mutex);

	count = hash_array->count;

	pthread_mutex_unlock(&hash_array->mutex);

	return count;
}

/**
 * \brief This function destroy whole hashed linked list. The own bucket data
 * are destroyed too, when has_array->flag is set to HASH_COPY_BUCKET, otherwise
 * these data are not destroyed.
 * \param[in]	*hash_array	The pointer at hashed linked list to be destroyed
 * \return This function returns 1, when hashed linked list was destroyed
 * successfully and it return 0, when it failed.
 */
int v_hash_array_destroy(struct VHashArrayBase *hash_array)
{
	uint32 i;
	struct VBucket *vbucket;
	struct VBucketP *vbucket_p, *next_vbucket_p;

	pthread_mutex_lock(&hash_array->mutex);

	if(hash_array->flags & HASH_COPY_BUCKET) {
		for(vbucket = hash_array->lb.first;
				vbucket != NULL;
				vbucket = vbucket->next)
		{
			free(vbucket->data);
			vbucket->data = NULL;
		}
	}

	v_list_free(&hash_array->lb);

	/* Go to through the whole access array and free VBucketP for indexes
	 * with collisions */
	for(i = 0; i< hash_array->length; i++) {
		if(hash_array->buckets[i].next != NULL) {
			vbucket_p = hash_array->buckets[i].next;
			while(vbucket_p != NULL) {
				next_vbucket_p = vbucket_p->next;
				free(vbucket_p);
				vbucket_p = next_vbucket_p;
			}
		}
	}

	free(hash_array->buckets);
	hash_array->buckets = NULL;

	hash_array->count = 0;
	hash_array->hash_func = NULL;
	hash_array->key_offset = 0;
	hash_array->key_size = 0;
	hash_array->length = 0;

	pthread_mutex_unlock(&hash_array->mutex);

	pthread_mutex_destroy(&hash_array->mutex);

	return 1;
}

/**
 * \brief This function initialize new hashed linked list. This hashed linked
 * list could store hashed data, or it could include only pointers at external
 * data. Two hash functions are supported now.
 * \param[out]	*hash_array	The pointer at hashed linked list to be initialized
 * \param[in]	flags		The flags, where type of hash is specified and this
 * flag specify, if items will be copied to the bucket, or buckets will include
 * only pointers at items.
 * \param[in]	key_offset	The offset of key in item
 * \param[in]	key_size	The size of key in item
 * \return This function returns 1, when hashed linked list is initialized and
 * it returns 0, when initialization failed.
 */
int v_hash_array_init(struct VHashArrayBase *hash_array,
		uint16 flags,
		uint8 key_offset,
		uint8 key_size)
{
	uint32 i;
	int res, ret = 0;

	hash_array->lb.first = NULL;
	hash_array->lb.last = NULL;

	/* Initialize mutex of this array */
	if((res = pthread_mutex_init(&hash_array->mutex, NULL)) != 0) {
		/* This function always return 0 on Linux */
		v_print_log(VRS_PRINT_ERROR, "pthread_mutex_init(): %d\n", res);
		hash_array->length = 0;
		hash_array->hash_func = NULL;
		hash_array->key_offset = 0;
		hash_array->key_size = 0;
		hash_array->flags = 0;
		return 0;
	}

	pthread_mutex_lock(&hash_array->mutex);

	if(flags & HASH_MOD_256) {
		hash_array->length = 256;
		hash_array->buckets = (struct VBucketP*)calloc(hash_array->length,
				sizeof(struct VBucketP));
		if(hash_array->buckets == NULL) {
			v_print_log(VRS_PRINT_ERROR, "calloc(): no memory allocated\n");
			hash_array->length = 0;
			hash_array->hash_func = NULL;
			hash_array->key_offset = 0;
			hash_array->key_size = 0;
			hash_array->flags = 0;
			goto end;
		}
		for(i=0; i<hash_array->length; i++) {
			hash_array->buckets[i].vbucket = NULL;
			hash_array->buckets[i].next = NULL;
		}
		hash_array->hash_func = v_hash_func_uint16;
	} else if(flags & HASH_MOD_65536) {
		hash_array->length = 65536;
		hash_array->buckets = (struct VBucketP*)calloc(hash_array->length,
				sizeof(struct VBucketP));
		if(hash_array->buckets == NULL) {
			v_print_log(VRS_PRINT_ERROR, "calloc(): no memory allocated\n");
			hash_array->length = 0;
			hash_array->hash_func = NULL;
			hash_array->key_offset = 0;
			hash_array->key_size = 0;
			hash_array->flags = 0;
			goto end;
		}
		for(i=0; i<hash_array->length; i++) {
			hash_array->buckets[i].vbucket = NULL;
			hash_array->buckets[i].next = NULL;
		}
		hash_array->hash_func = v_hash_func_uint32;
	} else {
		v_print_log(VRS_PRINT_ERROR, "Unsupported hash function\n");
		hash_array->length = 0;
		hash_array->buckets = NULL;
		hash_array->hash_func = NULL;
		hash_array->key_offset = 0;
		hash_array->key_size = 0;
		hash_array->flags = 0;
		hash_array->hash_func = NULL;
		goto end;
	}

	ret = 1;
	hash_array->count = 0;
	hash_array->key_offset = key_offset;
	hash_array->key_size = key_size;
	hash_array->flags = flags;

end:
	pthread_mutex_unlock(&hash_array->mutex);

	return ret;
}
