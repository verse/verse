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

#ifndef V_COMMANDS_H_
#define V_COMMANDS_H_

#include "verse_types.h"

/* Reserver ID for command. Such ID should never be used for real command. */
#define CMD_RESERVED_ID				0

/* Two system commands, that are used for positive and negative acknowledgment */
#define CMD_ACK_ID					1
#define CMD_NAK_ID					2

/* Four system command used for negotiation of features */
#define CMD_CHANGE_L_ID				3
#define CMD_CHANGE_R_ID				4
#define CMD_CONFIRM_L_ID			5
#define CMD_CONFIRM_R_ID			6

/* Three system commands used for user authentication */
#define CMD_USER_AUTH_REQUEST		7
#define CMD_USER_AUTH_FAILURE		8
#define CMD_USER_AUTH_SUCCESS		9

/* Basic Node Commands */
#define CMD_NODE_CREATE				32
#define CMD_NODE_DESTROY			33
#define CMD_NODE_SUBSCRIBE			34
#define CMD_NODE_UNSUBSCRIBE		35

/* Node Parent */
#define CMD_NODE_LINK				37

/* Node Permission*/
#define CMD_NODE_PERMISSION			38
#define CMD_DEFAULT_PERMISSION		39
#define CMD_NODE_OWNER				40

/* Node Locks */
#define CMD_NODE_LOCK				41
#define CMD_NODE_UNLOCK				42

/* Node Priority */
#define CMD_NODE_PRIORITY			43

/* TagGroup Commands */
#define CMD_TAGGROUP_CREATE			64
#define CMD_TAGGROUP_DESTROY		65
#define CMD_TAGGROUP_SUBSCRIBE		66
#define CMD_TAGGROUP_UNSUBSCRIBE	67

/* Tag Commands */
#define CMD_TAG_CREATE				68
#define CMD_TAG_DESTROY				69

/* Tag Set UInt8 */
#define CMD_TAG_SET_UINT8			70
#define CMD_TAG_SET_VEC2_UINT8		71
#define CMD_TAG_SET_VEC3_UINT8		72
#define CMD_TAG_SET_VEC4_UINT8		73

/* Tag Set UInt16 */
#define CMD_TAG_SET_UINT16			74
#define CMD_TAG_SET_VEC2_UINT16		75
#define CMD_TAG_SET_VEC3_UINT16		76
#define CMD_TAG_SET_VEC4_UINT16		77

/* Tag Set UInt32 */
#define CMD_TAG_SET_UINT32			78
#define CMD_TAG_SET_VEC2_UINT32		79
#define CMD_TAG_SET_VEC3_UINT32		80
#define CMD_TAG_SET_VEC4_UINT32		81

/* Tag Set UInt64 */
#define CMD_TAG_SET_UINT64			82
#define CMD_TAG_SET_VEC2_UINT64		83
#define CMD_TAG_SET_VEC3_UINT64		84
#define CMD_TAG_SET_VEC4_UINT64		85

/* Tag Set Real16 */
#define CMD_TAG_SET_REAL16			86
#define CMD_TAG_SET_VEC2_REAL16		87
#define CMD_TAG_SET_VEC3_REAL16		88
#define CMD_TAG_SET_VEC4_REAL16		89

/* Tag Set Real32 */
#define CMD_TAG_SET_REAL32			90
#define CMD_TAG_SET_VEC2_REAL32		91
#define CMD_TAG_SET_VEC3_REAL32		92
#define CMD_TAG_SET_VEC4_REAL32		93

/* Tag Set Real64 */
#define CMD_TAG_SET_REAL64			94
#define CMD_TAG_SET_VEC2_REAL64		95
#define CMD_TAG_SET_VEC3_REAL64		96
#define CMD_TAG_SET_VEC4_REAL64		97

/* Tag Set String8 */
#define CMD_TAG_SET_STRING8			98


#define CMD_LAYER_CREATE			128
#define CMD_LAYER_DESTROY			129
#define CMD_LAYER_SUBSCRIBE			130
#define CMD_LAYER_UNSUBSCRIBE		131

#define CMD_LAYER_UNSET_VALUE		132

#define CMD_LAYER_SET_UINT8			133
#define CMD_LAYER_SET_VEC2_UINT8	134
#define CMD_LAYER_SET_VEC3_UINT8	135
#define CMD_LAYER_SET_VEC4_UINT8	136

#define CMD_LAYER_SET_UINT16		137
#define CMD_LAYER_SET_VEC2_UINT16	138
#define CMD_LAYER_SET_VEC3_UINT16	139
#define CMD_LAYER_SET_VEC4_UINT16	140

#define CMD_LAYER_SET_UINT32		141
#define CMD_LAYER_SET_VEC2_UINT32	142
#define CMD_LAYER_SET_VEC3_UINT32	143
#define CMD_LAYER_SET_VEC4_UINT32	144

#define CMD_LAYER_SET_UINT64		145
#define CMD_LAYER_SET_VEC2_UINT64	146
#define CMD_LAYER_SET_VEC3_UINT64	147
#define CMD_LAYER_SET_VEC4_UINT64	148

#define CMD_LAYER_SET_REAL16		149
#define CMD_LAYER_SET_VEC2_REAL16	150
#define CMD_LAYER_SET_VEC3_REAL16	151
#define CMD_LAYER_SET_VEC4_REAL16	152

