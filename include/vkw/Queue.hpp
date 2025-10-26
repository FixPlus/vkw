#ifndef VKRENDERER_QUEUE_HPP
#define VKRENDERER_QUEUE_HPP

#include <vkw/CommandBuffer.hpp>
#include <vkw/Containers.hpp>
#include <vkw/RangeConcepts.hpp>
#include <vkw/Semaphore.hpp>
#include <vkw/Surface.hpp>
#include <vkw/SwapChain.hpp>

#include <cassert>
#include <utility>

namespace vkw {

class PresentInfo {
public:
  template <forward_range_of<SwapChain> SWA, forward_range_of<Semaphore> SMA>
  PresentInfo(SWA const &swapChains,
              SMA const &waitFor) noexcept(ExceptionsDisabled)
      : m_swp_ext(decltype(ranges::make_subrange<SwapChain>(swapChains))::get(
                      *(ranges::make_subrange<SwapChain>(swapChains).begin()))
                      .ext()) {
    auto swapChainsSubrange = ranges::make_subrange<SwapChain>(swapChains);
    using swapChainsSubrangeT = decltype(swapChainsSubrange);
    auto waitForSub = ranges::make_subrange<Semaphore>(waitFor);
    using SMASubT = decltype(waitForSub);

    std::transform(swapChainsSubrange.begin(), swapChainsSubrange.end(),
                   std::back_inserter(m_swapChains),
                   [](auto const &swp) -> VkSwapchainKHR {
                     return swapChainsSubrangeT::get(swp);
                   });

    std::transform(swapChainsSubrange.begin(), swapChainsSubrange.end(),
                   std::back_inserter(m_pswapChains),
                   [](auto const &swp) -> SwapChain const * {
                     return swapChainsSubrangeT::get(swp);
                   });
    std::transform(
        waitForSub.begin(), waitForSub.end(),
        std::back_inserter(m_wait_semaphores),
        [](auto const &smr) -> VkSemaphore { return SMASubT::get(smr); });

    std::transform(swapChainsSubrange.begin(), swapChainsSubrange.end(),
                   std::back_inserter(m_images), [](auto const &swp) {
                     return swapChainsSubrangeT::get(swp).currentImage();
                   });

    m_fill_info();
  }

  template <forward_range_of<Semaphore> SMA = cntr::vector<Semaphore, 2>>
  PresentInfo(SwapChain const &swapChain,
              SMA const &waitFor = {}) noexcept(ExceptionsDisabled)
      : m_swp_ext(swapChain.extension()) {

    auto waitForSub = ranges::make_subrange<Semaphore>(waitFor);
    using SMASubT = decltype(waitForSub);

    m_swapChains.emplace_back(swapChain);
    m_pswapChains.emplace_back(&swapChain);
    m_images.emplace_back(swapChain.currentImage());

    std::transform(
        waitForSub.begin(), waitForSub.end(),
        std::back_inserter(m_wait_semaphores),
        [](auto const &smr) -> VkSemaphore { return SMASubT::get(smr); });

    m_fill_info();
  }

  PresentInfo(SwapChain const &swapChain,
              Semaphore const &waitFor) noexcept(ExceptionsDisabled)
      : m_swp_ext(swapChain.extension()) {

    m_swapChains.emplace_back(swapChain);
    m_pswapChains.emplace_back(swapChain);
    m_images.emplace_back(swapChain.currentImage());
    m_wait_semaphores.emplace_back(waitFor);

    m_fill_info();
  }

  PresentInfo(SwapChain const &swapChain) noexcept(ExceptionsDisabled)
      : m_swp_ext(swapChain.extension()) {

    m_swapChains.emplace_back(swapChain);
    m_pswapChains.emplace_back(swapChain);
    m_images.emplace_back(swapChain.currentImage());

    m_fill_info();
  }

