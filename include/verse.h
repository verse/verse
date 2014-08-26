/*
 *
 * ***** BEGIN BSD LICENSE BLOCK *****
 *
 * Copyright (c) 2009-2014, Jiri Hnidek
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

/**
 * \file	verse.h
 * \author	Jiri Hnidek (jiri.hnidek@tul.cz)
 * \brief	This file includes type definitions and function prototypes
 * for Verse API.
 *
 * Detail description of C Verse API could be found at:
 * https://github.com/verse/verse/wiki/C-API
 *
 */

#if !defined VERSE_H

#if defined __cplusplus
extern "C" {
#endif

#define VERSE_H

#include <stdint.h>
#include <limits.h>

/* Version of Verse protocol */
#define VRS_VERSION	1

/* Verse session will be closed, when verse server or client does not respond
 * for more then 30 seconds */
#define VRS_TIMEOUT	30

/* Default ports */
#define VRS_DEFAULT_TCP_PORT	12344
#define VRS_DEFAULT_TLS_PORT	12345
#define VRS_DEFAULT_WEB_PORT	23456

/* Return codes */
#define VRS_SUCCESS					 0
#define VRS_FAILURE					-1
#define	VRS_NO_CB_FUNC				-2
#define VRS_NO_CB_CONN_FUNC			-3
#define VRS_NO_CB_TERM_FUNC			-4
#define VRS_NO_CB_USER_AUTH			-5
#if 0
/* Proposal of new error codes */
#define VRS_ERR_WRONG_SESSION_ID	-6
#define VRS_ERR_FULL_QUEUE			-7
#endif

/* Error codes for connect terminate command */
#define VRS_CONN_TERM_RESERVED		0	/* Reserved code */
#define VRS_CONN_TERM_HOST_UNKNOWN	1	/* Host could not be found */
#define VRS_CONN_TERM_HOST_DOWN		2	/* Host is not accessible */
#define VRS_CONN_TERM_SERVER_DOWN	3	/* Server isn't running */
#define VRS_CONN_TERM_AUTH_FAILED	4	/* Bad username or password */
#define VRS_CONN_TERM_TIMEOUT		5	/* Connection timed out */
#define VRS_CONN_TERM_ERROR			6	/* Connection was broken */
#define VRS_CONN_TERM_CLIENT		7	/* Connection terminated by client */
#define VRS_CONN_TERM_SERVER		8	/* Connection terminated by server */

/* Access permissions */
#define VRS_PERM_NODE_READ			1	/* User can read content of the node */
#define VRS_PERM_NODE_WRITE			2	/* User can do anything with content of the node */
#if 0
/* Proposal of new permission flags */
#define VRS_PERM_NODE_APPEND		4	/* User can add new entities to node */
#define VRS_PERM_NODE_CHANGE		8	/* User can change content of the node */
#define VRS_PERM_NODE_CLEAR			16	/* User can delete any entity in the node */
#endif

/* Methods of user authentication */
#define VRS_UA_METHOD_RESERVED		0
#define VRS_UA_METHOD_NONE			1	/* Server should never support this method
 	 	 	 	 	 	 	 	 	 	 * and return list of supported methods */
#define VRS_UA_METHOD_PASSWORD		2	/* Password authentication method */
#if 0
#define VRS_UA_METHOD_HOSTBASED		3	/* Not implemented yet */
#define VRS_UA_METHOD_CERTIFICATE	4	/* Not implemented yet */
#endif

#define VRS_MAX_USERNAME_LENGTH		64		/* Maximal length of the user name */
#define VRS_MAX_DATA_LENGTH			128		/* Maximal length of authentication data */
#define VRS_STRING8_MAX_SIZE		255		/* Maximal length of string8 */

/* There some special nodes with defined Node ID */
#define VRS_ROOT_NODE_ID			0
#define VRS_AVATAR_PARENT_NODE_ID	1
#define VRS_USERS_PARENT_NODE_ID	2
#define VRS_SCENE_PARENT_NODE_ID	3

/* There are some special custom_types of special nodes */
#define VRS_ROOT_NODE_CT			0
#define VRS_AVATAR_PARENT_NODE_CT	1
#define VRS_USERS_PARENT_NODE_CT	2
#define VRS_SCENE_PARENT_NODE_CT	3
#define VRS_AVATAR_NODE_CT			4
#define VRS_AVATAR_INFO_NODE_CT		5
#define VRS_USER_NODE_CT			6

/* Superuser of verse server has defined UID */
#define VRS_SUPER_USER_UID			100
#define VRS_OTHER_USERS_UID			65535	/* 2^16 - 1*/

/* Range of common node id */
#define VRS_FIRST_COMMON_NODE_ID	65536	/* 2^16 */
#define VRS_LAST_COMMON_NODE_ID		4294967294U	/* 2^32 - 2 */

#define VRS_MAX_COMMON_NODE_COUNT	4294901760U /* 2^32 - 2^16*/

/* Default priority of commands */
#define VRS_DEFAULT_PRIORITY		128

/* Several levels of debug print */
#define	VRS_PRINT_NONE				0
#define VRS_PRINT_INFO				1
#define VRS_PRINT_ERROR				2
#define VRS_PRINT_WARNING			3
#define VRS_PRINT_DEBUG_MSG			4

/* Flags used in function verse_send_connect_request */
#define VRS_SEC_DATA_NONE			1	/* No security at data exchange connection */
#define VRS_SEC_DATA_TLS			2	/* Use TLS/DTLS at data exchange connection */
#define VRS_TP_UDP					4	/* Transport protocol: UDP */
#define VRS_TP_TCP					8	/* Transport protocol: TCP */
#define VRS_TP_WEBSOCKET			16
#define VRS_CMD_CMPR_NONE			32	/* No command compression */
#define VRS_CMD_CMPR_ADDR_SHARE		64	/* Share command addresses to compress commands */

/* Types of verse values */
#define VRS_VALUE_TYPE_RESERVED		0
#define VRS_VALUE_TYPE_UINT8		1
#define VRS_VALUE_TYPE_UINT16		2
#define VRS_VALUE_TYPE_UINT32		3
#define VRS_VALUE_TYPE_UINT64		4
#define VRS_VALUE_TYPE_REAL16		5
#define VRS_VALUE_TYPE_REAL32		6
#define VRS_VALUE_TYPE_REAL64		7
#define VRS_VALUE_TYPE_STRING8		8

/* Parameters of session */
#define VRS_SESSION_IN_QUEUE_MAX_SIZE		1
#define VRS_SESSION_IN_QUEUE_FREE_SPACE		2
#define VRS_SESSION_OUT_QUEUE_MAX_SIZE		3
#define VRS_SESSION_OUT_QUEUE_FREE_SPACE	4

/**
 * \brief This function tries to connect to verse server
 * \param[in]	*hostname	The string with hostname of the server
 * \param[in]	*service	The string with port number of the server
 * \param[in]	flags		The flags with options of connection
 * \param[out]	*session_id	There will be stored ID of session with verse server
 *
 * \return This function will return VRS_SUCCESS (0), when the client was able
 * to start connection to verse server.
 */
int32_t vrs_send_connect_request(const char *hostname,
		const char *service,
		const uint16_t flags,
		uint8_t *session_id);

/**
 * \brief This function tries to send username and some authentication data
 * (usually password) to the verse server
 *
 * This function should be called, when client receive User_Authenticate
 * command and callback function registered with
 * register_receive_user_authenticate() is called.
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	*username	The string of username
 * \param[in]	auth_type	The authentication method
 * \param[in]	data_length	The length of authentication data in bytes
 * \param[in]	*data		The pointer at authentication data
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_user_authenticate(const uint8_t session_id,
		const char *username,
		const uint8_t auth_type,
		const char *data);

/**
 * \brief This function register callback function for situation, when server
 * require user authentication from the user
 *
 * \param[in]	(*func)				The pointer at callback function
 * \param[in]	session_id			The ID of session with verse server.
 * \param[in]	*username			The string with username
 * \param[in]	auth_methods_count	The number of previous authentication attempts to the server
 * \param[in]	*methods			The list of supported authentication methods
 */
void vrs_register_receive_user_authenticate(void (*func)(const uint8_t session_id,
		const char *username,
		const uint8_t auth_meth_count,
		const uint8_t *methods));

/**
 * \brief This function register callback function for command Connect_Accept
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server
 * \param[in]	user_id		The ID of user account
 * \param[in]	avatar_id	The ID of avatar and avatar node
 */
void vrs_register_receive_connect_accept(void (*func)(const uint8_t session_id,
		const uint16_t user_id,
		const uint32_t avatar_id));

/**
 * \brief This function switch connection to CLOSING state (start teardown,
 * exit thread) and close connection to verse server.
 *
 * \param[in]	session_id	The ID of session with verse server.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_connect_terminate(const uint8_t session_id);

/**
 * \brief This function register callback function for situation, when
 * connection to verse server is closed or lost.
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	error_num	The error code
 */
void vrs_register_receive_connect_terminate(void (*func)(const uint8_t session_id,
		const uint8_t error_num));

/**
 * \brief This function calls appropriate callback functions, when some system
 * or node commands are in incoming queue.
 *
 * This function should be called periodically in 'never ending loop' e.g.:
 *
 * \code
 * while(1) {
 *   vrs_callback_update(my_session_id);
 *   sleep(1);
 * }
 * \endcode
 *
 * \param[in]	session_id			The ID of session with verse server.
 *
 * \return This function returns VRS_SUCCESS, when session was found and when
 * at least basic callback functions were registered.
 */
int32_t vrs_callback_update(const uint8_t session_id);

/**
 * \brief This function can set debug level of verse client.
 * \param[in]	debug_level	This parameter can have values defined in verse.h
 */
int32_t vrs_set_debug_level(uint8_t debug_level);

/**
 * \brief This function can set name and version of current verse client.
 *
 * The name and version can be set only once. Changing of client name nor
 * version is not allowed. Client name and version can be sent during
 * handshake. Thus it is possible to call this function before calling
 * vrs_send_connect_request().
 *
 * \param[in] *name		The name of verse client.
 * \param[in] *version	The version og verse client.
 *
 * \return This function returns VRS_SUCCESS.
 */
int32_t vrs_set_client_info(char *name, char *version);

/**
 * \brief This function tries to get specified session parameter.
 *
 * There are currently four parameters of session that can be queried.
 * It is maximum size of outgoing/incoming queue and it is free space
 * in outgoing/incoming queue.
 *
 * \param session_id	The ID of session with verse server.
 * \param param			The ID of parameter that is queried.
 *
 * \return This function returns value of queried session parameter. When
 * wrong session ID is specified, then zero is returned.
 */
int32_t vrs_get(const uint8_t session_id,
	    const uint8_t param);

/**
 * \brief This function tries to set specified session parameter.
 *
 * There are currently two parameters of session that can be set.
 * It is maximal size of outgoing and incoming queue.
 *
 * \param session_id	The ID of session with verse server.
 * \param param			The ID of parameter that is set.
 * \param value			The value of parameter.
 *
 * \return
 */
int32_t vrs_set(const uint8_t session_id,
	    const uint8_t param,
	    const uint32_t value);

/**
 * \brief Return error message for error_num returned by Verse API functions.
 *
 * This function should return some string with detail description of error,
 * but it doesn't do anything now.
 *
 * \param[in]	error_num	The identifier of error
 *
 * \return	This function should return string with detail description of
 * errror.
 */
char *vrs_strerror(const uint32_t error_num);

/**
 * \brief This function tries to negotiate new FPS with server
 *
 * Note: default FPS of client is 60 FPS. If you will try to negotiation this
 * value with server, then no command will be sent, because there will be no
 * need to change this value.
 */
int32_t vrs_send_fps(const uint8_t session_id,
		const uint8_t prio,
		const float fps);

/**
 * \brief This function tries to send Node_Create command to the server.
 *
 * This function does not send this command directly, but this function
 * add Node_Create to the sending queue. Sending of this command could be
 * delayed due to congestion control or flow control. When client sends this
 * command, then this command is sent with special values: node_id==-1,
 * parent_id==avatar_id and user has to be equal of current user. Note: calling
 * of this function does not guarantee delivery of this command, because
 * connection to the server could be lost before command Node_Create is sent to
 * the server. The command is added to the priority queue.
 *
 * \param[in]	session_id	The ID of session with verse server
 * \param[in]	prio		The priority of the command.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_node_create(const uint8_t session_id,
		const uint8_t prio,
		const uint16_t type);

/**
 * \brief This function register callback function for command Node_Create
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	node_id		The ID of new created node
 * \param[in]	parent_id	The ID of parent of new created node
 * \param[in]	user_id		The ID of the owner of new created node
 * \param[in]	type		The client defined type of received node
 */
void vrs_register_receive_node_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t parent_id,
		const uint16_t user_id,
		const uint16_t type));

