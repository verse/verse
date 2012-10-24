/*
 * $Id: v_unpack.c 1268 2012-07-24 08:14:52Z jiri $
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

#include "verse_types.h"
#include "v_unpack.h"

#include <stdlib.h>

/* Following functions are used for unpacking basic data types from
 * received packet. All multi-byte quantities are transmitted in network
 * byte order (most significant byte first). Most of following functions
 * use trick with registers (values in registers are always in big
 * endian format on all platforms). It is reason of platform independence
 * of following code. */

/**
 * \brief Unpack byte (one octect) from buffer (uint8)
 */
size_t vnp_raw_unpack_uint8(const void *buffer, uint8 *data)
{
	*data = *(uint8 *) buffer;
	return sizeof *data;
}

/**
 * \brief Unpack two bytes (two octets) from buffer (uint16)
 */
size_t vnp_raw_unpack_uint16(const void *buffer, uint16 *data)
{
	register const uint8 *b = buffer;
	register uint16	tmp;
	tmp = ((uint16) *b++) << 8;
	tmp |= (uint16) *b;
	*data = tmp;
	return sizeof *data;
}

/* Unpack four bytes (four octets) from buffer (int32)*/
size_t vnp_raw_unpack_uint32(const void *buffer, uint32 *data)
{
	register const uint8	*b = buffer;
	*data  = ((uint32) *b++) << 24;
	*data |= ((uint32) *b++) << 16;
	*data |= ((uint32) *b++) << 8;
	*data |= *b;
	return sizeof *data;
}

/* Unpack eight bytes from buffer */
size_t vnp_raw_unpack_uint64(const void *buffer, uint64 *data)
{
	union { uint32 uint[2]; uint64 ulong; } punt;
	uint32	size;
	size =  vnp_raw_unpack_uint32(buffer, &punt.uint[0]);
	size += vnp_raw_unpack_uint32(((uint8 *)buffer) + size, &punt.uint[1]);
	*data = punt.ulong;
	return size;
}

/* Unpack float value */
size_t vnp_raw_unpack_real16(const void *buffer, real16 *data)
{
	return vnp_raw_unpack_uint16(buffer, (uint16 *) data);
}

/* Unpack float value */
size_t vnp_raw_unpack_real32(const void *buffer, real32 *data)
{
	return vnp_raw_unpack_uint32(buffer, (uint32 *) data);
}

/* Unpack double value */
size_t vnp_raw_unpack_real64(const void *buffer, real64 *data)
{
	union { uint32 uint[2]; real64 real; } punt;
	uint32	size;
	size =  vnp_raw_unpack_uint32(buffer, &punt.uint[0]);
	size += vnp_raw_unpack_uint32(((uint8 *)buffer) + size, &punt.uint[1]);
	*data = punt.real;
	return size;
}

/**
 * \brief		Unpack string8 from the buffer
 * \details		This function check if it is possible to unpack string8 from the buffer,
 * 				because minimal length of string8 has to be 2, then data are copied to the
 * 				structure.
 * \param[in]	*buffer			The received buffer
 * \param[in]	*buffer_size	The remaining size of buffer, that could be processed
 * \param[out]	*str			The pointer at string8. Informations from buffer are
 * 								copied to this variable.
 * \return		This function return size of unpacked data in bytes.
 */
size_t vnp_raw_unpack_string8_(const void *buffer, const size_t buffer_size, struct string8 *data)
{
	uint32 i, size=0;

	/* Check if buffer_size is bigger then minimal length of string8 */
	if(buffer_size<2) {
		return buffer_size;
	}

	/* Unpack length of the command */
	size += vnp_raw_unpack_uint8(((uint8 *)buffer) + size, &data->length);

	/* Crop length of the string, when length of the string is
	 * bigger then available buffer */
	if(data->length > buffer_size) {
		data->length = buffer_size;
	}

	for(i=0; i<data->length; i++) {
		size += vnp_raw_unpack_uint8(((uint8 *)buffer) + size, &data->str[i]);
	}
	/* Make from received string real string terminated with '\0' character. */
	data->str[data->length] = '\0';

	return size;
}

/**
 * \brief		Unpack string8 from the buffer
 * \details		This function check if it is possible to unpack string8 from the buffer,
 * 				because minimal length of string8 has to be 2, then data are copied to the
 * 				structure.
 * \param[in]	*buffer			The received buffer
 * \param[in]	*buffer_size	The remaining size of buffer, that could be processed
 * \param[out]	str[VRS_STRING8_MAX_SIZE+1]			The pointer at string8. Informations from buffer are
 * 								copied to this variable.
 * \return		This function return size of unpacked data in bytes.
 */
size_t vnp_raw_unpack_string8(const char *buffer,
		const size_t buffer_size,
		char **str)
{
	char *string8;
	uint32 i, buffer_pos=0;
	uint8 length;

	/* Check if buffer_size is bigger then minimal length of string8 */
	if(buffer_size<2) {
		return buffer_size;
	}

	/* Unpack length of the command */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &length);

	/* Crop length of the string, when length of the string is
	 * bigger then available buffer */
	if(length > buffer_size) {
		length = buffer_size;
	}

	*str = string8 = (char*)malloc((length+1)*sizeof(char));

	for(i=0; i<length; i++) {
		buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], (uint8*)&string8[i]);
	}
	string8[length] = '\0';

	return buffer_pos;
}
