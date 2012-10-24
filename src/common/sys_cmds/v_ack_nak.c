/*
 * $Id$
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

#include "v_sys_commands.h"
#include "v_commands.h"
#include "v_common.h"
#include "v_pack.h"
#include "v_unpack.h"

/* Debug print of ACK command */
void v_print_ack_nak_cmd(const unsigned char level, struct Ack_Nak_Cmd *ack_nak_cmd)
{
	if(ack_nak_cmd->id==CMD_ACK_ID) {
		v_print_log_simple(level, "\tACK COMMAND, %d\n", ack_nak_cmd->pay_id);
	}
	else if(ack_nak_cmd->id==CMD_NAK_ID){
		v_print_log_simple(level, "\tNAK COMMAND, %d\n", ack_nak_cmd->pay_id);
	}
	else {
		v_print_log_simple(level, "\tAckNak COMMAND: bad command id: %d\n", ack_nak_cmd->pay_id);
	}
}

/* Unpack ACK or NAK system command */
int v_raw_unpack_ack_nak_cmd(const char *buffer, struct Ack_Nak_Cmd *ack_nak_cmd)
{
	unsigned short buffer_pos = 0;

	/* Unpack command ID */
	buffer_pos += vnp_raw_unpack_uint8(&buffer[buffer_pos], &ack_nak_cmd->id);
	/* Unpack ID of acknowledged packet */
	buffer_pos += vnp_raw_unpack_uint32(&buffer[buffer_pos], &ack_nak_cmd->pay_id);

	return buffer_pos;
}

/* Pack ACK or NAK command to the buffer */
int v_raw_pack_ack_nak_cmd(char *buffer, const struct Ack_Nak_Cmd *ack_nak_cmd)
{
	int buffer_pos = 0;

	/* Pack command ID first */
	buffer_pos += vnp_raw_pack_uint8(&buffer[buffer_pos], ack_nak_cmd->id);
	/* Pack ID of payload packet */
	buffer_pos += vnp_raw_pack_uint32(&buffer[buffer_pos], ack_nak_cmd->pay_id);

	return buffer_pos;
}
