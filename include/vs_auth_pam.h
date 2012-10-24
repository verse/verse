/*
 * $Id: vs_auth_pam.h 1348 2012-09-19 20:08:18Z jiri $
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


#if !defined VS_PAM_H
#define VS_PAM_H

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

int vs_pam_conv(int num_msg,
					const struct pam_message **msg,
					struct pam_response **resp,
					void *appdata_ptr);
int vs_pam_auth_user(struct vContext *C, const char *username, const char *pass);
int vs_pam_end_transaction(struct VSession *vsession);
int vs_pam_start_transaction(struct VSession *vsession, const char *username);

#endif

#endif
