
# CMakeFile.txt for Python module

if (CMAKE_COMPILER_IS_GNUCC)
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		set (CMAKE_C_FLAGS "-D_REETRANT -ggdb -fPIC -Wall -Wextra -pedantic -std=c99 -O0")
	elseif( CMAKE_BUILD_TYPE STREQUAL "Release" )
		set (CMAKE_C_FLAGS "-D_REETRANT -DNDEBUG -fPIC -Wall -Wextra -pedantic -std=c99 -O3")
	endif()
endif (CMAKE_COMPILER_IS_GNUCC)

include_directories (../include)
include_directories (${PYTHON_INCLUDE_DIRS})

set (verse_module_src verse_module.c)

# Basic libraries used by verse Python module
set ( verse_python_libs ${CMAKE_THREAD_LIBS_INIT} )

# When OpenSSL is enabled
if (OPENSSL_FOUND)
    set (verse_python_libs ${verse_python_libs} ${OPENSSL_LIBRARIES})
    include_directories (${OPENSSL_INCLUDE_DIR})
endif (OPENSSL_FOUND)

add_library (pyverse SHARED ${verse_module_src})
set_target_properties (pyverse PROPERTIES PREFIX "")
set_target_properties (pyverse PROPERTIES 
		OUTPUT_NAME "verse"
		clean_direct_output 1)
add_dependencies (pyverse verse_shared_lib)
target_link_libraries (pyverse
		verse_shared_lib
		${verse_python_libs})

install (TARGETS pyverse
	LIBRARY
	DESTINATION lib/python${PYTHON_VERSION}/site-packages)
