/*
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

#include <stdio.h>
#include <stdlib.h>
#include <wslay.h>

#include "vs_main.h"
#include "vs_websocket.h"
#include "v_common.h"

/**
 * \brief The function with websocket infinite loop
 */
void *vs_websocket_loop(void *arg)
{
	struct vContext *C = (struct vContext*)arg;

end:
	v_print_log(VRS_PRINT_DEBUG_MSG, "Exiting WebSocket thread\n");

	pthread_exit(NULL);
	return NULL;
}
