#ifndef VKRENDERER_FENCE_HPP
#define VKRENDERER_FENCE_HPP

#include <concepts>
#include <vkw/Device.hpp>

namespace vkw {

class Fence;
template <typename T>
concept FenceIterator =
    std::forward_iterator<T> && requires(T a) {
                                  { *a } -> std::same_as<Fence &>;
                                };

class Fence : public vk::Fence {
public:
  Fence(Device const &device,
        bool createSignaled = false) noexcept(ExceptionsDisabled)
      : vk::Fence(device, [&]() {
          VkFenceCreateInfo createInfo{};
          VkFenceCreateFlags flags{};
          if (createSignaled)
            flags |= VK_FENCE_CREATE_SIGNALED_BIT;

          createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
          createInfo.pNext = nullptr;
          createInfo.flags = flags;
          return createInfo;
        }()) {}

  void reset() noexcept(ExceptionsDisabled) {
    auto h = handle();
    VK_CHECK_RESULT(parent().core<1, 0>().vkResetFences(parent(), 1, &h))
  }

  // returns true if fence condition satisfied before timeout

  bool wait(uint64_t timeout = UINT64_MAX) {
    auto h = handle();
    return wait_impl(parent(), &h, 1, true, timeout);
  }

  bool signaled() const noexcept(ExceptionsDisabled) {
    auto result = parent().core<1, 0>().vkGetFenceStatus(parent(), handle());
    if (result == VK_SUCCESS)
      return true;
    if (result == VK_NOT_READY)
      return false;
    VK_CHECK_RESULT(result)

    return false;
  }

  template <FenceIterator Iter>
  static bool
  wait_any(Iter begin, Iter end,
           uint64_t timeout = UINT64_MAX) noexcept(ExceptionsDisabled) {
    cntr::vector<VkFence, 4> fences{};
    for (auto it = begin; it != end; ++it) {
      fences.push_back((*it).m_fence);
    }
    return wait_impl(begin->m_device, fences.data(), fences.size(), false,
                     timeout);
  }

  template <FenceIterator Iter>
  static bool
  wait_all(Iter begin, Iter end,
           uint64_t timeout = UINT64_MAX) noexcept(ExceptionsDisabled) {
    cntr::vector<VkFence, 4> fences{};
    for (auto it = begin; it != end; ++it) {
      fences.push_back((*it).m_fence);
    }
    return wait_impl(begin->m_device, fences.data(), fences.size(), true,
                     timeout);
  }

private:
  static bool wait_impl(Device const &device, VkFence const *pFences,
                        uint32_t fenceCount, bool waitAll,
                        uint64_t timeout) noexcept(ExceptionsDisabled) {
    auto result = device.core<1, 0>().vkWaitForFences(
        device, fenceCount, pFences, static_cast<VkBool32>(waitAll), timeout);
    if (result == VK_TIMEOUT)
      return false;
    if (result == VK_SUCCESS)
      return true;
    VK_CHECK_RESULT(result)

    return false;
  }
};

} // namespace vkw
#endif // VKRENDERER_FENCE_HPP