  void updateImages() noexcept(ExceptionsDisabled) {
    auto swpIt = m_pswapChains.begin();
    auto swpIm = m_images.begin();
    for (; swpIt != m_pswapChains.end(); ++swpIt, ++swpIm) {
      *swpIm = (*swpIt).get().currentImage();
    }
  }

  operator VkPresentInfoKHR() const noexcept { return m_info; }

  Extension<ext::KHR_swapchain> const &swapChainExtension() const noexcept {
    return m_swp_ext.get();
  }

  PresentInfo(PresentInfo const &another) noexcept(ExceptionsDisabled)
      : m_wait_semaphores(another.m_wait_semaphores),
        m_swapChains(another.m_swapChains), m_images(another.m_images),
        m_swp_ext(another.m_swp_ext) {
    m_fill_info();
  }
  PresentInfo(PresentInfo &&another) noexcept = default;

  PresentInfo &
  operator=(PresentInfo const &another) noexcept(ExceptionsDisabled) {
    m_wait_semaphores = another.m_wait_semaphores;
    m_swapChains = another.m_swapChains;
    m_images = another.m_images;
    m_swp_ext = another.m_swp_ext;
    m_fill_info();
    return *this;
  }

  PresentInfo &operator=(PresentInfo &&another) noexcept = default;

  virtual ~PresentInfo() = default;

private:
  void m_fill_info() noexcept {
    m_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    m_info.pNext = nullptr;
    m_info.waitSemaphoreCount = m_wait_semaphores.size();
    m_info.pWaitSemaphores = m_wait_semaphores.data();
    m_info.swapchainCount = m_swapChains.size();
    m_info.pSwapchains = m_swapChains.data();
    m_info.pImageIndices = m_images.data();
    m_info.pResults = nullptr;
  }

  cntr::vector<VkSemaphore, 2> m_wait_semaphores;
  cntr::vector<StrongReference<SwapChain const>, 2> m_pswapChains;
  cntr::vector<VkSwapchainKHR, 2> m_swapChains;
  cntr::vector<uint32_t, 2> m_images;

  // TODO: rewrite to have a StrongReference or just copy it
  std::reference_wrapper<Extension<ext::KHR_swapchain> const> m_swp_ext;
  VkPresentInfoKHR m_info{};
};

class SubmitInfo {
public:
  template <forward_range_of<PrimaryCommandBuffer const> PCMDA,
            forward_range_of<Semaphore const> SMA,
            forward_range_of<VkPipelineStageFlags const> PSFA =
                cntr::vector<VkPipelineStageFlags, 2>>
  SubmitInfo(PCMDA const &commandBuffer, SMA const &waitFor = {},
             PSFA const &waitTill = {},
             SMA const &signalTo = {}) noexcept(ExceptionsDisabled) {
    auto commandBufferSub =
        ranges::make_subrange<PrimaryCommandBuffer const>(commandBuffer);
    using commandBufferSubT = decltype(commandBufferSub);

    auto waitForSub = ranges::make_subrange<Semaphore const>(waitFor);
    auto signalToSub = ranges::make_subrange<Semaphore const>(signalTo);
    using semaphoreSubT = decltype(waitForSub);

    auto waitTillSub = ranges::make_subrange<VkPipelineStageFlags>(waitTill);
    using FSFASubT = decltype(waitTillSub);

    std::transform(commandBufferSub.begin(), commandBufferSub.end(),
                   std::back_inserter(m_cmd_buffers),
                   [](auto const &cmd) -> VkCommandBuffer {
                     return commandBufferSubT::get(cmd);
                   });
    std::transform(
        waitForSub.begin(), waitForSub.end(),
        std::back_inserter(m_wait_semaphores),
        [](auto const &smr) -> VkSemaphore { return semaphoreSubT::get(smr); });
    std::transform(
        signalToSub.begin(), signalToSub.end(),
        std::back_inserter(m_signal_semaphores),
        [](auto const &smr) -> VkSemaphore { return semaphoreSubT::get(smr); });

    std::transform(waitTillSub.begin(), waitTillSub.end(),
                   std::back_inserter(m_wait_stage),
                   [](auto const &stage) -> VkPipelineStageFlags {
                     return FSFASubT::get(stage);
                   });

    m_fill_info();
  }

