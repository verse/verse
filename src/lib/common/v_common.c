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
 * author: Jiri Hnidek <jiri.hnidek@tul.cz>
 *
 */

#include <stdio.h>
#include <stdarg.h>

#include "verse_types.h"

#include "v_common.h"

static uint8 log_print_level = VRS_PRINT_NONE;
static FILE *log_file = NULL;

/**
 * \brief This function tests, if print log could be printed. This is usefull,
 * when we want to call v_print_log_* many times.
 */
int is_log_level(const uint8 level)
{
	return (level <= log_print_level);
}

/**
 * \brief Initialize log level.
 */
void v_init_log_level(const uint8 level)
{
	log_print_level = level;
}

/**
 * \brief Initialize log file.
 */
void v_init_log_file(FILE *file)
{
	log_file = file;
}

/**
 * \brief This function returns current debug log level.
 */
uint8 v_log_level(void)
{
	return log_print_level;
}

/**
 * \brief This function returns pointer at currently used log file.
 */
FILE *v_log_file(void)
{
	return log_file;
}

/**
 * Print message to the log file. The message will be prineted only if current
 * debug level is bigger then the level. The level of debug print is printed
 * before own message. This function could have variable amount of parameters. It
 * behaves similar to the printf().
 * \param[in]	level	The level of debug print
 * \param[in]	*format	The format of string
 */
void v_print_log(const uint8 level, const char *format, ...)
{
	va_list ap;

	if(log_file == NULL) return;

	va_start(ap, format);

	switch(level) {
		case VRS_PRINT_ERROR:
			if( log_print_level>=VRS_PRINT_ERROR ) {
				fprintf(log_file, "ERROR: ");
				vfprintf(log_file, format, ap);
			}
			break;
		case VRS_PRINT_INFO:
			if( log_print_level>=VRS_PRINT_INFO ) {
				fprintf(log_file, "INFO: ");
				vfprintf(log_file, format, ap);
			}
			break;
		case VRS_PRINT_WARNING:
			if( log_print_level>=VRS_PRINT_WARNING ) {
				fprintf(log_file, "WARNING: ");
				vfprintf(log_file, format, ap);
			}
			break;
		case VRS_PRINT_DEBUG_MSG:
			if( log_print_level>=VRS_PRINT_DEBUG_MSG ) {
				fprintf(log_file, "DEBUG: ");
				vfprintf(log_file, format, ap);
			}
			break;
	}

	va_end(ap);
}

/**
 * \brief This function is similar to the v_print_log(), but debug level is not
 * printed before own message.
 * */
void v_print_log_simple(const uint8 level, const char *format, ...)
{
	va_list ap;

	if(log_file == NULL) return;

	va_start(ap, format);

	switch(level) {
		case VRS_PRINT_ERROR:
			if( log_print_level>=VRS_PRINT_ERROR ) {
				vfprintf(log_file, format, ap);
			}
			break;
		case VRS_PRINT_INFO:
			if( log_print_level>=VRS_PRINT_INFO ) {
				vfprintf(log_file, format, ap);
			}
			break;
		case VRS_PRINT_WARNING:
			if( log_print_level>=VRS_PRINT_WARNING ) {
				vfprintf(log_file, format, ap);
			}
			break;
		case VRS_PRINT_DEBUG_MSG:
			if(log_print_level>=VRS_PRINT_DEBUG_MSG) {
				vfprintf(log_file, format, ap);
			}
			break;
	}

	va_end(ap);
}
