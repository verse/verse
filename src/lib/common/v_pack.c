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

#include <string.h>
#include <limits.h>

#include "verse_types.h"
#include "v_pack.h"

/* Pack string8 to the buffer */
size_t vnp_raw_pack_string8(void *buffer, char *string)
{
	size_t str_len = strlen(string);
	unsigned char i;

	str_len = (str_len > VRS_STRING8_MAX_SIZE) ? VRS_STRING8_MAX_SIZE : str_len;

	/* Pack length of the string */
	*(uint8 *) buffer = str_len;

	/* Pack the string */
	for(i = 0; i < str_len; i++) {
		*((uint8 *) buffer + 1 + i) = string[i];
	}

	return 1 + str_len;
}

/* Pack one byte (one octet) to the buffer */
size_t vnp_raw_pack_uint8(void *buffer, uint8 data)
{
	*(uint8 *) buffer = data;
	return sizeof(data);
}

/* Pack two byte (two octet) to the buffer */
size_t vnp_raw_pack_uint16(void *buffer, uint16 data)
{
	*(uint8 *) buffer = (data & 0xFF00) >> 8;
	*((uint8 *) buffer + 1) = data & 0xFF;
	return sizeof(data);
}

/* Pack four byte (four octet) to the buffer */
size_t vnp_raw_pack_uint32(void *buffer, uint32 data)
{
	register uint8 *b = buffer;
	*b++ = (data >> 24) & 0xFF;
	*b++ = (data >> 16) & 0xFF;
	*b++ = (data >> 8)  & 0xFF;
	*b++ = data & 0xFF;
	return sizeof(data);
}

/* Pack double (eight octet) to the buffer */
size_t vnp_raw_pack_uint64(void *buffer, uint64 data)
{
	union { uint32 uint[2]; uint64 ulong; } punt;
	uint32	size;

	punt.ulong = data;
	size = vnp_raw_pack_uint32(buffer, punt.uint[1]);
	buffer = (uint8 *) buffer + size;
	size += vnp_raw_pack_uint32(buffer, punt.uint[0]);
	return size;
}

/* Pack half-float (two octets) to the buffer */
size_t vnp_raw_pack_real16(void *buffer, real16 data)
{
	union { uint16 uint; real16 real; } punt;
	punt.real = data;
	return vnp_raw_pack_uint16(buffer, punt.uint);
}

/* Pack float (four octets) to the buffer */
size_t vnp_raw_pack_real32(void *buffer, real32 data)
{
	union { uint32 uint; real32 real; } punt;
	punt.real = data;
	return vnp_raw_pack_uint32(buffer, punt.uint);
}

/* Pack double (eight octets) to the buffer */
size_t vnp_raw_pack_real64(void *buffer, real64 data)
{
	union { uint32 uint[2]; real64 real; } punt;
	uint32	size;

	punt.real = data;
	size = vnp_raw_pack_uint32(buffer, punt.uint[1]);
	buffer = (uint8 *) buffer + size;
	size += vnp_raw_pack_uint32(buffer, punt.uint[0]);
	return size;
}
