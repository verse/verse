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

#include <check.h>

#include "v_common.h"
#include "v_commands.h"
#include "v_sys_commands.h"
#include "v_in_queue.h"
#include "v_out_queue.h"


#define TEST_VEC_SIZE	1


#if 0
/**
 * Structure holding test values of negotiation command
 */
typedef struct NEG_cmd_values {
	uint8	opcode;
	uint8	length;
	uint8	feature;
	uint8	values[256];
} NEG_cmd_values;

static struct NEG_cmd_values cmd_values[TEST_VEC_SIZE] = {
	{
		CMD_CHANGE_L_ID,
		4,
		FTR_FC_ID,
		{
			FC_NONE, 0
		}
	}
};
#endif

/**
 * \brief Test simple adding negotiate command to the list of system commands
 */
START_TEST ( test_add_negotiate_cmd_single_value )
{
	union VSystemCommands sys_cmd[1];
	uint8 cmd_op_code = CMD_CHANGE_L_ID;
	uint8 ftr_op_code = FTR_FC_ID;
	uint8 value = FC_NONE;
	int ret;

	v_add_negotiate_cmd(sys_cmd, 0, cmd_op_code, ftr_op_code, &value, NULL);

	fail_unless( ret != 1,
			"Adding negotiate command failed");
	fail_unless( sys_cmd->negotiate_cmd.id == cmd_op_code,
			"Negotiate command OpCode: %d != %d",
			sys_cmd->negotiate_cmd.id, cmd_op_code);
	fail_unless( sys_cmd->negotiate_cmd.feature == ftr_op_code,
			"Negotiate command feature: %d != %d",
			sys_cmd->negotiate_cmd.feature, ftr_op_code);
	fail_unless( sys_cmd->negotiate_cmd.count == 1,
			"Negotiate command feature count: %d != %d",
			sys_cmd->negotiate_cmd.count, 1);
	fail_unless( sys_cmd->negotiate_cmd.value[0].uint8 == value,
			"Negotiate command value: %d != %d",
			sys_cmd->negotiate_cmd.value[0].uint8, value);

}
END_TEST

/**
 * \brief Test of adding negotiate command with multiple values
 * to the list of system commands
 */
START_TEST ( test_add_negotiate_cmd_multiple_values )
{
	union VSystemCommands sys_cmd[2];
	uint8 cmd_op_code = CMD_CHANGE_L_ID;
	uint8 ftr_op_code = FTR_FC_ID;
	uint8 values[2] = {FC_TCP_LIKE, FC_NONE};
	int ret;

	v_add_negotiate_cmd(sys_cmd, 0, cmd_op_code, ftr_op_code,
			&values[0], &values[1], NULL);

	fail_unless( ret != 1,
			"Adding negotiate command failed");
	fail_unless( sys_cmd->negotiate_cmd.id == cmd_op_code,
			"Negotiate command OpCode: %d != %d",
			sys_cmd->negotiate_cmd.id, cmd_op_code);
	fail_unless( sys_cmd->negotiate_cmd.feature == ftr_op_code,
			"Negotiate command feature: %d != %d",
			sys_cmd->negotiate_cmd.feature, ftr_op_code);
	fail_unless( sys_cmd->negotiate_cmd.count == 2,
			"Negotiate command feature count: %d != %d",
			sys_cmd->negotiate_cmd.count, 2);
	fail_unless( sys_cmd->negotiate_cmd.value[0].uint8 == values[0],
			"Negotiate command value[0]: %d != %d",
			sys_cmd->negotiate_cmd.value[0].uint8, values[0]);
	fail_unless( sys_cmd->negotiate_cmd.value[1].uint8 == values[1],
			"Negotiate command value[1]: %d != %d",
			sys_cmd->negotiate_cmd.value[1].uint8, values[1]);

}
END_TEST


/**
 * \brief Test simple packing and unpacking of negotiate command
 */