  operator VkSubmitInfo() const noexcept { return m_info; }

  SubmitInfo(const PrimaryCommandBuffer &commandBuffer,
             Semaphore const &waitFor, VkPipelineStageFlags waitTill,
             Semaphore const &signalTo) noexcept(ExceptionsDisabled)
      : m_wait_stage(1, waitTill), m_cmd_buffers(1, commandBuffer),
        m_wait_semaphores(1, waitFor), m_signal_semaphores(1, signalTo) {
    m_fill_info();
  }

  template <forward_range_of<Semaphore const> SMA = cntr::vector<Semaphore, 2>,
            forward_range_of<VkPipelineStageFlags const> PSFA =
                cntr::vector<VkPipelineStageFlags, 2>>
  SubmitInfo(SMA const &waitFor = {}, PSFA const &waitTill = {},
             SMA const &signalTo = {}) noexcept(ExceptionsDisabled) {
    auto waitForSub = ranges::make_subrange<Semaphore const>(waitFor);
    auto signalToSub = ranges::make_subrange<Semaphore const>(signalTo);
    auto waitTillSub =
        ranges::make_subrange<VkPipelineStageFlags const>(waitTill);
    using FSFASubT = decltype(waitTillSub);
    using SMASubT = decltype(waitForSub);

    std::transform(
        waitForSub.begin(), waitForSub.end(),
        std::back_inserter(m_wait_semaphores),
        [](auto const &smr) -> VkSemaphore { return SMASubT::get(smr); });
    std::transform(
        signalToSub.begin(), signalToSub.end(),
        std::back_inserter(m_signal_semaphores),
        [](auto const &smr) -> VkSemaphore { return SMASubT::get(smr); });

    std::transform(waitTillSub.begin(), waitTillSub.end(),
                   std::back_inserter(m_wait_stage),
                   [](auto const &stage) -> VkPipelineStageFlags {
                     return FSFASubT::get(stage);
                   });

    m_fill_info();
  }

  template <forward_range_of<Semaphore const> SMA = cntr::vector<Semaphore, 2>,
            forward_range_of<VkPipelineStageFlags const> PSFA =
                cntr::vector<VkPipelineStageFlags, 2>>
  SubmitInfo(const PrimaryCommandBuffer &commandBuffer, SMA const &waitFor = {},
             PSFA const &waitTill = {},
             SMA const &signalTo = {}) noexcept(ExceptionsDisabled)
      : SubmitInfo(waitFor, waitTill, signalTo) {

    m_cmd_buffers.emplace_back(commandBuffer);

    m_info.commandBufferCount = m_cmd_buffers.size();
    m_info.pCommandBuffers = m_cmd_buffers.data();
  }

  SubmitInfo &
  operator=(SubmitInfo const &another) noexcept(ExceptionsDisabled) {
    m_cmd_buffers = another.m_cmd_buffers;
    m_signal_semaphores = another.m_signal_semaphores;
    m_wait_semaphores = another.m_wait_semaphores;
    m_wait_stage = another.m_wait_stage;
    m_fill_info();
    return *this;
  }
  SubmitInfo &operator=(SubmitInfo &&another) noexcept {
    m_cmd_buffers = std::move(another.m_cmd_buffers);
    m_signal_semaphores = std::move(another.m_signal_semaphores);
    m_wait_semaphores = std::move(another.m_wait_semaphores);
    m_wait_stage = std::move(another.m_wait_stage);
    m_fill_info();
    return *this;
  }

