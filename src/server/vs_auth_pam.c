/*
 * $Id: vs_auth_pam.c 1348 2012-09-19 20:08:18Z jiri $
 * 
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Contributor(s): Jiri Hnidek <jiri.hnidek@tul.cz>.
 *
 */

#if defined WITH_PAM

#if defined  __APPLE__
/* MAC OS X */
#include <pam/pam_appl.h>
#include <pam/pam_misc.h>
#elif defined __linux__
/* Linux */
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#else
/* Other UNIX like operation systems */
#endif

#include <stdlib.h>
#include <string.h>

#include "v_session.h"
#include "v_common.h"

#include "vs_auth_pam.h"

/* Verse server conversation function with PAM */
int vs_pam_conv(int num_msg,
					const struct pam_message **msg,
					struct pam_response **resp,
					void *appdata_ptr)
{
	struct pam_response *r;
	int i;

	*resp = NULL;

	if( appdata_ptr == NULL )
		return PAM_CONV_ERR;

	v_print_log(VRS_PRINT_DEBUG_MSG, "PAM: %s called with %d messages\n", __FUNCTION__, num_msg);

	if( (num_msg <= 0) || (num_msg > PAM_MAX_NUM_MSG))
		return PAM_CONV_ERR;

	if( (r=(struct pam_response*)calloc(num_msg, sizeof(struct pam_response))) == NULL)
		return PAM_BUF_ERR;

	for(i=0; i<num_msg; i++) {
		r[i].resp = NULL;
		r[i].resp_retcode = 0;

		switch(msg[i]->msg_style) {
			case PAM_PROMPT_ECHO_ON:
				v_print_log(VRS_PRINT_DEBUG_MSG, "PAM: %s:%d PAM_PROMPT_ECHO_ON %s\n",
						__FUNCTION__, __LINE__, msg[i]->msg);
				break;
			case PAM_PROMPT_ECHO_OFF:
				v_print_log(VRS_PRINT_DEBUG_MSG, "PAM: %s:%d PAM_PROMPT_ECHO_OFF %s\n",
						__FUNCTION__, __LINE__, msg[i]->msg);
				if(strncmp("Password:",msg[i]->msg,9)==0) {
					r[i].resp = strdup((char*)appdata_ptr);
					if(r[i].resp == NULL)
						goto fail;
					r[i].resp_retcode = PAM_SUCCESS;
				}
				break;
			case PAM_ERROR_MSG:
				v_print_log(VRS_PRINT_DEBUG_MSG, "PAM: %s:%d PAM_ERROR_MSG %s\n",
						__FUNCTION__, __LINE__, msg[i]->msg);
				break;
			case PAM_TEXT_INFO:
				v_print_log(VRS_PRINT_DEBUG_MSG, "PAM: %s:%d PAM_TEXT_INFO %s\n",
						__FUNCTION__, __LINE__, msg[i]->msg);
				break;
			default:
				goto fail;
		}
	}
	*resp = r;

	return PAM_SUCCESS;
fail:
	v_print_log(VRS_PRINT_DEBUG_MSG, "PAM: fail\n");

	for(i=0; i < num_msg; i++) {
		if (r[i].resp != NULL) {
			memset(r[i].resp, 0, strlen(r[i].resp));
			free(r[i].resp);
			r[i].resp = NULL;
		}
	}

	memset(r, 0, num_msg*sizeof(*r));
	free(r);
	r = NULL;

	*resp = NULL;

	return PAM_CONV_ERR;
}

/* NULL Verse server conversation function with PAM */
static int vs_pam_null_conv(int num_msg, const struct pam_message **msg,
    struct pam_response **resp, void *data)
{
	v_print_log(VRS_PRINT_DEBUG_MSG, "PAM: %s called with %d messages\n", __FUNCTION__, num_msg);
	return PAM_CONV_ERR;
}

static struct pam_conv null_conv = { vs_pam_null_conv, NULL };

/* Terminate PAM transaction */
static int vs_pam_end_user_auth(struct VSession *vsession, int retval)
{
	int ret;

	vsession->conv.appdata_ptr = NULL;

	if((ret = pam_set_item(vsession->pamh, PAM_CONV, (const void *)&null_conv)) != PAM_SUCCESS) {
		v_print_log(VRS_PRINT_ERROR, "pam_set_item(): failed to set up null conversation function.\n");
	} else {
		v_print_log(VRS_PRINT_ERROR, "pam_set_item(): successfully set up null conversation function.\n");
	}

	if((ret = pam_end(vsession->pamh, retval)) != PAM_SUCCESS) {
		v_print_log(VRS_PRINT_ERROR, "pam_end(): failed to release PAM authenticator.\n");
	} else {
		v_print_log(VRS_PRINT_DEBUG_MSG, "pam_end(): successfully released PAM authenticator.\n");
	}

	vsession->pamh = NULL;

	return ret;
}

/**
 * \brief Try to authentication user using PAM. Only password could be use now.
 */
int vs_pam_auth_user(struct vContext *C, const char *username, const char *pass)
{
	struct VSession *vsession = CTX_current_session(C);
	int retval;

	/* Use password for user authentication */
	vsession->conv.appdata_ptr = (void*)pass;

	/* Try to initialization PAM transaction */
	if((retval = pam_start("verse", username, &vsession->conv, &vsession->pamh)) != PAM_SUCCESS) {
		v_print_log(VRS_PRINT_ERROR, "pam_start() failed: %s\n", pam_strerror(vsession->pamh, retval));
		vs_pam_end_user_auth(vsession, retval);
		return -1;
	}

	/* Try to setup remote host for PAM */
	if((retval = pam_set_item(vsession->pamh, PAM_RHOST, vsession->peer_hostname))) {
		v_print_log(VRS_PRINT_ERROR, "pam_set_item(): hostname: %s failed: %s\n",
				vsession->peer_hostname, pam_strerror(vsession->pamh, retval));
		vs_pam_end_user_auth(vsession, retval);
		return -1;
	}

	/* Authenticate user: is the username in the "database" of valid usernames? */
	if((retval = pam_authenticate(vsession->pamh, 0)) != PAM_SUCCESS) {
		v_print_log(VRS_PRINT_ERROR, "pam_authenticate(): username: %s failed: %s\n", username, pam_strerror(vsession->pamh, retval));
		vs_pam_end_user_auth(vsession, retval);
		return -1;
	}

	/* Is user's account valid? */
	if((retval = pam_acct_mgmt(vsession->pamh, 0)) == PAM_SUCCESS) {
		v_print_log(VRS_PRINT_DEBUG_MSG, "pam_acct_mgmt(): username: %s: %s\n", username, pam_strerror(vsession->pamh, retval));
		vs_pam_end_user_auth(vsession, retval);
		/* TODO: get UID and return it */
		return -1;
	} else {
		v_print_log(VRS_PRINT_ERROR, "pam_cct_mgmt(): username: %s failed: %s\n", username, pam_strerror(vsession->pamh, retval));
		vs_pam_end_user_auth(vsession, retval);
		return -1;
	}

	vs_pam_end_user_auth(vsession, retval);

	return -1;
}

#endif

