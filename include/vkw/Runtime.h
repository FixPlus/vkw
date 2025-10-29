
#pragma once

#include <vkw/vkwrt.h>

// vulkan library is not a part of vkw's runtime, although some declarations
// from this header are required. All function symbols listed below are
// undefined in vkwrt and should not be used.
#include <vulkan/vulkan.h>

// vk mem alloc (vma) is part of vkw's runtime, it's api is used directly. All
// function symbols of vma are exported as part of vkwrt public interface.
#define VMA_CALL_PRE extern "C" VKWRT_EXPORT
#include <vma/vk_mem_alloc.h>

// spirv_reflect is part of vkw's runtime and there are some structures
// definitions that are used directly. However symbols from this header are not
// exportable and should not be used.
#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#define SPIRV_REFLECT_DISABLE_CPP_BINDINGS
#include <spirv_reflect.h>
#undef SPIRV_REFLECT_DISABLE_CPP_BINDINGS
#undef SPIRV_REFLECT_USE_SYSTEM_SPIRV_H

extern "C" {

struct VKW_RTLoader_T;
typedef VKW_RTLoader_T *VKW_RTLoader;
enum VKW_ErrorCode {
  VKW_OK = 0,
  VKW_VULKAN_LIB_MISSING,
  VKW_SPV_LINK_FAILED,
  VKW_FATAL
};

/* runtime version query */

VKWRT_EXPORT void vkw_getRuntimeVersion(uint32_t *maj, uint32_t *min,
                                        uint32_t *rev);

/// @brief If any function from this interface returns anything other than
/// VKW_OK - an internal string is filled with explanation message that can be
/// accessed via this function. Message string is thread_local.
///
/// @return message string that is possibly empty if no error occurred yet.
VKWRT_EXPORT const char *vkw_lastError();

/* default vulkan loader */

/// @brief Loads os-specific vulkan library into app's address space and gives
/// an opaque handle of it.
///
/// @param handle pointer to handle to be written. if null this function is
/// no-op.
/// @return VKW_OK on success. If library fails to be loaded returns
/// VKW_VULKAN_LIB_MISSING and sets handle to null.
VKWRT_EXPORT VKW_ErrorCode vkw_loadVulkan(VKW_RTLoader *handle);

/// @brief Retrieves vkGetInstanceProcAddr symbol from library pointed by
/// handle.
///
/// @param handle must be non-null and retrieved previously from vkw_loadVulkan.
/// @return address of vkGetInstanceProcAddr symbol.
VKWRT_EXPORT PFN_vkGetInstanceProcAddr
vkw_vkGetInstanceProcAddr(VKW_RTLoader handle);

/// @brief Unloads vulkan library pointed by handle. All symbols retrieved
/// previously by vkGetInstanceProcAddr become invalid and should not be used
/// further.
///
/// @param handle must be either null (then this function is no-op) or being
/// retrieved previously from vkw_loadVulkan.
VKWRT_EXPORT void vkw_closeVulkan(VKW_RTLoader handle);

/* spirv-link interafce */

struct VKW_spvContext_T;
typedef VKW_spvContext_T *VKW_spvContext;

// copy of spv_message_level_t
typedef enum VKW_spvMessageLevel {
  VKW_SPV_MSG_FATAL,          // Unrecoverable error due to environment.
                              // Will exit the program immediately. E.g.,
                              // out of memory.
  VKW_SPV_MSG_INTERNAL_ERROR, // Unrecoverable error due to SPIRV-Tools
                              // internals.
                              // Will exit the program immediately. E.g.,
                              // unimplemented feature.
  VKW_SPV_MSG_ERROR,          // Normal error due to user input.
  VKW_SPV_MSG_WARNING,        // Warning information.
  VKW_SPV_MSG_INFO,           // General information.
  VKW_SPV_MSG_DEBUG,          // Debug information.
} VKW_spvMessageLevel;

typedef void (*VKW_PFNspvMessageConsumer)(VKW_spvMessageLevel,
                                          const char * /* source */,
                                          size_t /*line*/, size_t /* column */,
                                          const char * /* message */,
                                          void * /* user data */);

/// @brief Creates spirv-tools context.
///
/// @param handle pointer to opaque handle to fill.
/// @param msgConsumer pointer to message consumer callback function. May be
/// null for no callback.
/// @param userData optional pointer to some user data that would be passed in
/// each invocation of message consumer. This parameter is ignored if message
/// consumer is null.
/// @return VKW_OK on success and VKW_FATAL otherwise.
VKWRT_EXPORT VKW_ErrorCode
vkw_createSpvContext(VKW_spvContext *handle,
                     VKW_PFNspvMessageConsumer msgConsumer, void *userData);

/// @brief Destroys spirv-tools context previously created by method above.
///
/// @param handle must be either null (then this function is no-op) or being
/// retrieved previously from vkw_createSpvContext.
VKWRT_EXPORT void vkw_destroySpvContext(VKW_spvContext handle);

typedef enum VKW_spvLinkFlagBits {
  VKW_SPV_LINK_CREATE_LIBRARY = 1,
  VKW_SPV_LINK_VERIFY_IDS = 1 << 1,
  VKW_SPV_LINK_ALLOW_PARTIAL_LINKAGE = 1 << 2
} VKW_spvLinkFlagBits;

typedef unsigned VKW_spvLinkFlags;
typedef struct VKW_spvLinkInfo {
  VKW_spvLinkFlags flags;
  const uint32_t *const *binaries;
  const size_t *binary_sizes;
  size_t num_binaries;

} VKW_spvLinkInfo;

/// @brief Performs linking of multiple spirv binary modules into one. Linked
/// module data is allocated internally and should be freed by calling
/// vkw_spvLinkedImageFree() declared below.
///
/// @param handle spirv-tools context.
/// @param linkInfo pointer to a structure defining process of linking
/// @param linked out-parameter filling pointer to linked data.
/// @param linkedSize out-parameter filling size of linked data.
/// @return VKW_OK on success and VKW_SPV_LINK_FAILED otherwise.
VKWRT_EXPORT VKW_ErrorCode vkw_spvLink(VKW_spvContext handle,
                                       const VKW_spvLinkInfo *linkInfo,
                                       uint32_t **linked, size_t *linkedSize);
/// @brief Frees memory backing linked data.
///
/// @param linked pointer to linked data that is either null or was previously
/// filled by vkw_spvLink.
VKWRT_EXPORT void vkw_spvLinkedImageFree(uint32_t *linked);

/* Wrappers for spirv-reflect */

/// @brief exported wrapper over spvReflectCreateShaderModule. It does not
/// updates error string itself.
///
/// @param  size      Size in bytes of SPIR-V code.
/// @param  p_code    Pointer to SPIR-V code.
/// @param  p_module  Pointer to an instance of SpvReflectShaderModule.
/// @return           SPV_REFLECT_RESULT_SUCCESS on success.
VKWRT_EXPORT SpvReflectResult vkw_spvReflectCreateShaderModule(
    size_t size, const void *p_code, SpvReflectShaderModule *p_module);

/// @brief spvReflectDestroyShaderModule
///
/// @param  p_module  Pointer to an instance of SpvReflectShaderModule.
VKWRT_EXPORT void
vkw_spvReflectDestroyShaderModule(SpvReflectShaderModule *p_module);

/* Embedded default allocators */

VKWRT_EXPORT void *vkw_hostMalloc(size_t size, size_t alignment,
                                  VkSystemAllocationScope scope);
VKWRT_EXPORT void *vkw_hostRealloc(void *original, size_t size,
                                   size_t alignment,
                                   VkSystemAllocationScope scope);
VKWRT_EXPORT void vkw_hostFree(void *memory);
}