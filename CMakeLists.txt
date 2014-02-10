
# Main CMakeFile.txt

# Minimal version of CMake
cmake_minimum_required (VERSION 2.6)

# Build type
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif ()

# Define project name
project (Verse)

# The version number
set (${PROJECT_NAME}_VERSION_MAJOR 0)
set (${PROJECT_NAME}_VERSION_MINOR 1)
set (${PROJECT_NAME}_PATCH_LEVEL 0)

set (${PROJECT_NAME}_VERSION
	"${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}")

# Build options
option(VERSE_OPENSSL "Support for OpenSSL" ON)
option(VERSE_CLANG "Use Clang Compiler" OFF)
option(VERSE_PYTHON2_MODULE "Verse Python2 Module" ON)
option(VERSE_PYTHON3_MODULE "Verse Python3 Module" ON)
option(VERSE_CHECK "Check Unit Tests" ON)
option(VERSE_DOXYGEN "Doxygen Documentation" ON)
option(VERSE_WEBSOCKET "Support for WebSocket" ON)
option(VERSE_INIPARSER "Iniparser library" ON)

# Clang compiler
if (VERSE_CLANG)
	set (CMAKE_C_COMPILER "/usr/bin/clang")
	set (CMAKE_CC_COMPILER "/usr/bin/clang++")
endif (VERSE_CLANG)

# Pthread is *prefered* (only supported) thread library
set (CMAKE_THREAD_PREFER_PTHREAD)

# Try to find required packages
find_package (Threads REQUIRED)

# Set output directory for libraries
set (LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib/)

# Set output directory for binaries
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/)

