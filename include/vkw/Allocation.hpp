#ifndef VKRENDERER_ALLOCATION_HPP
#define VKRENDERER_ALLOCATION_HPP

#include <vkw/Containers.hpp>
#include <vkw/Device.hpp>

#include <vkw/Runtime.h>

#include <span>
#include <variant>

namespace vkw {

// Allocation interface heavily re-uses structures from vulkan memory allocator.
using AllocationCreateInfo = VmaAllocationCreateInfo;

class DeviceAllocationBase {
public:
  virtual VkMemoryPropertyFlags properties() const noexcept = 0;
  virtual void map() noexcept(ExceptionsDisabled) = 0;
  virtual void unmap() noexcept = 0;
  virtual size_t size() const noexcept = 0;
  virtual void *mapped() const noexcept = 0;
  virtual void flush(VkDeviceSize offset,
                     VkDeviceSize size) noexcept(ExceptionsDisabled) = 0;
  virtual void invalidate(VkDeviceSize offset,
                          VkDeviceSize size) noexcept(ExceptionsDisabled) = 0;

  virtual ~DeviceAllocationBase() = default;
};

struct AllocationStatistics {
  struct HeapInfo {
    VkDeviceSize used;
    VkDeviceSize available;
  };
  cntr::vector<HeapInfo, 2> heaps;
};

struct DetailedAllocationStatistics {
  /// TODO: implement
};

class DeviceAllocator {
public:
  DeviceAllocator(Device &device) noexcept : m_device(device){};

  Device &parent() const noexcept { return m_device; }

  std::pair<VkBuffer, std::unique_ptr<DeviceAllocationBase>> allocateBuffer(
      const AllocationCreateInfo &allocInfo,
      VkBufferCreateInfo const &createInfo) noexcept(ExceptionsDisabled) {
    return m_allocateBuffer(allocInfo, createInfo);
  }
  std::pair<VkImage, std::unique_ptr<DeviceAllocationBase>> allocateImage(
      const AllocationCreateInfo &allocInfo,
      VkImageCreateInfo const &createInfo) noexcept(ExceptionsDisabled) {
    return m_allocateImage(allocInfo, createInfo);
  }

  virtual AllocationStatistics getAllocationStatistics() const
      noexcept(ExceptionsDisabled) = 0;
  virtual DetailedAllocationStatistics getDetailedAllocationStatistics() const
      noexcept(ExceptionsDisabled) = 0;

  virtual void onFrame() = 0;

  virtual ~DeviceAllocator() = default;

  static std::unique_ptr<DeviceAllocator>
  createDefault(Device &device) noexcept(ExceptionsDisabled);

private:
  virtual std::pair<VkBuffer, std::unique_ptr<DeviceAllocationBase>>
  m_allocateBuffer(
      const AllocationCreateInfo &allocInfo,
      VkBufferCreateInfo const &createInfo) noexcept(ExceptionsDisabled) = 0;
  virtual std::pair<VkImage, std::unique_ptr<DeviceAllocationBase>>
  m_allocateImage(
      const AllocationCreateInfo &allocInfo,
      VkImageCreateInfo const &createInfo) noexcept(ExceptionsDisabled) = 0;

  StrongReference<Device> m_device;
};

namespace __detail {
template <typename T> struct AllocatableObjectTraits {};
template <> struct AllocatableObjectTraits<VkImage> {
  static constexpr auto allocatePfn = &DeviceAllocator::allocateImage;
  using CreateInfoT = VkImageCreateInfo;
};
template <> struct AllocatableObjectTraits<VkBuffer> {
  static constexpr auto allocatePfn = &DeviceAllocator::allocateBuffer;
  using CreateInfoT = VkBufferCreateInfo;
};

// Default implementation uses vulkan memory allocator from vkwrt.

class DefaultDeviceAllocation final : public DeviceAllocationBase {
public:
  DefaultDeviceAllocation(
      VmaAllocator allocator, const AllocationCreateInfo &allocInfo,
      VkBufferCreateInfo const &createInfo) noexcept(ExceptionsDisabled) {
    VkBuffer tmpBuf{};
    VmaAllocation tmpAlloc{};
    VK_CHECK_RESULT(vmaCreateBuffer(allocator, &createInfo, &allocInfo, &tmpBuf,
                                    &tmpAlloc, &m_allocInfo));
    m_allocation = std::unique_ptr<VmaAllocation_T, AllocDeleter>{
        tmpAlloc, AllocDeleter{allocator, tmpBuf}};
  }

