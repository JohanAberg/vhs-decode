# FindOpenCL.cmake - Locate OpenCL library and headers
#
# This module defines:
#  OpenCL_FOUND - system has OpenCL
#  OpenCL_INCLUDE_DIRS - the OpenCL include directories
#  OpenCL_LIBRARIES - link these to use OpenCL
#  OpenCL::OpenCL - imported target for OpenCL

find_path(OpenCL_INCLUDE_DIR
    NAMES CL/cl.h OpenCL/cl.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/intel/opencl/include
        $ENV{CUDA_PATH}/include
        $ENV{AMDAPPSDKROOT}/include
        "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/*/include"
    PATH_SUFFIXES
        opencl
)

find_library(OpenCL_LIBRARY
    NAMES OpenCL
    PATHS
        /usr/lib
        /usr/local/lib
        /usr/lib/x86_64-linux-gnu
        /opt/intel/opencl/lib
        $ENV{CUDA_PATH}/lib/x64
        $ENV{CUDA_PATH}/lib64
        $ENV{AMDAPPSDKROOT}/lib/x86_64
        "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/*/lib/x64"
    PATH_SUFFIXES
        opencl
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCL
    REQUIRED_VARS OpenCL_LIBRARY OpenCL_INCLUDE_DIR
)

if(OpenCL_FOUND)
    set(OpenCL_LIBRARIES ${OpenCL_LIBRARY})
    set(OpenCL_INCLUDE_DIRS ${OpenCL_INCLUDE_DIR})
    
    if(NOT TARGET OpenCL::OpenCL)
        add_library(OpenCL::OpenCL UNKNOWN IMPORTED)
        set_target_properties(OpenCL::OpenCL PROPERTIES
            IMPORTED_LOCATION "${OpenCL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenCL_INCLUDE_DIR}"
        )
    endif()
    
    mark_as_advanced(OpenCL_INCLUDE_DIR OpenCL_LIBRARY)
endif()