/**
 * \brief This function tries to send command Node_Destroy to the server.
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of the command.
 * \param[in]	node_id		The ID of node that will be destroyed.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_node_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id);

/**
 * \brief This function register callback function for command Node_Destroy
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of destroyed node
 */
void vrs_register_receive_node_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id));

/**
 * \brief This function tries to send command Node_Subscribe to the server.
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID that client wants to be subscribed for.
 * \param[in]	version		The requested version that client request. The client
 * has to have local copy of this version.
 * \param[in]	crc32		The crc32 of requested version.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_node_subscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32);

/**
 * \brief This function register callback function for command Node_Subscribe
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID that client wants to be subscribed for.
 * \param[in]	version		The version of node
 * \param[in]	crc32		The CRC32 of the node
 */
void vrs_register_receive_node_subscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32));

/**
 * \brief This function tries to send command Node_Unsubscribe to the server.
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID of node that client wants to unsubscribe
 * \param[in]   versing	The flag that specify if versing will be requested.
 * If the versing is equal to -1 (0xFF), then versing will be requested. When the
 * versing is equal to 0 (0x00), then versing will not be requested. Other values
 * are not defined.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_node_unsubscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint8_t versing);

/**
 * \brief This function register callback function for command Node_UnSubscribe
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID that client wants to be unsubscribed from.
 * \param[in]	version		The version of node
 * \param[in]	crc32		The CRC32 of the node
 */
