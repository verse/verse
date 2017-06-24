
# CMakeFile.txt for verse server

# Basic source code for verse server (other files with source code can be
# enabled, when verse server is compiled with support of additional libraries)		
set (server_src
		./vs_user.c
		./vs_udp_connect.c
		./vs_tcp_connect.c
		./vs_taggroup.c
		./vs_tag.c
		./vs_node.c
		./vs_node_access.c
		./vs_sys_nodes.c
		./vs_main.c
		./vs_link.c
		./vs_layer.c
		./vs_data.c
		./vs_auth_csv.c
		./vs_handshake.c)


include_directories (../../include)


# Make build flags compiler specific for verse_server
if (CMAKE_COMPILER_IS_GNUCC)
    set (CMAKE_C_FLAGS "-D_REETRANT -Wall -Wextra -pedantic -Wno-long-long")
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
   	    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -O0")
	elseif( CMAKE_BUILD_TYPE STREQUAL "Release" )
	    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG -O3 -fno-strict-aliasing")
	endif ()
endif (CMAKE_COMPILER_IS_GNUCC)


set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D${PROJECT_NAME}_VERSION=\"\\\"${${PROJECT_NAME}_VERSION}.${${PROJECT_NAME}_PATCH_LEVEL}\\\"\"")


# Basic libraries used by verse_server
set ( verse_server_libs ${CMAKE_THREAD_LIBS_INIT} )


# When OpenSSL is enabled
if (OPENSSL_FOUND)
    set (verse_server_libs ${verse_server_libs} ${OPENSSL_LIBRARIES})
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWITH_OPENSSL")
    include_directories (${OPENSSL_INCLUDE_DIR})
endif (OPENSSL_FOUND)


# When WSLay library is enabled
if (WSLAY_FOUND AND OPENSSL_FOUND)
    set (verse_server_libs ${verse_server_libs} ${WSLAY_LIBRARIES})
    set (server_src ${server_src} ./vs_websocket.c)
    include_directories (${WSLAY_INCLUDE_DIR})
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWSLAY")
endif ()


# When iniparser is used
if (INIPARSER_FOUND)
    set (verse_server_libs ${verse_server_libs} ${INIPARSER_LIBRARIES})
    set (server_src ${server_src}  ./vs_config.c)
    include_directories (${INIPARSER_INCLUDE_DIRS})
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWITH_INIPARSER")
endif (INIPARSER_FOUND)


# When MongoDB is enabled
if (MongoDB_FOUND)
    set (verse_server_libs ${verse_server_libs} ${MongoDB_LIBRARIES})
    set (server_src ${server_src} ./mongodb/vs_mongo_main.c
            ./mongodb/vs_mongo_node.c
            ./mongodb/vs_mongo_taggroup.c
            ./mongodb/vs_mongo_layer.c)
    include_directories (${MongoDB_INCLUDE_DIR})
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWITH_MONGODB")
endif (MongoDB_FOUND)


# Set up verse server executable
add_executable (verse_server ${server_src})
add_dependencies (verse_server verse_shared_lib)
target_link_libraries (verse_server
	verse_shared_lib
	${verse_server_libs} )


# The rule to install Verse server
install (TARGETS verse_server
	RUNTIME
	DESTINATION bin)
