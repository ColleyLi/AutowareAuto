# Find headers and libraries
find_path(
    TinyXML2_INCLUDE_DIR
    NAMES tinyxml2.h
    PATH_SUFFIXES "tinyxml2" ${TinyXML2_INCLUDE_PATH})
find_library(
    TinyXML2_LIBRARY
    NAMES tinyxml2
    PATH_SUFFIXES "tinyxml2" ${TinyXML2_LIBRARY_PATH})

mark_as_advanced(TinyXML2_INCLUDE_DIR TinyXML2_LIBRARY)

# Output variables generation
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TinyXML2 DEFAULT_MSG TinyXML2_LIBRARY TinyXML2_INCLUDE_DIR)

set(TinyXML2_FOUND ${TINYXML2_FOUND}) # Enforce case-correctness: Set appropriately cased variable...
unset(TINYXML2_FOUND) # ...and unset uppercase variable generated by find_package_handle_standard_args

if(TinyXML2_FOUND)
    set(TinyXML2_INCLUDE_DIRS ${TinyXML2_INCLUDE_DIR})
    set(TinyXML2_LIBRARIES ${TinyXML2_LIBRARY})
endif()