void vrs_register_receive_node_unsubscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32));

/**
 * \brief This function tries to send command Node_Owner to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The owner will be changed for the node with this ID
 * \param[in]	user_id			The User ID of new owner
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_node_owner(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id,
		uint16_t user_id);

/**
 * \brief This function register callback function for command Node_Owner
 *
 * \param[in]	(*func)			The pointer at callback function
 * \param[in]	session_id		The ID of session with verse server
 * \param[in]	node_id			The owner will be changed for the node with this ID
 * \param[in]	user_id			The User ID of new owner
 *
 */
void vrs_register_receive_node_owner(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint16_t user_id));

/**
 * \brief This function tries to send command Node_Permission to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The permissions will be changed for the node with this ID
 * \param[in]	user_id			The permissions will be changed for the user with this ID
 * \param[in]	permissions		The new permissions
 * \param[in]	level			The permissions will be changed for child nodes up to this level
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_node_perm(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id,
		uint16_t user_id,
		uint8_t permissions);

/**
 * \brief This function register callback function for command Node_Perm
 *
 * \param[in]	(*func)			The pointer at callback function
 * \param[in]	session_id		The ID of session with verse server
 * \param[in]	node_id			The permissions will be changed for the node with this ID
 * \param[in]	user_id			The permissions will be changed for the user with this ID
 * \param[in]	permissions		The new permissions
 */
