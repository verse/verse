#------------------------------------------------------------------------------
# Desc:
# Tabs: 3
#
# Copyright (c) 2007-2008 Novell, Inc. All Rights Reserved.
#
# This program and the accompanying materials are made available 
# under, alternatively, the terms of:  a) the Eclipse Public License v1.0
# which accompanies this distribution, and is available at 
# http://www.eclipse.org/legal/epl-v10.html; or, b) the Apache License,
# Version 2.0 which accompanies this distribution and is available at
# www.opensource.org/licenses/apache2.0.php.
#
# To contact Novell about this file by physical or electronic mail, 
# you may find current contact information at www.novell.com. 
#
# Author: Andrew Hodgkinson <ahodgkinson@novell.com>
#------------------------------------------------------------------------------

# Locate Kerberos files

if( NOT KERBEROS_FOUND)

	if( UNIX)
	
		find_path( KERBEROS_INCLUDE_DIR krb5.h 
			PATHS	/usr
					/usr/local
			PATH_SUFFIXES include
			NO_DEFAULT_PATH
		)
		MARK_AS_ADVANCED( KERBEROS_INCLUDE_DIR)
			
		find_library( KERBEROS_LIBRARY 
			NAMES krb5
			PATHS	/usr
					/usr/local
			PATH_SUFFIXES lib lib64
			NO_DEFAULT_PATH
		) 
		MARK_AS_ADVANCED( KERBEROS_LIBRARY)
	
		if( KERBEROS_INCLUDE_DIR)
			if( KERBEROS_LIBRARY)
				set( KERBEROS_FOUND TRUE)
				set( KERBEROS_INCLUDE_DIRS ${KERBEROS_INCLUDE_DIR})
				set( KERBEROS_LIBRARIES ${KERBEROS_LIBRARY})
			endif( KERBEROS_LIBRARY)
		endif( KERBEROS_INCLUDE_DIR)
	
	endif( UNIX)
	
	if( KERBEROS_FOUND)
		if( NOT KERBEROS_FIND_QUIETLY)
			message( STATUS "Found Kerberos libraries: ${KERBEROS_LIBRARIES}")
			message( STATUS "Found Kerberos inc dirs: ${KERBEROS_INCLUDE_DIRS}")
		endif( NOT KERBEROS_FIND_QUIETLY)
	else( KERBEROS_FOUND)
		if( KERBEROS_FIND_REQUIRED)
			message( FATAL_ERROR "Could not find Kerberos")
		else( KERBEROS_FIND_REQUIRED)
			if( NOT KERBEROS_FIND_QUIETLY)
				message( STATUS "Could not find Kerberos")
			endif( NOT KERBEROS_FIND_QUIETLY)
		endif( KERBEROS_FIND_REQUIRED)
	endif( KERBEROS_FOUND)

endif( NOT KERBEROS_FOUND)
