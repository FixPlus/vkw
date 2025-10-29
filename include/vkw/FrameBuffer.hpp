#ifndef VKRENDERER_FRAMEBUFFER_HPP
#define VKRENDERER_FRAMEBUFFER_HPP

#include <vkw/Image.hpp>
#include <vkw/RenderPass.hpp>

#include <span>

namespace vkw {

class ImageViewBase;

class FrameBufferInfo {
public:
  FrameBufferInfo(RenderPass const &renderPass, VkExtent3D extents) noexcept
      : m_parent(renderPass), m_extents(extents) {}
  FrameBufferInfo(RenderPass const &renderPass, unsigned width, unsigned height,
                  unsigned layers = 1) noexcept
      : m_parent(renderPass), m_extents(width, height, layers) {}

  void addAttachment(auto &&imageView) noexcept(ExceptionsDisabled) {
    m_views.emplace_back(imageView);
    m_rawViews.emplace_back(imageView.operator VkImageView());
  }

  VkFramebufferCreateFlags flags = 0;

private:
  friend class FrameBuffer;
  StrongReference<RenderPass const> m_parent;
  cntr::vector<VkImageView, 2> m_rawViews;
  cntr::vector<StrongReference<ImageViewBase const>, 2> m_views;
  VkExtent3D m_extents;
};

class FrameBuffer : public vk::Framebuffer {
public:
  FrameBuffer(const FrameBufferInfo &info) noexcept(ExceptionsDisabled)
      : vk::Framebuffer(info.m_parent.get().parent(),
                        [&]() {
                          VkFramebufferCreateInfo CI{};
                          CI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                          CI.pNext = nullptr;
                          CI.width = info.m_extents.width;
                          CI.height = info.m_extents.height;
                          CI.layers = info.m_extents.depth;
                          CI.renderPass = info.m_parent.get();
                          CI.attachmentCount = info.m_rawViews.size();
                          CI.pAttachments = info.m_rawViews.data();
                          CI.flags = info.flags;
                          return CI;
                        }()),
        m_parent(info.m_parent), m_extents(info.m_extents) {
    m_views.reserve(info.m_views.size());
    std::ranges::copy(info.m_views, std::back_inserter(m_views));
  }

  VkExtent2D extents() const noexcept {
    return {m_extents.width, m_extents.height};
  }

  uint32_t layers() const noexcept { return m_extents.depth; }

  VkRect2D getFullRenderArea() const noexcept { return {{0, 0}, extents()}; }

  auto attachments() const noexcept {
    return m_views | std::views::transform([](auto &&ref) -> decltype(auto) {
             return ref.get();
           });
  }

  auto &pass() const noexcept { return m_parent.get(); }

private:
  StrongReference<RenderPass const> m_parent;
  cntr::vector<StrongReference<ImageViewBase const>, 2> m_views;
  VkExtent3D m_extents;
};

} // namespace vkw
#endif // VKRENDERER_FRAMEBUFFER_HPP
