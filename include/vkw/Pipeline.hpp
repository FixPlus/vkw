#ifndef VKRENDERER_PIPELINE_HPP
#define VKRENDERER_PIPELINE_HPP

#include <vkw/DescriptorSet.hpp>
#include <vkw/RenderPass.hpp>
#include <vkw/Shader.hpp>
#include <vkw/VertexBuffer.hpp>

#include <algorithm>
#include <functional>
#include <optional>
#include <span>

namespace vkw {

class PipelineCache;
/**
 *
 * @class SpecializationConstants
 *
 * @brief Represents VkSpecializationInfo structure.
 *
 *
 */

class SpecializationConstants {
public:
  SpecializationConstants();

  SpecializationConstants(SpecializationConstants &&another) noexcept
      : m_entries(std::move(another.m_entries)),
        m_data(std::move(another.m_data)), m_info(another.m_info) {
    m_info.pMapEntries = m_entries.data();
    m_info.pData = m_data.data();
  };
  SpecializationConstants(SpecializationConstants const &another) noexcept(
      ExceptionsDisabled)
      : m_entries(another.m_entries), m_data(another.m_data),
        m_info(another.m_info) {
    m_info.pMapEntries = m_entries.data();
    m_info.pData = m_data.data();
  };

  SpecializationConstants &
  operator=(SpecializationConstants &&another) noexcept {
    m_entries = std::move(another.m_entries);
    m_data = std::move(another.m_data);
    m_info = another.m_info;
    m_info.pMapEntries = m_entries.data();
    m_info.pData = m_data.data();
    return *this;
  }

  SpecializationConstants &operator=(
      SpecializationConstants const &another) noexcept(ExceptionsDisabled) {
    m_entries = another.m_entries;
    m_data = another.m_data;
    m_info = another.m_info;
    m_info.pMapEntries = m_entries.data();
    m_info.pData = m_data.data();
    return *this;
  }

  bool empty() const noexcept { return m_entries.empty(); }

  template <typename T>
  void addConstant(T const &constant,
                   uint32_t id) noexcept(ExceptionsDisabled) {
    m_addConstant(&constant, sizeof(constant), id);
  }

  void clear() {
    m_entries.clear();
    m_data.clear();
    m_info.mapEntryCount = 0;
  }

  operator VkSpecializationInfo const &() const noexcept { return m_info; }

private:
  void m_addConstant(const void *constant, size_t size,
                     uint32_t id) noexcept(ExceptionsDisabled);

  boost::container::small_vector<VkSpecializationMapEntry, 3> m_entries;
  boost::container::small_vector<unsigned char, 64> m_data;
  VkSpecializationInfo m_info{};
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
      ExceptionsDisabled) {
    m_fillInfo(flags);
  }
  template <forward_range_of<DescriptorSetLayout const> T>
  explicit PipelineLayoutInfo(
      T const &setLayouts,
      std::span<const VkPushConstantRange> pushConstants = {},
      VkPipelineLayoutCreateFlags flags = 0) noexcept(ExceptionsDisabled) {
    std::copy(pushConstants.begin(), pushConstants.end(),
              std::back_inserter(m_pushConstants));
    std::transform(setLayouts.begin(), setLayouts.end(),
                   std::back_inserter(m_descriptorLayouts),
                   [](std::ranges::range_value_t<T> const &layout) {
                     return StrongReference<DescriptorSetLayout const>(
                         layout.get());
                   });
    m_fillInfo(flags);
  }

  // overload for 1 element case
  explicit PipelineLayoutInfo(
      DescriptorSetLayout const &setLayout,
      std::span<const VkPushConstantRange> pushConstants = {},
      VkPipelineLayoutCreateFlags flags = 0) noexcept(ExceptionsDisabled) {
    std::copy(pushConstants.begin(), pushConstants.end(),
              std::back_inserter(m_pushConstants));
    m_descriptorLayouts.emplace_back(setLayout);
    m_fillInfo(flags);
  }