# Set up directory with 3rd party cmake modules
set (CMAKE_MODULE_PATH
	${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/build_files/cmake/modules/")

# Try to get information about using /lib/ or /lib64/
get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
if (${LIB64} STREQUAL "TRUE")
	set(LIBSUFFIX 64)
else()
	set(LIBSUFFIX "")
endif()

# Check OpenSSL library
if (VERSE_OPENSSL)

	# Try to find OpenSSL library
	find_package (OpenSSL)

endif (VERSE_OPENSSL)


# Check iniparser library
if (VERSE_INIPARSER)

	# Try to find iniparser library
	find_package (Iniparser)

endif (VERSE_INIPARSER)


# Check unit test
if (VERSE_CHECK)

	# Try to find optional package Check (unittests)
	find_package (Check)
	
	# Set up optional subdirectory for Check unittests 
	if (CHECK_FOUND)
		add_subdirectory (unittests)
		# Add custom target: make tests
		add_custom_target (test COMMAND "${PROJECT_BINARY_DIR}/bin/tests"
			DEPENDS tests)
	endif (CHECK_FOUND)

endif (VERSE_CHECK)


# Try to fing Python 2.x module, when Python 2.x is selected
if (VERSE_PYTHON2_MODULE)

	find_package (PythonLibs 2)

	# Set up optional subdirectory for Python 2
	if (PYTHONLIBS_FOUND)
		set (PYTHON_2_FOUND 1)
		add_subdirectory ("python/2.x")
	endif (PYTHONLIBS_FOUND)
endif (VERSE_PYTHON2_MODULE)

# Otherwise try to find Python 3.x module
if (VERSE_PYTHON3_MODULE)

	# Clear variables from previous find module
	if (PYTHONLIBS_FOUND)
		unset(PYTHONLIBS_FOUND)
		unset(PYTHONLIBS_FOUND CACHE)
		unset(PYTHON_LIBRARY)
		unset(PYTHON_LIBRARY CACHE)        
		unset(PYTHON_LIBRARIES)
		unset(PYTHON_LIBRARIES CACHE)
		unset(PYTHON_INCLUDE_PATH)
		unset(PYTHON_INCLUDE_PATH CACHE)
		unset(PYTHON_INCLUDE_DIRS)
		unset(PYTHON_INCLUDE_DIRS CACHE)
	endif (PYTHONLIBS_FOUND)

	# Try to find optional package Python
	if (UNIX AND NOT APPLE)
		# find_package (PythonLibs 3) # Does not work at Linux for Python 3
		find_package (PythonLibsUnix 3)
	else ()
		find_package (PythonLibs 3)
	endif ()

	# Set up optional subdirectory for Python 3
	if (PYTHON_LIBRARY)
		set (PYTHON_3_FOUND 1)
		add_subdirectory ("python/3.x")
	elseif (PYTHONLIBS_FOUND)
		set (PYTHON_3_FOUND 1)
		add_subdirectory ("python/3.x")
	endif ()

endif (VERSE_PYTHON3_MODULE)


# Support for WebSocket
if(VERSE_WEBSOCKET)

	# Try to find optional package libWebSockets
	find_package (WSLay)
	
endif(VERSE_WEBSOCKET)


# Doxygen
if (VERSE_DOXYGEN)

	# Try to find optional package Doxygen (generating documentation)
	find_package (Doxygen)
	
	if (DOXYGEN_FOUND)
		set (DOXYGEN_INPUT "${PROJECT_SOURCE_DIR}/src \\
				${PROJECT_SOURCE_DIR}/include \\
				${PROJECT_SOURCE_DIR}/example")
		configure_file ("${PROJECT_SOURCE_DIR}/doc/doxygen/verse2-cmake.doxyfile"
		"${PROJECT_BINARY_DIR}/verse2-cmake.doxyfile")
		# Add custom target: make doxy
		add_custom_target (doxy COMMAND "doxygen" "verse2-cmake.doxyfile")
	endif (DOXYGEN_FOUND)

endif (VERSE_DOXYGEN)


# Set up required subdirectories
add_subdirectory (src)
add_subdirectory (example)


# Copy ./pki directory to ${PROJECT_BINARY_DIR}
configure_file ("${PROJECT_SOURCE_DIR}/pki/certificate.pem"
		"${PROJECT_BINARY_DIR}/pki/certificate.pem")
configure_file ("${PROJECT_SOURCE_DIR}/pki/private.key.pem"
		"${PROJECT_BINARY_DIR}/pki/private.key.pem")

# Copy ./config directory to ${PROJECT_BINARY_DIR}
configure_file ("${PROJECT_SOURCE_DIR}/config/server.ini"
		"${PROJECT_BINARY_DIR}/config/server.ini")
configure_file ("${PROJECT_SOURCE_DIR}/config/users.csv.example"
		"${PROJECT_BINARY_DIR}/config/users.csv")

# Install verse.h file used for development of Verse clients
install (FILES "${PROJECT_SOURCE_DIR}/include/verse.h"
	DESTINATION include)

# Directory with configuration files
set(VERSE_CONF_DIR "/etc/verse")
 
# Create .cvs file without any valid user
file(WRITE ${PROJECT_BINARY_DIR}/tmp/users.csv "username,password,UID,real name")

# Macro for installing configuration files
function(install_conf src dest)
  if(NOT IS_ABSOLUTE "${src}")
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
  endif()
  get_filename_component(src_name "${src}" NAME)
  if (NOT IS_ABSOLUTE "${dest}")
    set(dest "${CMAKE_INSTALL_PREFIX}/${dest}")
  endif()
  install(CODE "
    if(NOT EXISTS \"\$ENV{DESTDIR}${dest}/${src_name}\")
      #file(INSTALL \"${src}\" DESTINATION \"${dest}\")
      message(STATUS \"Installing: \$ENV{DESTDIR}${dest}/${src_name}\")
      execute_process(COMMAND \${CMAKE_COMMAND} -E copy \"${src}\"
                      \"\$ENV{DESTDIR}${dest}/${src_name}\"
                      RESULT_VARIABLE copy_result
                      ERROR_VARIABLE error_output)
      if(copy_result)
        message(FATAL_ERROR \${error_output})
      endif()
    else()
      message(STATUS \"Skipping  : \$ENV{DESTDIR}${dest}/${src_name}\")
    endif()
  ")
endfunction(install_conf)

# Install "empty" users.csv file
install_conf(${PROJECT_BINARY_DIR}/tmp/users.csv ${VERSE_CONF_DIR})

# Install configuration file of Verse server
install_conf("${PROJECT_SOURCE_DIR}/config/server.ini" ${VERSE_CONF_DIR})

message ("")
message ("* Verse Configuration *")
message ("")

    message (" * Configuration: ${CMAKE_BUILD_TYPE}")
    message (" * Generator:     ${CMAKE_GENERATOR}")

if (OPENSSL_FOUND)
	message (" * OpenSSL:       ON")
else ()
	message (" * OpenSSL:       OFF")
endif (OPENSSL_FOUND)

if (INIPARSER_FOUND)
	message (" * IniParser:     ON")
else ()
	message (" * IniParser:     OFF")
endif (INIPARSER_FOUND)

if (PYTHON_2_FOUND)
	message (" * Python 2:      ON")
else ()
	message (" * Python 2:      OFF")
endif()

if (PYTHON_3_FOUND)
	message (" * Python 3:      ON")
else ()
	message (" * Python 3:      OFF")
endif ()

if (CHECK_FOUND)
	message (" * Check:         ON")
else ()
	message (" * Check:         OFF")
endif (CHECK_FOUND)

if (WSLAY_FOUND AND OPENSSL_FOUND)
	message (" * WebSocket:     ON")
else ()
	message (" * WebSocket:     OFF")
endif ()

if (DOXYGEN_FOUND)
	message (" * Doxygen:       ON")
else ()
	message (" * Doxygen:       OFF")
endif (DOXYGEN_FOUND)

message ("")
