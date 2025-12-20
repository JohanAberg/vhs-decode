# GPU Kernel Tests
if(ENABLE_GPU AND OpenCL_FOUND)
    set(GPU_TEST_SOURCES
        test_gpu_kernels.cpp
        ../src/gpu/opencl_context.cpp
        ../src/gpu/opencl_buffer.cpp
        ../src/gpu/kernel_loader.cpp
        ../src/gpu/phase_unwrap_kernel.cpp
        ../src/gpu/envelope_kernel.cpp
        ../src/gpu/filter_kernel.cpp
    )
    
    add_executable(test-gpu-kernels ${GPU_TEST_SOURCES})
    
    target_include_directories(test-gpu-kernels PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    
    target_link_libraries(test-gpu-kernels
        PRIVATE
            OpenCL::OpenCL
            Threads::Threads
    )
    
    add_test(NAME GPUKernels COMMAND test-gpu-kernels)
    
    message(STATUS "Building GPU kernel tests")
else()
    message(STATUS "GPU kernel tests require OpenCL - skipping")
endif()