  bool operator==(PipelineLayoutInfo const &rhs) const noexcept;

  bool operator!=(PipelineLayoutInfo const &rhs) const noexcept {
    return !(*this == rhs);
  }

  auto begin() noexcept { return m_descriptorLayouts.begin(); }

  auto end() noexcept { return m_descriptorLayouts.begin(); }

  auto begin() const noexcept { return m_descriptorLayouts.begin(); }

  auto end() const noexcept { return m_descriptorLayouts.begin(); }

  auto &info() const noexcept { return m_createInfo; }

private:
  void
  m_fillInfo(VkPipelineLayoutCreateFlags flags) noexcept(ExceptionsDisabled);

  boost::container::small_vector<StrongReference<DescriptorSetLayout const>, 4>
      m_descriptorLayouts{};
  boost::container::small_vector<VkDescriptorSetLayout, 4> m_rawLayout{};
  boost::container::small_vector<VkPushConstantRange, 4> m_pushConstants{};

  VkPipelineLayoutCreateInfo m_createInfo{};
};

/**
 *
 * @class PipelineLayout
 *
 * @brief Represents vulkan's VkPipelineLayout structure.
 *
 *
 */

class PipelineLayout : public PipelineLayoutInfo,
                       public UniqueVulkanObject<VkPipelineLayout> {
public:
  explicit PipelineLayout(
      Device const &device,
      VkPipelineLayoutCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : PipelineLayoutInfo(flags), UniqueVulkanObject<VkPipelineLayout>(
                                       device, info()) {}
  template <forward_range_of<DescriptorSetLayout const> T>
  PipelineLayout(
      Device const &device, T const &setLayouts,
      std::span<const VkPushConstantRange> pushConstants = {},
      VkPipelineLayoutCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : PipelineLayoutInfo(setLayouts, pushConstants, flags),
        UniqueVulkanObject<VkPipelineLayout>(device, info()) {}

  // overload for 1 element case
  PipelineLayout(
      Device const &device, DescriptorSetLayout const &setLayout,
      std::span<const VkPushConstantRange> pushConstants = {},
      VkPipelineLayoutCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : PipelineLayoutInfo(setLayout, pushConstants, flags),
        UniqueVulkanObject<VkPipelineLayout>(device, info()) {}

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
  VertexInputStateCreateInfoBase(
      uint32_t attributeDescCount,
      VkVertexInputAttributeDescription const *pAttributeDesc,
      uint32_t bindingDescCount,
      VkVertexInputBindingDescription const *pBindingDesc,
      VkPipelineVertexInputStateCreateFlags flags = 0) noexcept;

  operator VkPipelineVertexInputStateCreateInfo const &() const noexcept {
    return m_createInfo;
  }

  uint32_t totalAttributes() const noexcept {
    return m_createInfo.vertexAttributeDescriptionCount;
  }
  VkVertexInputAttributeDescription attribute(uint32_t index) const
      noexcept(ExceptionsDisabled);

  uint32_t totalBindings() const noexcept {
    return m_createInfo.vertexBindingDescriptionCount;
  }
  VkVertexInputBindingDescription binding(uint32_t index) const
      noexcept(ExceptionsDisabled);

  virtual ~VertexInputStateCreateInfoBase() = default;

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

class NullVertexInputState : public VertexInputStateCreateInfoBase {
public:
  /**
   * @return reference to statically created NullVertexInputState object
   */
  static NullVertexInputState &get() noexcept;

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
class VertexInputStateCreateInfo : public VertexInputStateCreateInfoBase {
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

class InputAssemblyStateCreateInfo {
public:
  InputAssemblyStateCreateInfo(
      VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      VkBool32 restartEnable = false) noexcept;

  operator VkPipelineInputAssemblyStateCreateInfo const &() const noexcept {
    return m_createInfo;
  }

  VkPrimitiveTopology topology() const noexcept {
    return m_createInfo.topology;
  }

  bool isPrimitivesRestartEnabled() const noexcept {
    return m_createInfo.primitiveRestartEnable;
  }

  virtual ~InputAssemblyStateCreateInfo() = default;

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

class RasterizationStateCreateInfo {
public:
  RasterizationStateCreateInfo(
      VkBool32 depthClampEnable = VK_FALSE,
      VkBool32 rasterizerDiscardEnable = VK_FALSE,
      VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
      VkCullModeFlags cullMode = VK_CULL_MODE_NONE,
      VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      VkBool32 depthBiasEnable = false, float depthBiasConstantFactor = 0,
      float depthBiasClamp = 0, float depthBiasSlopeFactor = 0,
      float lineWidth = 1.0f) noexcept;

  virtual ~RasterizationStateCreateInfo() = default;

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

class DepthTestStateCreateInfo {
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
public:
  GraphicsPipelineCreateInfo(
      RenderPass const &renderPass, uint32_t subpass,
      PipelineLayout const &layout) noexcept(ExceptionsDisabled);

  GraphicsPipelineCreateInfo(GraphicsPipelineCreateInfo &&another) noexcept;
  GraphicsPipelineCreateInfo(GraphicsPipelineCreateInfo const &another);

  GraphicsPipelineCreateInfo &
  addDepthTestState(DepthTestStateCreateInfo depthTest) noexcept;
  GraphicsPipelineCreateInfo &
  addVertexShader(VertexShader const &shader,
                  SpecializationConstants const &constants = {}) noexcept;
  GraphicsPipelineCreateInfo &
  addFragmentShader(FragmentShader const &shader,
                    SpecializationConstants const &constants = {}) noexcept;
  GraphicsPipelineCreateInfo &addVertexInputState(
      VertexInputStateCreateInfoBase const &vertexInputState) noexcept;
  GraphicsPipelineCreateInfo &addInputAssemblyState(
      InputAssemblyStateCreateInfo const &inputAssemblyState) noexcept;
  GraphicsPipelineCreateInfo &addRasterizationState(
      RasterizationStateCreateInfo const &rasterizationState) noexcept;
  GraphicsPipelineCreateInfo &
  addBlendState(VkPipelineColorBlendAttachmentState state,
                uint32_t attachment) noexcept;
  GraphicsPipelineCreateInfo &addDynamicState(VkDynamicState state) noexcept;
  GraphicsPipelineCreateInfo &
  enableMultisampling(bool alphaToCoverageEnable,
                      bool alphaToOneEnable) noexcept(ExceptionsDisabled);
  GraphicsPipelineCreateInfo &
  enableSampleRateShading(float minRate) noexcept(ExceptionsDisabled);

  GraphicsPipelineCreateInfo &
  setSampleMask(std::span<VkSampleMask> mask) noexcept(ExceptionsDisabled);
  operator VkGraphicsPipelineCreateInfo const &() const noexcept {
    return m_createInfo;
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

  uint32_t subpass() const noexcept { return m_createInfo.subpass; }

  auto vertexShader() const noexcept { return m_vertexShader; }

  auto fragmentShader() const noexcept { return m_fragmentShader; }

  void clearShaders() {
    m_vertexShader.reset();
    m_fragmentShader.reset();
    m_shaderStages.clear();
  }

private:
  StrongReference<RenderPass const> m_renderPass;

  // Shader Stages

  boost::container::small_vector<VkPipelineShaderStageCreateInfo, 2>
      m_shaderStages;
  std::optional<StrongReference<VertexShader const>> m_vertexShader;
  std::optional<StrongReference<FragmentShader const>> m_fragmentShader;

  // TODO: support other shader stages

  // Fixed pipeline stages

  vkw::StrongReference<VertexInputStateCreateInfoBase const>
      m_vertexInputStateCreateInfo = NullVertexInputState::get();
  VkPipelineInputAssemblyStateCreateInfo m_inputAssemblyStateCreateInfo;
  VkPipelineRasterizationStateCreateInfo m_rasterizationStateCreateInfo;

  VkPipelineMultisampleStateCreateInfo m_multisampleState{};
  boost::container::small_vector<VkSampleMask, 2> m_sampleMask;
  VkPipelineDepthStencilStateCreateInfo m_depthStencilState{};
  VkPipelineColorBlendStateCreateInfo m_colorBlendState{};
  boost::container::small_vector<VkPipelineColorBlendAttachmentState, 2>
      m_blendStates{};

  // TODO: support configure for viewport state
  VkPipelineViewportStateCreateInfo m_viewportState{};

  StrongReference<PipelineLayout const> m_layout;
  VkGraphicsPipelineCreateInfo m_createInfo{};

  // dynamic states

  VkPipelineDynamicStateCreateInfo m_dynamicState{};
  boost::container::small_vector<VkDynamicState, 4> m_dynStates;
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
                            SpecializationConstants constants = {}) noexcept;

  operator VkComputePipelineCreateInfo const &() const noexcept {
    return m_createInfo;
  }

  auto &layout() const noexcept { return m_layout.get(); }

private:
  StrongReference<PipelineLayout const> m_layout;
  SpecializationConstants m_constants;
  VkComputePipelineCreateInfo m_createInfo;
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
  Pipeline(Device &device, GraphicsPipelineCreateInfo const
                               &createInfo) noexcept(ExceptionsDisabled);
  Pipeline(
      Device &device,
      ComputePipelineCreateInfo const &createInfo) noexcept(ExceptionsDisabled);

  Pipeline(Device &device, GraphicsPipelineCreateInfo const &createInfo,
           PipelineCache const &cache) noexcept(ExceptionsDisabled);
  Pipeline(Device &device, ComputePipelineCreateInfo const &createInfo,
           PipelineCache const &cache) noexcept(ExceptionsDisabled);

  Pipeline(Pipeline &&another) noexcept
      : m_device(another.m_device), m_pipeline(another.m_pipeline),
        m_pipelineLayout(another.m_pipelineLayout) {
    another.m_pipeline = VK_NULL_HANDLE;
  }

  Pipeline &operator=(Pipeline &&another) noexcept {
    m_device = another.m_device;
    m_pipelineLayout = another.m_pipelineLayout;
    std::swap(m_pipeline, another.m_pipeline);

    return *this;
  }

  PipelineLayout const &layout() const noexcept { return m_pipelineLayout; }

  virtual ~Pipeline();

  operator VkPipeline() const noexcept { return m_pipeline; }

private:
  StrongReference<Device> m_device;
  StrongReference<PipelineLayout const> m_pipelineLayout;
  VkPipeline m_pipeline = VK_NULL_HANDLE;
};

/**
 *
 * @class GraphicsPipeline
 *
 * @brief Represents VKPipeline created by vkCreateGraphicsPipelines.
 *
 *
 */

class GraphicsPipeline : public Pipeline {
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

class ComputePipeline : public Pipeline {
public:
  ComputePipeline(
      Device &device,
      ComputePipelineCreateInfo const &createInfo) noexcept(ExceptionsDisabled)
      : Pipeline(device, createInfo), m_createInfo(createInfo){};

  ComputePipeline(Device &device, ComputePipelineCreateInfo const &createInfo,
                  PipelineCache const &cache) noexcept(ExceptionsDisabled)
      : Pipeline(device, createInfo, cache), m_createInfo(createInfo){};
  ComputePipelineCreateInfo const &info() const { return m_createInfo; }

private:
  ComputePipelineCreateInfo m_createInfo;
};
} // namespace vkw
#endif // VKRENDERER_PIPELINE_HPP
