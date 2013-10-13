
# CMakeFile.txt for Python 3.x module

if (CMAKE_COMPILER_IS_GNUCC)
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		set (CMAKE_C_FLAGS "-D_REETRANT -ggdb -fPIC -Wall -Wextra -pedantic -std=c99 -O0")
	elseif( CMAKE_BUILD_TYPE STREQUAL "Release" )
		set (CMAKE_C_FLAGS "-D_REETRANT -DNDEBUG -fPIC -Wall -Wextra -pedantic -std=c99 -O3")
	endif()
endif (CMAKE_COMPILER_IS_GNUCC)

include_directories (../../include)
include_directories (BEFORE ${PYTHON_INCLUDE_DIRS})

# Modify output directory of library
set (LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib/python3.x)

set (verse_module_src ../verse_module.c)

# Basic libraries used by verse Python module
set ( verse_python_libs ${CMAKE_THREAD_LIBS_INIT} ${PYTHON_LIBRARIES})

# When OpenSSL is enabled
if (OPENSSL_FOUND)
    set (verse_python_libs ${verse_python_libs} ${OPENSSL_LIBRARIES})
    include_directories (${OPENSSL_INCLUDE_DIR})
endif (OPENSSL_FOUND)

add_library (py3_verse SHARED ${verse_module_src})
set_target_properties (py3_verse PROPERTIES PREFIX "")
set_target_properties (py3_verse PROPERTIES 
		OUTPUT_NAME "verse"
		clean_direct_output 1)
add_dependencies (py3_verse verse_shared_lib)
target_link_libraries (py3_verse
		verse_shared_lib
		${verse_python_libs})

install (TARGETS py3_verse
	LIBRARY
	DESTINATION lib/python${PYTHON_VERSION}/site-packages)