  SubmitInfo(SubmitInfo const &another) noexcept(ExceptionsDisabled)
      : m_cmd_buffers(another.m_cmd_buffers),
        m_signal_semaphores(another.m_signal_semaphores),
        m_wait_semaphores(another.m_wait_semaphores),
        m_wait_stage(another.m_wait_stage) {
    m_fill_info();
  }
  SubmitInfo(SubmitInfo &&another) noexcept
      : m_cmd_buffers(std::move(another.m_cmd_buffers)),
        m_signal_semaphores(std::move(another.m_signal_semaphores)),
        m_wait_semaphores(std::move(another.m_wait_semaphores)),
        m_wait_stage(std::move(another.m_wait_stage)) {
    m_cmd_buffers = another.m_cmd_buffers;
    m_signal_semaphores = another.m_signal_semaphores;
    m_wait_semaphores = another.m_wait_semaphores;
    m_wait_stage = another.m_wait_stage;
    m_fill_info();
  }

private:
  cntr::vector<VkCommandBuffer, 2> m_cmd_buffers;
  cntr::vector<VkSemaphore, 2> m_signal_semaphores;
  cntr::vector<VkSemaphore, 2> m_wait_semaphores;
  cntr::vector<VkPipelineStageFlags, 2> m_wait_stage;

  void m_fill_info() noexcept {
    assert(
        m_wait_stage.size() == m_wait_semaphores.size() &&
        "Count of dst stage masks must be equal to count of wait semaphores");
    m_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    m_info.pNext = nullptr;
    m_info.commandBufferCount = m_cmd_buffers.size();
    m_info.pCommandBuffers = m_cmd_buffers.data();
    m_info.signalSemaphoreCount = m_signal_semaphores.size();
    m_info.pSignalSemaphores = m_signal_semaphores.data();
    m_info.waitSemaphoreCount = m_wait_semaphores.size();
    m_info.pWaitSemaphores = m_wait_semaphores.data();
    m_info.pWaitDstStageMask = m_wait_stage.data();
  }

  VkSubmitInfo m_info{};
};

class QueueMissing final : public Error {
public:
  QueueMissing(std::string_view what) : Error(what){};
  std::string_view codeString() const noexcept override {
    return "Queue missing";
  }
};

class Queue {
public:
  static Queue anyAvailable(Device &parent,
                            auto &&famPredicate) noexcept(ExceptionsDisabled) {
    auto fams = parent.physicalDevice().queueFamilies();
    auto family = std::find_if(fams.begin(), fams.end(), famPredicate);
    while (family != fams.end()) {
      if (family->hasRequestedQueues())
        return Queue(parent, family->index(), 0);
      family++;
      family = std::find_if(family, fams.end(), famPredicate);
    }

    postError(QueueMissing{"Device does not have matching queues"});
  }

  Queue(Device &parent, uint32_t queueFamilyIndex,
        uint32_t queueIndex) noexcept(ExceptionsDisabled)
      : m_parent(parent), m_familyIndex(queueFamilyIndex),
        m_queueIndex(queueIndex) {
    auto fams = parent.physicalDevice().queueFamilies();
    auto foundFam = std::ranges::find_if(
        fams, [&](auto &&f) { return f.index() == queueFamilyIndex; });
    if (foundFam == fams.end())
      postError(QueueMissing{[&]() {
        std::stringstream ss;
        ss << "device has no queue family at index " << queueFamilyIndex;
        return ss.str();
      }()});
    if (foundFam->queueRequestedCount() <= queueIndex)
      postError(QueueMissing{[&]() {
        std::stringstream ss;
        ss << "cannot create queue #" << queueIndex
           << " device queue family at index " << queueFamilyIndex
           << " has only " << foundFam->queueRequestedCount()
           << " queues requested.";
        return ss.str();
      }()});
    m_parent.get().core<1, 0>().vkGetDeviceQueue(parent, queueFamilyIndex,
                                                 queueIndex, &m_queue);
  }

