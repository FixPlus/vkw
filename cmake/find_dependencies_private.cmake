include("${CMAKE_CURRENT_LIST_DIR}/find_dependencies.cmake")

# Vulkan::SPIRV-Tools target from sdk is weird - it does not include opt and link components, plus
# it does not account for debug versions on msvc (the ones with 'd.lib' suffix).
# Following is manual fix for both issues.
get_target_property(VKW_SPIRV_LIB Vulkan::SPIRV-Tools IMPORTED_LOCATION_RELEASE)

if(NOT VKW_SPIRV_LIB)
    get_target_property(VKW_SPIRV_LIB Vulkan::SPIRV-Tools IMPORTED_LOCATION_DEBUG)
endif()

if(NOT VKW_SPIRV_LIB)
    message(FATAL "Vulkam::SPIRV-Tools imported target does not define imported location")
endif()

if(MSVC AND CMAKE_BUILD_TYPE STREQUAL Debug)
    string(REPLACE "SPIRV-Tools" "SPIRV-Toolsd" VKW_SPIRV_LIB ${VKW_SPIRV_LIB})
endif()

string(REPLACE "SPIRV-Tools" "SPIRV-Tools-opt" VKW_SPIRV_OPT_LIB ${VKW_SPIRV_LIB})
string(REPLACE "SPIRV-Tools" "SPIRV-Tools-link" VKW_SPIRV_LINK_LIB ${VKW_SPIRV_LIB})

add_library(VKW_SPIRV_TOOLS_BASE STATIC IMPORTED)
add_library(VKW_SPIRV_TOOLS_OPT STATIC IMPORTED)
add_library(VKW_SPIRV_TOOLS_LINK STATIC IMPORTED)
set_property(TARGET VKW_SPIRV_TOOLS_BASE PROPERTY IMPORTED_LOCATION ${VKW_SPIRV_LIB})
set_property(TARGET VKW_SPIRV_TOOLS_OPT PROPERTY IMPORTED_LOCATION ${VKW_SPIRV_OPT_LIB})
set_property(TARGET VKW_SPIRV_TOOLS_LINK PROPERTY IMPORTED_LOCATION ${VKW_SPIRV_LINK_LIB})

add_library(VKW_SPIRV_TOOLS INTERFACE)
target_link_libraries(VKW_SPIRV_TOOLS INTERFACE VKW_SPIRV_TOOLS_LINK VKW_SPIRV_TOOLS_OPT VKW_SPIRV_TOOLS_BASE)
message(STATUS "Using SPIRV-Tools library installed here -> ${VKW_SPIRV_LIB}")

find_program(PYTHON_EXE NAMES python python3 py REQUIRED)
