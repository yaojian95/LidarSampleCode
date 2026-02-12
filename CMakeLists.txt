cmake_minimum_required(VERSION 3.10)

project(VzCoalSample)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Fix encoding issues with MSVC (C4819 and newline eating)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# SDK Path
set(SDK_ROOT "E:/yaojian/lidar_profiling_SDK/SDK/VzCoalAPI")
set(SDK_INC "${SDK_ROOT}/Inc")
set(SDK_LIB_DIR "${SDK_ROOT}/Windows/x64/Release")

include_directories(${SDK_INC})
link_directories(${SDK_LIB_DIR})

add_executable(VzCoalSample main.cpp)

# Link against the necessary libraries
target_link_libraries(VzCoalSample VzCoalDevice)

# Copy DLLs to output directory for convenience
add_custom_command(TARGET VzCoalSample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${SDK_LIB_DIR}
    $<TARGET_FILE_DIR:VzCoalSample>
)
