cmake_minimum_required(VERSION 3.20)

project(
    cpp_rtos
    VERSION 0.1.0
    DESCRIPTION "Host-testable C++ framework for RTOS models and messaging"
    LANGUAGES CXX
)

option(RTOS_BUILD_TESTS "Build the unit tests" OFF)

add_library(rtos_framework
    src/logging/Logger.cpp
    src/messaging/DispatchPort.cpp
    src/messaging/SubscriptionRegistry.cpp
)
add_library(rtos::framework ALIAS rtos_framework)

target_compile_features(rtos_framework PUBLIC cxx_std_20)
target_include_directories(
    rtos_framework
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(rtos_framework PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(rtos_framework PRIVATE /W4 /permissive-)
endif()

add_executable(rtos_sim apps/rtos_sim/main.cpp)
target_compile_features(rtos_sim PRIVATE cxx_std_20)
target_link_libraries(rtos_sim PRIVATE rtos::framework)

if(RTOS_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
