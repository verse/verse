# This module tries to find WSLay library and include files
#
# WSLAY_INCLUDE_DIR, where to find wslay.h
# WSLAY_LIBRARIES, the library to link against
# WSLAY_FOUND, IF false, do not try to use WSLay
#
# This currently works probably only for Linux

FIND_PATH ( WSLAY_INCLUDE_DIR wslay/wslay.h
    /usr/local/include
    /usr/include
    /sw/include
)

FIND_LIBRARY ( WSLAY_LIBRARIES wslay
    /usr/local/lib
    /usr/local/lib64
    /usr/lib
    /usr/lib64
    /opt/local/lib
    /sw/lib
)

SET ( WSLAY_FOUND "NO" )
IF ( WSLAY_INCLUDE_DIR )
    IF ( WSLAY_LIBRARIES )
        SET ( WSLAY_FOUND "YES" )
    ENDIF ( WSLAY_LIBRARIES )
ENDIF ( WSLAY_INCLUDE_DIR )

MARK_AS_ADVANCED (
    WSLAY_INCLUDE_DIR
    WSLAY_LIBRARIES
)
