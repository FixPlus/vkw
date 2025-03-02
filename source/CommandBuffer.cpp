#include "vkw/CommandBuffer.hpp"
#include "Utils.hpp"
#include "vkw/Buffer.hpp"
#include "vkw/CommandPool.hpp"
#include "vkw/DescriptorSet.hpp"
#include "vkw/Device.hpp"
#include "vkw/FrameBuffer.hpp"
#include "vkw/Image.hpp"
#include "vkw/Pipeline.hpp"
#include "vkw/Query.hpp"
#include "vkw/RenderPass.hpp"

namespace vkw {

CommandBuffer::CommandBuffer(
    CommandPool &pool,
    VkCommandBufferLevel bufferLevel) noexcept(ExceptionsDisabled)
    : m_pool(pool), m_device(pool.parent()) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.pNext = nullptr;
  allocInfo.commandBufferCount = 1;
  allocInfo.commandPool = pool;

  VK_CHECK_RESULT(m_device.get().core<1, 0>().vkAllocateCommandBuffers(
      m_device.get(), &allocInfo, &m_commandBuffer))
}

uint32_t CommandBuffer::queueFamily() const noexcept {
  return m_pool.get().queueFamilyIndex();
}
void CommandBuffer::reset(VkCommandBufferResetFlags flags) noexcept(
    ExceptionsDisabled) {
  VK_CHECK_RESULT(
      m_device.get().core<1, 0>().vkResetCommandBuffer(m_commandBuffer, flags))
}
void CommandBuffer::m_begin(VkCommandBufferUsageFlags flags,
                            const VkCommandBufferInheritanceInfo
                                *inheritanceInfo) noexcept(ExceptionsDisabled) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = flags;
  beginInfo.pInheritanceInfo = inheritanceInfo;
  beginInfo.pNext = nullptr;

  VK_CHECK_RESULT(m_device.get().core<1, 0>().vkBeginCommandBuffer(
      m_commandBuffer, &beginInfo))

  m_recording = true;
}

void CommandBuffer::end() noexcept(ExceptionsDisabled) {
  VK_CHECK_RESULT(
      m_device.get().core<1, 0>().vkEndCommandBuffer(m_commandBuffer))
  m_recording = false;
  m_executable = true;
}

void CommandBuffer::copyBufferToBuffer(
    const BufferBase &src, const BufferBase &dst,
    std::span<const VkBufferCopy> regions) noexcept {
  m_device.get().core<1, 0>().vkCmdCopyBuffer(m_commandBuffer, src, dst,
                                              regions.size(), regions.data());
}

void CommandBuffer::copyBufferToImage(
    const BufferBase &src, const AllocatedImage &dst, VkImageLayout layout,
    std::span<const VkBufferImageCopy> regions) noexcept {
  m_device.get().core<1, 0>().vkCmdCopyBufferToImage(
      m_commandBuffer, src, dst, layout, regions.size(), regions.data());
}

void CommandBuffer::copyImageToImage(
    AllocatedImage const &src, VkImageLayout srcLayout,
    AllocatedImage const &dst, VkImageLayout dstLayout,
    std::span<const VkImageCopy> regions) noexcept {
  m_device.get().core<1, 0>().vkCmdCopyImage(m_commandBuffer, src, srcLayout,
                                             dst, dstLayout, regions.size(),
                                             regions.data());
}

void CommandBuffer::pipelineBarrier(
    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
    std::span<const VkMemoryBarrier> memBarriers,
    std::span<const VkImageMemoryBarrier> imageMemoryBarrier,
    std::span<const VkBufferMemoryBarrier> bufferMemoryBarrier,
    VkDependencyFlags flags) noexcept {
  m_device.get().core<1, 0>().vkCmdPipelineBarrier(
      m_commandBuffer, srcStage, dstStage, flags, memBarriers.size(),
      memBarriers.data(), bufferMemoryBarrier.size(),
      bufferMemoryBarrier.data(), imageMemoryBarrier.size(),
      imageMemoryBarrier.data());
}

