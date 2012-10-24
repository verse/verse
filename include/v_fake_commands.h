/*
 * $Id: v_fake_commands.h 1288 2012-08-05 20:34:43Z jiri $
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

#include "verse_types.h"

#ifndef V_FAKE_COMMANDS_H_
#define V_FAKE_COMMANDS_H_

/* Fake commands that are used at client (such commands are never sent)
 * The largest fake command ID used at client could be 15. ID equal to 16 is
 * the first fake command used at server. */
#define FAKE_CMD_CONNECT_ACCEPT			0
#define FAKE_CMD_CONNECT_TERMINATE		1
#define FAKE_CMD_USER_AUTHENTICATE		2
#define FAKE_CMD_FPS					3
#define FAKE_CMD_SECURITY_INFO			4

/* Fake commands that are used at server (such commands are never sent).
 * The largest fake command ID used at client could be 31. ID equal to 32 is
 * the first real command used for communication. */
#define FAKE_CMD_NODE_CREATE_ACK		16
#define FAKE_CMD_NODE_DESTROY_ACK		17
#define FAKE_CMD_NODE_LOCK_ACK			18
#define FAKE_CMD_NODE_UNLOCK_ACK		19
#define FAKE_CMD_TAGGROUP_CREATE_ACK	20
#define FAKE_CMD_TAGGROUP_DESTROY_ACK	21
#define FAKE_CMD_TAG_CREATE_ACK			22
#define FAKE_CMD_TAG_DESTROY_ACK		23
#define FAKE_CMD_LAYER_CREATE_ACK		24
#define FAKE_CMD_LAYER_DESTROY_ACK		25

/* -------------- Fake Client Commands -------------- */

/* Following commands are not send to the peer. Clients use is it for storing
 * data for callback functions after handshake or teardown. */

/**
 * Structure for storing data of ConnectAccept callback function in incoming
 * queue
 */
typedef struct Connect_Accept_Cmd {
	/* Fake cmd_id for compatibility with node commands */
	uint8						id;
	/* Connect accept does not have any unique address */
	/* Payload data */
	uint16						user_id;
	uint32						avatar_id;
} Connect_Accept_Cmd;

/**
 * Structure for storing data of connect termination in incoming and outgoing
 * queue
 */
typedef struct Connect_Terminate_Cmd {
	/* Fake cmd_id for compatibility with node commands */
	uint8						id;
	/* Connect terminate does not have any unique address */
	/* Payload data */
	uint8						error_num;
} Connect_Terminate_Cmd;

/**
 * Structure for storing data of user authentication in incoming and outgoing
 * queue
 */
typedef struct User_Authenticate_Cmd {
	/* Fake cmd_id for compatibility with node commands */
	uint8						id;
	/* User authenticate does not have any unique address */
	/* Payload data */
	char						*username;
	uint8						auth_meth_count;
	uint8						*methods;
	char						*data;
} User_Authenticate_Cmd;

/**
 * Structure used for negotiation of FPS at client
 */
typedef struct Fps_Cmd {
	/* Fake cmd_id for compatibility with node commands */
	uint8						id;
	/* Payload data */
	real32						fps;
	uint32						seconds;
	uint32						useconds;
} Fps_Cmd;

/**
 * Structure used for storing informations about certificates and DNSSEC
 */
typedef struct Security_Info_Cmd {
	/* Fake cmd_id for compatibility with node commands */
	uint8						id;
	/* Security informations */
	char						*peer_name;		/* Domain name of the server */
	char						*peer_addr;		/* IPv4 or IPv6 of the server */
	uint8						status;			/* Result of security check */
	char						*result;		/* Textual representation of security check */
} Security_Info_Cmd;

/* -------------- Fake Server Commands -------------- */

/* Following commands are not send to the peer. Server use them for
 * notification of data thread, that certain client confirmed receiving of
 * some create or destroy command. */

typedef struct Node_Create_Ack_Cmd {
	/* Fake cmd_id for compatibility with node commands */
	uint8			id;
	uint32			node_id;
} Node_Create_Ack_Cmd;

typedef struct Node_Destroy_Ack_Cmd {
	/* Fake cmd_id for compatibility with node commands */
	uint8			id;
	uint32			node_id;
} Node_Destroy_Ack_Cmd;

typedef struct TagGroup_Create_Ack_Cmd {
	/* Fake cmd_id for compatibility with node commands */
	uint8			id;
	uint32			node_id;
	uint16			taggroup_id;
} TagGroup_Create_Ack_Cmd;

typedef struct TagGroup_Destroy_Ack_Cmd {
	/* Fake cmd_id for compatibility with node commands */
	uint8			id;
	uint32			node_id;
	uint16			taggroup_id;
} TagGroup_Destroy_Ack_Cmd;

typedef struct Tag_Create_Ack_Cmd {
	uint8			id;
	uint32			node_id;
	uint16			taggroup_id;
	uint16			tag_id;
} Tag_Create_Ack_Cmd;

typedef struct Tag_Destroy_Ack_Cmd {
	uint8			id;
	uint32			node_id;
	uint16			taggroup_id;
	uint16			tag_id;
} Tag_Destroy_Ack_Cmd;

typedef struct Layer_Create_Ack_Cmd {
	uint8			id;
	uint32			node_id;
	uint16			layer_id;
} Layer_Create_Ack_Cmd;

