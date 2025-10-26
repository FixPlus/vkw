cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

project(vkw VERSION 0.0.1 LANGUAGES CXX C)

option(VKW_ENABLE_REFERENCE_GUARD "Toggle ReferenceGuard checker. Defaulted OFF" OFF)
option(VKW_ENABLE_EXCEPTIONS "Toggle exception use. Defaulted ON" ON)
option(VKW_ENABLE_EXAMPLE_BUILD "Toggle examples build. Defaulted OFF" OFF)

set(VKW_OPT_NAME_LIST VKW_ENABLE_REFERENCE_GUARD VKW_ENABLE_EXCEPTIONS)

include(cmake/find_dependencies_private.cmake)

set(VKW_RUNTIME_LIB "vkwrt")
set(VKW_PROJECT_BIN "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(VKW_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/vkw)

include_directories(include)
include_directories(external/SPIRV-Reflect)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

string(REPLACE "." ";" VERSION_LIST ${PROJECT_VERSION})
list(GET VERSION_LIST 0 VKW_VERSION_MAJOR)
list(GET VERSION_LIST 1 VKW_VERSION_MINOR)
list(GET VERSION_LIST 2 VKW_VERSION_REV)
configure_file(LibraryVersion.inc.in ${CMAKE_CURRENT_BINARY_DIR}/include/vkw/LibraryVersion.inc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/vkw/LibraryVersion.inc DESTINATION include/vkw)

configure_file(external/SPIRV-Reflect/include/spirv/unified1/spirv.h ${CMAKE_CURRENT_BINARY_DIR}/include/spirv/unified1/spirv.h COPYONLY)
configure_file(external/SPIRV-Reflect/spirv_reflect.h ${CMAKE_CURRENT_BINARY_DIR}/include/spirv_reflect.h COPYONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/spirv/unified1/spirv.h DESTINATION include/spirv/unified1)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/spirv_reflect.h DESTINATION include/)

add_subdirectory(source)

if(VKW_ENABLE_EXAMPLE_BUILD)
    message(STATUS "Building examples...")
    add_subdirectory(test/examples)
endif()

include(cmake/generate_export_configuration.cmake)