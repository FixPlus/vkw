#ifndef VKRENDERER_COMMANDBUFFER_HPP
#define VKRENDERER_COMMANDBUFFER_HPP

#include <vkw/CommandPool.hpp>
#include <vkw/DescriptorSet.hpp>
#include <vkw/RenderPass.hpp>
#include <vkw/VertexBuffer.hpp>

#include <boost/container/small_vector.hpp>
#include <optional>

namespace vkw {

class GraphicsPipeline;
class PipelineLayout;
class ComputePipeline;
class FrameBuffer;
class QueryPool;

class CommandBuffer : public ReferenceGuard {
public:
  CommandBuffer(CommandBuffer &&another) noexcept
      : m_device(another.m_device), m_pool(another.m_pool),
        m_executable(another.m_executable), m_recording(another.m_recording),
        m_commandBuffer(another.m_commandBuffer) {
    another.m_commandBuffer = VK_NULL_HANDLE;
  };

  CommandBuffer &operator=(CommandBuffer &&another) noexcept {
    m_device = another.m_device;
    m_pool = another.m_pool;
    m_executable = another.m_executable;
    m_recording = another.m_recording;
    std::swap(m_commandBuffer, another.m_commandBuffer);
    return *this;
  }

  uint32_t queueFamily() const noexcept;

  virtual ~CommandBuffer() = default;

  operator VkCommandBuffer() const noexcept { return m_commandBuffer; }

  bool isInRecordState() const noexcept { return m_recording; }

  bool isInExecutableState() const noexcept { return m_executable; }

  /** Transfer */

  void copyBufferToBuffer(BufferBase const &src, BufferBase const &dst,
                          std::span<const VkBufferCopy> regions) noexcept;
  void copyBufferToImage(BufferBase const &src, AllocatedImage const &dst,
                         VkImageLayout layout,
                         std::span<const VkBufferImageCopy> regions) noexcept;
  void copyImageToBuffer(AllocatedImage const &src, VkImageLayout layout,
                         BufferBase const &dst,
                         std::span<VkBufferImageCopy> regions) noexcept;

  void copyImageToImage(AllocatedImage const &src, VkImageLayout srcLayout,
                        AllocatedImage const &dst, VkImageLayout dstLayout,
                        std::span<const VkImageCopy> regions) noexcept;

  void blitImage(AllocatedImage const &targetImage, VkImageBlit blit,
                 bool usingGeneralLayout = false,
                 VkFilter filter = VK_FILTER_LINEAR) noexcept;

  void blitImage(AllocatedImage const &srcImage, AllocatedImage const &dstImage,
                 std::span<const VkImageBlit> blits,
                 VkFilter filter = VK_FILTER_LINEAR) noexcept;

  /** Synchronization */
  void
  pipelineBarrier(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                  std::span<const VkMemoryBarrier> memBarriers,
                  std::span<const VkImageMemoryBarrier> imageMemoryBarrier,
                  std::span<const VkBufferMemoryBarrier> bufferMemoryBarrier,
                  VkDependencyFlags flags = 0) noexcept;

  void
  imageMemoryBarrier(VkPipelineStageFlags srcStage,
                     VkPipelineStageFlags dstStage,
                     std::span<const VkImageMemoryBarrier> imageMemoryBarrier,
                     VkDependencyFlags flags = 0) noexcept {
    pipelineBarrier(srcStage, dstStage, {}, imageMemoryBarrier, {}, flags);
  }
  void bufferMemoryBarrier(
      VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
      std::span<const VkBufferMemoryBarrier> bufferMemoryBarrier,
      VkDependencyFlags flags = 0) noexcept {
    pipelineBarrier(srcStage, dstStage, {}, {}, bufferMemoryBarrier, flags);
  }
  void memoryBarrier(VkPipelineStageFlags srcStage,
                     VkPipelineStageFlags dstStage,
                     std::span<const VkMemoryBarrier> memBarriers,
                     VkDependencyFlags flags = 0) noexcept {
    pipelineBarrier(srcStage, dstStage, memBarriers, {}, {}, flags);
  }

  /** Binding operations */