#define CMD_LAYER_SET_REAL32		153
#define CMD_LAYER_SET_VEC2_REAL32	154
#define CMD_LAYER_SET_VEC3_REAL32	155
#define CMD_LAYER_SET_VEC4_REAL32	156

#define CMD_LAYER_SET_REAL64		157
#define CMD_LAYER_SET_VEC2_REAL64	158
#define CMD_LAYER_SET_VEC3_REAL64	159
#define CMD_LAYER_SET_VEC4_REAL64	160

/* Maximal theoretical number of command ID */
#define MIN_CMD_ID					32
#define MAX_CMD_ID					255

#define MAX_CMD_ITEMS				16

#define INT8_SIZE		(sizeof(int8))
#define UINT8_SIZE		(sizeof(uint8))
#define INT16_SIZE		(sizeof(int16))
#define UINT16_SIZE		(sizeof(uint16))
#define INT32_SIZE		(sizeof(int32))
#define UINT32_SIZE		(sizeof(uint32))
#define INT64_SIZE		(sizeof(int64))
#define UINT64_SIZE		(sizeof(uint64))
#define REAL16_SIZE		(sizeof(real16))
#define REAL32_SIZE		(sizeof(real32))
#define REAL64_SIZE		(sizeof(real64))
#define STRING8_SIZE	(sizeof(char*))

typedef enum Cmd_Item_Type {
	ITEM_RESERVED,
	ITEM_INT8,
	ITEM_UINT8,
	ITEM_INT16,
	ITEM_UINT16,
	ITEM_INT32,
	ITEM_UINT32,
	ITEM_INT64,
	ITEM_UINT64,
	ITEM_REAL16,
	ITEM_REAL32,
	ITEM_REAL64,
	ITEM_STRING8
} Cmd_Item_Type;

/**
 * Structure of command item
 */
typedef struct Cmd_Item {
	enum Cmd_Item_Type	type;
	uint8				size;
	uint8				offset;
	char				*name;
} Cmd_Item;

/**
 * Cmd_Struct flags
 */
#define NODE_CMD	1
#define FAKE_CMD	2
#define SHARE_ADDR	4
#define REM_DUP		8
#define VAR_LEN		16

/**
 * This structure is used for description of command structure
 */
typedef struct Cmd_Struct {
	uint8			id;			/* ID of command */
	uint8			flag;		/* Is it valid command */
	uint8			key_size;	/* Size of address */
	size_t			size;		/* Size of command in memory (Addr+Data)*/
	size_t			cmd_size;	/* Minimal size of command in packet */
	uint8			item_count;	/* Count of command items */
	uint8			key_count;	/* Count of command items that are part of address */
	char			*name;		/* Name of command */
	struct Cmd_Item	items[MAX_CMD_ITEMS];	/* Own items */
} Cmd_Struct;

/**
 * Generic Verse Command
 *
 * The trick with one size array is to make C compilers happy. The purpose of
 * following code is to create following structure in memory. First byte is
 * OpCode of command. Following bytes are own data of command.
 *
 * 0        8
 * +--------+--------+--------+-..  ..-+--------+--------+
 * | OpCode |                    Data                    |
 * +--------+--------+--------+-..  ..-+--------+--------+
 */
typedef struct Generic_Cmd {
	uint8			id;			/**< OpCode of command */
	uint8			data[1];	/**< Own data of command. */
} Generic_Cmd;

#define INT8(data)   *(int8*)&(data)
#define UINT8(data)  *(uint8*)&(data)
#define INT16(data)  *(int16*)&(data)
#define UINT16(data) *(uint16*)&(data)
#define INT32(data)  *(int32*)&(data)
#define UINT32(data) *(uint32*)&(data)
#define INT64(data)  *(int64*)&(data)
#define UINT64(data) *(uint64*)&(data)
#define REAL16(data) *(real16*)&(data)
#define REAL32(data) *(real32*)&(data)
#define REAL64(data) *(real64*)&(data)
#define PTR(data)    *(void**)&(data)

struct VInQueue;

uint16 v_cmd_unpack_len(const char *buffer,
		uint16 *length,
		uint8 *cmd_addr_len);
uint16 v_cmd_pack_len(char *buffer, const uint16 length);

void v_cmd_print(const unsigned char level,
		const struct Generic_Cmd *cmd);
void v_cmd_destroy(struct Generic_Cmd **cmd);
int v_cmd_struct_size(const struct Generic_Cmd *cmd);
int v_cmd_size(const struct Generic_Cmd *cmd);
int v_cmd_pack(char *buffer,
		const struct Generic_Cmd *cmd,
		const uint16 length,
		const uint8 type);
int v_cmd_unpack(const char *buffer,
		unsigned short buffer_len,
		struct VInQueue *v_in_queue);

uint8 v_cmd_cmp_addr(struct Generic_Cmd *cmd1,
		struct Generic_Cmd *cmd2,
		const uint8 current_size);
uint16 v_cmd_count(struct Generic_Cmd *cmd,
		uint16 max_len,
		uint8 share);
uint16 v_cmds_len(struct Generic_Cmd *cmd,
		uint16 count,
		uint8 share,
		uint16 len);
struct VCommandQueue *v_node_cmd_queue_create(uint8 id, uint8 copy_bucket);


#endif /* V_COMMANDS_H_ */
