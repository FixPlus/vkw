#ifndef VKRENDERER_COMMANDBUFFER_HPP
#define VKRENDERER_COMMANDBUFFER_HPP

#include <vkw/CommandPool.hpp>

namespace vkw {

class CommandBuffer : public ReferenceGuard {
public:
  CommandBuffer(CommandBuffer &&another) noexcept = default;
  CommandBuffer &operator=(CommandBuffer &&another) noexcept = default;

  CommandBuffer(const CommandBuffer &another) noexcept = delete;
  CommandBuffer &operator=(const CommandBuffer &another) noexcept = delete;

  ~CommandBuffer() override = default;

  operator VkCommandBuffer() const noexcept { return m_commandBuffer; }

  void reset(VkCommandBufferResetFlags flags) noexcept(ExceptionsDisabled) {
    VK_CHECK_RESULT(m_pool.get().parent().core<1, 0>().vkResetCommandBuffer(
        m_commandBuffer, flags))
  }

  auto &parent() const { return m_pool.get(); }

protected:
  CommandBuffer(CommandPool &pool,
                VkCommandBufferLevel bufferLevel) noexcept(ExceptionsDisabled)
      : m_pool(pool) {
    auto &device = pool.parent();
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.commandBufferCount = 1;
    allocInfo.commandPool = pool;

    VK_CHECK_RESULT(device.core<1, 0>().vkAllocateCommandBuffers(
        device, &allocInfo, &m_commandBuffer));
  }
  StrongReference<CommandPool> m_pool;
  VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
};

class SecondaryCommandBuffer : public CommandBuffer {
public:
  explicit SecondaryCommandBuffer(CommandPool &pool) noexcept(
      ExceptionsDisabled)
      : CommandBuffer(pool, VK_COMMAND_BUFFER_LEVEL_SECONDARY){};
};

class PrimaryCommandBuffer : public CommandBuffer {
public:
  explicit PrimaryCommandBuffer(CommandPool &pool) noexcept(ExceptionsDisabled)
      : CommandBuffer(pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY){};
};

} // namespace vkw
#endif // VKRENDERER_COMMANDBUFFER_HPP
