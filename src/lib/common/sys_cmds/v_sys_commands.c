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

#include <stdio.h>
#include <string.h>

#include "v_sys_commands.h"
#include "v_common.h"
#include "v_network.h"
#include "v_connection.h"
#include "v_unpack.h"

/**
 * \brief Print all system commands in the message to the log file
 */
void v_print_message_sys_cmds(const unsigned char level, const struct VMessage *vmessage)
{
	int i=0;

	while(i < MAX_SYSTEM_COMMAND_COUNT &&
		  vmessage->sys_cmd[i].cmd.id != CMD_RESERVED_ID)
	{
		switch(vmessage->sys_cmd[i].cmd.id) {
		v_print_log_simple(level, "\t");
		case CMD_USER_AUTH_REQUEST:
			v_print_user_auth_request(level,
					(struct User_Authentication_Request*)&vmessage->sys_cmd[i].ua_req);
			break;
		case CMD_USER_AUTH_FAILURE:
			v_print_user_auth_failure(level,
					(struct User_Authentication_Failure*)&vmessage->sys_cmd[i].ua_fail);
			break;
		case CMD_USER_AUTH_SUCCESS:
			v_print_user_auth_success(level,
					(struct User_Authentication_Success*)&vmessage->sys_cmd[i].ua_succ);
			break;
		case CMD_CHANGE_L_ID:
		case CMD_CONFIRM_L_ID:
		case CMD_CHANGE_R_ID:
		case CMD_CONFIRM_R_ID:
			v_print_negotiate_cmd(level,
					(struct Negotiate_Cmd*)&vmessage->sys_cmd[i].negotiate_cmd);
			break;
		}
		v_print_log_simple(level, "\n");
		i++;
	}
}

/**
 * \brief Print all system commands in the packet to the log file
 */
void v_print_packet_sys_cmds(const unsigned char level, const struct VPacket *vpacket)
{
	int i=0;

	while(i < MAX_SYSTEM_COMMAND_COUNT &&
		  vpacket->sys_cmd[i].cmd.id != CMD_RESERVED_ID)
	{
		switch(vpacket->sys_cmd[i].cmd.id) {
			case CMD_ACK_ID:
				v_print_ack_nak_cmd(level, (struct Ack_Nak_Cmd*)&vpacket->sys_cmd[i].ack_cmd);
				break;
			case CMD_NAK_ID:
				v_print_ack_nak_cmd(level, (struct Ack_Nak_Cmd*)&vpacket->sys_cmd[i].nak_cmd);
				break;
			case CMD_CHANGE_L_ID:
			case CMD_CONFIRM_L_ID:
			case CMD_CHANGE_R_ID:
			case CMD_CONFIRM_R_ID:
				v_print_negotiate_cmd(level, (struct Negotiate_Cmd*)&vpacket->sys_cmd[i].negotiate_cmd);
				break;
			default:
				v_print_log(VRS_PRINT_WARNING, "Unknown system command ID: %d\n",
						vpacket->sys_cmd[i].cmd.id);
				break;
		}
		i++;
	}
}

/**
 * \brief	Get system commands from received buffer and store them in VPacket
 * \param[in]	*buffer		The received buffer
 * \param[in]	buffer_len	The size of received buffer
 * \param[out]	*vpacket	The structure of packet, that will be filled with
 * information from the buffer.
 * \return	This function returns relative buffer position of buffer proceeding. */
int v_unpack_packet_system_commands(const char *buffer,
		unsigned short buffer_len,
		struct VPacket *vpacket)
{
	unsigned short not_used=0, buffer_pos=VERSE_PACKET_HEADER_SIZE;
	unsigned char length, cmd_id=CMD_RESERVED_ID;
	int i=0;

