/*
 * $Id$
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

#include <stdio.h>

/* TODO: add some real include here
#include "v_object.h"
*/

/**
 * \brief This function initialize structure of object
 */
void v_Object_init(struct Object *obj, /* values of members */ )
{
	if(obj != NULL) {
		/* TODO: initialize members with values */
	}
}

/**
 * \brief This function creates new structure of object
 */
struct Object *v_Object_create(/* TODO: values */)
{
	struct Object *obj = NULL;
	obj = (struct Object*)calloc(1, sizeof(struct Object));
	v_Object_init(obj /*, TODO: values */);
	return obj;
}

/**
 * \brief This function clear structure of object. It should free allocated
 * memory inside this structure.
 */
void v_Object_clear(struct Object *obj)
{
	if(obj != NULL) {
		/* TODO: free members, when it's needed */
	}
}

/**
 * \brief This function destroy structure of object
 */
void v_Object_destroy(struct Object **obj)
{
	if(obj != NULL) {
		v_Object_clear(*obj);
		free(*obj);
		*obj = NULL;
	}
}

