
# CMakeFile.txt for unit tests

if(CMAKE_COMPILER_IS_GNUCC)
	set (CMAKE_C_FLAGS "-D_REETRANT -ggdb -Wall -Wextra -pedantic -O0 --coverage -fno-strict-aliasing")
endif(CMAKE_COMPILER_IS_GNUCC)

include_directories (./include)
include_directories (../include)
include_directories (${CHECK_INCLUDE_DIRS})

set (tests_src
		t_main.c
		common/node_cmds/t_node_create.c
		common/node_cmds/taggroup_cmds/t_taggroup_create.c
		common/node_cmds/t_node_destroy.c
		common/sys_cmds/t_negotiate.c
		common/pack_unpack/t_pack.c
		common/pack_unpack/t_unpack.c)

# Basic libraries used by test executable
set ( verse_test_libs ${CHECK_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} )

# When OpenSSL is enabled
if (OPENSSL_FOUND)
    set (verse_test_libs ${verse_test_libs} ${OPENSSL_LIBRARIES})
    include_directories (${OPENSSL_INCLUDE_DIR})
endif (OPENSSL_FOUND)
  
add_executable (tests ${tests_src})
add_dependencies (tests verse_shared_lib)
target_link_libraries (tests
		verse_shared_lib
		${verse_test_libs})