START_TEST ( test_pack_unpack_negotiate_cmd_single_value )
{
	union VSystemCommands send_sys_cmd[1], recv_sys_cmd[1];
	uint8 cmd_op_code = CMD_CHANGE_L_ID;
	uint8 ftr_op_code = FTR_FC_ID;
	uint8 value = FC_NONE;
	char buffer[255];
	int buffer_pos = 0, cmd_len;

	/* Create negotiate command */
	v_add_negotiate_cmd(send_sys_cmd, 0, cmd_op_code, ftr_op_code,
			&value, NULL);

	/* Pack negotiate command */
	buffer_pos += v_raw_pack_negotiate_cmd(buffer,
			&send_sys_cmd[0].negotiate_cmd);

	fail_unless( buffer_pos == 4,
			"Length of packed cmd: %d != %d",
			buffer_pos, 4);

	/* Unpack system command */
	cmd_len = v_raw_unpack_negotiate_cmd(buffer, buffer_pos,
			&recv_sys_cmd[0].negotiate_cmd);

	fail_unless( cmd_len == buffer_pos,
			"Length of packed and unpacked cmd: %d != %d",
			buffer_pos, cmd_len);
	fail_unless( recv_sys_cmd->negotiate_cmd.id == cmd_op_code,
			"Negotiate command OpCode: %d != %d",
			recv_sys_cmd->negotiate_cmd.id, cmd_op_code);
	fail_unless( recv_sys_cmd->negotiate_cmd.feature == ftr_op_code,
			"Negotiate command feature: %d != %d",
			recv_sys_cmd->negotiate_cmd.feature, ftr_op_code);
	fail_unless( recv_sys_cmd->negotiate_cmd.count == 1,
			"Negotiate command feature count: %d != %d",
			recv_sys_cmd->negotiate_cmd.count, 1);
	fail_unless( recv_sys_cmd->negotiate_cmd.value[0].uint8 == value,
			"Negotiate command value: %d != %d",
			recv_sys_cmd->negotiate_cmd.value[0].uint8, value);
}
END_TEST


/**
 * \brief Test packing and unpacking of negotiate command with multiple values
 */
START_TEST ( test_pack_unpack_negotiate_cmd_multiple_values )
{
	union VSystemCommands send_sys_cmd[1], recv_sys_cmd[1];
	uint8 cmd_op_code = CMD_CHANGE_L_ID;
	uint8 ftr_op_code = FTR_FC_ID;
	uint8 values[2] = {FC_TCP_LIKE, FC_NONE};
	char buffer[255];
	int buffer_pos = 0, cmd_len;

	/* Create negotiate command */
	v_add_negotiate_cmd(send_sys_cmd, 0, cmd_op_code, ftr_op_code,
			&values[0], &values[1], NULL);

	/* Pack negotiate command */
	buffer_pos += v_raw_pack_negotiate_cmd(buffer,
			&send_sys_cmd[0].negotiate_cmd);

	fail_unless( buffer_pos == 5,
			"Length of packed cmd: %d != %d",
			buffer_pos, 5);

	/* Unpack system command */
	cmd_len = v_raw_unpack_negotiate_cmd(buffer, buffer_pos,
			&recv_sys_cmd[0].negotiate_cmd);

	fail_unless( cmd_len == buffer_pos,
			"Length of packed and unpacked cmd: %d != %d",
			buffer_pos, cmd_len);
	fail_unless( recv_sys_cmd->negotiate_cmd.id == cmd_op_code,
			"Negotiate command OpCode: %d != %d",
			recv_sys_cmd->negotiate_cmd.id, cmd_op_code);
	fail_unless( recv_sys_cmd->negotiate_cmd.feature == ftr_op_code,
			"Negotiate command feature: %d != %d",
			recv_sys_cmd->negotiate_cmd.feature, ftr_op_code);
	fail_unless( recv_sys_cmd->negotiate_cmd.count == 2,
			"Negotiate command feature count: %d != %d",
			recv_sys_cmd->negotiate_cmd.count, 2);
	fail_unless( recv_sys_cmd->negotiate_cmd.value[0].uint8 == values[0],
			"Negotiate command value[0]: %d != %d",
			recv_sys_cmd->negotiate_cmd.value[0].uint8, values[0]);
	fail_unless( recv_sys_cmd->negotiate_cmd.value[1].uint8 == values[1],
			"Negotiate command value[1]: %d != %d",
			recv_sys_cmd->negotiate_cmd.value[1].uint8, values[1]);
}
END_TEST

/**
 * \brief This function creates test suite for Node_Create command
 */
struct Suite *negotiate_suite(void)
{
	struct Suite *suite = suite_create("Negotiate_Cmd");
	struct TCase *tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_add_negotiate_cmd_single_value);
	tcase_add_test(tc_core, test_add_negotiate_cmd_multiple_values);
	tcase_add_test(tc_core, test_pack_unpack_negotiate_cmd_single_value);
	tcase_add_test(tc_core, test_pack_unpack_negotiate_cmd_multiple_values);

	suite_add_tcase(suite, tc_core);

	return suite;
}


