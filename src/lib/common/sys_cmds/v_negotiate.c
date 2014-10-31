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
 * authors: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "v_sys_commands.h"
#include "v_commands.h"
#include "v_common.h"
#include "v_pack.h"
#include "v_unpack.h"
#include "v_network.h"

/**
 * \brief Add next system command to the structure of array of union of
 * system commands
 *
 * \param[out]	packet		The pointer at packet, where negotiate commands will be added
 * \param[in]	cmd_rank	The rank of negotiate of command sequence
 * \param[in]	cmd_op_code	The OpCode of negotiate command
 * \param[in]	ftr_op_code	The OpCode of features that is negotiated
 * \param[in]	...			Arguments with pointers at values terminated with NULL pointer
 */
int v_add_negotiate_cmd(union VSystemCommands *sys_cmds,
		uint8 cmd_rank,
		uint8 cmd_op_code,
		uint8 ftr_op_code,
		...)
{
	va_list args;
	void *value;
	unsigned char ftr_rank = 0;
	size_t str_len;

	/* This need not be here, but check it. */
	if( !(cmd_op_code == CMD_CHANGE_L_ID ||
			cmd_op_code == CMD_CHANGE_R_ID ||
			cmd_op_code == CMD_CONFIRM_L_ID ||
			cmd_op_code == CMD_CONFIRM_R_ID) )
	{
		v_print_log(VRS_PRINT_WARNING, "This is not negotiation command\n");
		return 0;
	}

	/* Add command OpCode */
	sys_cmds[cmd_rank].negotiate_cmd.id = cmd_op_code;
	/* Add OpCode of features */
	sys_cmds[cmd_rank].negotiate_cmd.feature = ftr_op_code;

	/* Start of varargs */
	va_start(args, ftr_op_code);

	while( (value = va_arg(args, void*)) != NULL) {
		switch(ftr_op_code) {
		case FTR_FC_ID:
		case FTR_CC_ID:
		case FTR_RWIN_SCALE:
		case FTR_CMD_COMPRESS:
			/* Add unsigned char value */
			sys_cmds[cmd_rank].negotiate_cmd.value[ftr_rank].uint8 = *(uint8*)value;
			break;
		case FTR_HOST_URL:
		case FTR_TOKEN:
		case FTR_DED:
		case FTR_CLIENT_NAME:
		case FTR_CLIENT_VERSION:
			/* Add string */
			str_len = strlen((char*)value);
			str_len = (str_len > VRS_STRING8_MAX_SIZE) ? VRS_STRING8_MAX_SIZE : str_len;
			sys_cmds[cmd_rank].negotiate_cmd.value[ftr_rank].string8.length = str_len;
			strncpy((char*)sys_cmds[cmd_rank].negotiate_cmd.value[ftr_rank].string8.str,
					(char*)value,
					str_len);
			((char*)sys_cmds[cmd_rank].negotiate_cmd.value[ftr_rank].string8.str)[str_len] = '\0';
			break;
		case FTR_FPS:
			/* Add float value */
			sys_cmds[cmd_rank].negotiate_cmd.value[ftr_rank].real32 = *(real32*)value;
			break;
		default:
			/* When unsupported feature*/
			sys_cmds[cmd_rank].cmd.id = CMD_RESERVED_ID;
			va_end(args);
			return 0;
		}
		ftr_rank++;
	}

	/* End of varargs */
	va_end(args);

	/* Add count of values to the command */
	sys_cmds[cmd_rank].negotiate_cmd.count = ftr_rank;

	/* Add terminating fake command */
	sys_cmds[cmd_rank + 1].cmd.id = CMD_RESERVED_ID;

	return 1;
}

/**
 * \brief This function prints negotiate commands: Change_L/R and Confirm_L/R
 */
