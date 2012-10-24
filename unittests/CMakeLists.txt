
# CMakeFile.txt for unit tests

if(CMAKE_COMPILER_IS_GNUCC)
	set (CMAKE_C_FLAGS "-D_REETRANT -ggdb -Wall -Wextra -pedantic -O0")
endif(CMAKE_COMPILER_IS_GNUCC)

include_directories (./include)
include_directories (../include)
include_directories (${CHECK_INCLUDE_DIRS})
include_directories (${OPENSSL_INCLUDE_DIR})

set (tests_src
		t_main.c
		common/node_cmds/t_node_create.c
		common/node_cmds/taggroup_cmds/t_taggroup_create.c
		common/node_cmds/t_node_destroy.c)
  
add_executable (tests ${tests_src})
add_dependencies (tests verse_shared_lib)
target_link_libraries (tests
		verse_shared_lib
		${CHECK_LIBRARIES}
		${CMAKE_THREAD_LIBS_INIT}
		${OPENSSL_LIBRARIES})
