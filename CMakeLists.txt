
# Main CMakeFile.txt

# Minimal version of CMake
cmake_minimum_required (VERSION 2.6)

# Build type
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to 'Debug' as none was specified.")
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
		"MinSizeRel" "RelWithDebInfo")
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
option(VERSE_CLANG "Use Clang Compiler" OFF)
option(VERSE_PYTHON_MODULE "Verse Module" ON)
option(VERSE_CHECK "Check Unit Tests" ON)
option(VERSE_DOXYGEN "Doxygen Documentation" ON)
option(VERSE_WEBSOCKET "Support for WebSocket" ON)

# Clang compiler
if (VERSE_CLANG)
	set (CMAKE_C_COMPILER "/usr/bin/clang")
	set (CMAKE_CC_COMPILER "/usr/bin/clang++")
endif (VERSE_CLANG)

# Set up directory with 3rd party cmake modules
set (CMAKE_MODULE_PATH
	${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/build_files/cmake/modules/")

# Pthread is *prefered* (only supported) thread library
set (CMAKE_THREAD_PREFER_PTHREAD)

# Try to find required packages
find_package (OpenSSL REQUIRED)
find_package (Threads REQUIRED)
find_package (Iniparser REQUIRED)

# Set output directory for libraries
set (LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib/)

# Set output directory for binaries
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/)

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

# Python module
if (VERSE_PYTHON_MODULE)

	# Try to find optional package Python
	if (UNIX AND NOT APPLE)
		# find_package (PythonLibs 3) # Does not work at Linux for Python 3
		find_package (PythonLibsUnix 3)
	else ()
		find_package (PythonLibs 3)
	endif ()
	
	# Set up optional subdirectory for Python 3
	if (PYTHON_LIBRARY)
		add_subdirectory (python)
	elseif (PYTHONLIBS_FOUND)
		add_subdirectory (python)
	endif ()
	
	# Use only this, when CMake module for Python is fixed for Linux platform
	#if (PYTHONLIBS_FOUND)
	#	add_subdirectory (python)
	#endif (PYTHONLIBS_FOUND)
	
endif (VERSE_PYTHON_MODULE)

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
	CONFIGURATIONS "Debug"
	DESTINATION include)

# Install configuration file
install (FILES "${PROJECT_SOURCE_DIR}/config/server.ini"
	DESTINATION etc)


message ("")
message ("* Verse Configuration *")
message ("")

message (" * Configuration: ${CMAKE_BUILD_TYPE}")
	
if (PYTHON_LIBRARY)
	message (" * Python:        ON")
elseif (PYTHONLIBS_FOUND)
	message (" * Python:        ON")
else ()
	message (" * Python:        OFF")
endif ()

if (CHECK_FOUND)
	message (" * Check:         ON")
else ()
	message (" * Check:         OFF")
endif (CHECK_FOUND)

if (WSLAY_FOUND)
	message (" * WebSocket:     ON")
else ()
	message (" * WebSocket:     OFF")
endif (WSLAY_FOUND)

if (DOXYGEN_FOUND)
	message (" * Doxygen:       ON")
else ()
	message (" * Doxygen:       OFF")
endif (DOXYGEN_FOUND)

message ("")
