#pragma once

#include <vkw/CommandBuffer.hpp>
#include <vkw/DescriptorSet.hpp>
#include <vkw/FrameBuffer.hpp>
#include <vkw/Pipeline.hpp>
#include <vkw/Query.hpp>
#include <vkw/RenderPass.hpp>
#include <vkw/VertexBuffer.hpp>

namespace vkw {

class BasicRecorder {
public:
  BasicRecorder(CommandBuffer &buffer)
      : m_symbols(&buffer.parent().parent().core<1, 0>()), m_buffer(buffer){};
  BasicRecorder(BasicRecorder &&) noexcept = default;
  BasicRecorder &operator=(BasicRecorder &&) noexcept = default;
  BasicRecorder(const BasicRecorder &) = delete;
  BasicRecorder &operator=(const BasicRecorder &) = delete;

  /** Execute secondary **/

  template <forward_range_of<SecondaryCommandBuffer> T>
  void executeCommands(T const &commands) noexcept(ExceptionsDisabled) {
    cntr::vector<VkCommandBuffer, 5> rawBufs;
    std::transform(
        commands.begin(), commands.end(),
        [](std::ranges::range_value_t<T> const &command) { return command; });
    m_symbols->vkCmdExecuteCommands(m_buffer, rawBufs.size(), rawBufs.data());
  }

  /** Synchronization */

  void
  pipelineBarrier(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                  std::span<const VkMemoryBarrier> memBarriers,
                  std::span<const VkImageMemoryBarrier> imageMemoryBarrier,
                  std::span<const VkBufferMemoryBarrier> bufferMemoryBarrier,
                  VkDependencyFlags flags = 0) noexcept {
    m_symbols->vkCmdPipelineBarrier(
        m_buffer, srcStage, dstStage, flags, memBarriers.size(),
        memBarriers.data(), bufferMemoryBarrier.size(),
        bufferMemoryBarrier.data(), imageMemoryBarrier.size(),
        imageMemoryBarrier.data());
  }

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

  /** Query **/
  void resetQuery(const QueryPool &queryPool, uint32_t firstQuery,
                  uint32_t count) noexcept {
    m_symbols->vkCmdResetQueryPool(m_buffer, queryPool, firstQuery, count);
  }
  void resetQuery(const QueryPool &queryPool) noexcept {
    m_symbols->vkCmdResetQueryPool(m_buffer, queryPool, 0u, queryPool.size());
  }

  void beginQuery(const QueryPool &queryPool, uint32_t query,
                  VkQueryControlFlags flags = 0) noexcept {
    m_symbols->vkCmdBeginQuery(m_buffer, queryPool, query, flags);
  }

  void endQuery(const QueryPool &queryPool, uint32_t query) noexcept {
    m_symbols->vkCmdEndQuery(m_buffer, queryPool, query);
  }

  virtual ~BasicRecorder() = default;

protected:
  friend class BufferRecorder;
  DeviceCore<1, 0> const *m_symbols;
  VkCommandBuffer m_buffer;
};

class DescriptorRecorder : public BasicRecorder {
public:
  DescriptorRecorder(CommandBuffer &buffer) : BasicRecorder(buffer){};

  /** Binding operations **/

  template <forward_range_of<DescriptorSet> T>
  void bindDescriptorSets(PipelineLayout const &layout,
                          VkPipelineBindPoint bindPoint, T const &sets,
                          uint32_t firstSet) noexcept(ExceptionsDisabled) {
    auto setsSubrange = ranges::make_subrange<DescriptorSet>(sets);
    using setsSubrangeT = decltype(setsSubrange);

    cntr::vector<uint32_t, 3> dynamicOffsets{};
    cntr::vector<VkDescriptorSet, 3> rawSets{};
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

    m_symbols->vkCmdBindDescriptorSets(
        m_buffer, bindPoint, layout, firstSet, rawSets.size(), rawSets.data(),
        dynamicOffsets.size(), dynamicOffsets.data());
  }

  void bindDescriptorSet(PipelineLayout const &layout,
                         VkPipelineBindPoint bindPoint,
                         DescriptorSet const &set,
                         uint32_t firstSet) noexcept(ExceptionsDisabled) {
    cntr::vector<uint32_t, 3> dynamicOffsets{};

    std::ranges::transform(set.dynamicOffsets(),
                           std::back_inserter(dynamicOffsets),
                           [](auto &&a) { return a.offset; });

    VkDescriptorSet rawSet = set;
    m_symbols->vkCmdBindDescriptorSets(m_buffer, bindPoint, layout, firstSet, 1,
                                       &rawSet, dynamicOffsets.size(),
                                       dynamicOffsets.data());
  }

