cmake_minimum_required(VERSION 3.16)
project(Lightweight-Embedded-Event-Driven-Platform-Core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
add_executable(core
    src/EventLoop.cpp
    src/main.cpp
    src/Metrics.cpp
    src/Scheduler.cpp
)

# Include directories
target_include_directories(core PRIVATE include)
target_compile_options(core PRIVATE -O2 -Wall -Wextra -Wpedantic)