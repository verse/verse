/*
 * $Id: v_sys_commands.h 1268 2012-07-24 08:14:52Z jiri $
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

#if !defined V_COMMAND_H
#define V_COMMAND_H

#include <sys/types.h>
#include <limits.h>

#include "verse_types.h"

/* System Commands (IDs are in range 0-31). These commands can not be added to the
 * packet directly by client application*/
#define MIN_SYS_CMD_ID			0
#define MAX_SYS_CMD_ID			31

/* Connection feature IDs */
#define FTR_RSV_ID				0	/* Reserved feature ID */
#define FTR_FC_ID				1	/* Flow Control feature ID */
#define FTR_CC_ID				2	/* Congestion Control feature ID */
#define FTR_HOST_URL			3	/* Host URL defined in RFC 1738 */
#define FTR_COOKIE				4	/* Cookie used as identifier of Verse session */
#define FTR_DED					5	/* Data Exchange Definition */
#define FTR_RWIN_SCALE			6	/* Scale factor of rwin used for Flow Control */
#define FTR_FPS					7	/* FPS currently used at client */
#define FTR_CMD_COMPRESS		8	/* Command compression */

/* Minimal and maximal length of negotiate command */
#define MIN_FTR_CMD_LEN			3
#define MAX_FTR_CMD_LEN			48

/* Minimal and maximal count of values in negotiate command */
#define MIN_FTR_VALUE_COUNT		0
#define MAX_FTR_VALUE_COUNT		5

/* Methods of Flow Control */
#define FC_RESERVED				0	/* Should never be used */
#define FC_NONE					1
#define FC_TCP_LIKE				2

/* Methods of Congestion Control */
#define CC_RESERVED				0	/* Should never be used */
#define CC_NONE					1
#define CC_TCP_LIKE				2

/* Methods of Command Compression */
#define CMPR_RESERVED			0	/* Should never be used */
#define CMPR_NONE				1
#define CMPR_ADDR_SHARE			2


/* Following commands are real system commands, that are packed to the packets
 * and messages. On the other side these commands are never added to the
 * queues.
 */

/* Data structure for USER_AUTH_SUCCESS system command */
typedef struct User_Authentication_Success {
	uint8		 	id;				/* ID of command = (9) */
	uint16			user_id;		/* Unique ID of the user at Verse server */
	uint32			avatar_id;		/* Unique ID of session of this verse client */
} User_Authentication_Success;

/* Data structure for USER_AUTH_FAILURE system command */
typedef struct User_Authentication_Failure {
	uint8			id;				/* ID of command = (8) */
	uint8			count;			/* Count of supported user authentication methods */
	uint8			method[VRS_MAX_UA_METHOD_COUNT];	/* List of supported methods */
} User_Authentication_Failure;

/* Data structure for USER_AUTH_REQUEST system command */
typedef struct User_Authentication_Request {
	uint8			id;						/* ID of command = (7) */
	char			username[VRS_MAX_USERNAME_LENGTH];		/* User name */
	uint8			method_type;			/* Type of authentication*/
	char			data[VRS_MAX_DATA_LENGTH];	/* Data for user authentication method */
} User_Authentication_Request;

/* Data structure for CHANGE_L, CHANGE_R, CONFIRM_L and CONFIRM_R commands */
typedef struct Negotiate_Cmd {
	uint8			id;				/* ID of command = (3, 4, 5, 6) */
	uint8			count;			/* Count of values in preference list. This item
									   is not part of transfered command and count is
									   transformed to length of command during send proccess. */
	uint8			feature;		/* Feature ID */
	union {
		unsigned char	uint8;
		unsigned short	uint16;
		unsigned int	uint32;
		float			real32;
		string8			string8;
	} value[MAX_FTR_VALUE_COUNT];	/* Preference list of values */
} Negotiate_Cmd;

/* Data structure for ACK and NAK commands */
typedef struct Ack_Nak_Cmd {
	uint8			id;				/* ID of command = (1, 2) */
	uint32			pay_id;			/* ID of payload packet */
} Ack_Nak_Cmd;

struct VPacket;
struct VMessage;

int v_add_negotiate_cmd(struct VPacket *packet,
		uint8 cmd_rank,
		uint8 cmd_op_code,
		uint8 ftr_op_code,
		...);

void v_print_user_auth_success(const unsigned char level, struct User_Authentication_Success *ua_suc);
int v_raw_unpack_user_auth_success(const char *buffer, ssize_t buffer_size, struct User_Authentication_Success *ua_suc);
int v_raw_pack_user_auth_success(char *buffer, const struct User_Authentication_Success *ua_suc);

void v_print_user_auth_failure(const unsigned char level, struct User_Authentication_Failure *ua_fail);
int v_raw_unpack_user_auth_failure(const char *buffer, ssize_t buffer_size, struct User_Authentication_Failure *ua_fail);
int v_raw_pack_user_auth_failure(char *buffer, const struct User_Authentication_Failure *ua_fail);

void v_print_user_auth_request(const unsigned char level, struct User_Authentication_Request *ua_request_cmd);
int v_raw_unpack_user_auth_request(const char *buffer, ssize_t buffer_size, struct User_Authentication_Request *ua_request_cmd);
int v_raw_pack_user_auth_request(char *buffer, const struct User_Authentication_Request *ua_request_cmd);

void v_print_negotiate_cmd(const unsigned char level, struct Negotiate_Cmd *negotiate_cmd);
int v_raw_unpack_negotiate_cmd(const char *buffer, ssize_t buffer_size, struct Negotiate_Cmd *negotiate_cmd);
int v_raw_pack_negotiate_cmd(char *buffer, const struct Negotiate_Cmd *negotiate_cmd);

void v_print_ack_nak_cmd(const unsigned char level, struct Ack_Nak_Cmd *ack_nak_cmd);
int v_raw_unpack_ack_nak_cmd(const char *buffer, struct Ack_Nak_Cmd *ack_nak_cmd);
int v_raw_pack_ack_nak_cmd(char *buffer, const struct Ack_Nak_Cmd *ack_nak_cmd);

void v_print_packet_sys_cmds(const unsigned char level, const struct VPacket *vpacket);
void v_print_message_sys_cmds(const unsigned char level, const struct VMessage *vmessage);

int v_unpack_packet_system_commands(const char *buffer, unsigned short buffer_len, struct VPacket *vpacket);
int v_unpack_message_system_commands(const char *buffer, unsigned short buffer_len, struct VMessage *vmessage);

int v_pack_dgram_system_commands(const struct VPacket *vpacket, char *buffer);
int v_pack_stream_system_commands(const struct VMessage *vmessage, char *buffer);

#endif
