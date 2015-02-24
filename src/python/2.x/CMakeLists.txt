
# CMakeFile.txt for Python 2.x module

if (CMAKE_COMPILER_IS_GNUCC)
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		set (CMAKE_C_FLAGS "-D_REETRANT -ggdb -fPIC -Wall -Wextra -pedantic -std=c99 -O0")
	elseif( CMAKE_BUILD_TYPE STREQUAL "Release" )
		set (CMAKE_C_FLAGS "-D_REETRANT -DNDEBUG -fPIC -Wall -Wextra -pedantic -std=c99 -O3 -fno-strict-aliasing")
	endif()
endif (CMAKE_COMPILER_IS_GNUCC)

include_directories (../../../include)
include_directories (${PYTHON_INCLUDE_DIRS})

# Modify output directory of library
set (LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib/python2.x)

set (verse_module_src ../verse_module.c)

# Basic libraries used by verse Python module
set ( verse_python_libs ${CMAKE_THREAD_LIBS_INIT} ${PYTHON_LIBRARIES})

# When OpenSSL is enabled
if (OPENSSL_FOUND)
    set (verse_python_libs ${verse_python_libs} ${OPENSSL_LIBRARIES})
    include_directories (${OPENSSL_INCLUDE_DIR})
endif (OPENSSL_FOUND)

add_library (py2_verse SHARED ${verse_module_src})
set_target_properties (py2_verse PROPERTIES PREFIX "")
if(APPLE)
	set_target_properties (py2_verse PROPERTIES SUFFIX ".so")
endif()
set_target_properties (py2_verse PROPERTIES 
		OUTPUT_NAME "verse"
		clean_direct_output 1)
add_dependencies (py2_verse verse_shared_lib)
target_link_libraries (py2_verse
		verse_shared_lib
		${verse_python_libs})

# Try to get Python prefix for installing Python module
execute_process(COMMAND python2 -c "import sys; sys.stdout.write(sys.prefix)"
    OUTPUT_VARIABLE PYTHON_PREFIX)
# Try to get Python version
execute_process(COMMAND python2 -c "import sys; sys.stdout.write(sys.version[0:3])"
    OUTPUT_VARIABLE PYTHON_VERSION)

install (TARGETS py2_verse
	LIBRARY
	DESTINATION ${PYTHON_PREFIX}/lib${LIBSUFFIX}/python${PYTHON_VERSION}/site-packages)
