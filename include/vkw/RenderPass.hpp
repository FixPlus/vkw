#ifndef VKRENDERER_RENDERPASS_HPP
#define VKRENDERER_RENDERPASS_HPP

#include <vkw/Containers.hpp>
#include <vkw/Device.hpp>
#include <vkw/RangeConcepts.hpp>

#include <algorithm>
#include <optional>
#include <unordered_set>
#include <vector>

namespace vkw {

struct AttachmentID {
  explicit AttachmentID(unsigned id) : id(id) {}

  bool operator==(const AttachmentID &) const = default;
  operator unsigned() const { return id; }
  unsigned id;
};

class AttachmentDescription final : public VkAttachmentDescription {
public:
  AttachmentDescription(VkFormat viewFormat, VkSampleCountFlagBits samples,
                        VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                        VkAttachmentLoadOp stencilLoadOp,
                        VkAttachmentStoreOp stencilStoreOp,
                        VkImageLayout initialLayout, VkImageLayout finalLayout,
                        VkAttachmentDescriptionFlags flags = 0) noexcept
      : VkAttachmentDescription({flags, viewFormat, samples, loadOp, storeOp,
                                 stencilLoadOp, stencilStoreOp, initialLayout,
                                 finalLayout}) {}

  VkFormat format() const noexcept { return VkAttachmentDescription::format; }