  template <typename T>
  void pushConstant(PipelineLayout const &layout,
                    VkShaderStageFlagBits shaderStage, uint32_t offset,
                    T const &constant) noexcept {
    m_symbols->vkCmdPushConstants(m_buffer, layout, shaderStage, offset,
                                  sizeof(constant), &constant);
  }

  template <typename T>
  void pushConstants(PipelineLayout const &layout,
                     VkShaderStageFlagBits shaderStage, uint32_t offset,
                     std::span<const T> constantSpan) noexcept {
    m_symbols->vkCmdPushConstants(m_buffer, layout, shaderStage, offset,
                                  sizeof(T) * constantSpan.size(),
                                  constantSpan.data());
  }
};

// FIXME: multiple subpasses are unsupported.
class RenderPassRecorder final : public DescriptorRecorder {
public:
  // Secondary command buffers are fully enclosed by render pass, so this
  // structure starts and ends recording of it.
  RenderPassRecorder(SecondaryCommandBuffer &buffer,
                     const FrameBuffer &frameBuffer,
                     VkCommandBufferUsageFlags flags = 0)
      : DescriptorRecorder(buffer),
        m_ender(buffer, PassEnder(m_symbols, true)) {
    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;
    info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | flags;
    VkCommandBufferInheritanceInfo inhInfo{};
    inhInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inhInfo.pNext = nullptr;
    inhInfo.subpass = 0;
    inhInfo.renderPass = frameBuffer.pass();
    inhInfo.framebuffer = frameBuffer;

    info.pInheritanceInfo = &inhInfo;
    VK_CHECK_RESULT(m_symbols->vkBeginCommandBuffer(m_buffer, &info));
  }

  void bindPipeline(GraphicsPipeline const &pipeline) noexcept {
    m_symbols->vkCmdBindPipeline(m_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 pipeline);
  }
  /** Vertex/index buffer binding **/

  template <typename T>
  void bindVertexBuffer(VertexBuffer<T> const &vbuf, uint32_t binding,
                        VkDeviceSize offset) noexcept {
    VkBuffer buffer = vbuf;
    m_symbols->vkCmdBindVertexBuffers(m_buffer, binding, 1, &buffer, &offset);
  }

  template <VkIndexType type>
  void bindIndexBuffer(IndexBuffer<type> const &ibuf,
                       VkDeviceSize offset) noexcept {
    VkBuffer buffer = ibuf;
    m_symbols->vkCmdBindIndexBuffer(m_buffer, buffer, offset, type);
  }

  /** Draw commands */

  void draw(uint32_t vertexCount, uint32_t instanceCount = 0,
            uint32_t firstVertex = 0, uint32_t firstInstance = 0) noexcept {
    m_symbols->vkCmdDraw(m_buffer, vertexCount, instanceCount, firstVertex,
                         firstInstance);
  }

  void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 0,
                   uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                   uint32_t firstInstance = 0) noexcept {
    m_symbols->vkCmdDrawIndexed(m_buffer, indexCount, instanceCount, firstIndex,
                                vertexOffset, firstInstance);
  }

  /** Pipeline dynamic state sets */

  void setScissors(std::span<const VkRect2D> scissors,
                   uint32_t firstScissor = 0) noexcept {
    m_symbols->vkCmdSetScissor(m_buffer, firstScissor, scissors.size(),
                               scissors.data());
  }
  void setViewports(std::span<const VkViewport> viewports,
                    uint32_t firstViewport = 0) noexcept {
    m_symbols->vkCmdSetViewport(m_buffer, firstViewport, viewports.size(),
                                viewports.data());
  }

private:
  friend class BufferRecorder;
  RenderPassRecorder(PrimaryCommandBuffer &buffer,
                     const FrameBuffer &frameBuffer, VkRect2D renderArea,
                     bool useSecondary = false,
                     std::span<const VkClearValue> clearValues = {})
      : DescriptorRecorder(buffer),
        m_ender(buffer, PassEnder(m_symbols, false)) {
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.renderPass = frameBuffer.pass();
    beginInfo.framebuffer = frameBuffer;
    beginInfo.renderArea = renderArea;
    beginInfo.clearValueCount = clearValues.size();
    beginInfo.pClearValues = clearValues.data();

    m_symbols->vkCmdBeginRenderPass(
        m_buffer, &beginInfo,
        useSecondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS
                     : VK_SUBPASS_CONTENTS_INLINE);
  };

