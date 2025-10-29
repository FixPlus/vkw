#ifndef VKRENDERER_SEMAPHORE_HPP
#define VKRENDERER_SEMAPHORE_HPP

#include <vkw/Device.hpp>

namespace vkw {

class Semaphore : public vk::Semaphore {
public:
  Semaphore(Device const &device) noexcept(ExceptionsDisabled)
      : vk::Semaphore(device, [&]() {
          VkSemaphoreCreateInfo createInfo{};
          createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
          createInfo.pNext = nullptr;

          return createInfo;
        }()) {}
};

} // namespace vkw
#endif // VKRENDERER_SEMAPHORE_HPP
