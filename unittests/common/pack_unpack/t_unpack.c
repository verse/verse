/*
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2014, Jiri Hnidek
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

#include <check.h>

#include "v_unpack.h"


#define UINT8_BUF_SIZE		8
#define UINT8_TV_SIZE		8

#define UINT16_BUF_SIZE		16
#define UINT16_TV_SIZE		8

#define UINT32_BUF_SIZE		32
#define UINT32_TV_SIZE		8

#define UINT64_BUF_SIZE		64
#define UINT64_TV_SIZE		8

#define REAL32_BUF_SIZE		32
#define REAL32_TV_SIZE		8

#define REAL64_BUF_SIZE		64
#define REAL64_TV_SIZE		8


/**
 * \brief Unit test of packing uint8 values
 */
START_TEST ( test_UnPack_Uint8 )
{
	size_t buf_pos = 0;
	unsigned char buffer[UINT8_BUF_SIZE] = {
			0x00,	/* 0 */
			0x01,	/* 1 */
			0x02,	/* 2 */
			0x04,	/* 4 */
			0x10,	/* 16 */
			0x17,	/* 23 */
			0xff,	/* 255 */
			0,
	};
	uint8 expected_results[UINT8_TV_SIZE] = {
			0,	/* 0 */
			1,	/* 1 */
			2,	/* 2 */
			4,	/* 4 */
			16,	/* 16 */
			23,	/* 23 */
			255,	/* 255 */
			0,
	};
	uint8 results[UINT8_TV_SIZE] = {0,};
	int i = 0;

	buf_pos += vnp_raw_unpack_uint8((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint8((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint8((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint8((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint8((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint8((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint8((void*)&buffer[buf_pos],
			&results[i++]);

	fail_unless( buf_pos == 7*1,
			"Size of uint8 buffer: %d != 7",
			buf_pos);

	for(i = 0; i < UINT8_TV_SIZE; i++) {
		fail_unless( results[i] == expected_results[i],
				"Test vector of uint8 differs at position: %d (%d != %d)",
				i, results[i], expected_results[i]);
	}
}
END_TEST


/**
 * \brief Unit test of packing uint16 values
 */
START_TEST ( test_UnPack_Uint16 )
{
	size_t buf_pos = 0;
	unsigned char buffer[UINT16_BUF_SIZE] = {
			0x00, 0x00,	/* 0 */
			0x00, 0x01,	/* 1 */
			0x02, 0x00,	/* 512 */
			0x04, 0x06,	/* 1030 */
			0xff, 0xff,	/* 65535 */
			0,
	};
	uint16 expected_results[UINT16_TV_SIZE] = {
			0,
			1,
			512,
			1030,
			65535,
			0,
	};
	uint16 results[UINT16_TV_SIZE] = {0,};
	int i = 0;

	buf_pos += vnp_raw_unpack_uint16((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint16((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint16((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint16((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint16((void*)&buffer[buf_pos],
			&results[i++]);

	fail_unless( buf_pos == 5*2,
			"Size of uint16 buffer: %d != 10",
			buf_pos);

	for(i = 0; i < UINT16_TV_SIZE; i++) {
		fail_unless( results[i] == expected_results[i],
				"Test vector of uint16 differs at position: %d (%d != %d)",
				i, results[i], expected_results[i]);
	}
}
END_TEST


/**
 * \brief Unit test of packing uint32 values
 */
START_TEST ( test_UnPack_Uint32 )
{
	size_t buf_pos = 0;
	unsigned char buffer[UINT32_BUF_SIZE] = {
			0x00, 0x00, 0x00, 0x00,	/* 0 */
			0x00, 0x00, 0x00, 0x01,	/* 1 */
			0x00, 0x00, 0x02, 0x00,	/* 512 */
			0x00, 0x01, 0x00, 0x04,	/* 65540 */
			0x01, 0x00, 0x00, 0x00,	/* 16 777 216 */
			0xff, 0xff, 0xff, 0xff,	/* 4 294 967 295 */
			0,
	};
	uint32 expected_results[UINT32_TV_SIZE] = {
			0,
			1,
			512,
			65540,
			16777216,
			4294967295,
			0,
	};
	uint32 results[UINT32_TV_SIZE] = {0,};
	int i = 0;

	buf_pos += vnp_raw_unpack_uint32((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint32((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint32((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint32((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint32((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint32((void*)&buffer[buf_pos],
			&results[i++]);

	fail_unless( buf_pos == 6*4,
			"Size of uint32 buffer: %d != 24",
			buf_pos);

	for(i = 0; i < UINT32_TV_SIZE; i++) {
		fail_unless( results[i] == expected_results[i],
				"Test vector of uint32 differs at position: %d (%d != %d)",
				i, results[i], expected_results[i]);
	}
}
END_TEST


/**
 * \brief Unit test of packing uint64 values
 */
START_TEST ( test_UnPack_Uint64 )
{
	size_t buf_pos = 0;
	unsigned char buffer[UINT64_BUF_SIZE] = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0 */
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,	/* 1 */
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,	/* 512 */
			0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04,	/* 65540 */
			0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,	/* 4 294 967 296 */
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	/* 18 446 744 073 709 551 615 */
			0,
	};
	uint64 expected_results[UINT64_TV_SIZE] = {
			0,
			1,
			512,
			65540,
			4294967296,
			0xffffffffffffffff,
			0,
	};
	uint64 results[UINT64_TV_SIZE] = {0,};
	int i = 0;

	buf_pos += vnp_raw_unpack_uint64((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint64((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint64((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint64((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint64((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_uint64((void*)&buffer[buf_pos],
			&results[i++]);

	fail_unless( buf_pos == 6*8,
			"Size of uint64 buffer: %d != 48",
			buf_pos);

	for(i = 0; i < UINT64_TV_SIZE; i++) {
		fail_unless( results[i] == expected_results[i],
				"Test vector of uint64 differs at position: %d (%d != %d)",
				i, results[i], expected_results[i]);
	}
}
END_TEST


/**
 * \brief Unit test of packing real32 values
 */
START_TEST ( test_UnPack_Real32 )
{
	size_t buf_pos = 0;
	unsigned char buffer[REAL32_BUF_SIZE] = {
			0x00, 0x00, 0x00, 0x00,	/*  0.0 */
			0x40, 0x00, 0x00, 0x01, /*  2.00000024 */
			0x40, 0x00, 0x00, 0x00, /*  2.0 */
			0xc0, 0x00, 0x00, 0x00, /* -2.0 */
			0x3f, 0xff, 0xff, 0xfe, /*  1.99999976 */
			0,
	};
	real32 expected_results[REAL32_TV_SIZE] = {
			0.0,
			2.00000024,
			2.0,
			-2.0,
			1.99999976,
			0,
	};
	real32 results[REAL32_TV_SIZE] = {0,};
	int i = 0;

	buf_pos += vnp_raw_unpack_real32((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_real32((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_real32((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_real32((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_real32((void*)&buffer[buf_pos],
			&results[i++]);

	fail_unless( buf_pos == 5*4,
			"Size of real32 buffer: %d != 20",
			buf_pos);

	for(i = 0; i < REAL32_TV_SIZE; i++) {
		fail_unless( results[i] == expected_results[i],
				"Test vector of real32 differs at position: %d (%f != %f)",
				i, results[i], expected_results[i]);
	}
}
END_TEST


/**
 * \brief Unit test of packing real64 values
 */
START_TEST ( test_UnPack_Real64 )
{
	size_t buf_pos = 0;
	unsigned char buffer[REAL64_BUF_SIZE] = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/*  0.0 */
			0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*  1.0 */
			0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/*  2.0 */
			0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* -2.0 */
			0x3f, 0xd5, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,	/*  1/3 */
			0,
	};
	real64 expected_results[REAL64_TV_SIZE] = {
			0.0,
			1.0,
			2.0,
			-2.0,
			1.0/3.0,
			0,
	};
	real64 results[REAL64_TV_SIZE] = {0,};
	int i = 0;

	buf_pos += vnp_raw_unpack_real64((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_real64((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_real64((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_real64((void*)&buffer[buf_pos],
			&results[i++]);
	buf_pos += vnp_raw_unpack_real64((void*)&buffer[buf_pos],
			&results[i++]);

	fail_unless( buf_pos == 5*8,
			"Size of real64 buffer: %d != 40",
			buf_pos);

	for(i = 0; i < REAL64_TV_SIZE; i++) {
		fail_unless( results[i] == expected_results[i],
				"Test vector of real64 differs at position: %d (%f != %f)",
				i, results[i], expected_results[i]);
	}
}
END_TEST


/**
 * \brief This function creates test suite for Node_Destroy command
 */
struct Suite *unpack_suite(void)
{
	struct Suite *suite = suite_create("UnPack");
	struct TCase *tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_UnPack_Uint8);
	tcase_add_test(tc_core, test_UnPack_Uint16);
	tcase_add_test(tc_core, test_UnPack_Uint32);
	tcase_add_test(tc_core, test_UnPack_Uint64);
	tcase_add_test(tc_core, test_UnPack_Real32);
	tcase_add_test(tc_core, test_UnPack_Real64);

	suite_add_tcase(suite, tc_core);

	return suite;
}