void CommandBuffer::m_bindVertexBuffer(VkBuffer buffer, uint32_t binding,
                                       VkDeviceSize offset) noexcept {
  m_device.get().core<1, 0>().vkCmdBindVertexBuffers(m_commandBuffer, binding,
                                                     1, &buffer, &offset);
}

void CommandBuffer::m_bindIndexBuffer(VkBuffer buffer, VkIndexType type,
                                      VkDeviceSize offset) noexcept {
  m_device.get().core<1, 0>().vkCmdBindIndexBuffer(m_commandBuffer, buffer,
                                                   offset, type);
}

void CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount,
                         uint32_t firstVertex,
                         uint32_t firstInstance) noexcept {
  m_device.get().core<1, 0>().vkCmdDraw(
      m_commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                uint32_t firstIndex, int32_t vertexOffset,
                                uint32_t firstInstance) noexcept {
  m_device.get().core<1, 0>().vkCmdDrawIndexed(m_commandBuffer, indexCount,
                                               instanceCount, firstIndex,
                                               vertexOffset, firstInstance);
}

void CommandBuffer::dispatch(uint32_t groupCountX, uint32_t groupCountY,
                             uint32_t groupCountZ) noexcept {
  m_device.get().core<1, 0>().vkCmdDispatch(m_commandBuffer, groupCountX,
                                            groupCountY, groupCountZ);
}

void CommandBuffer::bindGraphicsPipeline(
    GraphicsPipeline const &pipeline) noexcept {
  m_device.get().core<1, 0>().vkCmdBindPipeline(
      m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void CommandBuffer::bindComputePipeline(
    ComputePipeline const &pipeline) noexcept {
  m_device.get().core<1, 0>().vkCmdBindPipeline(
      m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}
void CommandBuffer::copyImageToBuffer(
    AllocatedImage const &src, VkImageLayout layout, BufferBase const &dst,
    std::span<VkBufferImageCopy> regions) noexcept {
  m_device.get().core<1, 0>().vkCmdCopyImageToBuffer(
      m_commandBuffer, src, layout, dst, regions.size(), regions.data());
}
void CommandBuffer::setScissors(std::span<const VkRect2D> scissors,
                                uint32_t firstScissor) noexcept {
  m_device.get().core<1, 0>().vkCmdSetScissor(m_commandBuffer, firstScissor,
                                              scissors.size(), scissors.data());
}
void CommandBuffer::setViewports(std::span<const VkViewport> viewports,
                                 uint32_t firstViewport) noexcept {
  m_device.get().core<1, 0>().vkCmdSetViewport(
      m_commandBuffer, firstViewport, viewports.size(), viewports.data());
}

void PrimaryCommandBuffer::beginRenderPass(
    const RenderPass &renderPass, const FrameBuffer &frameBuffer,
    VkRect2D renderArea, bool useSecondary,
    std::span<const VkClearValue> clearValues) noexcept(ExceptionsDisabled) {
#ifdef VKW_COMMAND_BUFFER_TRACK_RENDER_PASSES
  if (m_currentPass.has_value())
    postError(
        Error("CommandBuffer record failed: called beginRenderPass() while "
              "having another RenderPass active"));
#endif
  VkRenderPassBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  beginInfo.pNext = nullptr;
  beginInfo.renderPass = renderPass;
  beginInfo.framebuffer = frameBuffer;
  beginInfo.renderArea = renderArea;
  beginInfo.clearValueCount = clearValues.size();
  beginInfo.pClearValues = clearValues.data();

  m_device.get().core<1, 0>().vkCmdBeginRenderPass(
      m_commandBuffer, &beginInfo,
      useSecondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS
                   : VK_SUBPASS_CONTENTS_INLINE);
#ifdef VKW_COMMAND_BUFFER_TRACK_RENDER_PASSES
  m_currentPass = renderPass;
  m_currentSubpass = 0;
#endif
}

void PrimaryCommandBuffer::nextSubpass(bool useSecondary) noexcept(
    ExceptionsDisabled) {
#ifdef VKW_COMMAND_BUFFER_TRACK_RENDER_PASSES
  if (!m_currentPass.has_value())
    postError(Error("CommandBuffer record failed: called nextSubpass() while "
                    "having no RenderPass active"));
  if (m_currentPass.value().get().info().subpassCount() == m_currentSubpass)
    postError(Error("CommandBuffer record failed: call to nextSubpass() caused "
                    "subpass overflow"));
#endif
  m_device.get().core<1, 0>().vkCmdNextSubpass(
      m_commandBuffer, useSecondary
                           ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS
                           : VK_SUBPASS_CONTENTS_INLINE);
#ifdef VKW_COMMAND_BUFFER_TRACK_RENDER_PASSES
  m_currentSubpass++;
#endif
}

void PrimaryCommandBuffer::endRenderPass() noexcept(ExceptionsDisabled) {
#ifdef VKW_COMMAND_BUFFER_TRACK_RENDER_PASSES
  if (!m_currentPass.has_value())
    postError(Error("CommandBuffer record failed: called endRenderPass() while "
                    "having no RenderPass active"));
#endif
  m_device.get().core<1, 0>().vkCmdEndRenderPass(m_commandBuffer);
#ifdef VKW_COMMAND_BUFFER_TRACK_RENDER_PASSES
  m_currentPass.reset();
#endif
}

void PrimaryCommandBuffer::m_executeCommands(
    size_t nbufs, const VkCommandBuffer *buffers) noexcept {
  m_device.get().core<1, 0>().vkCmdExecuteCommands(m_commandBuffer, nbufs,
                                                   buffers);
}

void CommandBuffer::m_pushConstants(PipelineLayout const &layout,
                                    VkShaderStageFlagBits shaderStage,
                                    uint32_t offset, uint32_t size,
                                    const void *data) noexcept {
  m_device.get().core<1, 0>().vkCmdPushConstants(
      m_commandBuffer, layout, shaderStage, offset, size, data);
}
void CommandBuffer::blitImage(const AllocatedImage &targetImage,
                              VkImageBlit blit, bool usingGeneralLayout,
                              VkFilter filter) noexcept {
  auto srcLayout = usingGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL
                                      : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  auto dstLayout = usingGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL
                                      : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  m_device.get().core<1, 0>().vkCmdBlitImage(m_commandBuffer, targetImage,
                                             srcLayout, targetImage, dstLayout,
                                             1, &blit, filter);
}

void CommandBuffer::blitImage(AllocatedImage const &srcImage,
                              AllocatedImage const &dstImage,
                              std::span<const VkImageBlit> blits,
                              VkFilter filter) noexcept {
  m_device.get().core<1, 0>().vkCmdBlitImage(
      m_commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, blits.size(), blits.data(), filter);
}

void CommandBuffer::m_bindDescriptorSets(const PipelineLayout &layout,
                                         VkPipelineBindPoint bindPoint,
                                         size_t firstSet,
                                         VkDescriptorSet const *sets,
                                         size_t nsets, uint32_t *dynOffsets,
                                         size_t ndynOffsets) noexcept {
  m_device.get().core<1, 0>().vkCmdBindDescriptorSets(
      m_commandBuffer, bindPoint, layout, firstSet, nsets, sets, ndynOffsets,
      dynOffsets);
}

void CommandBuffer::beginQuery(const QueryPool &queryPool, uint32_t query,
                               VkQueryControlFlags flags) noexcept {
  m_device.get().core<1, 0>().vkCmdBeginQuery(m_commandBuffer, queryPool, query,
                                              flags);
}

void CommandBuffer::endQuery(const QueryPool &queryPool,
                             uint32_t query) noexcept {
  m_device.get().core<1, 0>().vkCmdEndQuery(m_commandBuffer, queryPool, query);
}

void CommandBuffer::resetQuery(const QueryPool &queryPool, uint32_t firstQuery,
                               uint32_t count) noexcept {
  m_device.get().core<1, 0>().vkCmdResetQueryPool(m_commandBuffer, queryPool,
                                                  firstQuery, count);
}
void CommandBuffer::resetQuery(const QueryPool &queryPool) noexcept {
  m_device.get().core<1, 0>().vkCmdResetQueryPool(m_commandBuffer, queryPool,
                                                  0u, queryPool.size());
}

} // namespace vkw