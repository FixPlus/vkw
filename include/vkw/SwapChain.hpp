#ifndef VKRENDERER_SWAPCHAIN_HPP
#define VKRENDERER_SWAPCHAIN_HPP

#include <vkw/Containers.hpp>
#include <vkw/Extensions.hpp>
#include <vkw/Fence.hpp>
#include <vkw/Image.hpp>
#include <vkw/Semaphore.hpp>

#include <optional>

namespace vkw {

class SwapChain : public ReferenceGuard {
public:
  SwapChain(
      Device &device,
      const VkSwapchainCreateInfoKHR &createInfo) noexcept(ExceptionsDisabled)
      : m_swapchain(nullptr, SwapChainDestructor{device}) {
    VkSwapchainKHR tmpSwapchain = nullptr;
    VK_CHECK_RESULT(extension().vkCreateSwapchainKHR(
        device, &createInfo, HostAllocator::get(), &tmpSwapchain));
    m_swapchain.reset(tmpSwapchain);
    uint32_t imageCount = 0;

    VK_CHECK_RESULT(extension().vkGetSwapchainImagesKHR(
        device, m_swapchain.get(), &imageCount, nullptr));

    cntr::vector<VkImage, 3> images(imageCount);
    VK_CHECK_RESULT(extension().vkGetSwapchainImagesKHR(
        device, m_swapchain.get(), &imageCount, images.data()))
    m_images.reserve(imageCount);
    std::ranges::transform(
        images, std::back_inserter(m_images), [&](auto &&image) {
          return SwapChainImage{image,
                                createInfo.imageFormat,
                                createInfo.imageExtent.width,
                                createInfo.imageExtent.height,
                                createInfo.imageArrayLayers,
                                createInfo.imageUsage};
        });
  }

  enum AcquireStatus {
    SUCCESSFUL,
    SUBOPTIMAL,
    NOT_READY,
    TIMEOUT,
    OUT_OF_DATE
  };

  AcquireStatus
  acquireNextImage(Semaphore const &signalSemaphore, Fence const &signalFence,
                   uint64_t timeout = UINT64_MAX) noexcept(ExceptionsDisabled) {
    return acquireNextImageImpl(signalSemaphore, signalFence, timeout);
  }
  AcquireStatus
  acquireNextImage(Semaphore const &signalSemaphore,
                   uint64_t timeout = UINT64_MAX) noexcept(ExceptionsDisabled) {
    return acquireNextImageImpl(signalSemaphore, VK_NULL_HANDLE, timeout);
  }
  AcquireStatus
  acquireNextImage(Fence const &signalFence,
                   uint64_t timeout = UINT64_MAX) noexcept(ExceptionsDisabled) {
    return acquireNextImageImpl(VK_NULL_HANDLE, signalFence, timeout);
  }

  auto images() const noexcept {
    return std::ranges::subrange(m_images.begin(), m_images.end());
  }

  uint32_t currentImage() const noexcept(ExceptionsDisabled) {
    assert(m_currentImage && "No image has been acquired yet");

    return m_currentImage.value();
  }

  Extension<ext::KHR_swapchain> const &extension() const noexcept {
    return m_swapchain.get_deleter().swapExt;
  }

  operator VkSwapchainKHR() const noexcept { return m_swapchain.get(); }

private:
  AcquireStatus
  acquireNextImageImpl(VkSemaphore semaphore, VkFence fence,
                       uint64_t timeout) noexcept(ExceptionsDisabled) {
    uint32_t imageIndex;
    auto result = extension().vkAcquireNextImageKHR(
        m_swapchain.get_deleter().device.get(), m_swapchain.get(), timeout,
        semaphore, fence, &imageIndex);

    switch (result) {
    case VK_SUBOPTIMAL_KHR:
      m_currentImage = imageIndex;
      return AcquireStatus::SUBOPTIMAL;
    case VK_SUCCESS:
      m_currentImage = imageIndex;
      return AcquireStatus::SUCCESSFUL;
    case VK_NOT_READY:
      return AcquireStatus::NOT_READY;
    case VK_TIMEOUT:
      return AcquireStatus::TIMEOUT;
    case VK_ERROR_OUT_OF_DATE_KHR:
      return AcquireStatus::OUT_OF_DATE;
    default:
      VK_CHECK_RESULT(result)
      return AcquireStatus::OUT_OF_DATE;
    }
  }

  struct SwapChainDestructor {
    SwapChainDestructor(Device &dev) : device(dev), swapExt(device){};
    void operator()(VkSwapchainKHR swapchain) const {
      if (!swapchain)
        return;
      swapExt.vkDestroySwapchainKHR(device.get(), swapchain,
                                    HostAllocator::get());
    }
    StrongReference<Device> device;
    Extension<ext::KHR_swapchain> swapExt;
  };
  std::unique_ptr<VkSwapchainKHR_T, SwapChainDestructor> m_swapchain;
  std::optional<uint32_t> m_currentImage{};
  cntr::vector<SwapChainImage, 3> m_images;
};

} // namespace vkw
#endif // VKRENDERER_SWAPCHAIN_HPP
