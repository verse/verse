# - Find MongoDB
#
# Find the includes and client library for MongoDB C Driver developed by
# MongoDB Inc.: https://github.com/mongodb/mongo-c-driver 
#
# This module defines:
#
#  MongoDB_INCLUDE_DIR, where to find mongo.h
#  MongoDB_LIBRARIES, the libraries needed to use MongoDB.
#  MongoDB_FOUND, If false, do not try to use MongoDB.
#

find_path(MongoDB_INCLUDE_DIR mongo.h
  /usr/include/
  /usr/local/include/
)

find_library(MongoDB_LIBRARIES NAMES mongoc
  PATHS
  /usr/lib
  /usr/lib64
  /usr/local/lib
  /usr/local/lib64
)

if(MongoDB_INCLUDE_DIR AND MongoDB_LIBRARIES)
  set(MongoDB_FOUND TRUE)
  message(STATUS "Found MongoDB: ${MongoDB_INCLUDE_DIR}, ${MongoDB_LIBRARIES}")
else(MongoDB_INCLUDE_DIR AND MongoDB_LIBRARIES)
  set(MongoDB_FOUND FALSE)
  if (MongoDB_FIND_REQUIRED)
    message(FATAL_ERROR "MongoDB not found.")
  else (MongoDB_FIND_REQUIRED)
    message(STATUS "MongoDB not found.")
  endif (MongoDB_FIND_REQUIRED)
endif(MongoDB_INCLUDE_DIR AND MongoDB_LIBRARIES)

mark_as_advanced(MongoDB_INCLUDE_DIR MongoDB_LIBRARIES)
