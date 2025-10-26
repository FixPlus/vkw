set(VULKAN_REGISTRY_LOCATION $ENV{VULKAN_SDK}/share/vulkan/registry)

if(WIN32)
    set(VULKAN_LAYER_DESC_LOCATION $ENV{VULKAN_SDK}/Bin)
elseif(UNIX)
    set(VULKAN_LAYER_DESC_LOCATION $ENV{VULKAN_SDK}/etc/vulkan/explicit_layer.d)
endif()

file(MAKE_DIRECTORY ${VKW_GENERATED_DIR})

function(vkw_generate_headers_with_args script_name header_name extra_args)
    add_custom_command(OUTPUT ${VKW_GENERATED_DIR}/${header_name}
        COMMAND ${PYTHON_EXE} ${CMAKE_SOURCE_DIR}/scripts/${script_name} ${extra_args} >${VKW_GENERATED_DIR}/${header_name}
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/${script_name})
    list(APPEND VKW_GENERATED_HEADERS "${VKW_GENERATED_DIR}/${header_name}")
    set(VKW_GENERATED_HEADERS ${VKW_GENERATED_HEADERS} PARENT_SCOPE)
endfunction()

macro(vkw_generate_headers script_name header_name)
    vkw_generate_headers_with_args(${script_name} ${header_name} "-path=${VULKAN_REGISTRY_LOCATION}")
endmacro()