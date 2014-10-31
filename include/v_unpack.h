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
 */

#if !defined V_UNPACK_H
#define V_UNPACK_H

#include <stddef.h>

#include "verse_types.h"

size_t vnp_raw_unpack_uint8(const void *buffer, uint8 *data);
size_t vnp_raw_unpack_uint16(const void *buffer, uint16 *data);
size_t vnp_raw_unpack_uint32(const void *buffer, uint32 *data);
size_t vnp_raw_unpack_uint64(const void *buffer, uint64 *data);
size_t vnp_raw_unpack_real16(const void *buffer, real16 *data);
size_t vnp_raw_unpack_real32(const void *buffer, real32 *data);
size_t vnp_raw_unpack_real64(const void *buffer, real64 *data);

size_t vnp_raw_unpack_string8_to_string8(const void *buffer, const size_t buffer_size, struct string8 *data);
size_t vnp_raw_unpack_string8_to_str(const char *buffer, const size_t buffer_size, char **str);

#endif