  struct PassEnder {
    void operator()(VkCommandBuffer buffer) {
      if (!buffer)
        return;
      if (!isSecondary)
        symbols->vkCmdEndRenderPass(buffer);
      else
        VK_CHECK_RESULT(symbols->vkEndCommandBuffer(buffer));
    }
    DeviceCore<1, 0> const *symbols;
    bool isSecondary;
  };
  std::unique_ptr<VkCommandBuffer_T, PassEnder> m_ender;
};

class ComputePassRecorder final : public DescriptorRecorder {
public:
  void bindPipeline(ComputePipeline const &pipeline) noexcept {
    m_symbols->vkCmdBindPipeline(m_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                 pipeline);
  }
  /** Dispatch commands */

  void dispatch(uint32_t groupCountX, uint32_t groupCountY,
                uint32_t groupCountZ) noexcept {
    m_symbols->vkCmdDispatch(m_buffer, groupCountX, groupCountY, groupCountZ);
  }

private:
  friend class BufferRecorder;
  ComputePassRecorder(CommandBuffer &buffer) : DescriptorRecorder(buffer) {}
};

class TransferPassRecorder final : public BasicRecorder {
public:
  /** Transfer */

  void copyBufferToBuffer(BufferBase const &src, BufferBase const &dst,
                          std::span<const VkBufferCopy> regions) noexcept {
    m_symbols->vkCmdCopyBuffer(m_buffer, src, dst, regions.size(),
                               regions.data());
  }

  void copyBufferToImage(BufferBase const &src, AllocatedImage const &dst,
                         VkImageLayout layout,
                         std::span<const VkBufferImageCopy> regions) noexcept {
    m_symbols->vkCmdCopyBufferToImage(m_buffer, src, dst, layout,
                                      regions.size(), regions.data());
  }
  void copyImageToBuffer(AllocatedImage const &src, VkImageLayout layout,
                         BufferBase const &dst,
                         std::span<VkBufferImageCopy> regions) noexcept {
    m_symbols->vkCmdCopyImageToBuffer(m_buffer, src, layout, dst,
                                      regions.size(), regions.data());
  }

  void copyImageToImage(AllocatedImage const &src, VkImageLayout srcLayout,
                        AllocatedImage const &dst, VkImageLayout dstLayout,
                        std::span<const VkImageCopy> regions) noexcept {
    m_symbols->vkCmdCopyImage(m_buffer, src, srcLayout, dst, dstLayout,
                              regions.size(), regions.data());
  }

  void blitImage(AllocatedImage const &targetImage, VkImageBlit blit,
                 bool usingGeneralLayout = false,
                 VkFilter filter = VK_FILTER_LINEAR) noexcept {
    auto srcLayout = usingGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL
                                        : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    auto dstLayout = usingGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL
                                        : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    m_symbols->vkCmdBlitImage(m_buffer, targetImage, srcLayout, targetImage,
                              dstLayout, 1, &blit, filter);
  }

  void blitImage(AllocatedImage const &srcImage, AllocatedImage const &dstImage,
                 std::span<const VkImageBlit> blits,
                 VkFilter filter = VK_FILTER_LINEAR) noexcept {
    m_symbols->vkCmdBlitImage(m_buffer, srcImage,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              blits.size(), blits.data(), filter);
  }

private:
  friend class BufferRecorder;
  TransferPassRecorder(CommandBuffer &buffer) : BasicRecorder(buffer) {}
};

// FIXME: buffer recorder is only supported for primary command buffer now.
class BufferRecorder final {
public:
  BufferRecorder(PrimaryCommandBuffer &buffer, VkCommandBufferUsageFlags flags)
      : m_buffer(&buffer, RecordEnder{&buffer.parent().parent().core<1, 0>()}) {
    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    info.pInheritanceInfo = nullptr;
    VK_CHECK_RESULT(buffer.parent().parent().core<1, 0>().vkBeginCommandBuffer(
        buffer, &info));
  }
  RenderPassRecorder
  beginRenderPass(const FrameBuffer &frameBuffer, VkRect2D renderArea,
                  bool useSecondary = false,
                  std::span<const VkClearValue> clearValues = {}) {
    return RenderPassRecorder(*m_buffer, frameBuffer, renderArea, useSecondary,
                              clearValues);
  }

  ComputePassRecorder beginComputePass() {
    return ComputePassRecorder(*m_buffer);
  }

  TransferPassRecorder beginTransferPass() {
    return TransferPassRecorder(*m_buffer);
  }

private:
  struct RecordEnder {
    void operator()(PrimaryCommandBuffer *buffer) {
      if (buffer)
        symbols->vkEndCommandBuffer(*buffer);
    }
    DeviceCore<1, 0> const *symbols;
  };
  std::unique_ptr<PrimaryCommandBuffer, RecordEnder> m_buffer;
};

} // namespace vkw
