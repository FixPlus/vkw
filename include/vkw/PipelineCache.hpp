#ifndef VKWRAPPER_PIPELINECACHE_HPP
#define VKWRAPPER_PIPELINECACHE_HPP

#include <vkw/Device.hpp>

namespace vkw {

class PipelineCache : public vk::PipelineCache {
public:
  explicit PipelineCache(
      Device const &device, size_t initDataSize = 0, void *initData = nullptr,
      VkPipelineCacheCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : vk::PipelineCache(device, [&]() {
          VkPipelineCacheCreateInfo createInfo{};
          createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
          createInfo.pNext = nullptr;
          createInfo.flags = flags;
          createInfo.initialDataSize = initDataSize;
          createInfo.pInitialData = initData;
          return createInfo;
        }()) {}

  size_t dataSize() const noexcept(ExceptionsDisabled) {
    size_t ret{};
    VK_CHECK_RESULT(parent().core<1, 0>().vkGetPipelineCacheData(
        parent(), handle(), &ret, nullptr))

    return ret;
  }

  /** @return true if whole cache was copied, false otherwise. */
  bool getData(void *buffer, size_t bufferLength) const
      noexcept(ExceptionsDisabled) {

    auto result = parent().core<1, 0>().vkGetPipelineCacheData(
        parent(), handle(), &bufferLength, buffer);
    if (result == VK_INCOMPLETE) {
      return false;
    } else if (result == VK_SUCCESS) {
      return true;
    }

    VK_CHECK_RESULT(result)

    return false;
  }
};

} // namespace vkw
#endif // VKWRAPPER_PIPELINECACHE_HPP