  DefaultDeviceAllocation(
      VmaAllocator allocator, const AllocationCreateInfo &allocInfo,
      VkImageCreateInfo const &createInfo) noexcept(ExceptionsDisabled) {
    VkImage tmpImage{};
    VmaAllocation tmpAlloc{};
    VK_CHECK_RESULT(vmaCreateImage(allocator, &createInfo, &allocInfo,
                                   &tmpImage, &tmpAlloc, &m_allocInfo));
    m_allocation = std::unique_ptr<VmaAllocation_T, AllocDeleter>{
        tmpAlloc, AllocDeleter{allocator, tmpImage}};
  }
  DefaultDeviceAllocation(DefaultDeviceAllocation &&) = delete;
  DefaultDeviceAllocation &operator=(DefaultDeviceAllocation &&) = delete;

  VkMemoryPropertyFlags properties() const noexcept override {
    const VkPhysicalDeviceMemoryProperties *pMemProps;
    vmaGetMemoryProperties(m_allocator(), &pMemProps);
    return pMemProps->memoryTypes[m_allocInfo.memoryType].propertyFlags;
  }
  void map() noexcept(ExceptionsDisabled) override {
    if (m_allocInfo.pMappedData || m_mustUnmap())
      return;
    m_setMustUnmap(true);
    VK_CHECK_RESULT(vmaMapMemory(m_allocator(), m_allocation.get(),
                                 &m_allocInfo.pMappedData));
  }
  void unmap() noexcept override {
    if (!m_allocInfo.pMappedData || !m_mustUnmap())
      return;
    m_setMustUnmap(false);
    vmaUnmapMemory(m_allocator(), m_allocation.get());
    m_allocInfo.pMappedData = nullptr;
  }
  size_t size() const noexcept override { return m_allocInfo.size; }

  void *mapped() const noexcept override { return m_allocInfo.pMappedData; }
  void flush(VkDeviceSize offset,
             VkDeviceSize size) noexcept(ExceptionsDisabled) override {
    VK_CHECK_RESULT(
        vmaFlushAllocation(m_allocator(), m_allocation.get(), offset, size));
  }
  void invalidate(VkDeviceSize offset,
                  VkDeviceSize size) noexcept(ExceptionsDisabled) override {
    VK_CHECK_RESULT(vmaInvalidateAllocation(m_allocator(), m_allocation.get(),
                                            offset, size));
  }

  VkImage getImage() const {
    return std::get<VkImage>(m_allocation.get_deleter().objectHandle);
  }

  VkBuffer getBuffer() const {
    return std::get<VkBuffer>(m_allocation.get_deleter().objectHandle);
  }

private:
  VmaAllocator m_allocator() const {
    return m_allocation.get_deleter().allocator;
  }
  bool m_mustUnmap() const { return m_allocation.get_deleter().mustUnmap; }
  void m_setMustUnmap(bool newVal) {
    m_allocation.get_deleter().mustUnmap = newVal;
  }
  struct AllocDeleter {
    void operator()(VmaAllocation allocation) const {
      if (mustUnmap)
        vmaUnmapMemory(allocator, allocation);
      if (std::holds_alternative<VkImage>(objectHandle)) {
        vmaDestroyImage(allocator, std::get<VkImage>(objectHandle), allocation);
      } else {
        vmaDestroyBuffer(allocator, std::get<VkBuffer>(objectHandle),
                         allocation);
      }
    }
    VmaAllocator allocator = nullptr;
    std::variant<VkImage, VkBuffer> objectHandle = VkImage{};
    bool mustUnmap = false;
  };
  std::unique_ptr<VmaAllocation_T, AllocDeleter> m_allocation;
  VmaAllocationInfo m_allocInfo{};
};

class DefaultDeviceAllocator final : public DeviceAllocator {
public:
  DefaultDeviceAllocator(Device &device)
      : DeviceAllocator(device), m_impl([&]() {
          VmaAllocatorCreateInfo allocatorInfo = {};
          allocatorInfo.vulkanApiVersion = device.apiVersion();
          allocatorInfo.physicalDevice = device.physicalDevice();
          allocatorInfo.device = device;
          allocatorInfo.instance = device.parent();
          if (std::ranges::any_of(
                  device.physicalDevice().enabledExtensions(),
                  [](auto &id) { return id == ext::EXT_memory_budget; }))
            allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

          VmaVulkanFunctions vmaVulkanFunctions{};
          vmaVulkanFunctions.vkGetInstanceProcAddr =
              device.parent().parent().vkGetInstanceProcAddr;
          vmaVulkanFunctions.vkGetDeviceProcAddr =
              device.parent().core<1, 0>().vkGetDeviceProcAddr;

          allocatorInfo.pVulkanFunctions = &vmaVulkanFunctions;
          allocatorInfo.pAllocationCallbacks = HostAllocator::get();
          VmaAllocator allocator;

          VK_CHECK_RESULT(vmaCreateAllocator(&allocatorInfo, &allocator))

          return allocator;
        }()),
        m_heapCount(
            device.physicalDevice().memoryProperties().memoryHeapCount) {}
  AllocationStatistics getAllocationStatistics() const
      noexcept(ExceptionsDisabled) override {
    std::vector<VmaBudget> heapsInfo;
    heapsInfo.resize(m_heapCount);
    vmaGetHeapBudgets(m_impl.get(), heapsInfo.data());
    AllocationStatistics ret{};

    std::transform(heapsInfo.begin(), heapsInfo.end(),
                   std::back_inserter(ret.heaps), [](auto &heapInfo) {
                     return AllocationStatistics::HeapInfo(heapInfo.usage,
                                                           heapInfo.budget);
                   });
    return ret;
  }
  DetailedAllocationStatistics getDetailedAllocationStatistics() const
      noexcept(ExceptionsDisabled) override {
    assert(0 && "unimplemented");
    return DetailedAllocationStatistics{};
  }

