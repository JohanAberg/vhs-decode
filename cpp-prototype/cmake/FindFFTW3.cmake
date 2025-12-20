# FindFFTW3.cmake - Locate FFTW3 library
# This module defines:
#  FFTW3_FOUND - System has FFTW3
#  FFTW3_INCLUDE_DIRS - FFTW3 include directories
#  FFTW3_LIBRARIES - Libraries needed to use FFTW3
#  FFTW3_VERSION - FFTW3 version

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_FFTW3 QUIET fftw3)
endif()

# Find include directory
find_path(FFTW3_INCLUDE_DIR
    NAMES fftw3.h
    HINTS
        ${PC_FFTW3_INCLUDE_DIRS}
        $ENV{FFTW3_ROOT}/include
        /usr/include
        /usr/local/include
        /opt/local/include
        /opt/homebrew/include
)

# Find library
find_library(FFTW3_LIBRARY
    NAMES fftw3 libfftw3
    HINTS
        ${PC_FFTW3_LIBRARY_DIRS}
        $ENV{FFTW3_ROOT}/lib
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        /opt/homebrew/lib
)

# Get version
if(PC_FFTW3_VERSION)
    set(FFTW3_VERSION ${PC_FFTW3_VERSION})
elseif(FFTW3_INCLUDE_DIR AND EXISTS "${FFTW3_INCLUDE_DIR}/fftw3.h")
    file(STRINGS "${FFTW3_INCLUDE_DIR}/fftw3.h" FFTW3_VERSION_LINE
         REGEX "^#define FFTW_VERSION")
    if(FFTW3_VERSION_LINE)
        string(REGEX REPLACE ".*\"(.*)\".*" "\\1" FFTW3_VERSION "${FFTW3_VERSION_LINE}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFTW3
    REQUIRED_VARS FFTW3_LIBRARY FFTW3_INCLUDE_DIR
    VERSION_VAR FFTW3_VERSION
)

if(FFTW3_FOUND)
    set(FFTW3_LIBRARIES ${FFTW3_LIBRARY})
    set(FFTW3_INCLUDE_DIRS ${FFTW3_INCLUDE_DIR})
    
    if(NOT TARGET FFTW3::fftw3)
        add_library(FFTW3::fftw3 UNKNOWN IMPORTED)
        set_target_properties(FFTW3::fftw3 PROPERTIES
            IMPORTED_LOCATION "${FFTW3_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFTW3_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(FFTW3_INCLUDE_DIR FFTW3_LIBRARY)
