#ifndef VKRENDERER_PIPELINE_HPP
#define VKRENDERER_PIPELINE_HPP

#include <vkw/DescriptorSet.hpp>
#include <vkw/PipelineCache.hpp>
#include <vkw/RenderPass.hpp>
#include <vkw/Shader.hpp>
#include <vkw/VertexBuffer.hpp>

#include <algorithm>
#include <functional>
#include <optional>
#include <span>

namespace vkw {

/**
 *
 * @class SpecializationConstants
 *
 * @brief Represents VkSpecializationInfo structure.
 *
 *
 */

class SpecializationConstants final {
public:
  SpecializationConstants() = default;

  bool empty() const noexcept { return m_entries.empty(); }

  template <typename T>
  void addConstant(T const &constant,
                   uint32_t id) noexcept(ExceptionsDisabled) {
    constexpr auto size = sizeof(constant);
    if (std::find_if(m_entries.begin(), m_entries.end(),
                     [id](VkSpecializationMapEntry const &entry) {
                       return entry.constantID == id;
                     }) != m_entries.end())
      postError(LogicError(
          "Tying to assign duplicate specialization constants. id = " +
          std::to_string(id)));

    auto offset = m_data.size();
    m_data.resize(offset + size);
    memcpy(m_data.data() + offset, &constant, size);

    VkSpecializationMapEntry newEntry{};
    newEntry.constantID = id;
    newEntry.size = size;
    newEntry.offset = offset;
    m_entries.push_back(newEntry);
  }

  void clear() {
    m_entries.clear();
    m_data.clear();
  }