  void onFrame() override {
    vmaSetCurrentFrameIndex(m_impl.get(), ++m_curretFrame);
  }

private:
  std::pair<VkBuffer, std::unique_ptr<DeviceAllocationBase>>
  m_allocateBuffer(const AllocationCreateInfo &allocInfo,
                   VkBufferCreateInfo const
                       &createInfo) noexcept(ExceptionsDisabled) override {
    auto ret = std::make_unique<DefaultDeviceAllocation>(m_impl.get(),
                                                         allocInfo, createInfo);
    auto buf = ret->getBuffer();
    return {buf, std::move(ret)};
  }
  std::pair<VkImage, std::unique_ptr<DeviceAllocationBase>>
  m_allocateImage(const AllocationCreateInfo &allocInfo,
                  VkImageCreateInfo const
                      &createInfo) noexcept(ExceptionsDisabled) override {
    auto ret = std::make_unique<DefaultDeviceAllocation>(m_impl.get(),
                                                         allocInfo, createInfo);
    auto image = ret->getImage();
    return {image, std::move(ret)};
  }

  struct ImplDeleter {
    void operator()(VmaAllocator a) { vmaDestroyAllocator(a); }
  };
  std::unique_ptr<std::remove_pointer_t<VmaAllocator>, ImplDeleter> m_impl;
  size_t m_curretFrame = 0;
  const size_t m_heapCount;
};
} // namespace __detail

inline std::unique_ptr<DeviceAllocator>
DeviceAllocator::createDefault(Device &device) {
  return std::make_unique<__detail::DefaultDeviceAllocator>(device);
}

template <typename ObjT> class Allocation {
private:
  using Traits = __detail::AllocatableObjectTraits<ObjT>;

public:
  Allocation(
      DeviceAllocator &allocator, const AllocationCreateInfo &allocInfo,
      const Traits::CreateInfoT &createInfo) noexcept(ExceptionsDisabled) {
    std::tie(m_handle, m_pimpl) =
        std::invoke(Traits::allocatePfn, allocator, allocInfo, createInfo);
  };

  Allocation(Allocation &&) noexcept = default;
  Allocation &operator=(Allocation &&) noexcept = default;

  bool mappable() const noexcept {
    auto bits = m_pimpl->properties().VkMemoryPropertyFlags;
    return bits & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ||
           bits & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  }

  bool coherent() const noexcept {
    auto bits = m_pimpl->properties().VkMemoryPropertyFlags;
  }

  auto allocationSize() const noexcept { return m_pimpl->size(); }

  template <typename T> std::span<T> mapped() const noexcept {
    auto *ptr = reinterpret_cast<T *>(m_pimpl->mapped());
    auto count = ptr ? allocationSize() / sizeof(T) : 0;
    return {ptr, ptr + count};
  }

  void map() noexcept(ExceptionsDisabled) { m_pimpl->map(); }

  void unmap() noexcept { m_pimpl->unmap(); }

  void flush(VkDeviceSize offset = 0,
             VkDeviceSize size = VK_WHOLE_SIZE) noexcept(ExceptionsDisabled) {
    m_pimpl->flush(offset, size);
  }

  void
  invalidate(VkDeviceSize offset = 0,
             VkDeviceSize size = VK_WHOLE_SIZE) noexcept(ExceptionsDisabled) {
    m_pimpl->invalidate(offset, size);
  }

  virtual ~Allocation() = default;

protected:
  ObjT handle() const { return m_handle; }

private:
  ObjT m_handle = nullptr;
  std::unique_ptr<DeviceAllocationBase> m_pimpl = nullptr;
};

class SharingInfo {
public:
  SharingInfo() = default;
  auto sharingMode() const { return m_sharingMode; }

  std::span<unsigned const> queueFamilies() const {
    return {m_queueFamilies.data(), m_queueFamilies.size()};
  }

  SharingInfo &addQueueFamily(unsigned index) {
    m_queueFamilies.push_back(index);
    m_sharingMode = VK_SHARING_MODE_CONCURRENT;
    return *this;
  }

private:
  VkSharingMode m_sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  cntr::vector<unsigned, 3> m_queueFamilies;
};

} // namespace vkw
#endif // VKRENDERER_ALLOCATION_HPP