	if(buffer_len < VERSE_PACKET_HEADER_SIZE) {
		vpacket->sys_cmd[0].cmd.id = CMD_RESERVED_ID;
		return -1;	/* Corrupted data was received ... buffer can not be proceeded further */
	} else if(buffer_len == VERSE_PACKET_HEADER_SIZE) {
		vpacket->sys_cmd[0].cmd.id = CMD_RESERVED_ID;
		return VERSE_PACKET_HEADER_SIZE;
	} else {
		while(buffer_pos < buffer_len &&
				cmd_id <= MAX_SYS_CMD_ID && /* System command IDs are in range 0-31 */
				i < MAX_SYSTEM_COMMAND_COUNT-1 )
		{
			/* Unpack Command ID */
			not_used += vnp_raw_unpack_uint8(&buffer[buffer_pos], &cmd_id);

			/* Is it still system command or is it node command */
			if(cmd_id>MAX_SYS_CMD_ID) {
				vpacket->sys_cmd[i].cmd.id = CMD_RESERVED_ID;
				break;
			} else {
				vpacket->sys_cmd[i].cmd.id = cmd_id;
				vpacket->sys_cmd[i+1].cmd.id = CMD_RESERVED_ID;
				switch(cmd_id) {
					case CMD_ACK_ID:
						buffer_pos += v_raw_unpack_ack_nak_cmd(&buffer[buffer_pos],
								&vpacket->sys_cmd[i].ack_cmd);
						break;
					case CMD_NAK_ID:
						buffer_pos += v_raw_unpack_ack_nak_cmd(&buffer[buffer_pos],
								&vpacket->sys_cmd[i].nak_cmd);
						break;
					case CMD_CHANGE_L_ID:
					case CMD_CONFIRM_L_ID:
					case CMD_CHANGE_R_ID:
					case CMD_CONFIRM_R_ID:
						buffer_pos += v_raw_unpack_negotiate_cmd(&buffer[buffer_pos],
								buffer_len - buffer_pos,
								&vpacket->sys_cmd[i].negotiate_cmd);
						break;
					default:
						/* This is unknown system command. Unpack length of
						 * the command and skip this command. */
						not_used = vnp_raw_unpack_uint8(&buffer[buffer_pos+1], &length);
						/* Warning print */
						v_print_log(VRS_PRINT_WARNING, "Unknown system command ID: %d, Length: %d\n", cmd_id, length);
						/* Skip this command */
						if(length < (buffer_len - buffer_pos))
							buffer_pos += length;
						else
							buffer_pos = buffer_len;
						break;
				}
			}
			i++;
		}
	}

	return buffer_pos;
}

/**
 * \brief	Get system commands from received buffer and store them in VMessage
 * \param[in]	*buffer		The received buffer
 * \param[in]	buffer_len	The size of received buffer
 * \param[out]	*vmessage	The structure of message, that will be filled with
 * information from the buffer.
 * \return	This function returns relative buffer position of buffer proceeding. */
int v_unpack_message_system_commands(const char *buffer,
		unsigned short buffer_len,
		struct VMessage *vmessage)
{
	unsigned short not_used = 0, buffer_pos = 0;
	unsigned char length, cmd_id=CMD_RESERVED_ID;
	int i=0;

	if(buffer_len<1) {
		vmessage->sys_cmd[0].cmd.id = CMD_RESERVED_ID;
		return 0;	/* Corrupted data was received ... buffer can not be proceeded further */
	} else {
		while(buffer_pos<buffer_len && cmd_id <= MAX_SYS_CMD_ID) {	/* System command IDs are in range 0-31 */
			/* Unpack Command ID */
			not_used += vnp_raw_unpack_uint8(&buffer[buffer_pos], &cmd_id);

			/* Is it still system command or is it node command */
			if(cmd_id>MAX_SYS_CMD_ID) {
				vmessage->sys_cmd[i].cmd.id = CMD_RESERVED_ID;
				break;
			} else {
				vmessage->sys_cmd[i].cmd.id = cmd_id;
				vmessage->sys_cmd[i+1].cmd.id = CMD_RESERVED_ID;
				switch(cmd_id) {
				case CMD_USER_AUTH_REQUEST:
					buffer_pos += v_raw_unpack_user_auth_request(&buffer[buffer_pos],
							buffer_len - buffer_pos,
							&vmessage->sys_cmd[i].ua_req);
					break;
				case CMD_USER_AUTH_FAILURE:
					buffer_pos += v_raw_unpack_user_auth_failure(&buffer[buffer_pos],
							buffer_len - buffer_pos,
							&vmessage->sys_cmd[i].ua_fail);
					break;
				case CMD_USER_AUTH_SUCCESS:
					buffer_pos += v_raw_unpack_user_auth_success(&buffer[buffer_pos],
							buffer_len - buffer_pos,
							&vmessage->sys_cmd[i].ua_succ);
					break;
				case CMD_CHANGE_L_ID:
				case CMD_CONFIRM_L_ID:
				case CMD_CHANGE_R_ID:
				case CMD_CONFIRM_R_ID:
					buffer_pos += v_raw_unpack_negotiate_cmd(&buffer[buffer_pos],
							buffer_len - buffer_pos,
							&vmessage->sys_cmd[i].negotiate_cmd);
					break;
				default:	/* Unknown system command. */
					/* Unpack length of the command */
					not_used = vnp_raw_unpack_uint8(&buffer[buffer_pos+1], &length);
					/* Warning print */
					v_print_log(VRS_PRINT_WARNING, "Unknown system command ID: %d, Length: %d\n", cmd_id, length);
					/* Skip this command */
					if(length < (buffer_len - buffer_pos))
						buffer_pos += length;
					else
						buffer_pos = buffer_len;
					break;
				}
				i++;
			}
		}
	}

