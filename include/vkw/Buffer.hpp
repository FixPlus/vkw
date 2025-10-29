#ifndef VKRENDERER_BUFFER_HPP
#define VKRENDERER_BUFFER_HPP

#include <vkw/Allocation.hpp>

namespace vkw {

class BufferBase : public Allocation<VkBuffer>, public ReferenceGuard {
public:
  BufferBase(
      DeviceAllocator &allocator, VkBufferCreateInfo const &createInfo,
      AllocationCreateInfo const &allocCreateInfo) noexcept(ExceptionsDisabled)
      : Allocation<VkBuffer>(allocator, allocCreateInfo, createInfo) {}

  auto bufferSize() const noexcept { return m_createInfo.size; }

  operator VkBuffer() const noexcept { return handle(); }

  VkBufferUsageFlags usage() const noexcept { return m_createInfo.usage; }

  bool canBeCopySrc() const noexcept {
    return m_createInfo.usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }

  bool canBeCopyDst() const noexcept {
    return m_createInfo.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }

  bool isVertexBuffer() const noexcept {
    return m_createInfo.usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }

  bool isIndexBuffer() const noexcept {
    return m_createInfo.usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }

  bool isUniformBuffer() const noexcept {
    return m_createInfo.usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }

  bool isStorageBuffer() const noexcept {
    return m_createInfo.usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }

protected:
  VkBufferCreateInfo m_createInfo;
};

template <typename T> class Buffer : public BufferBase {
public:
  Buffer(DeviceAllocator &allocator, uint64_t count, VkBufferUsageFlags usage,
         AllocationCreateInfo const &allocCreateInfo,
         SharingInfo const &sharingInfo = {}) noexcept(ExceptionsDisabled)
      : BufferBase(allocator, m_fillInfo(count, usage, sharingInfo),
                   allocCreateInfo),
        m_count(count) {}

  std::span<T> mapped() const noexcept { return Allocation::mapped<T>(); }

  uint64_t size() const noexcept { return m_count; }

private:
  VkBufferCreateInfo m_fillInfo(uint64_t count, VkBufferUsageFlags usage,
                                SharingInfo const &sharingInfo) noexcept {
    VkBufferCreateInfo createInfo{};
    createInfo.size = count * sizeof(T);
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.sharingMode = sharingInfo.sharingMode();
    if (sharingInfo.sharingMode() != VK_SHARING_MODE_EXCLUSIVE) {
      createInfo.pQueueFamilyIndices = sharingInfo.queueFamilies().data();
      createInfo.queueFamilyIndexCount = sharingInfo.queueFamilies().size();
    }
    createInfo.usage = usage;
    createInfo.pNext = nullptr;
    return createInfo;
  }
  uint64_t m_count;
};

} // namespace vkw
#endif // VKRENDERER_BUFFER_HPP