void vrs_register_receive_node_perm(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint16_t user_id,
		uint8_t permissions));

/**
 * \brief This function tries to send command Node_Lock to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			Verse server will try to lock node with this ID
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_node_lock(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id);

/**
 * \brief This function register callback function for command Node_Lock
 *
 * \param[in]	(*func)			The pointer at callback function
 * \param[in]	session_id		The ID of session with verse server
 * \param[in]	node_id			Verse server will try to lock node with this ID
 * \param[in]	avatar_id		The ID of avatar that locked the node
 *
 */
void vrs_register_receive_node_lock(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint32_t avatar_id));

/**
 * \brief This function tries to send command Node_UnLock to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			Verse server will try to unlock node with this ID
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_node_unlock(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id);

/**
 * \brief This function register callback function for command Node_UnLock
 *
 * \param[in]	(*func)			The pointer at callback function
 * \param[in]	session_id		The ID of session with verse server
 * \param[in]	node_id			Verse server will try to unlock node with this ID
 * \param[in]	avatar_id		The ID of avatar that unlocked the node
 *
 */
void vrs_register_receive_node_unlock(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint32_t avatar_id));

/**
 * \brief This function tries to send command Node_Link to the server.
 *
 * When client has permission to write to child and parent node, then child node
 * will have new parent node.
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	parent_node_id	The ID of parent node
 * \param[in]	child_node_id	The ID of child node
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_node_link(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t parent_node_id,
		const uint32_t child_node_id);

/**
 * \brief This function register callback function for command Node_Link
 *
 * \param[in]	(*func)			The pointer at callback function
 * \param[in]	session_id		The ID of session with verse server
 * \param[in]	parent_node_id	The ID of parent node
 * \param[in]	child_node_id	The ID of child node
 */
