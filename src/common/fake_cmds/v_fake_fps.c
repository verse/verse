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

#include <stdlib.h>

#include "v_common.h"
#include "v_fake_commands.h"

/**
 * \brief This function prints content of command used for negotiation of FPS
 */
void v_fake_fps_print(const unsigned char level, struct Fps_Cmd *fps_cmd)
{
	v_print_log_simple(level, "\tFPS: fps: %6.3f\n",
			fps_cmd->fps);
}

/**
 * \brief This function initialize members of structure for Fps command
 */
void v_Fps_init(struct Fps_Cmd *fps_cmd,
		real32 fps,
		uint32 seconds,
		uint32 useconds)
{
	if(fps_cmd != NULL) {
		/* initialize members with values */
		fps_cmd->id = FAKE_CMD_FPS;
		fps_cmd->fps = fps;
		fps_cmd->seconds = seconds;
		fps_cmd->useconds = useconds;
	}
}

/**
 * \brief this function creates new structure of Connect_Accept command
 */
struct Generic_Cmd *v_Fps_create(real32 fps,
		uint32 seconds,
		uint32 useconds)
{
	struct Generic_Cmd *fps_cmd = NULL;
	fps_cmd = (struct Generic_Cmd*)calloc(1, sizeof(struct Fps_Cmd));
	v_Fps_init((struct Fps_Cmd *)fps_cmd, fps, seconds, useconds);
	return fps_cmd;
}

/**
 * \brief This function clear members of structure for Connect_Accept command
 */
void v_Fps_clear(struct Fps_Cmd *fps_cmd)
{
	if(fps_cmd != NULL) {
		fps_cmd->fps = 0.0;
		fps_cmd->seconds = 0;
		fps_cmd->useconds = 0;
	}
}

/**
 * \brief This function destroy Connect_Accept command
 */
void v_Fps_destroy(struct Fps_Cmd **fps_cmd)
{
	if(fps_cmd != NULL) {
		v_Fps_clear(*fps_cmd);
		free(*fps_cmd);
		*fps_cmd = NULL;
	}
}
