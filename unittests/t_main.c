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
#include <stdio.h>
#include <check.h>
#include <getopt.h>

#include "verse_types.h"
#include "t_suites.h"

/**
 * \brief Master blind test suite
 */
static struct Suite *verse_master_suite(void)
{
	struct Suite *master_suite = suite_create("Master_Suite");

	return master_suite;
}

/**
 * \brief This function print help of test command (options and
 * parameters )
 */
static void print_help(char *prog_name)
{
	printf("\n Usage: %s [OPTION...]\n\n", prog_name);
	printf("  This program runs unit tests of verse library\n\n");
	printf("  Options:\n");
	printf("   -h               display this help and exit\n");
	printf("   -d debug_level   use debug level [none|info|error|warning|debug]\n\n");
}

/**
 * \brief This function set debug level of verse library
 */
static int set_debug_level(char *debug_level)
{
	int ret;
	if( strcmp(debug_level, "debug") == 0) {
		ret = vrs_set_debug_level(VRS_PRINT_DEBUG_MSG);
		return ret;
	} else if( strcmp(debug_level, "warning") == 0 ) {
		ret = vrs_set_debug_level(VRS_PRINT_WARNING);
		return ret;
	} else if( strcmp(debug_level, "error") == 0 ) {
		ret = vrs_set_debug_level(VRS_PRINT_ERROR);
		return ret;
	} else if( strcmp(debug_level, "info") == 0 ) {
		ret = vrs_set_debug_level(VRS_PRINT_INFO);
		return ret;
	} else if( strcmp(debug_level, "none") == 0 ) {
		ret = vrs_set_debug_level(VRS_PRINT_NONE);
		return ret;
	} else {
		printf("Unsupported debug level: %s\n", debug_level);
		return VRS_FAILURE;
	}

	return 0;
}


/**
 * \brief Main function of unit tests
 */
int main(int argc, char *argv[])
{
	int opt, ret, number_failed;
	SRunner *master_sr = srunner_create(verse_master_suite());
	srunner_add_suite(master_sr, node_create_suite());
	srunner_add_suite(master_sr, node_destroy_suite());
	srunner_add_suite(master_sr, taggroup_create_suite());
	srunner_add_suite(master_sr, negotiate_suite());

	/* When client was started with some arguments */
	if(argc>1) {
		/* Parse all options */
		while( (opt = getopt(argc, argv, "hd:")) != -1) {
			switch(opt) {
				case 'd':
					ret = set_debug_level(optarg);
					if(ret != VRS_SUCCESS) {
						print_help(argv[0]);
						exit(EXIT_FAILURE);
					}
					break;
				case 'h':
					print_help(argv[0]);
					exit(EXIT_SUCCESS);
				case ':':
					exit(EXIT_FAILURE);
				case '?':
					exit(EXIT_FAILURE);
					break;
			}
		}
	}

	srunner_run_all(master_sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(master_sr);
	srunner_free(master_sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
