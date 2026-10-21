cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(InsiriusModMenu VERSION 2.0.0)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# ImGui for the menu (works with mouse on PC and touch on Android)
set(IMGUI_VERSION "v1.92.4")
set(IMGUI_HOOK_EARLY OFF)   # draw on top of everything, we are a mod menu
CPMAddPackage("gh:matcool/gd-imgui-cocos#b93f08c")
target_link_libraries(${PROJECT_NAME} imgui-cocos)

setup_geode_mod(${PROJECT_NAME})