void v_print_negotiate_cmd(const unsigned char level, struct Negotiate_Cmd *negotiate_cmd)
{
	int i;

	switch(negotiate_cmd->id) {
		case CMD_CHANGE_L_ID:
			v_print_log_simple(level, "\tCHANGE_L COMMAND, ");
			break;
		case CMD_CONFIRM_L_ID:
			v_print_log_simple(level, "\tCONFIRM_L COMMAND, ");
			break;
		case CMD_CHANGE_R_ID:
			v_print_log_simple(level, "\tCHANGE_R COMMAND, ");
			break;
		case CMD_CONFIRM_R_ID:
			v_print_log_simple(level, "\tCONFIRM_R COMMAND, ");
			break;
		default:
			v_print_log_simple(level, "\tNEGOTIATE DEBUG PRINT ERROR: bad command id\n");
			break;
	}

	v_print_log_simple(level, "count: %d, ", negotiate_cmd->count);

	switch(negotiate_cmd->feature) {
		case FTR_FC_ID:
			v_print_log_simple(level, "feature: FC_ID, ");
			break;
		case FTR_CC_ID:
			v_print_log_simple(level, "feature: CC_ID, ");
			break;
		case FTR_HOST_URL:
			v_print_log_simple(level, "feature: HOST_URL, ");
			break;
		case FTR_TOKEN:
			v_print_log_simple(level, "feature: TOKEN, ");
			break;
		case FTR_DED:
			v_print_log_simple(level, "feature: DED, ");
			break;
		case FTR_RWIN_SCALE:
			v_print_log_simple(level, "feature: RWIN_SCALE, ");
			break;
		case FTR_FPS:
			v_print_log_simple(level, "feature: FPS, ");
			break;
		case FTR_CMD_COMPRESS:
			v_print_log_simple(level, "feature: CMD_COMPRESS, ");
			break;
		case FTR_CLIENT_NAME:
			v_print_log_simple(level, "feature: CLIENT_NAME, ");
			break;
		case FTR_CLIENT_VERSION:
			v_print_log_simple(level, "feature: CLIENT_VERSION, ");
			break;
		default:
			v_print_log_simple(level, "unknown feature, ");
			break;
	}

	for(i = 0; i < negotiate_cmd->count; i++) {
		switch(negotiate_cmd->feature) {
			case FTR_FC_ID:
			case FTR_CC_ID:
			case FTR_RWIN_SCALE:
			case FTR_CMD_COMPRESS:
				v_print_log_simple(level, "%d, ",
						negotiate_cmd->value[i].uint8);
				break;
			case FTR_HOST_URL:
			case FTR_TOKEN:
			case FTR_DED:
			case FTR_CLIENT_NAME:
			case FTR_CLIENT_VERSION:
				v_print_log_simple(level, "%d:%s, ",
						negotiate_cmd->value[i].string8.length,
						negotiate_cmd->value[i].string8.str);
				break;
			case FTR_FPS:
				v_print_log_simple(level, "%6.3f, ",
						negotiate_cmd->value[i].real32);
				break;
			default:
				break;
		}
	}
	v_print_log_simple(level, "\n");
}

/**
 * \brief This function unpack negotiate commands: Change_L/R and Confirm_L/R
 * from the buffer
 *
 * Unpack negotiate command (CHANGE_L, CONFIRM_L, CHANGE_R, CONFIRM_R). Buffer
 * size has to be at least 2 bytes long (this check has to be done before
 * calling this function)
 */
int v_raw_unpack_negotiate_cmd(const char *buffer, ssize_t buffer_size,
		struct Negotiate_Cmd *negotiate_cmd)
{
	int str_len = 0;
	unsigned short buffer_pos = 0, cmd_length, i;
	unsigned char cmd_header_len,length8 = 0, len8, len_of_len;

	/* Unpack command ID */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &negotiate_cmd->id);

	/* Unpack Length of values */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &length8);

	/* Check if the length of the command is stored in the second byte of
	 * command or is it stored in two bytes after second byte of the command */
	if(length8 == 0xFF) {
		buffer_pos += vnp_raw_unpack_uint16(&buffer[buffer_pos], &cmd_length);
		len_of_len = 3;
	} else {
		cmd_length = length8;
		len_of_len = 1;
	}

	cmd_header_len = 1 + len_of_len + 1;

	/* Security check: check if the length of the command is not bigger
	 * then buffer_size. If this test failed, then return buffer_size and set
	 * count of received values to the zero. */
	if(buffer_size < cmd_length) {
		v_print_log(VRS_PRINT_WARNING,
				"Buffer size: %d < command length: %d.\n",
				buffer_size, cmd_length);
		negotiate_cmd->count = 0;
		return buffer_size;
	/* Security check: check if the length of the command is equal or bigger
	 * then minimal length of the negotiate command: ID_len + Length_len +
	 * Feature_len*/
	} else if(cmd_length < cmd_header_len) {
		v_print_log(VRS_PRINT_WARNING,
				"Command length: %d < Command header length: %d.\n",
				cmd_length, cmd_header_len);
		negotiate_cmd->count = 0;
		return buffer_size;
	}

	/* Unpack Feature ID */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos],
			&negotiate_cmd->feature);

	/* Compute count of values in preference list. When unknown or unsupported
	 * feature is detected, then processing of this command is stopped and it
	 * returns the length of command. */
	switch(negotiate_cmd->feature) {
		case FTR_RSV_ID:
			v_print_log(VRS_PRINT_WARNING, "Received RESERVED feature ID\n");
			negotiate_cmd->count = 0;
			/* This feature id should never be sent or received */
			return cmd_length;
		case FTR_FC_ID:
		case FTR_CC_ID:
		case FTR_RWIN_SCALE:
		case FTR_CMD_COMPRESS:
			negotiate_cmd->count = cmd_length - cmd_header_len;
			break;
		case FTR_HOST_URL:
		case FTR_TOKEN:
		case FTR_DED:
		case FTR_CLIENT_NAME:
		case FTR_CLIENT_VERSION:
			negotiate_cmd->count = 0;
			while((buffer_pos + str_len) < (cmd_length - cmd_header_len)) {
				vnp_raw_unpack_uint8(&buffer[buffer_pos + str_len], &len8);
				str_len += 1 + len8;
				negotiate_cmd->count++;
			}
			break;
		case FTR_FPS:
			negotiate_cmd->count = (cmd_length - cmd_header_len)/4;
			break;
		default:
			v_print_log(VRS_PRINT_WARNING, "Received UNKNOWN feature ID\n");
			negotiate_cmd->count = 0;
			return cmd_length;
	}

	/* Unpack values (preference list) */
	for(i = 0; i < negotiate_cmd->count; i++) {
		switch(negotiate_cmd->feature) {
			case FTR_FC_ID:
			case FTR_CC_ID:
			case FTR_RWIN_SCALE:
			case FTR_CMD_COMPRESS:
				buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos],
						&negotiate_cmd->value[i].uint8);
				break;
			case FTR_HOST_URL:
			case FTR_TOKEN:
			case FTR_DED:
			case FTR_CLIENT_NAME:
			case FTR_CLIENT_VERSION:
				buffer_pos += vnp_raw_unpack_string8_to_string8(&buffer[buffer_pos],
						buffer_size-buffer_pos,
						&negotiate_cmd->value[i].string8);
				break;
			case FTR_FPS:
				buffer_pos += vnp_raw_unpack_real32(&buffer[buffer_pos],
						&negotiate_cmd->value[i].real32);
				break;
		}
	}

	/* Check if length and buffer_pos match */
	if(buffer_pos != cmd_length) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "%s: buffer_pos: %d != length: %d\n",
				__FUNCTION__, buffer_pos, cmd_length);
		return cmd_length;
	}

	return cmd_length;
}

