#include "vkw/RenderPass.hpp"
#include "Utils.hpp"
#include "vkw/Device.hpp"

#include <unordered_set>

namespace vkw {

bool AttachmentDescription::isDepthStencil() const noexcept {
  switch (format()) {
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_S8_UINT:
    return true;
  default:
    return false;
  }
}
bool AttachmentDescription::formatHasDepthAspect() const noexcept {
  switch (format()) {
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
    return true;
  default:
    return false;
  }
}
bool AttachmentDescription::formatHasStencilAspect() const noexcept {
  switch (format()) {
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_S8_UINT:
    return true;
  default:
    return false;
  }
}
bool AttachmentDescription::isColor() const noexcept {
  return !isDepthStencil();
}

SubpassDescription &SubpassDescription::addInputAttachment(
    AttachmentDescription const &attachment,
    VkImageLayout layout) noexcept(ExceptionsDisabled) {
  m_inputAttachments.emplace_back(attachment.id, layout);
  return *this;
}
SubpassDescription &SubpassDescription::addColorAttachment(
    AttachmentDescription const &attachment,
    VkImageLayout layout) noexcept(ExceptionsDisabled) {
  m_colorAttachments.emplace_back(attachment.id, layout);
  return *this;
}

SubpassDescription &SubpassDescription::addResolveAttachment(
    AttachmentDescription const &attachment,
    VkImageLayout layout) noexcept(ExceptionsDisabled) {
  m_resolveAttachments.emplace_back(attachment.id, layout);
  return *this;
}

SubpassDescription &SubpassDescription::addDepthAttachment(
    AttachmentDescription const &attachment,
    VkImageLayout layout) noexcept(ExceptionsDisabled) {
  m_depthAttachment = std::make_pair(attachment.id, layout);
  return *this;
}
SubpassDescription &SubpassDescription::addPreserveAttachment(
    AttachmentDescription const &attachment) noexcept(ExceptionsDisabled) {
  m_preserveAttachments.emplace_back(attachment.id);
  return *this;
}

RenderPassCreateInfo::RenderPassCreateInfo(
    RenderPassCreateInfo &&another) noexcept
    : m_createInfo(another.m_createInfo),
      m_attachments(std::move(another.m_attachments)),
      m_attachments_raw(std::move(another.m_attachments_raw)),
      m_subpasses(another.m_subpasses),
      m_subpassesDescs(another.m_subpassesDescs),
      m_dependencies(another.m_dependencies) {
  m_createInfo.pSubpasses = m_subpasses.data();
  m_createInfo.pAttachments = m_attachments_raw.data();
  m_createInfo.pDependencies = m_dependencies.data();
  auto descIter = m_subpassesDescs.begin();
  for (auto &subpass : m_subpasses) {
    subpass.pInputAttachments = descIter->inputAttachments.data();
    subpass.pColorAttachments = descIter->colorAttachments.data();
    subpass.pDepthStencilAttachment = descIter->depthAttachment.has_value()
                                          ? &descIter->depthAttachment.value()
                                          : nullptr;
    subpass.pPreserveAttachments = descIter->preserveAttachments.data();
  }
}
void RenderPassCreateInfo::m_init(
    const std::vector<StrongReference<SubpassDescription const>> &subpasses,
    const std::vector<SubpassDependency> &dependencies,
    VkRenderPassCreateFlags flags) noexcept(ExceptionsDisabled) {

  auto duplicateAttachment = [&]() -> std::optional<unsigned> {
    std::unordered_set<unsigned> seen;
    for (auto &&attachment : m_attachments) {
      if (seen.contains(attachment.id))
        return attachment.id;
      seen.emplace(attachment.id);
    }
    return std::nullopt;
  }();

  if (duplicateAttachment)
    postError(Error([&]() {
      std::stringstream ss;
      ss << "Duplicate attachment id: " << *duplicateAttachment;
      return ss.str();
    }()));

  std::ranges::sort(m_attachments,
                    [](auto &&a1, auto &&a2) { return a1.id < a2.id; });
  std::transform(m_attachments.begin(), m_attachments.end(),
                 std::back_inserter(m_attachments_raw),
                 [](auto &attachment) { return attachment; });

  for (auto &subpass : subpasses) {
    M_subpassDesc subpassDesc{};
    VkSubpassDescription desc{};
    desc.flags = subpass.get().flags;
    desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    for (auto &inputAttachment : subpass.get().inputAttachments()) {
      auto found = std::find_if(m_attachments.begin(), m_attachments.end(),
                                [&inputAttachment](auto &attachment) {
                                  return attachment.id == inputAttachment.first;
                                });
      if (found == m_attachments.end()) {
        postError(Error("Subpass referenced unknown input attachment"));
      }
      subpassDesc.inputAttachments.emplace_back(VkAttachmentReference{
          static_cast<uint32_t>(found - m_attachments.begin()),
          inputAttachment.second});
    }

    for (auto &colorAttachment : subpass.get().colorAttachments()) {
      auto found = std::find_if(m_attachments.begin(), m_attachments.end(),
                                [&colorAttachment](auto &attachment) {
                                  return attachment.id == colorAttachment.first;
                                });
      if (found == m_attachments.end()) {
        postError(Error("Subpass referenced unknown color attachment"));
      }
      if (!found->isColor()) {
        postError(Error("Subpass referenced color attachment with bad format"));
      }
      subpassDesc.colorAttachments.emplace_back(VkAttachmentReference{
          static_cast<uint32_t>(found - m_attachments.begin()),
          colorAttachment.second});
    }

    if (!subpass.get().resolveAttachments().empty()) {
      if (subpass.get().resolveAttachments().size() !=
          subpass.get().colorAttachments().size()) {
        std::stringstream ss{};
        ss << "Subpass has different count of color and resolve attachments: "
           << subpass.get().colorAttachments().size()
           << " != " << subpass.get().resolveAttachments().size();
        postError(Error(ss.view()));
      }
    }

    for (auto &resolveAttachment : subpass.get().resolveAttachments()) {
      auto found =
          std::find_if(m_attachments.begin(), m_attachments.end(),
                       [&resolveAttachment](auto &attachment) {
                         return attachment.id == resolveAttachment.first;
                       });
      if (found == m_attachments.end()) {
        postError(Error("Subpass referenced unknown resolve attachment"));
      }
      if (!found->isColor()) {
        postError(Error("Subpass referenced color attachment with bad format"));
      }
      subpassDesc.resolveAttachments.emplace_back(VkAttachmentReference{
          static_cast<uint32_t>(found - m_attachments.begin()),
          resolveAttachment.second});
    }

    for (auto &preserveAttachment : subpass.get().preserveAttachments()) {
      auto found = std::find_if(m_attachments.begin(), m_attachments.end(),
                                [&preserveAttachment](auto &attachment) {
                                  return attachment.id == preserveAttachment;
                                });
      if (found == m_attachments.end()) {
        postError(Error("Subpass referenced unknown attachment to preserve"));
      }
      subpassDesc.preserveAttachments.emplace_back(
          static_cast<uint32_t>(found - m_attachments.begin()));
    }
    if (subpass.get().depthAttachments().has_value()) {
      auto &depthAttachment = subpass.get().depthAttachments().value();

      auto found = std::find_if(m_attachments.begin(), m_attachments.end(),
                                [&depthAttachment](auto &attachment) {
                                  return attachment.id == depthAttachment.first;
                                });
      if (found == m_attachments.end()) {
        postError(Error("Subpass referenced unknown depth/stencil attachment"));
      }
      if (!found->isDepthStencil()) {
        postError(Error(
            "Subpass referenced depth/stencil attachment with bad format"));
      }
      subpassDesc.depthAttachment.emplace(VkAttachmentReference{
          static_cast<uint32_t>(found - m_attachments.begin()),
          depthAttachment.second});
    }
    desc.inputAttachmentCount = subpassDesc.inputAttachments.size();
    desc.pInputAttachments = subpassDesc.inputAttachments.data();
    desc.colorAttachmentCount = subpassDesc.colorAttachments.size();
    desc.pColorAttachments = subpassDesc.colorAttachments.data();
    desc.preserveAttachmentCount = subpassDesc.preserveAttachments.size();
    desc.pPreserveAttachments = subpassDesc.preserveAttachments.data();
    desc.pDepthStencilAttachment = subpassDesc.depthAttachment.has_value()
                                       ? &subpassDesc.depthAttachment.value()
                                       : nullptr;
    desc.pResolveAttachments = subpassDesc.resolveAttachments.empty()
                                   ? nullptr
                                   : subpassDesc.resolveAttachments.data();

    m_subpassesDescs.emplace_back(std::move(subpassDesc));
    m_subpasses.emplace_back(desc);
  }

  for (auto &dependency : dependencies) {
    VkSubpassDependency dep{};

    if (dependency.srcSubpass().has_value()) {
      auto srcSubpassRef = dependency.srcSubpass().value();
      auto found_src = std::find_if(
          subpasses.begin(), subpasses.end(), [&srcSubpassRef](auto &subpass) {
            return &subpass.get() == &srcSubpassRef.get();
          });
      if (found_src == subpasses.end()) {
        postError(Error("Subpass Dependency referenced bad subpass"));
      } else {
        dep.srcSubpass = found_src - subpasses.begin();
      }
    } else {
      dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    }

    if (dependency.dstSubpass().has_value()) {
      auto dstSubpassRef = dependency.dstSubpass().value();
      auto found_dst = std::find_if(
          subpasses.begin(), subpasses.end(), [&dstSubpassRef](auto &subpass) {
            return &subpass.get() == &dstSubpassRef.get();
          });
      if (found_dst == subpasses.end()) {
        postError(Error("Subpass Dependency referenced bad subpass"));
      } else {
        dep.dstSubpass = found_dst - subpasses.begin();
      }
    } else {
      dep.dstSubpass = VK_SUBPASS_EXTERNAL;
    }

    if (dep.srcSubpass == VK_SUBPASS_EXTERNAL &&
        dep.dstSubpass == VK_SUBPASS_EXTERNAL)
      postError(Error(
          "Dependency can't have both SRC and DST subpasses be EXTERNAL"));

    dep.srcAccessMask = dependency.srcAccessMask;
    dep.dstAccessMask = dependency.dstAccessMask;
    dep.srcStageMask = dependency.srcStageMask;
    dep.dstStageMask = dependency.dstStageMask;
    dep.dependencyFlags = dependency.dependencyFlags;
    m_dependencies.push_back(dep);
  }

  m_createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  m_createInfo.pNext = nullptr;
  m_createInfo.attachmentCount = m_attachments_raw.size();
  m_createInfo.pAttachments = m_attachments_raw.data();
  m_createInfo.subpassCount = m_subpasses.size();
  m_createInfo.pSubpasses = m_subpasses.data();
  m_createInfo.dependencyCount = m_dependencies.size();
  m_createInfo.pDependencies = m_dependencies.data();
  m_createInfo.flags = flags;
}
} // namespace vkw