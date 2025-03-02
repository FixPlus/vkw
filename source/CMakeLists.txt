find_program(PYTHON_EXE NAMES python python3 py REQUIRED)

set(VULKAN_REGISTRY_LOCATION $ENV{VULKAN_SDK}/share/vulkan/registry)

if(WIN32)
    set(VULKAN_LAYER_DESC_LOCATION $ENV{VULKAN_SDK}/Bin)
elseif(UNIX)
    set(VULKAN_LAYER_DESC_LOCATION $ENV{VULKAN_SDK}/etc/vulkan/explicit_layer.d)
endif()

add_custom_command(OUTPUT SymbolTable.inc
    COMMAND ${PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_headers.py -path=${VULKAN_REGISTRY_LOCATION} >${CMAKE_CURRENT_BINARY_DIR}/SymbolTable.inc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_headers.py)

add_custom_command(OUTPUT DeviceFeatures.inc
    COMMAND ${PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_device_feature_map.py -path=${VULKAN_REGISTRY_LOCATION} >${CMAKE_CURRENT_BINARY_DIR}/DeviceFeatures.inc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_device_feature_map.py)

add_custom_command(OUTPUT LayerMap.inc
    COMMAND ${PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_layer_map.py -path=${VULKAN_LAYER_DESC_LOCATION} >${CMAKE_CURRENT_BINARY_DIR}/LayerMap.inc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_layer_map.py)

add_custom_command(OUTPUT VulkanTypeTraits.inc
    COMMAND ${PYTHON_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_type_constructors.py -path=${VULKAN_REGISTRY_LOCATION} >${CMAKE_CURRENT_BINARY_DIR}/VulkanTypeTraits.inc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_type_constructors.py)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SymbolTable.inc ${CMAKE_CURRENT_BINARY_DIR}/DeviceFeatures.inc ${CMAKE_CURRENT_BINARY_DIR}/LayerMap.inc ${CMAKE_CURRENT_BINARY_DIR}/VulkanTypeTraits.inc DESTINATION include/vkw)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(WIN32)
    set(VULKAN_SDK_LIB_DIR $ENV{VULKAN_SDK}/Lib)
elseif(UNIX)
    set(VULKAN_SDK_LIB_DIR $ENV{VULKAN_SDK}/lib)
endif()

#find_package(SPIRV-Tools-opt REQUIRED)
#find_package(SPIRV-Tools-link REQUIRED)

file(GLOB ${PROJECT_NAME}_SOURCE *.cpp *.h *.hpp ../include/vkw/*.h ../include/vkw/*.hpp)

add_library(${MAIN_LIB_NAME} SHARED ${${PROJECT_NAME}_SOURCE} ../external/SPIRV-Reflect/spirv_reflect.cpp SymbolTable.inc DeviceFeatures.inc LayerMap.inc VulkanTypeTraits.inc)
add_library(${MAIN_LIB_NAME}s STATIC ${${PROJECT_NAME}_SOURCE} ../external/SPIRV-Reflect/spirv_reflect.cpp SymbolTable.inc DeviceFeatures.inc LayerMap.inc VulkanTypeTraits.inc)

add_subdirectory(loader)

target_link_libraries(${MAIN_LIB_NAME} PRIVATE LOADER_LIB SPIRV_LINK)
target_include_directories(${MAIN_LIB_NAME} PRIVATE $ENV{Vulkan_INCLUDE_DIR} ../external/SPIRV-Reflect)

target_link_libraries(${MAIN_LIB_NAME}s PRIVATE LOADER_LIB SPIRV_LINK)
target_include_directories(${MAIN_LIB_NAME}s PRIVATE $ENV{Vulkan_INCLUDE_DIR} ../external/SPIRV-Reflect)