  operator VkSpecializationInfo() const noexcept {
    VkSpecializationInfo info{};
    info.mapEntryCount = m_entries.size();
    info.pMapEntries = m_entries.data();
    info.dataSize = m_data.size();
    info.pData = m_data.data();

    return info;
  }

private:
  cntr::vector<VkSpecializationMapEntry, 3> m_entries;
  cntr::vector<unsigned char, 64> m_data;
};

/**
 *
 * @class PipelineLayoutInfo
 *
 * @brief Holds info needed for initialization of VkPipelineLayout
 *
 *
 */

class PipelineLayoutInfo {
public:
  explicit PipelineLayoutInfo(VkPipelineLayoutCreateFlags flags = 0) noexcept(
      ExceptionsDisabled)
      : m_flags(flags) {
    m_initRaw();
  }
  template <forward_range_of<DescriptorSetLayout const> T>
  explicit PipelineLayoutInfo(
      T const &setLayouts,
      std::span<const VkPushConstantRange> pushConstants = {},
      VkPipelineLayoutCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : m_flags(flags) {
    std::copy(pushConstants.begin(), pushConstants.end(),
              std::back_inserter(m_pushConstants));
    std::transform(setLayouts.begin(), setLayouts.end(),
                   std::back_inserter(m_descriptorLayouts),
                   [](std::ranges::range_value_t<T> const &layout) {
                     return StrongReference<DescriptorSetLayout const>(
                         layout.get());
                   });
    m_initRaw();
  }

  // overload for 1 element case
  explicit PipelineLayoutInfo(
      DescriptorSetLayout const &setLayout,
      std::span<const VkPushConstantRange> pushConstants = {},
      VkPipelineLayoutCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : m_flags(flags) {
    std::copy(pushConstants.begin(), pushConstants.end(),
              std::back_inserter(m_pushConstants));
    m_descriptorLayouts.emplace_back(setLayout);
    m_initRaw();
  }

  bool operator==(PipelineLayoutInfo const &rhs) const noexcept {
    if (m_flags != rhs.m_flags ||
        m_descriptorLayouts.size() != rhs.m_descriptorLayouts.size())
      return false;
    auto rhsLayoutIter = rhs.m_descriptorLayouts.begin();
    return std::all_of(m_descriptorLayouts.begin(), m_descriptorLayouts.end(),
                       [&rhsLayoutIter](DescriptorSetLayout const &layout) {
                         return layout == *(rhsLayoutIter++);
                       });
  }

  bool operator!=(PipelineLayoutInfo const &rhs) const noexcept {
    return !(*this == rhs);
  }

  auto begin() noexcept { return m_descriptorLayouts.begin(); }

  auto end() noexcept { return m_descriptorLayouts.begin(); }

  auto begin() const noexcept { return m_descriptorLayouts.begin(); }

  auto end() const noexcept { return m_descriptorLayouts.begin(); }

  auto info() const noexcept {
    VkPipelineLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = m_flags;
    createInfo.setLayoutCount = m_rawLayout.size();
    createInfo.pSetLayouts = m_rawLayout.data();
    // TODO : verify push constant ranges against the device limits
    createInfo.pushConstantRangeCount = m_pushConstants.size();
    createInfo.pPushConstantRanges = m_pushConstants.data();
    return createInfo;
  }

private:
  void m_initRaw() noexcept(ExceptionsDisabled) {
    std::transform(m_descriptorLayouts.begin(), m_descriptorLayouts.end(),
                   std::back_inserter(m_rawLayout),
                   [](auto &layout) -> VkDescriptorSetLayout {
                     return layout.operator const vkw::DescriptorSetLayout &();
                   });
  }

  cntr::vector<StrongReference<DescriptorSetLayout const>, 4>
      m_descriptorLayouts{};
  cntr::vector<VkDescriptorSetLayout, 4> m_rawLayout{};
  cntr::vector<VkPushConstantRange, 4> m_pushConstants{};
  VkPipelineLayoutCreateFlags m_flags{};
};

/**
 *
 * @class PipelineLayout
 *
 * @brief Represents vulkan's VkPipelineLayout structure.
 *
 *
 */

class PipelineLayout : public PipelineLayoutInfo, public vk::PipelineLayout {
public:
  explicit PipelineLayout(
      Device const &device,
      VkPipelineLayoutCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : PipelineLayoutInfo(flags), vk::PipelineLayout(device, info()) {}
  template <forward_range_of<DescriptorSetLayout const> T>
  PipelineLayout(
      Device const &device, T const &setLayouts,
      std::span<const VkPushConstantRange> pushConstants = {},
      VkPipelineLayoutCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : PipelineLayoutInfo(setLayouts, pushConstants, flags),
        vk::PipelineLayout(device, info()) {}

  // overload for 1 element case
  PipelineLayout(
      Device const &device, DescriptorSetLayout const &setLayout,
      std::span<const VkPushConstantRange> pushConstants = {},
      VkPipelineLayoutCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : PipelineLayoutInfo(setLayout, pushConstants, flags), vk::PipelineLayout(
                                                                 device,
                                                                 info()) {}

  bool operator==(PipelineLayout const &rhs) const noexcept {
    return PipelineLayoutInfo::operator==(rhs);
  }

  bool operator!=(PipelineLayout const &rhs) const noexcept {
    return !(*this == rhs);
  }
};

/*
 *
 * Following structures represent:
 *
 * VERTEX INPUT STATE STAGE DESCRIPTION
 *
 */

/**
 *
 * @class VertexInputStateCreateInfoBase
 *
 * @brief Base class for VertexInputStateCreateInfo structures
 *
 * @note Derived classes must provide storage for data referenced
 * by VkPipelineVertexInputStateCreateInfo.
 *
 *
 * You may want to add your own derived classes that will fit
 * your needs
 *
 */

class VertexInputStateCreateInfoBase : public ReferenceGuard {
public:
  operator VkPipelineVertexInputStateCreateInfo const &() const noexcept {
    return m_createInfo;
  }

  uint32_t totalAttributes() const noexcept {
    return m_createInfo.vertexAttributeDescriptionCount;
  }
  VkVertexInputAttributeDescription attribute(uint32_t index) const
      noexcept(ExceptionsDisabled) {
    if (index >= totalAttributes())
      postError(LogicError(
          "VertexInputStateCreateInfoBase::attribute(" + std::to_string(index) +
          ") exceeded pVertexAttributeDescriptions array bounds (size = " +
          std::to_string(totalAttributes()) + ")"));

    return m_createInfo.pVertexAttributeDescriptions[index];
  }

  uint32_t totalBindings() const noexcept {
    return m_createInfo.vertexBindingDescriptionCount;
  }
  VkVertexInputBindingDescription binding(uint32_t index) const
      noexcept(ExceptionsDisabled) {
    if (index >= totalBindings())
      postError(LogicError(
          "VertexInputStateCreateInfoBase::binding(" + std::to_string(index) +
          ") exceeded pVertexBindingDescriptions array bounds (size = " +
          std::to_string(totalBindings()) + ")"));

    return m_createInfo.pVertexBindingDescriptions[index];
  }

protected:
  VertexInputStateCreateInfoBase(
      uint32_t attributeDescCount,
      VkVertexInputAttributeDescription const *pAttributeDesc,
      uint32_t bindingDescCount,
      VkVertexInputBindingDescription const *pBindingDesc,
      VkPipelineVertexInputStateCreateFlags flags = 0) noexcept {

    m_createInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_createInfo.pNext = nullptr;
    m_createInfo.flags = flags;
    m_createInfo.pVertexAttributeDescriptions = pAttributeDesc;
    m_createInfo.pVertexBindingDescriptions = pBindingDesc;
    m_createInfo.vertexAttributeDescriptionCount = attributeDescCount;
    m_createInfo.vertexBindingDescriptionCount = bindingDescCount;
  }

private:
  VkPipelineVertexInputStateCreateInfo m_createInfo{};
};

/**
 *
 * @class NullVertexInputState
 * @brief Implementation of VertexInputStateCreateInfoBase data holder.
 * Represents null vertex input state with no bindings at all. As such
 * state has only one possible configuration, it is implemented as singleton.
 *
 */

class NullVertexInputState final : public VertexInputStateCreateInfoBase {
public:
  /**
   * @return reference to statically created NullVertexInputState object
   */
  static NullVertexInputState &get() noexcept {
    static NullVertexInputState handle;
    return handle;
  }

private:
  NullVertexInputState() noexcept
      : VertexInputStateCreateInfoBase(0, nullptr, 0, nullptr, 0){};
};

/**
 * @concept BindingPointDescriptionLike
 *
 * Used by @class VertexInputStateCreateInfo to check if parameters
 * representing VkVertexInputBindingDescription are satisfy following
 * constraints:
 *
 *   1. Class must have static constexpr @member 'binding' of type
 *   uint32_t, representing binding number.
 *   2. Class must have static constexpr @member 'value' of type
 *   VkVertexInputBindingDescription, representing binding info.
 *   3. Class must have 'Attributes' name alias for class that
 *   satisfy @concept AttributeArray.
 *
 */

template <typename T>
concept BindingPointDescriptionLike =
    requires(T desc) {
      T::binding;
      std::same_as<decltype(T::binding), uint32_t>;
      T::value;
      std::same_as<decltype(T::value), VkVertexInputBindingDescription>;
      typename T::Attributes;
    } && AttributeArray<typename T::Attributes>;

/**
 *
 * @class per_vertex
 * @brief Implementation of BindingPointDescriptionLike class that represents
 * input binding description with inputRate=VK_VERTEX_INPUT_RATE_VERTEX
 *
 * @tparam T is AttributeArray structure type.
 * @tparam bindingT is uint32_t constant representing binding number.
 */

template <AttributeArray T, uint32_t bindingT = 0> struct per_vertex {
  using Attributes = T;
  constexpr static const uint32_t binding = bindingT;
  constexpr static const VkVertexInputBindingDescription value = {
      .binding = binding,
      .stride = sizeof(T),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
};

/**
 *
 * @class per_instance
 * @brief Implementation of BindingPointDescriptionLike class that represents
 * input binding description with inputRate=VK_VERTEX_INPUT_RATE_INSTANCE.
 *
 * @tparam T is AttributeArray structure type.
 * @tparam bindingT is uint32_t constant representing binding number.
 */

template <AttributeArray T, uint32_t bindingT = 0> struct per_instance {
  using Attributes = T;
  constexpr static const uint32_t binding = bindingT;
  constexpr static const VkVertexInputBindingDescription value = {
      .binding = binding,
      .stride = sizeof(T),
      .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE};
};

/**
 *
 * @class VertexInputStateCreateInfo
 * @brief Implementation of VertexInputStateCreateInfoBase data holder.
 * Used when vertex input state is known in compile time and can be
 * represented by a pack of BindingPointDescriptionLike structure types.
 *
 * @tparam Bindings is a pack of BindingPointDescriptionLike structure types.
 */

template <BindingPointDescriptionLike... Bindings>
class VertexInputStateCreateInfo final : public VertexInputStateCreateInfoBase {
private:
  template <BindingPointDescriptionLike First,
            BindingPointDescriptionLike Second,
            BindingPointDescriptionLike... Rest>
  constexpr static uint32_t m_total_attributes() {
    return First::Attributes::count() + m_total_attributes<Second, Rest...>();
  }

  template <BindingPointDescriptionLike First>
  constexpr static uint32_t m_total_attributes() {
    return First::Attributes::count();
  }

public:
  VertexInputStateCreateInfo(VkPipelineVertexInputStateCreateFlags flags = 0)
      : VertexInputStateCreateInfoBase(m_total_attributes<Bindings...>(),
                                       m_AttributeDescHolder,
                                       sizeof...(Bindings), m_bind_desc) {}

private:
  struct M_AttributeDescHolder {

    M_AttributeDescHolder() { m_init_array<Bindings...>(); }

    operator VkVertexInputAttributeDescription const *() const {
      return m_attr_desc;
    }

  private:
    template <BindingPointDescriptionLike M_binding,
              BindingPointDescriptionLike M_dummy,
              BindingPointDescriptionLike... M_bindings>
    void m_init_array(uint32_t initIndex = 0, uint32_t location = 0) noexcept {
      uint32_t offset = 0u;
      for (uint32_t i = initIndex;
           i < initIndex + M_binding::Attributes::count(); ++i) {
        auto attrType = M_binding::Attributes::getAttrType(i - initIndex);
        m_attr_desc[i].binding = M_binding::binding;
        m_attr_desc[i].location = location;
        m_attr_desc[i].offset = offset;
        m_attr_desc[i].format = format_of(attrType);
        location += locations_hold(attrType);
        offset += size_of(attrType);
      }
      m_init_array<M_dummy, M_bindings...>(
          initIndex + M_binding::Attributes::count(), location);
    }

    template <BindingPointDescriptionLike M_binding>
    void m_init_array(uint32_t initIndex = 0, uint32_t location = 0) noexcept {
      uint32_t offset = 0u;
      for (uint32_t i = initIndex;
           i < initIndex + M_binding::Attributes::count(); ++i) {
        auto attrType = M_binding::Attributes::getAttrType(i - initIndex);
        m_attr_desc[i].binding = M_binding::binding;
        m_attr_desc[i].location = location;
        m_attr_desc[i].offset = offset;
        m_attr_desc[i].format = format_of(attrType);
        location += locations_hold(attrType);
        offset += size_of(attrType);
      }
    }
    VkVertexInputAttributeDescription
        m_attr_desc[m_total_attributes<Bindings...>()]{};
  };

  static const M_AttributeDescHolder m_AttributeDescHolder;

  constexpr static const VkVertexInputBindingDescription
      m_bind_desc[sizeof...(Bindings)] = {Bindings::value...};
};

template <BindingPointDescriptionLike... Bindings>
typename VertexInputStateCreateInfo<Bindings...>::M_AttributeDescHolder const
    VertexInputStateCreateInfo<Bindings...>::m_AttributeDescHolder = {};

/*
 *
 * Following structures represent:
 *
 * INPUT ASSEMBLY STATE DESCRIPTION
 *
 */

class InputAssemblyStateCreateInfo final {
public:
  InputAssemblyStateCreateInfo(
      VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      VkBool32 restartEnable = false) noexcept {
    m_createInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_createInfo.pNext = nullptr;
    m_createInfo.topology = topology;
    m_createInfo.primitiveRestartEnable = restartEnable;
    m_createInfo.flags = 0;
  }

  operator VkPipelineInputAssemblyStateCreateInfo const &() const noexcept {
    return m_createInfo;
  }

  VkPrimitiveTopology topology() const noexcept {
    return m_createInfo.topology;
  }

  bool isPrimitivesRestartEnabled() const noexcept {
    return m_createInfo.primitiveRestartEnable;
  }

private:
  VkPipelineInputAssemblyStateCreateInfo m_createInfo{};
};

/*
 *
 * Following structures represent:
 *
 * RASTERIZATION STATE DESCRIPTION
 *
 */

class RasterizationStateCreateInfo final {
public:
  RasterizationStateCreateInfo(
      VkBool32 depthClampEnable = VK_FALSE,
      VkBool32 rasterizerDiscardEnable = VK_FALSE,
      VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
      VkCullModeFlags cullMode = VK_CULL_MODE_NONE,
      VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      VkBool32 depthBiasEnable = false, float depthBiasConstantFactor = 0,
      float depthBiasClamp = 0, float depthBiasSlopeFactor = 0,
      float lineWidth = 1.0f) noexcept {
    m_createInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_createInfo.pNext = nullptr;
    m_createInfo.flags = 0;
    m_createInfo.depthClampEnable = depthClampEnable;
    m_createInfo.rasterizerDiscardEnable = rasterizerDiscardEnable;
    m_createInfo.polygonMode = polygonMode;
    m_createInfo.cullMode = cullMode;
    m_createInfo.frontFace = frontFace;
    m_createInfo.depthBiasEnable = depthBiasEnable;
    m_createInfo.depthBiasConstantFactor = depthBiasConstantFactor;
    m_createInfo.depthBiasClamp = depthBiasClamp;
    m_createInfo.depthBiasSlopeFactor = depthBiasSlopeFactor;
    m_createInfo.lineWidth = lineWidth;
  }

  operator VkPipelineRasterizationStateCreateInfo const &() const noexcept {
    return m_createInfo;
  }

private:
  VkPipelineRasterizationStateCreateInfo m_createInfo{};
};

/*
 *
 * Following structures represent:
 *
 * DEPTH TEST STATE DESCRIPTION
 *
 */

class DepthTestStateCreateInfo final {
public:
  DepthTestStateCreateInfo(VkCompareOp compareOp, bool writeEnable,
                           float minDepth = 0.0f,
                           float maxDepth = 1.0f) noexcept {
    m_createInfo.depthCompareOp = compareOp;
    m_createInfo.depthWriteEnable = writeEnable;
    m_createInfo.minDepthBounds = minDepth;
    m_createInfo.maxDepthBounds = maxDepth;
  }
  operator VkPipelineDepthStencilStateCreateInfo const &() const noexcept {
    return m_createInfo;
  }

private:
  VkPipelineDepthStencilStateCreateInfo m_createInfo{};
};

/**
 *
 * @class GraphicsPipelineCreateInfo
 *
 * @brief Is used to aggregate all needed data for
 * VkGraphicsPipelineCreateInfo structure.
 *
 */

class GraphicsPipelineCreateInfo {
private:
  template <typename Stage>
  using ShaderInfo =
      std::pair<StrongReference<const Stage>, VkSpecializationInfo>;
  template <typename... Stage>
  using ShaderInfos = std::tuple<std::optional<ShaderInfo<Stage>>...>;

  template <typename Stage> static auto &m_getStage(auto &&infos) {
    return std::get<std::optional<ShaderInfo<Stage>>>(infos);
  }

public:
  GraphicsPipelineCreateInfo(
      RenderPass const &renderPass,
      PipelineLayout const &layout) noexcept(ExceptionsDisabled)
      : m_renderPass(renderPass), m_layout(layout) {

    m_inputAssemblyStateCreateInfo = vkw::InputAssemblyStateCreateInfo{};
    m_rasterizationStateCreateInfo = vkw::RasterizationStateCreateInfo{};

    // Default MultisampleStateCreateInfo
    m_multisampleState.sampleShadingEnable = VK_FALSE;
    m_multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    m_multisampleState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_multisampleState.pNext = nullptr;
    m_multisampleState.flags = 0;

    // Default Depth/stencil state create info
    m_depthStencilState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    m_depthStencilState.pNext = nullptr;
    m_depthStencilState.depthTestEnable = VK_FALSE;
    m_depthStencilState.depthWriteEnable = VK_FALSE;
    m_depthStencilState.stencilTestEnable = VK_FALSE;
    m_depthStencilState.depthBoundsTestEnable = VK_FALSE;

    // Default Color blending state create info
    m_colorBlendState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_colorBlendState.pNext = nullptr;
    m_colorBlendState.flags = 0;
    m_colorBlendState.attachmentCount =
        m_renderPass.get().numColorAttachments();
    VkPipelineColorBlendAttachmentState state{};
    state.blendEnable = VK_FALSE;
    state.colorWriteMask = 0xf;
    m_blendStates.resize(m_colorBlendState.attachmentCount, state);
    m_colorBlendState.pAttachments = m_blendStates.data();
    m_colorBlendState.logicOpEnable = VK_FALSE;

    m_viewportState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    m_viewportState.pNext = nullptr;
    m_viewportState.flags = 0;
    m_viewportState.viewportCount = m_viewportState.scissorCount = 1;

    m_dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    m_dynamicState.pNext = nullptr;
    m_dynamicState.dynamicStateCount = 0;
    m_dynamicState.pDynamicStates = nullptr;
    m_dynamicState.flags = 0;
  }

  GraphicsPipelineCreateInfo &
  addDepthTestState(DepthTestStateCreateInfo depthTest) noexcept {
    auto dTest =
        depthTest.operator const VkPipelineDepthStencilStateCreateInfo &();
    m_depthStencilState.depthTestEnable = VK_TRUE;
    m_depthStencilState.depthWriteEnable = dTest.depthWriteEnable;
    m_depthStencilState.depthCompareOp = dTest.depthCompareOp;
    m_depthStencilState.minDepthBounds = dTest.minDepthBounds;
    m_depthStencilState.maxDepthBounds = dTest.maxDepthBounds;
    return *this;
  }

  template <typename Stage>
  GraphicsPipelineCreateInfo &
  addShader(Stage const &shader, SpecializationConstants const &constants = {},
            VkPipelineShaderStageCreateFlags flags = 0) noexcept {
    auto &shaderInfo = m_getStage<Stage>(m_shaders);
    if (shaderInfo.has_value()) {
      VkShaderModule module = shaderInfo->first.get();
      auto newEnd =
          std::remove_if(m_shaderStages.begin(), m_shaderStages.end(),
                         [module](VkPipelineShaderStageCreateInfo info) {
                           return info.module == module;
                         });

      m_shaderStages.erase(newEnd);
    }
    VkPipelineShaderStageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.module = shader;
    createInfo.pName = "main";
    createInfo.flags = flags;
    createInfo.stage = shader.stage();

    m_shaderStages.push_back(createInfo);
    shaderInfo.emplace(shader, constants);
    return *this;
  }

  GraphicsPipelineCreateInfo &addVertexInputState(
      VertexInputStateCreateInfoBase const &vertexInputState) noexcept {
    m_vertexInputStateCreateInfo = vertexInputState;
    return *this;
  }

  GraphicsPipelineCreateInfo &addInputAssemblyState(
      InputAssemblyStateCreateInfo const &inputAssemblyState) noexcept {
    m_inputAssemblyStateCreateInfo = inputAssemblyState;
    return *this;
  }

  GraphicsPipelineCreateInfo &addRasterizationState(
      RasterizationStateCreateInfo const &rasterizationState) noexcept {
    m_rasterizationStateCreateInfo = rasterizationState;
    return *this;
  }

  GraphicsPipelineCreateInfo &
  addBlendState(VkPipelineColorBlendAttachmentState state,
                uint32_t attachment) noexcept {
    m_blendStates.at(attachment) = state;
    return *this;
  }

  GraphicsPipelineCreateInfo &addDynamicState(VkDynamicState state) noexcept {
    if (std::find(m_dynStates.begin(), m_dynStates.end(), state) ==
        m_dynStates.end()) {
      m_dynStates.emplace_back(state);
    }
    return *this;
  }
  GraphicsPipelineCreateInfo &
  enableMultisampling(VkSampleCountFlagBits sampleCount,
                      bool alphaToCoverageEnable,
                      bool alphaToOneEnable) noexcept(ExceptionsDisabled) {
    m_multisampleState.rasterizationSamples = sampleCount;
    m_multisampleState.alphaToCoverageEnable = alphaToCoverageEnable;
    m_multisampleState.alphaToOneEnable = alphaToOneEnable;
    return *this;
  };

  GraphicsPipelineCreateInfo &
  enableSampleRateShading(float minRate) noexcept(ExceptionsDisabled) {
    if (m_multisampleState.rasterizationSamples == VK_SAMPLE_COUNT_1_BIT) {
      postError(LogicError{"enableSampleRateShading() cannot be called if "
                           "multisampling is not enabled"});
    }
    if (pass().parent().physicalDevice().enabledFeatures().sampleRateShading ==
        VK_FALSE) {
      postError(LogicError{"enableSampleRateShading() cannot be called if "
                           "sampleRateShading feature is not enabled"});
    }

    m_multisampleState.sampleShadingEnable = VK_TRUE;
    m_multisampleState.minSampleShading = minRate;
    return *this;
  }

  GraphicsPipelineCreateInfo &
  setSampleMask(std::span<VkSampleMask> mask) noexcept(ExceptionsDisabled) {
    if (m_multisampleState.rasterizationSamples == VK_SAMPLE_COUNT_1_BIT) {
      postError(LogicError{"setSampleMask() cannot be called if "
                           "multisampling is not enabled"});
    }
    auto getProperMaskLength = [](VkSampleCountFlagBits samples) {
      switch (samples) {
      case VK_SAMPLE_COUNT_64_BIT:
        return 2;
      default:
        return 1;
      }
    };
    if (mask.size() !=
        getProperMaskLength(m_multisampleState.rasterizationSamples)) {
      postError(
          LogicError{"mask provided ro setSampleMask() has incorrect size"});
    }
    m_sampleMask.clear();
    std::copy(mask.begin(), mask.end(), std::back_inserter(m_sampleMask));
    return *this;
  }

  operator VkGraphicsPipelineCreateInfo() const noexcept {
    m_colorBlendState.attachmentCount = m_blendStates.size();
    m_colorBlendState.pAttachments = m_blendStates.data();
    m_dynamicState.dynamicStateCount = m_dynStates.size();
    m_dynamicState.pDynamicStates = m_dynStates.data();
    VkGraphicsPipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.renderPass = m_renderPass.get();
    createInfo.layout = m_layout.get();
    // multiple subpasses are not supported.
    createInfo.subpass = 0;
    createInfo.pColorBlendState = &m_colorBlendState;
    createInfo.pDepthStencilState = &m_depthStencilState;
    createInfo.pDynamicState = &m_dynamicState;
    createInfo.pViewportState = &m_viewportState;
    createInfo.pInputAssemblyState = &m_inputAssemblyStateCreateInfo;
    createInfo.pMultisampleState = &m_multisampleState;
    createInfo.pRasterizationState = &m_rasterizationStateCreateInfo;
    createInfo.pTessellationState =
        nullptr; // TODO: implement tesselation shader support
    createInfo.pVertexInputState =
        &(m_vertexInputStateCreateInfo.get().
          operator const VkPipelineVertexInputStateCreateInfo &());
    VkSpecializationInfo info{};
    auto applySpecConstants = [this](auto &&shader) {
      if (!shader)
        return;
      auto &&[shaderRef, specInfo] = *shader;
      if (specInfo.mapEntryCount == 0)
        return;
      VkShaderModule shadMod = shaderRef.get();
      auto foundInfo =
          std::ranges::find_if(m_shaderStages, [&shadMod](auto &&stage) {
            return stage.module == shadMod;
          });
      assert(foundInfo != m_shaderStages.end());
      foundInfo->pSpecializationInfo = &specInfo;
    };
    std::apply([&](auto &&...shaders) { (applySpecConstants(shaders), ...); },
               m_shaders);

    createInfo.stageCount = m_shaderStages.size();
    createInfo.pStages = m_shaderStages.data();
    return createInfo;
  }

  VertexInputStateCreateInfoBase const &vertexInputState() const noexcept {
    return m_vertexInputStateCreateInfo;
  }

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState() const noexcept {
    return m_inputAssemblyStateCreateInfo;
  }

  VkPipelineRasterizationStateCreateInfo rasterizationState() const noexcept {
    return m_rasterizationStateCreateInfo;
  }

  PipelineLayout const &layout() const noexcept { return m_layout; }

  RenderPass const &pass() const noexcept { return m_renderPass; }

  template <typename Stage> auto shader() const noexcept {
    return m_getStage<Stage>(m_shaders);
  }

  void clearShaders() {
    std::apply([](auto &&...shdrs) { (shdrs.reset(), ...); }, m_shaders);
    m_shaderStages.clear();
  }

private:
  StrongReference<RenderPass const> m_renderPass;
  StrongReference<PipelineLayout const> m_layout;

  // Shader Stages
  // TODO: support other shader stages
  mutable cntr::vector<VkPipelineShaderStageCreateInfo, 2> m_shaderStages;
  ShaderInfos<VertexShader, FragmentShader> m_shaders;

  // Fixed pipeline stages

  vkw::StrongReference<VertexInputStateCreateInfoBase const>
      m_vertexInputStateCreateInfo = NullVertexInputState::get();
  VkPipelineInputAssemblyStateCreateInfo m_inputAssemblyStateCreateInfo;
  VkPipelineRasterizationStateCreateInfo m_rasterizationStateCreateInfo;

  VkPipelineMultisampleStateCreateInfo m_multisampleState{};
  cntr::vector<VkSampleMask, 2> m_sampleMask;
  VkPipelineDepthStencilStateCreateInfo m_depthStencilState{};
  mutable VkPipelineColorBlendStateCreateInfo m_colorBlendState{};
  cntr::vector<VkPipelineColorBlendAttachmentState, 2> m_blendStates{};

  // TODO: support configure for viewport state
  VkPipelineViewportStateCreateInfo m_viewportState{};

  // dynamic states

  mutable VkPipelineDynamicStateCreateInfo m_dynamicState{};
  cntr::vector<VkDynamicState, 4> m_dynStates;
};

/**
 *
 * @class ComputePipelineCreateInfo
 *
 * @brief Is used to aggregate all needed data for
 * VkComputePipelineCreateInfo structure.
 *
 */

class ComputePipelineCreateInfo {
public:
  ComputePipelineCreateInfo(PipelineLayout const &layout,
                            ComputeShader const &shader,
                            SpecializationConstants constants = {}) noexcept
      : m_layout(layout), m_shader(shader), m_constants(constants) {}

  operator VkComputePipelineCreateInfo() const noexcept {
    VkComputePipelineCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.layout = m_layout.get();
    createInfo.flags = 0; // TODO
    createInfo.stage.module = m_shader.get();
    createInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    createInfo.stage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.stage.pNext = nullptr;
    createInfo.stage.pName = "main"; // TODO
    createInfo.stage.flags = 0;      // TODO
    if (!m_constants.mapEntryCount != 0)
      createInfo.stage.pSpecializationInfo = &m_constants;
    else
      createInfo.stage.pSpecializationInfo = nullptr;
    return createInfo;
  }

  auto &layout() const noexcept { return m_layout.get(); }

private:
  StrongReference<PipelineLayout const> m_layout;
  StrongReference<ComputeShader const> m_shader;
  VkSpecializationInfo m_constants;
};

/**
 *
 * @class Pipeline
 *
 * @brief Represents generic VkPipeline structure. It is advised to
 * use it's derived classes that represent specific pipelines.
 *
 *
 */

class Pipeline : public ReferenceGuard {
public:
  PipelineLayout const &layout() const noexcept { return m_pipelineLayout; }

  operator VkPipeline() const noexcept { return m_pipeline.get(); }

protected:
  Pipeline(
      Device &device,
      GraphicsPipelineCreateInfo const &createInfo) noexcept(ExceptionsDisabled)
      : m_pipelineLayout(createInfo.layout()),
        m_pipeline(
            [&]() {
              VkPipeline pipeline = nullptr;
              VkGraphicsPipelineCreateInfo CI = createInfo;
              VK_CHECK_RESULT(device.core<1, 0>().vkCreateGraphicsPipelines(
                  device, VK_NULL_HANDLE, 1, &CI, HostAllocator::get(),
                  &pipeline))
              return pipeline;
            }(),
            PipelineDestroyer{device}) {}
  Pipeline(
      Device &device,
      ComputePipelineCreateInfo const &createInfo) noexcept(ExceptionsDisabled)
      : m_pipelineLayout(createInfo.layout()),
        m_pipeline(
            [&]() {
              VkPipeline pipeline = nullptr;
              VkComputePipelineCreateInfo CI = createInfo;
              VK_CHECK_RESULT(device.core<1, 0>().vkCreateComputePipelines(
                  device, VK_NULL_HANDLE, 1, &CI, HostAllocator::get(),
                  &pipeline))
              return pipeline;
            }(),
            PipelineDestroyer{device}) {}

  Pipeline(Device &device, GraphicsPipelineCreateInfo const &createInfo,
           PipelineCache const &cache) noexcept(ExceptionsDisabled)
      : m_pipelineLayout(createInfo.layout()),
        m_pipeline(
            [&]() {
              VkPipeline pipeline = nullptr;
              VkGraphicsPipelineCreateInfo CI = createInfo;
              VK_CHECK_RESULT(device.core<1, 0>().vkCreateGraphicsPipelines(
                  device, cache, 1, &CI, HostAllocator::get(), &pipeline))
              return pipeline;
            }(),
            PipelineDestroyer{device}) {}
  Pipeline(Device &device, ComputePipelineCreateInfo const &createInfo,
           PipelineCache const &cache) noexcept(ExceptionsDisabled)
      : m_pipelineLayout(createInfo.layout()),
        m_pipeline(
            [&]() {
              VkPipeline pipeline = nullptr;
              VkComputePipelineCreateInfo CI = createInfo;
              VK_CHECK_RESULT(device.core<1, 0>().vkCreateComputePipelines(
                  device, cache, 1, &CI, HostAllocator::get(), &pipeline))
              return pipeline;
            }(),
            PipelineDestroyer{device}) {}

private:
  StrongReference<PipelineLayout const> m_pipelineLayout;
  struct PipelineDestroyer {
    void operator()(VkPipeline pipeline) {
      if (!pipeline)
        return;
      device.get().core<1, 0>().vkDestroyPipeline(device.get(), pipeline,
                                                  HostAllocator::get());
    }
    StrongReference<Device> device;
  };
  std::unique_ptr<VkPipeline_T, PipelineDestroyer> m_pipeline;
};

/**
 *
 * @class GraphicsPipeline
 *
 * @brief Represents VKPipeline created by vkCreateGraphicsPipelines.
 *
 *
 */

class GraphicsPipeline final : public Pipeline {
public:
  GraphicsPipeline(
      Device &device,
      GraphicsPipelineCreateInfo const &createInfo) noexcept(ExceptionsDisabled)
      : Pipeline(device, createInfo) {}

  GraphicsPipeline(Device &device, GraphicsPipelineCreateInfo const &createInfo,
                   PipelineCache const &cache) noexcept(ExceptionsDisabled)
      : Pipeline(device, createInfo, cache) {}
};

/**
 *
 * @class ComputePipeline
 *
 * @brief Represents VkPipeline created by vkCreateComputePipelines.
 *
 *
 */

class ComputePipeline final : public Pipeline {
public:
  ComputePipeline(
      Device &device,
      ComputePipelineCreateInfo const &createInfo) noexcept(ExceptionsDisabled)
      : Pipeline(device, createInfo){};

  ComputePipeline(Device &device, ComputePipelineCreateInfo const &createInfo,
                  PipelineCache const &cache) noexcept(ExceptionsDisabled)
      : Pipeline(device, createInfo, cache){};
};
} // namespace vkw
#endif // VKRENDERER_PIPELINE_HPP