/**
 * \brief This function packs negotiate commands: Change_L/R and Confirm_L/R
 * to the buffer
 */
int v_raw_pack_negotiate_cmd(char *buffer,
		const struct Negotiate_Cmd *negotiate_cmd)
{
	unsigned short length = 0, buffer_pos = 0;
	unsigned int i;

	/* Add some assertations */
	assert( (negotiate_cmd->id == CMD_CHANGE_L_ID ||
			negotiate_cmd->id == CMD_CHANGE_R_ID ||
			negotiate_cmd->id == CMD_CONFIRM_L_ID ||
			negotiate_cmd->id == CMD_CONFIRM_R_ID) );

	assert( (negotiate_cmd->feature == FTR_FC_ID ||
		negotiate_cmd->feature == FTR_CC_ID ||
		negotiate_cmd->feature == FTR_HOST_URL ||
		negotiate_cmd->feature == FTR_TOKEN ||
		negotiate_cmd->feature == FTR_DED ||
		negotiate_cmd->feature == FTR_RWIN_SCALE ||
		negotiate_cmd->feature == FTR_FPS ||
		negotiate_cmd->feature == FTR_CMD_COMPRESS ||
		negotiate_cmd->feature == FTR_CLIENT_NAME ||
		negotiate_cmd->feature == FTR_CLIENT_VERSION) );

	/* Pack command ID first */
	buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], negotiate_cmd->id);

	/* Pack length of command */
	switch(negotiate_cmd->feature) {
		case FTR_FC_ID:
		case FTR_CC_ID:
		case FTR_RWIN_SCALE:
		case FTR_CMD_COMPRESS:
			/* CommandID + Length + FeatureID + features */
			length = 1 + 1 + 1 + negotiate_cmd->count * sizeof(uint8);
			break;
		case FTR_HOST_URL:
		case FTR_TOKEN:
		case FTR_DED:
		case FTR_CLIENT_NAME:
		case FTR_CLIENT_VERSION:
			length = 1 + 1 + 1;	/* CommandID + Length + FeatureID */
			for(i = 0; i < negotiate_cmd->count; i++) {
				/* + String length + String */
				length += 1 + (sizeof(unsigned char)) * negotiate_cmd->value[i].string8.length;
			}
			break;
		case FTR_FPS:
			/* CommandID + Length + FeatureID + features */
			length = 1 + 1 + 1 + negotiate_cmd->count * sizeof(real32);
			break;
	}

	/* When length o command is bigger then 255, then length will be stored
	 * at 3 bytes (not only one byte) */
	if(length >= 0xFF) {
		length += 2;
	}

	/* Pack length of the command */
	buffer_pos += v_cmd_pack_len(&buffer[buffer_pos], length);

	/* Pack negotiated feature */
	buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos],
			negotiate_cmd->feature);

	/* Pack values of preference list */
	for(i = 0; i < negotiate_cmd->count; i++) {
		switch(negotiate_cmd->feature) {
			case FTR_FC_ID:
			case FTR_CC_ID:
			case FTR_RWIN_SCALE:
			case FTR_CMD_COMPRESS:
				buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos],
						negotiate_cmd->value[i].uint8);
				break;
			case FTR_HOST_URL:
			case FTR_TOKEN:
			case FTR_DED:
			case FTR_CLIENT_NAME:
			case FTR_CLIENT_VERSION:
				buffer_pos += vnp_raw_pack_string8(&buffer[buffer_pos],
						(char*)negotiate_cmd->value[i].string8.str);
				break;
			case FTR_FPS:
				buffer_pos += vnp_raw_pack_real32(&buffer[buffer_pos],
						negotiate_cmd->value[i].real32);
				break;
		}
	}

	/* Check if length and buffer_pos match */
	assert(buffer_pos == length);

	return buffer_pos;
}