void vrs_register_receive_node_link(void (*func)(const uint8_t session_id,
		const uint32_t parent_node_id,
		const uint32_t child_node_id));

/**
 * \brief This function tries to send command Node_Priority to the server.
 *
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID of node, where client wants to change priority
 * \param[in]	node_prio	The new priority of Node
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_node_prio(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint8_t node_prio);

/**
 * \brief This function send command TagGroup_Create to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where new taggroup will be created
 * \param[in]	type			The client defined type of taggroup
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_taggroup_create(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t type);

/**
 * \brief This function register callback function for command TagGroup_Create
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where taggroup was created
 * \param[in]	taggroup_id	The ID of new taggroup
 * \param[in]	type		The client defined type of received taggroup
 */
void vrs_register_receive_taggroup_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t type));

/**
 * \brief This function send command TagGroup_Destroy to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where new taggroup will be destroyed
 * \param[in]	taggroup_id		The ID of taggroup that will be destroyed
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_taggroup_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id);

/**
 * \brief This function register callback function for command TagGroup_Destroy
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where taggroup was destroyed
 * \param[in]	taggroup_id	The ID of deleted taggroup
 */
void vrs_register_receive_taggroup_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id));

/**
 * \brief This function send command TagGroup_Subscribe to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where is subscribed taggroup
 * \param[in]	taggroup_id		The ID subscribed taggroup
 * \param[in]	version			The version that client tries to subscribed for
 * \param[in]	crc32			The crc32 of subscribed version (when version is 0,
 * then the crc32 is defined as zero)
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_taggroup_subscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32);

/**
 * \brief This function register callback function for command TagGroup_Subscribe
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where taggroup was subscribed
 * \param[in]	taggroup_id	The ID of subscribed taggroup
 * \param[in]	version		The version that client subscribed for
 * \param[in]	crc32		The crc32 of subscribed version (when version is 0,
 * then the crc32 is defined as zero)
 */