  bool present(PresentInfo const &presentInfo) const
      noexcept(ExceptionsDisabled) {
    VkPresentInfoKHR info = presentInfo;
    return m_presentImpl(presentInfo.swapChainExtension().vkQueuePresentKHR,
                         m_queue, &info);
  }

  void submit(SubmitInfo const &info) const noexcept(ExceptionsDisabled) {
    VkSubmitInfo rawInfo = info;
    m_submit(&rawInfo, 1, nullptr);
  }

  void submit(SubmitInfo const &info, Fence const &fence) const
      noexcept(ExceptionsDisabled) {
    VkSubmitInfo rawInfo = info;
    m_submit(&rawInfo, 1, &fence);
  }

  template <forward_range_of<SubmitInfo const> SubmitRange>
  void submit(SubmitRange const &info, Fence const &fence) const
      noexcept(ExceptionsDisabled) {
    auto infoSubrange = ranges::make_subrange<SubmitInfo const>(info);
    using infoSubrangeT = decltype(infoSubrange);
    cntr::vector<VkSubmitInfo, 3> m_infos;
    std::transform(infoSubrange.begin(), infoSubrange.end(),
                   std::back_inserter(m_infos),
                   [](auto const &info) -> VkSubmitInfo {
                     return infoSubrangeT::get(info);
                   });
    m_submit(m_infos.data(), m_infos.size(), &fence);
  }

  template <forward_range_of<SubmitInfo const> SubmitRange>
  void submit(SubmitRange const &info) const noexcept(ExceptionsDisabled) {
    auto infoSubrange = ranges::make_subrange<SubmitInfo const>(info);
    using infoSubrangeT = decltype(infoSubrange);
    cntr::vector<VkSubmitInfo, 3> m_infos;
    std::transform(infoSubrange.begin(), infoSubrange.end(),
                   std::back_inserter(m_infos),
                   [](auto const &info) -> VkSubmitInfo {
                     return infoSubrangeT::get(info);
                   });
    m_submit(m_infos.data(), m_infos.size(), nullptr);
  }

  QueueFamily const &family() const noexcept(ExceptionsDisabled) {
    return *(m_parent.get().physicalDevice().queueFamilies().begin() +
             m_familyIndex);
  }

  unsigned index() const noexcept { return m_queueIndex; }

  bool supportsPresenting(Surface const &surface) const
      noexcept(ExceptionsDisabled) {
    VkBool32 ret;
    VK_CHECK_RESULT(surface.ext().vkGetPhysicalDeviceSurfaceSupportKHR(
        m_parent.get().physicalDevice(), m_familyIndex, surface, &ret))
    return ret;
  }

  operator VkQueue() const noexcept { return m_queue; }

  void waitIdle() const noexcept(ExceptionsDisabled) {
    VK_CHECK_RESULT(m_parent.get().core<1, 0>().vkQueueWaitIdle(m_queue))
  }

private:
  static bool m_presentImpl(
      PFN_vkQueuePresentKHR p_vkQueuePresentKHR, VkQueue queue,
      VkPresentInfoKHR const *pPresentInfo) noexcept(ExceptionsDisabled) {
    auto result = p_vkQueuePresentKHR(queue, pPresentInfo);

    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
      return true;
    }

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
      return false;

    VK_CHECK_RESULT(result)

    return false;
  }

  void m_submit(VkSubmitInfo const *info, size_t infoCount,
                Fence const *fence) const noexcept(ExceptionsDisabled) {

    VK_CHECK_RESULT(m_parent.get().core<1, 0>().vkQueueSubmit(
        m_queue, infoCount, info,
        fence ? fence->operator VkFence_T *() : VK_NULL_HANDLE));
  }
  StrongReference<Device> m_parent;
  VkQueue m_queue = VK_NULL_HANDLE;
  uint32_t m_familyIndex;
  uint32_t m_queueIndex;
};

} // namespace vkw
#endif // VKRENDERER_QUEUE_HPP