  template <typename T>
  void bindVertexBuffer(VertexBuffer<T> const &vbuf, uint32_t binding,
                        VkDeviceSize offset) noexcept {
    m_bindVertexBuffer(static_cast<BufferBase const &>(vbuf), binding, offset);
  }

  template <VkIndexType type>
  void bindIndexBuffer(IndexBuffer<type> const &ibuf,
                       VkDeviceSize offset) noexcept {
    m_bindIndexBuffer(static_cast<BufferBase const &>(ibuf), type, offset);
  }

  void bindGraphicsPipeline(GraphicsPipeline const &pipeline) noexcept;
  void bindComputePipeline(ComputePipeline const &pipeline) noexcept;

  template <forward_range_of<DescriptorSet> T>
  void bindDescriptorSets(PipelineLayout const &layout,
                          VkPipelineBindPoint bindPoint, T const &sets,
                          uint32_t firstSet) noexcept(ExceptionsDisabled) {
    auto setsSubrange = ranges::make_subrange<DescriptorSet>(sets);
    using setsSubrangeT = decltype(setsSubrange);

    boost::container::small_vector<uint32_t, 3> dynamicOffsets{};
    boost::container::small_vector<VkDescriptorSet, 3> rawSets{};
    std::transform(setsSubrange.begin(), setsSubrange.end(),
                   std::back_inserter(rawSets),
                   [](auto const &set) -> VkDescriptorSet {
                     return setsSubrangeT::get(set);
                   });

    for (auto const &seth : setsSubrange) {
      auto &set = setsSubrangeT::get(seth);
      auto offsetCount = set.dynamicOffsetsCount();
      if (offsetCount == 0)
        continue;
      auto cachedSize = dynamicOffsets.size();
      dynamicOffsets.resize(cachedSize + offsetCount);
      set.copyOffsets(dynamicOffsets.data() + cachedSize);
    }
    m_bindDescriptorSets(layout, bindPoint, firstSet, rawSets.data(),
                         rawSets.size(), dynamicOffsets.data(),
                         dynamicOffsets.size());
  }

  void bindDescriptorSets(PipelineLayout const &layout,
                          VkPipelineBindPoint bindPoint,
                          DescriptorSet const &set,
                          uint32_t firstSet) noexcept(ExceptionsDisabled) {
    boost::container::small_vector<uint32_t, 3> dynamicOffsets{};

    auto offsetCount = set.dynamicOffsetsCount();
    if (offsetCount != 0) {
      auto cachedSize = dynamicOffsets.size();
      dynamicOffsets.resize(cachedSize + offsetCount);
      set.copyOffsets(dynamicOffsets.data() + cachedSize);
    }

    VkDescriptorSet rawSet = set;
    m_bindDescriptorSets(layout, bindPoint, firstSet, &rawSet, 1,
                         dynamicOffsets.data(), dynamicOffsets.size());
  }

  template <typename T>
  void pushConstants(PipelineLayout const &layout,
                     VkShaderStageFlagBits shaderStage, uint32_t offset,
                     T const &constant) noexcept {
    m_pushConstants(layout, shaderStage, offset, sizeof(constant), &constant);
  }

  template <typename T>
  void pushConstantsSpan(PipelineLayout const &layout,
                         VkShaderStageFlagBits shaderStage, uint32_t offset,
                         std::span<const T> constantSpan) noexcept {
    m_pushConstants(layout, shaderStage, offset,
                    sizeof(T) * constantSpan.size(), constantSpan.data());
  }

  /** Draw commands */

  void draw(uint32_t vertexCount, uint32_t instanceCount = 0,
            uint32_t firstVertex = 0, uint32_t firstInstance = 0) noexcept;
  void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 0,
                   uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                   uint32_t firstInstance = 0) noexcept;

  /** Dispatch commands */

  void dispatch(uint32_t groupCountX, uint32_t groupCountY,
                uint32_t groupCountZ) noexcept;

  /** Pipeline dynamic state sets */

  void setScissors(std::span<const VkRect2D> scissors,
                   uint32_t firstScissor = 0) noexcept;
  void setViewports(std::span<const VkViewport> viewports,
                    uint32_t firstViewport = 0) noexcept;