void vrs_register_receive_taggroup_subscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32));

/**
 * \brief This function send command TagGroup_Unsubscribe to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where is unsubscribed taggroup
 * \param[in]	taggroup_id		The ID of unsubscribed taggroup
 * \param[in]   versing			The flag that specify if versioning will be requested.
 * If the versing is equal to -1 (0xFF), then versing will be requested. When the
 * versing is equal to 0 (0x00), then versing will not be requested. Other values
 * are not defined.
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_taggroup_unsubscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint8_t versing);

/**
 * \brief This function register callback function for command TagGroup_UnSubscribe
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where taggroup was unsubscribed
 * \param[in]	taggroup_id	The ID of unsubscribed taggroup
 * \param[in]	version		The version of unsubscibed taggroup (generated by server)
 * \param[in]	crc32		The crc32 of unsubscribed version (when version is 0,
 * then the crc32 is defined as zero)
 */
void vrs_register_receive_taggroup_unsubscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32));

/**
 * \brief This function send command Tag_Create to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where new tag will be created
 * \param[in]	taggroup_id		The ID of taggroup, where new tag will be created
 * \param[in]	data_type		The type of value stored in tag
 * \param[in]	count			The number of values stored in one tag (1,2,3,4)
 * \param[in]	type			The client defined type of received tag
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_tag_create(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type);

/**
 * \brief This function register callback function for command TagGroup_Create
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where tag was created
 * \param[in]	taggroup_id	The ID of taggroup, where new tag was created
 * \param[in]	tag_id		The ID of new tag
 * \param[in]	data_type	The type of value stored in tag
 * \param[in]	count		The number of values stored in one tag (1,2,3,4)
 * \param[in]	type		The client defined type of received tag
 */
void vrs_register_receive_tag_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type));

/**
 * \brief This function send command Tag_Destroy to the server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of command
 * \param[in]	node_id			The ID of node, where new tag will be destroyed
 * \param[in]	taggroup_id		The ID of taggroup, where new tag will be destroyed
 * \param[in[	tag_id			The ID of tag that will be destroyed
 *
 * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_tag_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id);

/**
 * \brief This function register callback function for command Tag_Destroy
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where tag was destroyed
 * \param[in]	taggroup_id	The ID of taggroup, where tag was destroyed
 * \param[in]	tag_id		The ID of destroyed tag
 */
void vrs_register_receive_tag_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id));

/**
 * \brief This function sends command Tag_Set_Value to the server
 *
 * \param[in]	session_id	The ID of session with verse server
 * \param[in]	prio		The priority of command
 * \param[in]	node_id		The ID of node, where value of tag will be set
 * \param[in]	taggroup_id	The ID of taggroup, where value of tag will be set
 * \param[in]	tag_id		The ID of tag
 * \param[in]	type		The type of value
 * \param[in]	count		The count of values (1,2,3,4)
 * \param[in]	*value		The pointer at value(s)
 *
 *  * \return		This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_tag_set_value(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const void *value);

/**
 * \brief This function register callback function for command Tag_Set_Value
 *
 * \param[in]	(*func)		The pointer at callback function
 * \param[in]	session_id	The ID of session with verse server.
 * \param[in]	node_id		The ID of node, where value of tag was set
 * \param[in]	taggroup_id	The ID of taggroup, where value of tag was set
 * \param[in]	tag_id		The ID of tag
 * \param[in]	type		The type of value
 * \param[in]	count		The count of values (1,2,3,4)
 * \param[in]	*value		The pointer at value(s)
 */
void vrs_register_receive_tag_set_value(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const void *value));

