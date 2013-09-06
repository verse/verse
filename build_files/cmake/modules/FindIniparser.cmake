# - Try to find Iniparser
# Once done this will define
#
#  INIPARSER_FOUND - system has Iniparser
#  INIPARSER_INCLUDE_DIRS - the Iniparser include directory
#  INIPARSER_LIBRARIES - Link these to use Iniparser
#  INIPARSER_DEFINITIONS - Compiler switches required for using Iniparser
#
#  Copyright (c) 2007 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


find_path(INIPARSER_INCLUDE_DIRS
  NAMES
    iniparser.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
)

find_library(INIPARSER_LIBRARIES
  NAMES
    iniparser
  PATHS
    /usr/lib
    /usr/lib64
    /usr/local/lib
    /usr/local/lib64
    /opt/local/lib
    /sw/lib
)

if (INIPARSER_INCLUDE_DIRS AND INIPARSER_LIBRARIES)
   set(INIPARSER_FOUND TRUE)
endif (INIPARSER_INCLUDE_DIRS AND INIPARSER_LIBRARIES)


# Show the INIPARSER_INCLUDE_DIRS and INIPARSER_LIBRARIES variables
# only in the advanced view
mark_as_advanced(INIPARSER_INCLUDE_DIRS INIPARSER_LIBRARIES)