	return buffer_pos;
}

/**
 * \brief Pack all system commands stored in VPacket structure to the buffer.
 */
int v_pack_dgram_system_commands(const struct VPacket *vpacket, char *buffer)
{
	unsigned short buffer_pos=0;
	int i=0;

	while(i < MAX_SYSTEM_COMMAND_COUNT &&
		  vpacket->sys_cmd[i].cmd.id != CMD_RESERVED_ID)
	{
		switch(vpacket->sys_cmd[i].cmd.id) {
			case CMD_ACK_ID:
				buffer_pos += v_raw_pack_ack_nak_cmd(&buffer[buffer_pos],
						&vpacket->sys_cmd[i].ack_cmd);
				break;
			case CMD_NAK_ID:
				buffer_pos += v_raw_pack_ack_nak_cmd(&buffer[buffer_pos],
						&vpacket->sys_cmd[i].nak_cmd);
				break;
			case CMD_CHANGE_L_ID:
			case CMD_CONFIRM_L_ID:
			case CMD_CHANGE_R_ID:
			case CMD_CONFIRM_R_ID:
				buffer_pos += v_raw_pack_negotiate_cmd(&buffer[buffer_pos],
						&vpacket->sys_cmd[i].negotiate_cmd);
				break;
		}
		i++;
	}

	return buffer_pos;
}

/**
 * \brief Pack all system commands stored in VPacket structure to the buffer
 * \param[in]	vmessage	The structure with data for message
 * \param[out]	buffer		The buffer, that will be sent to the peer.
 * \return This function return of bytes packed to the buffer.
 */
int v_pack_stream_system_commands(const struct VMessage *vmessage, char *buffer)
{
	unsigned short buffer_pos=0;
	int i=0;

	while(i < MAX_SYSTEM_COMMAND_COUNT &&
		  vmessage->sys_cmd[i].cmd.id != CMD_RESERVED_ID)
	{
		switch(vmessage->sys_cmd[i].cmd.id) {
		case CMD_USER_AUTH_REQUEST:
			buffer_pos += v_raw_pack_user_auth_request(&buffer[buffer_pos],
					&vmessage->sys_cmd[i].ua_req);
			break;
		case CMD_USER_AUTH_FAILURE:
			buffer_pos += v_raw_pack_user_auth_failure(&buffer[buffer_pos],
					&vmessage->sys_cmd[i].ua_fail);
			break;
		case CMD_USER_AUTH_SUCCESS:
			buffer_pos += v_raw_pack_user_auth_success(&buffer[buffer_pos],
					&vmessage->sys_cmd[i].ua_succ);
			break;
		case CMD_CHANGE_L_ID:
		case CMD_CONFIRM_L_ID:
		case CMD_CHANGE_R_ID:
		case CMD_CONFIRM_R_ID:
			buffer_pos += v_raw_pack_negotiate_cmd(&buffer[buffer_pos],
					&vmessage->sys_cmd[i].negotiate_cmd);
			break;
		}
		i++;
	}

	return buffer_pos;
}
