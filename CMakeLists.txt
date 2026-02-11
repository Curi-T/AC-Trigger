cmake_minimum_required(VERSION 3.31)
project(AC-Trigger)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_CXX_STANDARD 20)

# 使用的是 MSVC，这可以确保它是以 Windows 窗口模式启动（无控制台黑框）
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

add_executable(AC-Trigger WIN32 main.cpp resource/resource.rc)

set_target_properties(AC-Trigger PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

target_link_libraries(AC-Trigger powrprof shell32 user32)