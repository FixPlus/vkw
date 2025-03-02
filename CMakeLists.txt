cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJECT_NAME VkWrapper)

project(${PROJECT_NAME} LANGUAGES CXX)
set(PROJECT_VERSION 3.0)
set(MAIN_LIB_NAME ${PROJECT_NAME})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        set(MAIN_LIB_NAME ${MAIN_LIB_NAME}d)
    endif()
endif()

include(cmake/find_dependencies.cmake)
include(cmake/find_vulkan_sdk.cmake)

option(VKW_ENABLE_REFERENCE_GUARD "Toggle ReferenceGuard checker. Defaulted OFF" OFF)
option(VKW_ENABLE_EXCEPTIONS "Toggle exception use. Defaulted" ON)

if(VKW_ENABLE_REFERENCE_GUARD)
    add_definitions(-DVKW_ENABLE_REFERENCE_GUARD)
endif()

if(VKW_ENABLE_EXCEPTIONS)
    add_definitions(-DVKW_ENABLE_EXCEPTIONS)
endif()

if(MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS 1)

    # add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

include_directories(include)
string(REPLACE "." ";" VERSION_LIST ${PROJECT_VERSION})
list(GET VERSION_LIST 0 VKW_VERSION_MAJOR)
list(GET VERSION_LIST 1 VKW_VERSION_MINOR)

configure_file(LibraryVersion.inc.in ${CMAKE_CURRENT_BINARY_DIR}/include/vkw/LibraryVersion.inc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/vkw/LibraryVersion.inc DESTINATION include/vkw)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
add_subdirectory(source)

include(cmake/generate_export_configuration.cmake)