#ifndef VKWRAPPER_STAGINGBUFFER_HPP
#define VKWRAPPER_STAGINGBUFFER_HPP

#include <vkw/Buffer.hpp>

namespace vkw {
/**
 * @class StagingBuffer
 *
 * is used to copy data to device local memory.
 *
 */
template <typename T> class StagingBuffer : public vkw::Buffer<T> {
public:
  // Create initialized.
  StagingBuffer(DeviceAllocator &allocator, std::span<T const> data)
      : vkw::Buffer<T>(
            allocator, data.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VmaAllocationCreateInfo{.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                    .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                    .requiredFlags =
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}) {
    std::copy(data.begin(), data.end(), vkw::Buffer<T>::mapped().begin());
  }
  // Create uninitialized.
  StagingBuffer(DeviceAllocator &allocator, size_t size)
      : vkw::Buffer<T>(
            allocator, size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VmaAllocationCreateInfo{.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                    .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                    .requiredFlags =
                                        VK_MEMORY_PROPERTY_HOST_CACHED_BIT}) {}
};

} // namespace vkw
#endif // VKWRAPPER_STAGINGBUFFER_HPP
