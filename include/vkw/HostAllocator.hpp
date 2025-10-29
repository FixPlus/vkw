#ifndef VKWRAPPER_HOSTALLOCATOR_HPP
#define VKWRAPPER_HOSTALLOCATOR_HPP

#include <vkw/Runtime.h>

#include <memory>

namespace vkw {

class HostAllocator {
private:
  class GlobalAllocatorKeeper final {
  public:
    void update(std::unique_ptr<HostAllocator> newAlloc) {
      m_alloc = std::move(newAlloc);
      m_callbacks.pUserData = m_alloc.get();
      m_callbacks.pfnAllocation = &m_allocate;
      m_callbacks.pfnReallocation = &m_reallocate;
      m_callbacks.pfnFree = &m_free;
      m_callbacks.pfnInternalAllocation = &m_internalAllocNotify;
      m_callbacks.pfnInternalFree = &m_internalFreeNotify;
    }
    GlobalAllocatorKeeper() { update(std::make_unique<HostAllocator>()); }
    GlobalAllocatorKeeper(GlobalAllocatorKeeper &&) = delete;
    GlobalAllocatorKeeper &operator=(GlobalAllocatorKeeper &&) = delete;
    GlobalAllocatorKeeper(const GlobalAllocatorKeeper &) = delete;
    GlobalAllocatorKeeper &operator=(const GlobalAllocatorKeeper &) = delete;
    const VkAllocationCallbacks *callbacks() const noexcept {
      return m_alloc ? &m_callbacks : nullptr;
    }
    ~GlobalAllocatorKeeper() = default;

  private:
    VkAllocationCallbacks m_callbacks{};
    std::unique_ptr<HostAllocator> m_alloc = nullptr;
  };

  static GlobalAllocatorKeeper &m_keeper() {
    static GlobalAllocatorKeeper keeper;
    return keeper;
  }

public:
  HostAllocator() noexcept = default;

  static VkAllocationCallbacks const *get() noexcept {
    return m_keeper().callbacks();
  }

  static void set(std::unique_ptr<HostAllocator> &&newAlloc) {
    m_keeper().update(std::move(newAlloc));
  }

  virtual ~HostAllocator() = default;

protected:
  virtual void *allocate(size_t size, size_t alignment,
                         VkSystemAllocationScope scope) noexcept {
    return vkw_hostMalloc(size, alignment, scope);
  }

  virtual void *reallocate(void *original, size_t size, size_t alignment,
                           VkSystemAllocationScope scope) noexcept {
    return vkw_hostRealloc(original, size, alignment, scope);
  }

  virtual void free(void *memory) noexcept { return vkw_hostFree(memory); }

  virtual void
  internalAllocNotify(size_t size, VkInternalAllocationType allocationType,
                      VkSystemAllocationScope allocationScope) noexcept {}

  virtual void
  internalFreeNotify(size_t size, VkInternalAllocationType allocationType,
                     VkSystemAllocationScope allocationScope) noexcept {}

private:
  static void *m_allocate(void *m_this, size_t size, size_t alignment,
                          VkSystemAllocationScope scope) noexcept {
    return reinterpret_cast<HostAllocator *>(m_this)->allocate(size, alignment,
                                                               scope);
  }

  static void *m_reallocate(void *m_this, void *original, size_t size,
                            size_t alignment,
                            VkSystemAllocationScope scope) noexcept {
    return reinterpret_cast<HostAllocator *>(m_this)->reallocate(
        original, size, alignment, scope);
  }

  static void m_free(void *m_this, void *memory) noexcept {
    return reinterpret_cast<HostAllocator *>(m_this)->free(memory);
  }

  static void
  m_internalAllocNotify(void *m_this, size_t size,
                        VkInternalAllocationType allocationType,
                        VkSystemAllocationScope allocationScope) noexcept {
    reinterpret_cast<HostAllocator *>(m_this)->internalAllocNotify(
        size, allocationType, allocationScope);
  }

  static void
  m_internalFreeNotify(void *m_this, size_t size,
                       VkInternalAllocationType allocationType,
                       VkSystemAllocationScope allocationScope) noexcept {
    reinterpret_cast<HostAllocator *>(m_this)->internalFreeNotify(
        size, allocationType, allocationScope);
  }
};
} // namespace vkw
#endif // VKWRAPPER_HOSTALLOCATOR_HPP
