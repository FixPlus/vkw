
# automatic setup of platform specific macro
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
elseif (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_XLIB_KHR -DVK_USE_PLATFORM_XCB_KHR")
elseif (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK -DVK_EXAMPLE_XCODE_GENERATED")
else ()
    message(FATAL "Unsupported platform")
endif ()

# Boost dependencies
find_package(Boost 1.81 REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

#Vulkan include dependencies

find_package(Vulkan 1.3 REQUIRED COMPONENTS SPIRV-Tools)

# FIXME: this works only on linux now, we need some another way to get spirv-link
add_library(SPIRV_LINK INTERFACE IMPORTED)
target_link_options(SPIRV_LINK INTERFACE "-L$ENV{VULKAN_SDK}/lib")
target_link_libraries(SPIRV_LINK INTERFACE SPIRV-Tools-link SPIRV-Tools-opt SPIRV-Tools)

include_directories(${Vulkan_INCLUDE_DIRS})
