# This file is exported for managing transitive dependencies of this library.

# automatic setup of platform specific macro
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
elseif(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XLIB_KHR -DVK_USE_PLATFORM_XCB_KHR")
elseif(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK -DVK_EXAMPLE_XCODE_GENERATED")
else()
    message(FATAL "Unsupported platform")
endif()

# boost import using cmake module FindBoost is deprecated in cmake 3.30, suppress warning about that.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

# Boost is used primarily for containers.
find_package(Boost 1.80 REQUIRED)

# Vulkan include dependencies
find_package(Vulkan 1.4.328 REQUIRED COMPONENTS SPIRV-Tools)
include_directories(${Vulkan_INCLUDE_DIRS})