  /** Query **/
  void resetQuery(const QueryPool &queryPool, uint32_t firstQuery,
                  uint32_t count) noexcept;
  void resetQuery(const QueryPool &queryPool) noexcept;

  void beginQuery(const QueryPool &queryPool, uint32_t query,
                  VkQueryControlFlags flags = 0) noexcept;

  void endQuery(const QueryPool &queryPool, uint32_t query) noexcept;

  /** Finishing */

  void end() noexcept(ExceptionsDisabled);

  void reset(VkCommandBufferResetFlags flags) noexcept(ExceptionsDisabled);

protected:
  CommandBuffer(CommandPool &pool,
                VkCommandBufferLevel bufferLevel) noexcept(ExceptionsDisabled);
  StrongReference<Device const> m_device;
  StrongReference<CommandPool const> m_pool;
  VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
  void m_begin(VkCommandBufferUsageFlags flags,
               VkCommandBufferInheritanceInfo const
                   *inheritanceInfo) noexcept(ExceptionsDisabled);

private:
  void m_pushConstants(PipelineLayout const &layout,
                       VkShaderStageFlagBits shaderStage, uint32_t offset,
                       uint32_t size, const void *data) noexcept;
  void m_bindVertexBuffer(VkBuffer buffer, uint32_t binding,
                          VkDeviceSize offset) noexcept;
  void m_bindIndexBuffer(VkBuffer buffer, VkIndexType type,
                         VkDeviceSize offset) noexcept;

  void m_bindDescriptorSets(const PipelineLayout &layout,
                            VkPipelineBindPoint bindPoint, size_t firstSet,
                            VkDescriptorSet const *sets, size_t nsets,
                            uint32_t *dynOffsets, size_t ndynOffsets) noexcept;
  bool m_recording = false;
  bool m_executable = false;
};

class SecondaryCommandBuffer : public CommandBuffer {
public:
  explicit SecondaryCommandBuffer(CommandPool &pool) noexcept(
      ExceptionsDisabled)
      : CommandBuffer(pool, VK_COMMAND_BUFFER_LEVEL_SECONDARY){};

  void begin(VkCommandBufferUsageFlags flags,
             VkCommandBufferInheritanceInfo const *inheritanceInfo =
                 nullptr) noexcept(ExceptionsDisabled) {
    m_begin(flags, inheritanceInfo);
  }
};

class PrimaryCommandBuffer : public CommandBuffer {
public:
  explicit PrimaryCommandBuffer(CommandPool &pool) noexcept(ExceptionsDisabled)
      : CommandBuffer(pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY){};

  void beginRenderPass(RenderPass const &renderPass,
                       FrameBuffer const &frameBuffer, VkRect2D renderArea,
                       bool useSecondary = false,
                       std::span<const VkClearValue> clearValues =
                           {}) noexcept(ExceptionsDisabled);

  void nextSubpass(bool useSecondary = false) noexcept(ExceptionsDisabled);

  template <forward_range_of<SecondaryCommandBuffer> T>
  void executeCommands(T const &commands) noexcept(ExceptionsDisabled) {
    boost::container::small_vector<VkCommandBuffer, 5> rawBufs;

    std::transform(
        commands.begin(), commands.end(),
        [](std::ranges::range_value_t<T> const &command) { return command; });
    m_executeCommands(rawBufs.size(), rawBufs.data());
  }

  void endRenderPass() noexcept(ExceptionsDisabled);

  void begin(VkCommandBufferUsageFlags flags) noexcept(ExceptionsDisabled) {

    // According to vulkan specification primary command buffers don't use
    // VkCommandBufferInheritanceInfo

    m_begin(flags, nullptr);
  }

private:
  void m_executeCommands(size_t nbufs, VkCommandBuffer const *buffers) noexcept;
  // FIXME: This strong reference causes irrecoverable errors when exception is
  // thrown while recording. Better to use something else.
#ifdef VKW_COMMAND_BUFFER_TRACK_RENDER_PASSES
  std::optional<StrongReference<RenderPass const>> m_currentPass;
  uint32_t m_currentSubpass;
#endif
};

} // namespace vkw
#endif // VKRENDERER_COMMANDBUFFER_HPP