  bool isDepthStencil() const noexcept {
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
  bool formatHasDepthAspect() const noexcept {
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
  bool formatHasStencilAspect() const noexcept {
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
  bool isColor() const noexcept { return !isDepthStencil(); }

  VkSampleCountFlagBits samples() const {
    return VkAttachmentDescription::samples;
  }
  VkAttachmentLoadOp loadOp() const noexcept {
    return VkAttachmentDescription::loadOp;
  }

  VkAttachmentLoadOp stencilLoadOp() const noexcept {
    return VkAttachmentDescription::stencilLoadOp;
  }

  VkAttachmentStoreOp storeOp() const noexcept {
    return VkAttachmentDescription::storeOp;
  }

  VkAttachmentStoreOp stencilStoreOp() const noexcept {
    return VkAttachmentDescription::stencilStoreOp;
  }
};

static_assert(sizeof(AttachmentDescription) == sizeof(VkAttachmentDescription));

class SubpassDescription final {
public:
  SubpassDescription &
  addInputAttachment(AttachmentID id,
                     VkImageLayout layout) noexcept(ExceptionsDisabled) {
    m_inputAttachments.emplace_back(id, layout);
    return *this;
  }
  SubpassDescription &
  addColorAttachment(AttachmentID id,
                     VkImageLayout layout) noexcept(ExceptionsDisabled) {
    m_colorAttachments.emplace_back(id, layout);
    return *this;
  }
  SubpassDescription &
  addDepthAttachment(AttachmentID id,
                     VkImageLayout layout) noexcept(ExceptionsDisabled) {
    m_depthAttachment.emplace(id, layout);
    return *this;
  }
  SubpassDescription &
  addResolveAttachment(AttachmentID id,
                       VkImageLayout layout) noexcept(ExceptionsDisabled) {
    m_resolveAttachments.emplace_back(id, layout);
    return *this;
  }
  SubpassDescription &
  addPreserveAttachment(AttachmentID id) noexcept(ExceptionsDisabled) {
    m_preserveAttachments.emplace_back(id);
    return *this;
  }

  using SubpassAttachmentContainerT = cntr::vector<VkAttachmentReference, 2>;
  using PreservedAttachmentContainerT = cntr::vector<uint32_t, 2>;
  std::span<const VkAttachmentReference> inputAttachments() const noexcept {
    return m_inputAttachments;
  }

  std::span<const VkAttachmentReference> colorAttachments() const noexcept {
    return m_colorAttachments;
  }

  std::span<const VkAttachmentReference> resolveAttachments() const noexcept {
    return m_resolveAttachments;
  }

  std::optional<VkAttachmentReference> depthAttachment() const noexcept {
    return m_depthAttachment;
  }
  std::span<const uint32_t> preserveAttachments() const noexcept {
    return m_preserveAttachments;
  }

  VkSubpassDescriptionFlags flags = 0;
  VkPipelineBindPoint bind = VK_PIPELINE_BIND_POINT_GRAPHICS;

private:
  SubpassAttachmentContainerT m_inputAttachments;
  SubpassAttachmentContainerT m_colorAttachments;
  SubpassAttachmentContainerT m_resolveAttachments;
  std::optional<VkAttachmentReference> m_depthAttachment;
  PreservedAttachmentContainerT m_preserveAttachments;
};

class RenderPassCreateInfoBuilder {
public:
  RenderPassCreateInfoBuilder(unsigned maxSubpasses) noexcept
      : m_maxSubpasses(maxSubpasses) {
    m_subpasses.reserve(maxSubpasses);
  }

  AttachmentID addAttachment(
      VkFormat viewFormat, VkSampleCountFlagBits samples,
      VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
      VkAttachmentLoadOp stencilLoadOp, VkAttachmentStoreOp stencilStoreOp,
      VkImageLayout initialLayout, VkImageLayout finalLayout,
      VkAttachmentDescriptionFlags flags = 0) noexcept(ExceptionsDisabled) {
    auto id = AttachmentID(m_attachments.size());
    m_attachments.emplace_back(viewFormat, samples, loadOp, storeOp,
                               stencilLoadOp, stencilStoreOp, initialLayout,
                               finalLayout, flags);
    return id;
  }

  SubpassDescription &addSubpass() noexcept(ExceptionsDisabled) {
    /// TODO: may be postError() ?
    assert(m_subpasses.size() < m_maxSubpasses);
    return m_subpasses.emplace_back();
  }

  void addDependency(
      SubpassDescription *a, SubpassDescription *b,
      VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
      VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
      VkDependencyFlags dependencyFlags) noexcept(ExceptionsDisabled) {
    auto &res = m_deps.emplace_back();
    /// TODO: add assertions/errors if a or b do not come from m_subpasses and
    /// are not null.
    unsigned src = a ? a - m_subpasses.data() : VK_SUBPASS_EXTERNAL;
    unsigned dst = b ? b - m_subpasses.data() : VK_SUBPASS_EXTERNAL;
    res.srcSubpass = src;
    res.dstSubpass = dst;
    res.srcStageMask = srcStageMask;
    res.dstStageMask = dstStageMask;
    res.srcAccessMask = srcAccessMask;
    res.dstAccessMask = dstAccessMask;
    res.dependencyFlags = dependencyFlags;
  }

  VkRenderPassCreateFlags flags = 0;

private:
  friend class RenderPassCreateInfo;
  unsigned m_maxSubpasses;
  cntr::vector<AttachmentDescription, 2> m_attachments;
  cntr::vector<SubpassDescription, 2> m_subpasses;
  cntr::vector<VkSubpassDependency, 2> m_deps;
};

class BadRenderPassCreateInfo : public Error {
public:
  BadRenderPassCreateInfo(std::string_view msg) : Error(msg){};

  std::string_view codeString() const noexcept override {
    return "Bad render pass create info";
  }
};

class RenderPassCreateInfo final {
public:
  RenderPassCreateInfo(RenderPassCreateInfoBuilder &&builder)
      : m_attachments(std::move(builder.m_attachments)),
        m_subpasses(std::move(builder.m_subpasses)),
        m_deps(std::move(builder.m_deps)), m_flags(builder.flags) {
    /// TODO: make this CHECKS optional.
    if (m_attachments.empty())
      postError(BadRenderPassCreateInfo("no attachments given"));
    if (m_subpasses.empty())
      postError(BadRenderPassCreateInfo("no subpasses given"));
    auto unboundAttachmentMessage = [&](auto subpassIndex, auto attIndex,
                                        auto &&name) {
      postError(BadRenderPassCreateInfo([&]() {
        std::stringstream ss;
        ss << "subpass #" << subpassIndex << " referenced unbound " << name
           << " attachment #" << attIndex;
        ss << " - only have " << m_attachments.size() << " attachments bound";
        return ss.str();
      }()));
    };
    auto badFormatAttachment = [](auto subpassIndex, auto attIndex, auto &&name,
                                  VkFormat format) {
      postError(BadRenderPassCreateInfo([&]() {
        std::stringstream ss;
        ss << "subpass #" << subpassIndex << " referenced " << name
           << " attachment #" << attIndex;
        ss << " whose pixel format is incompatible (VkFormat = " << format
           << ")";
        return ss.str();
      }()));
    };
    auto duplicateAttachmentIndex = [](auto subpassIndex, auto attIndex,
                                       auto &&name) {
      postError(BadRenderPassCreateInfo([&]() {
        std::stringstream ss;
        ss << "subpass #" << subpassIndex << " references " << name
           << " attachment #" << attIndex;
        ss << " twice";
        return ss.str();
      }()));
    };
    for (auto &&subpass : m_subpasses) {
      auto index = &subpass - m_subpasses.data();
      std::unordered_set<uint32_t> seenIds;
      for (auto &&iA : subpass.inputAttachments()) {
        if (iA.attachment >= m_attachments.size())
          unboundAttachmentMessage(index, iA.attachment, "input");
        if (!seenIds.emplace(iA.attachment).second)
          duplicateAttachmentIndex(index, iA.attachment, "input");
      }
      for (auto &&cA : subpass.colorAttachments()) {
        if (cA.attachment >= m_attachments.size())
          unboundAttachmentMessage(index, cA.attachment, "color");
        auto &attachment = m_attachments.at(cA.attachment);
        if (!attachment.isColor())
          badFormatAttachment(index, cA.attachment, "color",
                              attachment.format());
        if (!seenIds.emplace(cA.attachment).second)
          duplicateAttachmentIndex(index, cA.attachment, "color");
      }
      for (auto &&pA : subpass.preserveAttachments()) {
        if (pA >= m_attachments.size())
          unboundAttachmentMessage(index, pA, "preserved");
      }
      if (auto dAOpt = subpass.depthAttachment()) {
        auto &dA = dAOpt.value();
        if (dA.attachment >= m_attachments.size())
          unboundAttachmentMessage(index, dA.attachment, "depth/stencil");
        auto &attachment = m_attachments.at(dA.attachment);
        if (!attachment.isDepthStencil())
          badFormatAttachment(index, dA.attachment, "depth/stencil",
                              attachment.format());
        if (!seenIds.emplace(dA.attachment).second)
          duplicateAttachmentIndex(index, dA.attachment, "depth/stencil");
      }
      if (!subpass.resolveAttachments().empty() &&
          subpass.resolveAttachments().size() !=
              subpass.colorAttachments().size())
        postError(BadRenderPassCreateInfo([&]() {
          std::stringstream ss;
          ss << "subpass #" << index
             << " has different count of color and resolve attachments - "
             << subpass.colorAttachments().size() << " vs "
             << subpass.resolveAttachments().size();
          return ss.str();
        }()));
    }
  }

  auto subpasses() const noexcept {
    return std::ranges::subrange(m_subpasses.begin(), m_subpasses.end());
  }

  auto attachments() const noexcept {
    return std::ranges::subrange(m_attachments.begin(), m_attachments.end());
  }

  [[nodiscard]] VkRenderPassCreateInfo
  get() const &noexcept(ExceptionsDisabled) {
    // Create info structure is filled from scratch each time to fill-in valid
    // pointer locations that may have changed since last get() call.
    VkRenderPassCreateInfo ret{};
    m_subpassesRaw.clear();
    m_subpassesRaw.reserve(m_subpasses.size());
    std::ranges::transform(
        m_subpasses, std::back_inserter(m_subpassesRaw), [](auto &subpass) {
          VkSubpassDescription desc{};
          auto &&iAtt = subpass.inputAttachments();
          auto &&cAtt = subpass.colorAttachments();
          auto &&rAtt = subpass.resolveAttachments();
          auto &&pAtt = subpass.preserveAttachments();
          auto &&dAtt = subpass.depthAttachment();

          desc.inputAttachmentCount = iAtt.size();
          desc.pInputAttachments = iAtt.data();
          desc.colorAttachmentCount = cAtt.size();
          desc.pColorAttachments = cAtt.data();
          desc.preserveAttachmentCount = pAtt.size();
          desc.pPreserveAttachments = pAtt.data();
          desc.pDepthStencilAttachment =
              dAtt.has_value() ? &dAtt.value() : nullptr;
          desc.pResolveAttachments = rAtt.empty() ? nullptr : rAtt.data();
          desc.flags = subpass.flags;
          desc.pipelineBindPoint = subpass.bind;
          return desc;
        });
    ret.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ret.pNext = nullptr;
    ret.attachmentCount = m_attachments.size();
    ret.pAttachments = m_attachments.data();
    ret.subpassCount = m_subpassesRaw.size();
    ret.pSubpasses = m_subpassesRaw.data();
    ret.dependencyCount = m_deps.size();
    ret.pDependencies = m_deps.data();
    ret.flags = m_flags;
    return ret;
  }

private:
  cntr::vector<AttachmentDescription, 2> m_attachments;
  cntr::vector<SubpassDescription, 2> m_subpasses;
  mutable cntr::vector<VkSubpassDescription, 2> m_subpassesRaw;
  cntr::vector<VkSubpassDependency, 2> m_deps;
  VkRenderPassCreateFlags m_flags;
};

class RenderPass : public vk::RenderPass {
public:
  RenderPass(
      Device const &device,
      const RenderPassCreateInfo &createInfo) noexcept(ExceptionsDisabled)
      : vk::RenderPass(device, createInfo.get()),
        m_numColorAttachments(std::ranges::count_if(
            createInfo.attachments(), [](auto &&a) { return a.isColor(); })) {}

  auto numColorAttachments() const { return m_numColorAttachments; }

private:
  // this one ois needed for default pipeline blend state creation.
  unsigned m_numColorAttachments;
};

} // namespace vkw
#endif // VKRENDERER_RENDERPASS_HPP