typedef struct Layer_Destroy_Ack_Cmd {
	uint8			id;
	uint32			node_id;
	uint16			layer_id;
} Layer_Destroy_Ack_Cmd;

struct Generic_Cmd;

void v_fake_cmd_print(const unsigned char level,
		const struct Generic_Cmd *cmd);
void v_fake_cmd_destroy(struct Generic_Cmd **cmd);
int v_fake_cmd_struct_size(const struct Generic_Cmd *cmd);
struct VCommandQueue *v_fake_cmd_queue_create(uint8 id, uint8 copy_bucket, uint8 fake_cmds);

void v_fake_fps_print(const unsigned char level, struct Fps_Cmd *fps_cmd);
void v_Fps_init(struct Fps_Cmd *fps_cmd,
		real32 fps,
		uint32 seconds,
		uint32 useconds);
struct Generic_Cmd *v_Fps_create(real32 fps,
		uint32 seconds,
		uint32 useconds);
void v_Fps_clear(struct Fps_Cmd *fps_cmd);
void v_Fps_destroy(struct Fps_Cmd **fps_cmd);

void v_fake_connect_accept_print(const unsigned char level, struct Connect_Accept_Cmd *connect_accpet);
void v_Connect_Accept_init(struct Connect_Accept_Cmd *connect_accept,
		uint32 avatar_id,
		uint16 user_id);
struct Connect_Accept_Cmd *v_Connect_Accept_create(uint32 avatar_id,
		uint16 user_id);
void v_Connect_Accept_clear(struct Connect_Accept_Cmd *connect_accept);
void v_Connect_Accept_destroy(struct Connect_Accept_Cmd **connect_accept);

void v_fake_connect_terminate_print(const unsigned char level, struct Connect_Terminate_Cmd *connect_terminate);
void v_Connect_Terminate_init(struct Connect_Terminate_Cmd *conn_term,
		uint8 error);
struct Connect_Terminate_Cmd *v_Connect_Terminate_create(uint8 error);
void v_Connect_Terminate_clear(struct Connect_Terminate_Cmd *conn_term);
void v_Connect_Terminate_destroy(struct Connect_Terminate_Cmd **conn_term);


void v_fake_user_auth_print(const unsigned char level, struct User_Authenticate_Cmd *user_auth);
void v_User_Authenticate_init(struct User_Authenticate_Cmd *user_auth,
		const char *username,
		uint8 auth_meth_count,
		uint8 *methods,
		const char *data);
struct User_Authenticate_Cmd *v_User_Authenticate_create(const char *username,
		uint8 auth_meth_count,
		uint8 *methods,
		const char *data);
void v_User_Authenticate_clear(struct User_Authenticate_Cmd *user_auth);
void v_User_Authenticate_destroy(struct User_Authenticate_Cmd **user_auth);


/* Node_Create_Ack */
void v_fake_node_create_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd);
struct Generic_Cmd *v_fake_node_create_ack_create(uint32 node_id);
void v_fake_node_create_ack_destroy(struct Generic_Cmd **cmd);


/* Node_Destroy_Ack */
void v_fake_node_destroy_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd);
struct Generic_Cmd *v_fake_node_destroy_ack_create(uint32 node_id);
void v_fake_node_destroy_ack_destroy(struct Generic_Cmd **cmd);


/* TagGroup_Create_Ack */
void v_fake_taggroup_create_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd);
struct Generic_Cmd *v_fake_taggroup_create_ack_create(uint32 node_id,
		uint16 taggroup_id);
void v_fake_taggroup_create_ack_destroy(struct Generic_Cmd **cmd);


/* TagGroup_Destroy_Ack */
void v_fake_taggroup_destroy_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd);
struct Generic_Cmd *v_fake_taggroup_destroy_ack_create(uint32 node_id,
		uint16 taggroup_id);
void v_fake_taggroup_destroy_ack_destroy(struct Generic_Cmd **cmd);

/* Tag_Create_Ack */
void v_fake_tag_create_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd);
void v_tag_create_ack_destroy(struct Generic_Cmd **cmd);
struct Generic_Cmd *v_tag_create_ack_create(uint32 node_id,
		uint16 taggroup_id,
		uint16 tag_id);

/* Tag_Destroy_Ack */
void v_fake_tag_destroy_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd);
struct Generic_Cmd *v_tag_destroy_ack_create(uint32 node_id,
		uint16 taggroup_id,
		uint16 tag_id);
void v_tag_destroy_ack_destroy(struct Generic_Cmd **cmd);

/* Layer_Create_Ack */
void v_fake_layer_create_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd);
struct Generic_Cmd *v_fake_layer_create_ack_create(uint32 node_id,
		uint16 layer_id);
void v_fake_layer_create_ack_destroy(struct Generic_Cmd **cmd);

/* Layer_Destroy_Ack */
void v_fake_layer_destroy_ack_print(const unsigned char level,
		const struct Generic_Cmd *cmd);
struct Generic_Cmd *v_fake_layer_destroy_ack_create(uint32 node_id,
		uint16 layer_id);
void v_fake_layer_destroy_ack_destroy(struct Generic_Cmd **cmd);

#endif /* V_FAKE_COMMANDS_H_ */
