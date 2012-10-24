/*
 * $Id$
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

#ifndef V_COMMAND_H_
#define V_COMMAND_H_

#include <stdio.h>
#include <sys/types.h>

#include "verse_types.h"

#define COMMAND_NAME_CMD_DATA_LEN		(4)
#define COMMAND_NAME_CMD_LEN			(1+1+COMMAND_NAME_CMD_DATA_LEN)

/* Unique address of Node Destroy Command */
typedef struct Command_Name_Addr {
	/* TODO: add here some items */
} Command_Name_Addr;

/* Data structure used for queuing */
typedef struct Command_Name_Cmd {
	uint8						id;
	struct Command_Name_Addr	addr;
	/* TODO: add here some items */
} Command_Name_Cmd;

void v_command_name_print(const unsigned char level,
		struct Command_Name_Cmd *command_name);
uint16 v_command_name_cmds_len(uint16 count);
int v_command_name_unpack(const char *buffer,
		const ssize_t buffer_size,
		struct VInQueue *v_in_queue);
int v_command_name_pack(char *buffer,
		const struct Command_Name_Cmd *command_name,
		const uint16 length);
void v_command_name_init(struct Command_Name_Cmd *command_name /*, TODO: values of items */ );
struct Command_Name_Cmd *v_command_name_create(/* TODO: values */);
void v_command_name_clear(struct Command_Name_Cmd *command_name);
void v_command_name_destroy(struct Command_Name_Cmd **command_name);

#endif /* V_COMMAND_H_ */
