/*
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Contributor(s): Jiri Hnidek <jiri.hnidek@tul.cz>.
 *
 */

#include <check.h>

#include "v_common.h"
#include "v_pack.h"


#define UINT8_BUF_SIZE		8
#define UINT16_BUF_SIZE		16
#define REAL64_BUF_SIZE		64


/**
 * \brief Unit test of packing uint8 values
 */
START_TEST ( test_Pack_Uint8 )
{
	size_t buf_pos = 0;
	unsigned char buffer[UINT8_BUF_SIZE] = {0, };
	unsigned char expected_results[UINT8_BUF_SIZE] = {
			0x00,	/* 0 */
			0x01,	/* 1 */
			0x02,	/* 2 */
			0x04,	/* 4 */
			0x10,	/* 16 */
			0x17,	/* 23 */
			0xff,	/* 255 */
			0
	};
	int i;

	buf_pos += vnp_raw_pack_uint8((void*)&buffer[buf_pos], 0);
	buf_pos += vnp_raw_pack_uint8((void*)&buffer[buf_pos], 1);
	buf_pos += vnp_raw_pack_uint8((void*)&buffer[buf_pos], 2);
	buf_pos += vnp_raw_pack_uint8((void*)&buffer[buf_pos], 4);
	buf_pos += vnp_raw_pack_uint8((void*)&buffer[buf_pos], 16);
	buf_pos += vnp_raw_pack_uint8((void*)&buffer[buf_pos], 23);
	buf_pos += vnp_raw_pack_uint8((void*)&buffer[buf_pos], 255);

	fail_unless( buf_pos == 7,
			"Size of uint8 buffer: %d != 7",
			buf_pos);

	for(i = 0; i < UINT8_BUF_SIZE; i++) {
		fail_unless( buffer[i] == expected_results[i],
			"Buffer of uint8 differs at position: %d (%d != %d)",
			i, buffer[i], expected_results[i]);
	}
}
END_TEST


/**
 * \brief Unit test of packing uint8 values
 */
START_TEST ( test_Pack_Uint16 )
{
	size_t buf_pos = 0;
	unsigned char buffer[UINT16_BUF_SIZE] = {0, };
	unsigned char expected_results[UINT16_BUF_SIZE] = {
			0x00, 0x00,	/* 0 */
			0x00, 0x01,	/* 1 */
			0x02, 0x00,	/* 512 */
			0x04, 0x06,	/* 1030 */
			0xff, 0xff,	/* 65535 */
			0,
	};
	int i;

	buf_pos += vnp_raw_pack_uint16((void*)&buffer[buf_pos], 0);
	buf_pos += vnp_raw_pack_uint16((void*)&buffer[buf_pos], 1);
	buf_pos += vnp_raw_pack_uint16((void*)&buffer[buf_pos], 512);
	buf_pos += vnp_raw_pack_uint16((void*)&buffer[buf_pos], 1030);
	buf_pos += vnp_raw_pack_uint16((void*)&buffer[buf_pos], 65535);

	fail_unless( buf_pos == 10,
			"Size of uint16 buffer: %d != 10",
			buf_pos);

	for(i = 0; i < UINT16_BUF_SIZE; i++) {
		fail_unless( buffer[i] == expected_results[i],
			"Buffer of uint16 differs at position: %d (%d != %d)",
			i, buffer[i], expected_results[i]);
	}
}
END_TEST


/**
 * \brief Unit test of packing real64 (double) values
 */
START_TEST ( test_Pack_Real64 )
{
	size_t buf_pos = 0;
	unsigned char buffer[REAL64_BUF_SIZE] = {0, };
	unsigned char expected_results[REAL64_BUF_SIZE] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/*  0.0 */
		0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*  1.0 */
		0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/*  2.0 */
		0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* -2.0 */
		0x3f, 0xd5, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,	/*  1/3 */
	};
	int i;

	buf_pos += vnp_raw_pack_real64((void*)&buffer[buf_pos], 0);
	buf_pos += vnp_raw_pack_real64((void*)&buffer[buf_pos], 1.0);
	buf_pos += vnp_raw_pack_real64((void*)&buffer[buf_pos], 2.0);
	buf_pos += vnp_raw_pack_real64((void*)&buffer[buf_pos], -2.0);
	buf_pos += vnp_raw_pack_real64((void*)&buffer[buf_pos], 1.0/3.0);

	fail_unless( buf_pos == 5*8,
			"Size of real64 buffer: %d != 16 (bytes)",
			buf_pos);

	for(i = 0; i < REAL64_BUF_SIZE; i++) {
		fail_unless( buffer[i] == expected_results[i],
							"Buffer of real64 differs at position: %d",
							i);
	}
}
END_TEST

/**
 * \brief This function creates test suite for Node_Destroy command
 */
struct Suite *pack_suite(void)
{
	struct Suite *suite = suite_create("Pack");
	struct TCase *tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_Pack_Uint8);
	tcase_add_test(tc_core, test_Pack_Uint16);
	tcase_add_test(tc_core, test_Pack_Real64);

	suite_add_tcase(suite, tc_core);

	return suite;
}