/**
 * \brief This function sends command layer_create to verse server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be created
 * \param[in]	parent_layer_id	The ID of parent layer (0xFFFF means no parent layer layer)
 * \param[in]	data_type		The type of value (uint8, uint16, uint32, etc.)
 * \param[in]	count			The count of values in one layer item
 * \param[in]	type			The client defined type
 *
 * \return	This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_layer_create(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t parent_layer_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type);

/**
 * \brief This function register callback function for command Layer_Create
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be created
 * \param[in]	parent_layer_id	The ID of parent layer (0xFFFF means no parent layer layer)
 * \param[in]	data_type		The type of value (uint8, uint16, uint32, etc.)
 * \param[in]	count			The count of values in one layer item
 * \param[in]	type			The client defined type
 */
void vrs_register_receive_layer_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t parent_layer_id,
		const uint16_t layer_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type));

/**
 * \brief This function sends command layer_destroy to verse server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be destroyed
 * \param[in]	layer_id		The ID of layer that will be destroyed
 *
 * \return	This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_layer_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id);

/**
 * \brief This function register callback function for command Layer_Destroy
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be destroyed
 * \param[in]	layer_id		The ID of layer that will be destroyed
 */
void vrs_register_receive_layer_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id));

/**
 * \brief This function sends command layer_subscribe to verse server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be destroyed
 * \param[in]	layer_id		The ID of layer that will be destroyed
 * \param[in]	version			The version that client wants to subscribe to
 * \param[in]	crc32			The CRC32 code
 *
 * \return	This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_layer_subscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32);

/**
 * \brief This function register callback function for command Layer_Subscribe
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be destroyed
 * \param[in]	layer_id		The ID of layer that will be destroyed
 * \param[in]	version			The version that client wants to subscribe to
 * \param[in]	crc32			The CRC32 code
 */
void vrs_register_receive_layer_subscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32));

/**
 * \brief This function sends command layer_subscribe to verse server
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be destroyed
 * \param[in]	layer_id		The ID of layer that will be destroyed
 * \param[in]	versing			The flag if client requires versing of this layer
 *
 * \return	This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_layer_unsubscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint8_t versing);

/**
 * \brief This function register callback function for command Layer_UnSubscribe
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where layer will be destroyed
 * \param[in]	layer_id		The ID of layer that will be destroyed
 * \param[in]	versing			The flag if client requires versing of this layer
 */
void vrs_register_receive_layer_unsubscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32));

/**
 * \brief This function sets value of layer item
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where item will be set
 * \param[in]	layer_id		The ID of layer, where item will be set
 * \param[in]	item_id			The ID of layer item that will be set
 * \param[in]	type			The data type of value
 * \param[in]	count			The count of values that will be set
 * \param[in]	*value			The pointer at value(s)
 *
 * \return	This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_layer_set_value(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id,
		const uint8_t type,
		const uint8_t count,
		const void *value);

/**
 * \brief This function register callback function for command Layer_Set_Value
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where item will be set
 * \param[in]	layer_id		The ID of layer, where item will be set
 * \param[in]	item_id			The ID of layer item that will be set
 * \param[in]	type			The data type of value
 * \param[in]	count			The count of values that will be set
 * \param[in]	*value			The pointer at value(s)
 */
void vrs_register_receive_layer_set_value(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id,
		const uint8_t type,
		const uint8_t count,
		const void *value));

/**
 * \brief This function unset (delete) one value in layer
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where item will be unset
 * \param[in]	layer_id		The ID of layer, where item will be unset
 * \param[in]	item_id			The ID of layer item that will be unset
 *
 * \return	This function returns VRS_SUCCESS (0), when the session_id
 * was valid value, it returns VRS_FAILURE (1) otherwise.
 */
int32_t vrs_send_layer_unset_value(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id);

/**
 * \brief This function register callback function for command Layer_Value_Unset
 *
 * \param[in]	session_id		The ID of session with verse server.
 * \param[in]	prio			The priority of node
 * \param[in]	node_id			The ID of node, where item will be unset
 * \param[in]	layer_id		The ID of layer, where item will be unset
 * \param[in]	item_id			The ID of layer item that will be unset
 */
void vrs_register_receive_layer_unset_value(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id));

#if defined __cplusplus
}
#endif

#endif

