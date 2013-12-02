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
 * Authors: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

/* This .h file include only type definitions and functions for Verse API */

#if !defined VERSE_H

#if defined __cplusplus
extern "C" {
#endif

#define VERSE_H

/*#define WITH_KERBEROS*/
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
#define VRS_SUCCESS				0
#define VRS_FAILURE				1

#define	VRS_NO_CB_FUNC			2
#define VRS_NO_CB_CONN_FUNC		3
#define VRS_NO_CB_TERM_FUNC		4
#define VRS_NO_CB_USER_AUTH		5

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
/* Proposal */
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

/* Reserved IDs for user and avatar */
#define VRS_RESERVED_USER_ID		0	/* Notification, that command connect_accpet could not be unpacked correctly */
#define VRS_RESERVED_AVATAR_ID		0	/* Notification, that command connect_accpet could not be unpacked correctly */

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

#define VRS_RESERVED_NODE_ID		0xFFFFFFFF
#define VRS_RESERVED_TAGGROUP_ID	0xFFFF

#define VRS_MAX_COMMON_NODE_COUNT	4294901760U /* 2^32 - 2^16*/

#define VRS_DEFAULT_PRIORITY		128

/* Minimal and maximal count of method types in USER_AUTH_FAILURE system command */
#define VRS_MIN_UA_METHOD_COUNT		0
#define VRS_MAX_UA_METHOD_COUNT		1

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

/* Type of verse value */
#define VRS_VALUE_TYPE_RESERVED		0
#define VRS_VALUE_TYPE_UINT8		1
#define VRS_VALUE_TYPE_UINT16		2
#define VRS_VALUE_TYPE_UINT32		3
#define VRS_VALUE_TYPE_UINT64		4
#define VRS_VALUE_TYPE_REAL16		5
#define VRS_VALUE_TYPE_REAL32		6
#define VRS_VALUE_TYPE_REAL64		7
#define VRS_VALUE_TYPE_STRING8		8

/* Type of layer */
#define VRS_LAYER_RESERVED		0
#define VRS_LAYER_ROOT			1
#define VRS_LAYER_SLAVE			2


int vrs_send_connect_request(const char *hostname,
		const char *service,
		const uint16_t flags,
		uint8_t *session_id);

int vrs_send_user_authenticate(const uint8_t session_id,
		const char *username,
		const uint8_t auth_type,
		const char *data);
void vrs_register_receive_user_authenticate(void (*func)(const uint8_t session_id,
		const char *username,
		const uint8_t auth_meth_count,
		const uint8_t *methods));

void vrs_register_receive_connect_accept(void (*func)(const uint8_t session_id,
		const uint16_t user_id,
		const uint32_t avatar_id));

int vrs_send_connect_terminate(const uint8_t session_id);
void vrs_register_receive_connect_terminate(void (*func)(const uint8_t session_id,
		const uint8_t error_num));

int vrs_callback_update(const uint8_t session_id);

int vrs_set_debug_level(uint8_t debug_level);

int vrs_set_client_info(char *name, char *version);

char *vrs_strerror(const uint32_t error_num);

int vrs_send_fps(const uint8_t session_id,
		const uint8_t prio,
		const float fps);


int vrs_send_node_create(const uint8_t session_id,
		const uint8_t prio,
		const uint16_t type);
void vrs_register_receive_node_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t parent_id,
		const uint16_t user_id,
		const uint16_t type));

int vrs_send_node_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id);
void vrs_register_receive_node_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id));

int vrs_send_node_subscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32);
void vrs_register_receive_node_subscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32));

int vrs_send_node_unsubscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint8_t versing);
void vrs_register_receive_node_unsubscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint32_t version,
		const uint32_t crc32));

int vrs_send_node_owner(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id,
		uint16_t user_id);
void vrs_register_receive_node_owner(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint16_t user_id));

int vrs_send_node_perm(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id,
		uint16_t user_id,
		uint8_t permissions);
void vrs_register_receive_node_perm(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint16_t user_id,
		uint8_t permissions));

int vrs_send_node_lock(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id);
void vrs_register_receive_node_lock(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint32_t avatar_id));

int vrs_send_node_unlock(const uint8_t session_id,
		const uint8_t prio,
		uint32_t node_id);
void vrs_register_receive_node_unlock(void (*func)(const uint8_t session_id,
		uint32_t node_id,
		uint32_t avatar_id));

int vrs_send_node_link(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t parent_node_id,
		const uint32_t child_node_id);
void vrs_register_receive_node_link(void (*func)(const uint8_t session_id,
		const uint32_t parent_node_id,
		const uint32_t child_node_id));

int vrs_send_node_prio(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint8_t node_prio);

int vrs_send_taggroup_create(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t type);
void vrs_register_receive_taggroup_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t type));

int vrs_send_taggroup_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id);

void vrs_register_receive_taggroup_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id));

int vrs_send_taggroup_subscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32);
void vrs_register_receive_taggroup_subscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32));

int vrs_send_taggroup_unsubscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint8_t versing);
void vrs_register_receive_taggroup_unsubscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint32_t version,
		const uint32_t crc32));

int vrs_send_tag_create(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type);
void vrs_register_receive_tag_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type));

int vrs_send_tag_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id);
void vrs_register_receive_tag_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id));

int vrs_send_tag_set_value(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const void *value);
void vrs_register_receive_tag_set_value(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t taggroup_id,
		const uint16_t tag_id,
		const uint8_t data_type,
		const uint8_t count,
		const void *value));

int vrs_send_layer_create(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t parent_layer_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type);
void vrs_register_receive_layer_create(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t parent_layer_id,
		const uint16_t layer_id,
		const uint8_t data_type,
		const uint8_t count,
		const uint16_t type));

int vrs_send_layer_destroy(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id);
void vrs_register_receive_layer_destroy(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id));

int vrs_send_layer_subscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32);
void vrs_register_receive_layer_subscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32));

int vrs_send_layer_unsubscribe(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint8_t versing);
void vrs_register_receive_layer_unsubscribe(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t version,
		const uint32_t crc32));

int vrs_send_layer_set_value(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id,
		const uint8_t type,
		const uint8_t count,
		const void *value);
void vrs_register_receive_layer_set_value(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id,
		const uint8_t type,
		const uint8_t count,
		const void *value));

int vrs_send_layer_unset_value(const uint8_t session_id,
		const uint8_t prio,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id);
void vrs_register_receive_layer_unset_value(void (*func)(const uint8_t session_id,
		const uint32_t node_id,
		const uint16_t layer_id,
		const uint32_t item_id));

#if defined __cplusplus
}
#endif

#endif

