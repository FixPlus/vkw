#include "vkw/Runtime.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vma/vk_mem_alloc.h>

#ifdef _WIN32
#include <Libloaderapi.h>
#elif defined __linux__
#include <dlfcn.h>
#endif

#include "spirv-tools/linker.hpp"

#include "vkw/Exception.hpp"

#include <cassert>
#include <span>

namespace vkw {
#include <vkw/LibraryVersion.inc>
namespace {
VKW_spvMessageLevel translateLevel(const spv_message_level_t &level) noexcept {
  switch (level) {
#define VKW_LEVEL_CASE(X)                                                      \
  case SPV_MSG_##X:                                                            \
    return VKW_SPV_MSG_##X;
    VKW_LEVEL_CASE(FATAL)
    VKW_LEVEL_CASE(INTERNAL_ERROR)
    VKW_LEVEL_CASE(ERROR)
    VKW_LEVEL_CASE(WARNING)
    VKW_LEVEL_CASE(INFO)
    VKW_LEVEL_CASE(DEBUG)
  }
  return VKW_SPV_MSG_INFO;
}

std::span<char> getErrorString() noexcept {
  static thread_local char str[1000] = {0};
  return str;
}

void setErrorString(const char *str) {
  auto errStr = getErrorString();
  std::strncpy(errStr.data(), str, errStr.size());
}

} // namespace
} // namespace vkw
extern "C" {

void vkw_getRuntimeVersion(uint32_t *maj, uint32_t *min, uint32_t *rev) {
  if (maj)
    *maj = vkw::MajorVersion;
  if (min)
    *min = vkw::MinorVersion;
  if (rev)
    *rev = vkw::RevVersion;
}

const char *vkw_lastError() { return vkw::getErrorString().data(); }

VKW_ErrorCode vkw_loadVulkan(VKW_RTLoader *handle) try {
#ifdef _WIN32
  constexpr static auto *libName = "vulkan-1.dll";
  auto ret =
      reinterpret_cast<VKW_RTLoader>(::LoadLibraryExA(libName, nullptr, 0u));

  if (ret) {
    *handle = ret;
    return VKW_OK;
  }
  std::stringstream ss{};
  ss << libName << " was not found. Error code: 0x" << std::hex
     << ::GetLastError();
  vkw::setErrorString(ss.str().c_str());
  return VKW_VULKAN_LIB_MISSING;
#elif defined __linux__
  constexpr static auto *libName = "libvulkan.so.1";
  auto ret = reinterpret_cast<VKW_RTLoader>(dlopen(libName, RTLD_LAZY));
  if (ret) {
    *handle = ret;
    return VKW_OK;
  }

  std::stringstream ss{};
  ss << libName << " was not found. Error message: " << dlerror();
  vkw::getErrorString() = std::move(ss).str();
  return VKW_VULKAN_LIB_MISSING;

#else
#error "unsupported platform"
#endif
} catch (...) {
  return VKW_FATAL;
}

PFN_vkGetInstanceProcAddr vkw_vkGetInstanceProcAddr(VKW_RTLoader handle) {
  constexpr static auto *symbolName = "vkGetInstanceProcAddr";
#ifdef _WIN32
  return reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      ::GetProcAddress(reinterpret_cast<HMODULE>(handle), symbolName));
#elif defined __linux__
  return reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(handle, symbolName));
#else
#error "unsupported platform"
#endif
}

void vkw_closeVulkan(VKW_RTLoader handle) {
#ifdef _WIN32
  ::FreeLibrary(reinterpret_cast<HMODULE>(handle));
#elif defined __linux__
  dlclose(handle);
#else
#error "unsupported platform"
#endif
}

VKW_ErrorCode vkw_createSpvContext(VKW_spvContext *handle,
                                   VKW_PFNspvMessageConsumer msgConsumer,
                                   void *userData) try {
  spvtools::Context *context = nullptr;
  try {
    context = new spvtools::Context(SPV_ENV_VULKAN_1_0);
  } catch (std::runtime_error &e) {
    *handle = nullptr;
    return VKW_FATAL;
  }
  if (msgConsumer) {
    context->SetMessageConsumer(
        [msgConsumer,
         userData](spv_message_level_t messageLevel, const char *input,
                   const spv_position_t position, const char *message) {
          msgConsumer(vkw::translateLevel(messageLevel), input, position.line,
                      position.column, message, userData);
        });
  }
  *handle = reinterpret_cast<VKW_spvContext_T *>(context);
  return VKW_OK;
} catch (...) {
  return VKW_FATAL;
}

void vkw_destroySpvContext(VKW_spvContext handle) {
  auto *context = reinterpret_cast<spvtools::Context *>(handle);
  delete context;
}

VKW_ErrorCode vkw_spvLink(VKW_spvContext handle,
                          const VKW_spvLinkInfo *linkInfo, uint32_t **linked,
                          size_t *linkedSize) try {
  assert(handle && linkInfo);
  if (!linked || !linkedSize)
    return VKW_SPV_LINK_FAILED;
  auto *context = reinterpret_cast<spvtools::Context *>(handle);
  spvtools::LinkerOptions options{};
  auto flags = linkInfo->flags;
  options.SetCreateLibrary(flags | VKW_SPV_LINK_CREATE_LIBRARY);
  options.SetVerifyIds(flags | VKW_SPV_LINK_VERIFY_IDS);
  options.SetAllowPartialLinkage(flags | VKW_SPV_LINK_ALLOW_PARTIAL_LINKAGE);
  std::vector<uint32_t> output;
  auto result =
      spvtools::Link(*context, linkInfo->binaries, linkInfo->binary_sizes,
                     linkInfo->num_binaries, &output, options);
  if (result == SPV_SUCCESS) {
    *linked = new uint32_t[output.size()];
    std::ranges::copy(output, *linked);
    *linkedSize = output.size();
    return VKW_OK;
  }
  *linked = nullptr;
  *linkedSize = 0;
  std::stringstream ss;
  ss << "spvtools::Link returned " << result
     << " status. more information is sent to message consumer.";
  vkw::setErrorString(ss.str().c_str());
  return VKW_SPV_LINK_FAILED;
} catch (...) {
  return VKW_FATAL;
}

void vkw_spvLinkedImageFree(uint32_t *linked) { delete[] linked; }

SpvReflectResult
vkw_spvReflectCreateShaderModule(size_t size, const void *p_code,
                                 SpvReflectShaderModule *p_module) {
  return spvReflectCreateShaderModule(size, p_code, p_module);
}

void vkw_spvReflectDestroyShaderModule(SpvReflectShaderModule *p_module) {
  spvReflectDestroyShaderModule(p_module);
}

void *vkw_hostMalloc(size_t size, size_t alignment,
                     VkSystemAllocationScope scope) {
#if _WIN32
  return _aligned_malloc(size, alignment);
#else
  return std::aligned_alloc(alignment, size);
#endif
}
void *vkw_hostRealloc(void *original, size_t size, size_t alignment,
                      VkSystemAllocationScope scope) {
#if _WIN32
  return _aligned_realloc(original, size, alignment);
#else
  auto *newData = std::realloc(original, size);
  if ((uint64_t)newData % alignment == 0)
    return newData;

  auto *alignedData = std::aligned_alloc(alignment, size);
  memcpy(alignedData, newData, size);
  return alignedData;
#endif
}
void vkw_hostFree(void *memory) {
#if _WIN32
  return _aligned_free(memory);
#else
  return std::free(memory);
#endif
}
}