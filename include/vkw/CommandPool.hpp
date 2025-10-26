#ifndef VKRENDERER_COMMANDPOOL_HPP
#define VKRENDERER_COMMANDPOOL_HPP

#include <vkw/Device.hpp>

namespace vkw {

class CommandPool : public vk::CommandPool {
public:
  CommandPool(Device const &device, VkCommandPoolCreateFlags flags,
              uint32_t queueFamily) noexcept(ExceptionsDisabled)
      : vk::CommandPool(device,
                        [&]() {
                          VkCommandPoolCreateInfo createInfo{};
                          createInfo.sType =
                              VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                          createInfo.pNext = nullptr;
                          createInfo.queueFamilyIndex = queueFamily;
                          createInfo.flags = flags;

                          return createInfo;
                        }()),
        m_queueFamily(queueFamily), m_createFlags(flags) {}

  bool canResetCommandBuffer() const noexcept {
    return m_createFlags & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  }

  bool transientAllocations() const noexcept {
    return m_createFlags & VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  }

  uint32_t queueFamilyIndex() const noexcept { return m_queueFamily; }

private:
  VkCommandPoolCreateFlags m_createFlags;
  uint32_t m_queueFamily;
};
} // namespace vkw
#endif // VKRENDERER_COMMANDPOOL_HPP
