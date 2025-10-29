include(../cmake/generate_headers.cmake)

vkw_generate_headers(generate_headers.py SymbolTable.inc)
vkw_generate_headers(generate_device_feature_map.py DeviceFeatures.inc)
vkw_generate_headers(generate_type_constructors.py VulkanTypeTraits.inc)
vkw_generate_headers_with_args(generate_layer_map.py LayerMap.inc "-path=${VULKAN_LAYER_DESC_LOCATION}")

install(FILES ${VKW_GENERATED_HEADERS} DESTINATION include/vkw)

# Runtime shared library.
add_library(${VKW_RUNTIME_LIB} SHARED Runtime.cpp ${CMAKE_SOURCE_DIR}/external/SPIRV-Reflect/spirv_reflect.c)
set_target_properties(${VKW_RUNTIME_LIB} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${VKW_PROJECT_BIN}")

# Generated headers
target_sources(${VKW_RUNTIME_LIB} PRIVATE ${VKW_GENERATED_HEADERS})

# explicitly source api-headers as sources for ide reasons
file(GLOB VKW_HEADERS RELATIVE . ../include/vkw/*)
target_sources(${VKW_RUNTIME_LIB} PRIVATE ${VKW_HEADERS})

target_compile_definitions(${VKW_RUNTIME_LIB} PUBLIC ${VKW_OPT_NAME_LIST})

if(UNIX)
    target_compile_options(${VKW_RUNTIME_LIB} PRIVATE -fPIC)
endif()

target_link_libraries(${VKW_RUNTIME_LIB} PRIVATE VKW_SPIRV_TOOLS)
target_link_libraries(${VKW_RUNTIME_LIB} PUBLIC Boost::boost)
