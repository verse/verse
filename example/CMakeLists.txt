
# Cmake file for example of verse client

# Build flags
if (CMAKE_COMPILER_IS_GNUCC)
    set (CMAKE_C_FLAGS "-D_REETRANT -Wall -Wextra -pedantic -Wno-long-long")
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
   	    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -O0 --coverage")
	elseif( CMAKE_BUILD_TYPE STREQUAL "Release" )
	    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG -O3")
	endif ()
endif (CMAKE_COMPILER_IS_GNUCC)


# Source code
set (verse_client_src verse_client.c)


# Directory with required .h files
include_directories (../include)


# Basic libraries used by verse_client
set ( verse_client_libs ${CMAKE_THREAD_LIBS_INIT} )


# When OpenSSL is enabled
if (OPENSSL_FOUND)
    set (verse_client_libs ${verse_client_libs} ${OPENSSL_LIBRARIES})
    include_directories (${OPENSSL_INCLUDE_DIR})
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWITH_OPENSSL")
endif (OPENSSL_FOUND)


# Define executable
add_executable (verse_client ${verse_client_src})
add_dependencies (verse_client verse_shared_lib)
target_link_libraries (verse_client
		verse_shared_lib
		${verse_client_libs})
