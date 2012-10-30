# This module tries to find Verse library and include files
#
# VERSE_INCLUDE_DIR, where to find verse.h
# VERSE_LIBRARIES, the library to link against
# VERSE_FOUND, IF false, do not try to use Verse
#
# This currently works probably only for Linux

FIND_PATH ( VERSE_INCLUDE_DIR verse.h
    /usr/local/include
    /usr/include
)

FIND_LIBRARY ( VERSE_LIBRARIES verse
    /usr/local/lib
    /usr/lib
)

SET ( VERSE_FOUND "NO" )
IF ( VERSE_INCLUDE_DIR )
    IF ( VERSE_LIBRARIES )
        SET ( VERSE_FOUND "YES" )
    ENDIF ( VERSE_LIBRARIES )
ENDIF ( VERSE_INCLUDE_DIR )
