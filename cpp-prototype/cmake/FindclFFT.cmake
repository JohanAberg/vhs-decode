# FindclFFT.cmake - Locate clFFT library and headers
#
# This module defines:
#  clFFT_FOUND - system has clFFT
#  clFFT_INCLUDE_DIRS - the clFFT include directories
#  clFFT_LIBRARIES - link these to use clFFT
#  clFFT::clFFT - imported target for clFFT

find_path(clFFT_INCLUDE_DIR
    NAMES clFFT.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/clFFT/include
        $ENV{CLFFT_ROOT}/include
        $ENV{AMDAPPSDKROOT}/include
        "C:/Program Files/clFFT/*/include"
        "C:/Program Files (x86)/clFFT/*/include"
    PATH_SUFFIXES
        clFFT
)

find_library(clFFT_LIBRARY
    NAMES clFFT
    PATHS
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
        /opt/clFFT/lib
        $ENV{CLFFT_ROOT}/lib
        $ENV{CLFFT_ROOT}/lib64
        $ENV{AMDAPPSDKROOT}/lib/x86_64
        "C:/Program Files/clFFT/*/lib"
        "C:/Program Files (x86)/clFFT/*/lib"
    PATH_SUFFIXES
        clFFT
        x64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(clFFT
    REQUIRED_VARS clFFT_LIBRARY clFFT_INCLUDE_DIR
)

if(clFFT_FOUND)
    set(clFFT_LIBRARIES ${clFFT_LIBRARY})
    set(clFFT_INCLUDE_DIRS ${clFFT_INCLUDE_DIR})
    
    if(NOT TARGET clFFT::clFFT)
        add_library(clFFT::clFFT UNKNOWN IMPORTED)
        set_target_properties(clFFT::clFFT PROPERTIES
            IMPORTED_LOCATION "${clFFT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${clFFT_INCLUDE_DIR}"
        )
    endif()
    
    mark_as_advanced(clFFT_INCLUDE_DIR clFFT_LIBRARY)
endif()
