/*
 * $Id$
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2011, TODO: Your Name
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
 * Authors: TODO: Your Name <your@email.com>
 *
 */

#include <stdio.h>
#include <assert.h>

/* TODO: replace with real .h file including definition of command structure */
#include "v_command_template.h"

#include "v_in_queue.h"
#include "v_common.h"
#include "v_unpack.h"
#include "v_pack.h"
#include "v_network.h"

/*
 * This is template of source code for command. Replace following strings with
 * some real strings:
 *
 * CMD_COMMAND_NAME
 * Command_Name
 * Command_Name_Cmd
 * command_name
 * COMMAND_NAME_CMD_LEN
 * COMMAND_NAME_CMD_DATA_LEN
 *
 * Parts requiring some complicated changes are marked with "TODO:". Modern IDE
 * can highlight these lines :-).
 *
 * Note: this template can't be used for all commands. For example: commands
 * with variable length (e.g.: containing strings) require special approach.
 */

#if 0

/**
 * \brief Print content of structure containing Command_Name command
 */
void v_command_name_print(const unsigned char level,
		struct Command_Name_Cmd *command_name)
{
	v_print_log_simple(level, "\tCOMMAND_NAME, NodeID: %d\n",
			command_name->addr.node_id /*, TODO: other items */);
}

/**
 * \brief This function computes the length of compressed commands
 */
uint16 v_command_name_cmds_len(uint16 count)
{
	uint16 data_len;

	data_len = count*1/* TODO: replace with real size of data items */;

	if( (1+1+data_len) < 0xFF ) {
		return 1 + 1 + data_len;
	} else {
		return 1 + 1 + 2 + data_len;
	}
}

/**
 * \brief Unpack Command_Name command from the buffer of the received packet
 */
int v_command_name_unpack(const char *buffer,
		const ssize_t buffer_size,
		struct VInQueue *v_in_queue)
{
	struct Command_Name_Cmd *command_name = NULL;
	uint16 buffer_pos = 0;
	uint8 id, cmd_addr_len;
	uint16 length;

	/* Is the remaining size of buffer big enough for Command_Name command */
	if(buffer_size < COMMAND_NAME_CMD_LEN) {
		v_print_log(VRS_PRINT_WARNING, "Buffer size: %d < minimal command length: %d, skipping rest of packet.\n",
				buffer_size, COMMAND_NAME_CMD_LEN);
		return buffer_size;
	}

	/* Unpack ID */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &id);
	/* Assert command id */
	assert(id == CMD_COMMAND_NAME);

	/* Unpack length of the command */
	buffer_pos += v_cmd_unpack_len(&buffer[buffer_pos], &length, &cmd_addr_len);

	/* Check length of the command */
	if( ((length - cmd_addr_len) % COMMAND_NAME_CMD_DATA_LEN) != 0 ) {
		v_print_log(VRS_PRINT_WARNING, "Bad length: %d != %d of Command_Name command.\n", length, COMMAND_NAME_CMD_LEN);
		return length;
	}

	for(i = 0; i< ((length - cmd_addr_len) / COMMAND_NAME_CMD_DATA_LEN); i++) {

		/* TODO: Add unpacking code here */

		/* Create Command_Name command */
		command_name = v_command_name_create(/* unpacked values */);

		/* Push command to the queue of incoming commands */
		v_in_queue_push(v_in_queue, (struct Generic_Cmd*)command_name);

		/* Print content of received command */
		if(is_log_level(VRS_PRINT_DEBUG_MSG)) {
			printf("%c[%d;%dm", 27, 1, 34);
			v_command_name_print(VRS_PRINT_DEBUG_MSG, command_name);
			printf("%c[%dm", 27, 0);
		}
	}


	/* Buffer_pos has to be equal to command length. */
	assert(buffer_pos == length);

	return buffer_pos;
}

/**
 * \brief Pack Command_Name command to the buffer
 */
int v_command_name_pack(char *buffer,
		const struct Command_Name_Cmd *command_name,
		const uint16 length)
{
	uint16 buffer_pos = 0;

	if(length != 0) {
		/* Pack Command ID */
		buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], CMD_COMMAND_NAME);

		/* Pack length of the command */
		buffer_pos += v_cmd_pack_len(&buffer[buffer_pos], length);

		/* TODO: Pack rest of the command */

	} else {
		/* TODO: Pack rest of the command */
	}

	return buffer_pos;
}

/**
 * \brief This function initialize command queue with Command_Name commands
 */
struct VCommandQueue *v_command_name_cmd_queue(uint16 flag)
{
	struct VCommandQueue *cmd_queue = NULL;
	struct Command_Name_Cmd command_name;

	cmd_queue = (struct VCommandQueue*)calloc(1, sizeof(struct VCommandQueue));
	cmd_queue->item_size = sizeof(struct Command_Name_Cmd);
	cmd_queue->flag = 0;
	v_hash_array_init(&cmd_queue->cmds,
			HASH_MOD_256 | flag,
			(char*)&(command_name.addr) - (char*)&(command_name),
			sizeof(struct Command_Name_Addr));

	return cmd_queue;
}

/**
 * \brief This function initialize structure of command
 */
void v_command_name_init(struct Command_Name_Cmd *command_name /*, TODO: values of items */ )
{
	if(command_name != NULL) {
		/* TODO: initialize members with values */
	}
}

/**
 * \brief This function creates new structure of command
 */
struct Command_Name_Cmd *v_command_name_create(/* TODO: values */)
{
	struct Command_Name_Cmd *command_name = NULL;
	command_name = (struct Command_Name_Cmd*)calloc(1, sizeof(struct Command_Name_Cmd));
	v_command_name_init(command_name /*, TODO: values */);
	return command_name;
}

/**
 * \brief This function clear structure of command. It should free allocated
 * memory inside this structure.
 */
void v_command_name_clear(struct Command_Name_Cmd *command_name)
{
	if(command_name != NULL) {
		/* TODO: set to default values and free members, when it's needed */
	}
}

/**
 * \brief This function destroy structure of command
 */
void v_command_name_destroy(struct Command_Name_Cmd **command_name)
{
	if(command_name != NULL) {
		v_command_name_clear(*command_name);
		free(*command_name);
		*command_name = NULL;
	}
}

#endif
