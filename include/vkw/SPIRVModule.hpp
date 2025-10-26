#ifndef VKWRAPPER_SPIRVMODULE_HPP
#define VKWRAPPER_SPIRVMODULE_HPP

#include <algorithm>
#include <memory>
#include <ranges>
#include <span>
#include <vector>
#include <vkw/Exception.hpp>
#include <vkw/Runtime.h>

namespace vkw {

class SPIRVModule final {
public:
  explicit SPIRVModule(std::span<const unsigned> code) noexcept(
      ExceptionsDisabled) {
    m_code.reserve(code.size());
    std::copy(code.begin(), code.end(), std::back_inserter(m_code));
    // TODO: maybe some code verification checks here
  }

  std::span<const unsigned> code() const noexcept { return m_code; }

private:
  cntr::vector<unsigned, 1> m_code;
};

class SPIRVLinkError : public Error {
public:
  SPIRVLinkError(std::string_view what) : Error(what){};
  std::string_view codeString() const noexcept override {
    return "spirv-link error";
  }
};

class SPIRVLinkMessageConsumer {
public:
  virtual void onMessage(VKW_spvMessageLevel level, const char *source,
                         size_t line, size_t column, const char *message) = 0;
  virtual ~SPIRVLinkMessageConsumer() = default;
};

class SPIRVLinkContext final {
public:
  SPIRVLinkContext(SPIRVLinkMessageConsumer *consumer = nullptr)
      : m_context([&]() {
          VKW_spvContext handle = nullptr;
          auto result = vkw_createSpvContext(
              &handle, consumer ? &m_consumer_impl : nullptr, consumer);
          if (result == VKW_OK)
            return handle;
          postError(
              SPIRVLinkError("create spv context returned non-zero status"));
        }()) {}

  SPIRVModule link(auto &&modules, bool linkLibrary = false) const {
    cntr::vector<size_t, 3> codeSizes;
    cntr::vector<const unsigned int *, 3> codes;

    std::ranges::transform(modules, std::back_inserter(codes),
                           [](auto &&module) { return module.code().data(); });
    std::ranges::transform(modules, std::back_inserter(codeSizes),
                           [](auto &&module) { return module.code().size(); });
    VKW_spvLinkInfo linkInfo{};
    linkInfo.binaries = codes.data();
    linkInfo.binary_sizes = codeSizes.data();
    linkInfo.num_binaries = codes.size();
    if (linkLibrary)
      linkInfo.flags =
          VKW_SPV_LINK_ALLOW_PARTIAL_LINKAGE | VKW_SPV_LINK_CREATE_LIBRARY;

    uint32_t *linkedData;
    size_t linkedSize;

    auto result =
        vkw_spvLink(m_context.get(), &linkInfo, &linkedData, &linkedSize);

    if (result == VKW_OK) {
      auto freeLinkedData = [](uint32_t *data) {
        vkw_spvLinkedImageFree(data);
      };
      std::unique_ptr<uint32_t, decltype(freeLinkedData)> linkedDataKeeper(
          linkedData);
      return SPIRVModule(
          std::span<const unsigned>{linkedData, linkedData + linkedSize});
    }
    /// TODO: pass result value may be?
    postError(SPIRVLinkError("link failed"));
  }

private:
  static void m_consumer_impl(VKW_spvMessageLevel level, const char *source,
                              size_t line, size_t column, const char *message,
                              void *userData) {
    if (!userData)
      return;
    auto *consumer = reinterpret_cast<SPIRVLinkMessageConsumer *>(userData);
    consumer->onMessage(level, source, line, column, message);
  }
  struct ContextDestroyer {
    void operator()(VKW_spvContext handle) { vkw_destroySpvContext(handle); }
  };
  std::unique_ptr<VKW_spvContext_T, ContextDestroyer> m_context;
};

class SPIRVReflectError : public Error {
private:
  static std::string_view reflectErrorStr(SpvReflectResult result) noexcept {
    switch (result) {
#define CASE(X)                                                                \
  case X:                                                                      \
    return #X;
      CASE(SPV_REFLECT_RESULT_SUCCESS)
      CASE(SPV_REFLECT_RESULT_NOT_READY)
      CASE(SPV_REFLECT_RESULT_ERROR_PARSE_FAILED)
      CASE(SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED)
      CASE(SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED)
      CASE(SPV_REFLECT_RESULT_ERROR_NULL_POINTER)
      CASE(SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR)
      CASE(SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH)
      CASE(SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_CODE_SIZE)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_MAGIC_NUMBER)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_SET_NUMBER_OVERFLOW)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_STORAGE_CLASS)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_RECURSION)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_INSTRUCTION)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_BLOCK_DATA)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_BLOCK_MEMBER_REFERENCE)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ENTRY_POINT)
      CASE(SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_EXECUTION_MODE)
#undef CASE
    default:
      return "**UNKNOWN**";
    }
  }

public:
  SPIRVReflectError(SpvReflectResult result, std::string_view libCallName)
      : Error([&]() {
          std::stringstream ss;
          ss << "SPIRV-Reflect - call to " << libCallName << " returned "
             << reflectErrorStr(result) << " code";
          return ss.str();
        }()),
        m_result(result), m_libCallName(libCallName) {}

  auto result() const noexcept { return m_result; }

  auto libCallName() const noexcept { return m_libCallName; }

  std::string_view codeString() const noexcept override {
    return "spirv-reflect error";
  }

private:
  SpvReflectResult m_result;
  std::string_view m_libCallName;
};

template <typename T>
concept ReflectInfoIteratorTraits = requires {
                                      typename T::value_type;
                                      typename T::super_type;
                                    };

template <ReflectInfoIteratorTraits T> struct ReflectInfoIteratorTraitsCommon {
  static typename T::value_type get_value(typename T::super_type *s,
                                          unsigned idx) noexcept;
  static unsigned get_size(typename T::super_type *s) noexcept;
};

template <ReflectInfoIteratorTraits T> class ReflectInfoIterator {
public:
  using super_type = typename T::super_type;
  using value_type = typename T::value_type;
  using reference = void;
  using pointer = void;
  using difference_type = long;
  using iterator_category = std::input_iterator_tag;

  explicit ReflectInfoIterator(super_type *s = nullptr,
                               unsigned index = 0) noexcept
      : m_super(s), m_index(index) {}

  auto operator*() const noexcept {
    return ReflectInfoIteratorTraitsCommon<T>::get_value(m_super, m_index);
  }

  auto &operator++() noexcept {
    m_index++;
    return *this;
  }

  const auto operator++(int) noexcept {
    auto ret = *this;
    m_index++;
    return ret;
  }

  bool operator==(ReflectInfoIterator const &another) const noexcept {
    return m_super == another.m_super && m_index == another.m_index;
  }

  bool operator!=(ReflectInfoIterator const &another) const noexcept {
    return !(*this == another);
  }

private:
  super_type *m_super;
  unsigned m_index;
};

template <ReflectInfoIteratorTraits T>
class ReflectInfoRange
    : public std::ranges::view_interface<ReflectInfoRange<T>> {
public:
  using super_type = typename T::super_type;

  explicit ReflectInfoRange(super_type *s) noexcept
      : m_super(s), m_size(ReflectInfoIteratorTraitsCommon<T>::get_size(s)){};

  auto begin() const noexcept { return ReflectInfoIterator<T>{m_super, 0}; }

  auto end() const noexcept { return ReflectInfoIterator<T>{m_super, m_size}; }

  auto size() const noexcept { return m_size; }

  auto empty() const noexcept { return m_size == 0; }

private:
  super_type *m_super;
  unsigned m_size;
};

class SPIRVInterfaceVariable;

struct ReflectInputVariableTraits {
  using value_type = SPIRVInterfaceVariable;
  using super_type = const SpvReflectEntryPoint;
};

struct ReflectOutputVariableTraits {
  using value_type = SPIRVInterfaceVariable;
  using super_type = const SpvReflectEntryPoint;
};

struct ReflectInterfaceVariableTraits {
  using value_type = SPIRVInterfaceVariable;
  using super_type = const SpvReflectEntryPoint;
};

class SPIRVInterfaceVariable final {
public:
  /// This class represents signle shader attribute.
  /// E.G.:
  /// (GLSL)
  ///
  /// ...
  /// layout(location = 3) in vec4 Pos;
  /// ...
  ///
  /// attr.location() == 3;
  /// attr.format() == VK_FORMAT_R32G32B32_SFLOAT;
  /// attr.name() == "Pos";
  /// attr.isInput() == true;
  /// attr.isOutput() == false;

  unsigned location() const noexcept { return m_var->location; }
  VkFormat format() const noexcept {
    // VkFormat <-> SpvReflectFormat are mapped one to one.
    return VkFormat(m_var->format);
  }
  std::string_view name() const noexcept { return m_var->name; }
  bool isInput() const noexcept {
    return m_var->storage_class == SpvStorageClassInput;
  }
  bool isOutput() const noexcept {
    return m_var->storage_class == SpvStorageClassOutput;
  }

private:
  friend class ReflectInfoIteratorTraitsCommon<ReflectInputVariableTraits>;
  friend class ReflectInfoIteratorTraitsCommon<ReflectOutputVariableTraits>;
  friend class ReflectInfoIteratorTraitsCommon<ReflectInterfaceVariableTraits>;

  explicit SPIRVInterfaceVariable(
      const SpvReflectInterfaceVariable *var) noexcept
      : m_var{var} {}
  const SpvReflectInterfaceVariable *m_var;
};

class SPIRVDescriptorBindingInfo;

struct ReflectDescriptorBindingTraits {
  using value_type = SPIRVDescriptorBindingInfo;
  using super_type = const SpvReflectDescriptorSet;
};

class SPIRVDescriptorBindingInfo {
public:
  /// Descriptor type that should be passed to
  /// VkDescriptorSetLayoutBinding.
  VkDescriptorType descriptorType() const noexcept {
    // VkDescriptorType <-> SpvReflectDescriptorType are mapped one to one.
    return VkDescriptorType(m_binding->descriptor_type);
  }

  /// Descriptor count that should be passed to
  /// VkDescriptorSetLayoutBinding.
  unsigned descriptorCount() const noexcept { return m_binding->count; }

  /// Name of binding given in source code.
  std::string_view name() const noexcept { return m_binding->name; }

  /// Index of binding from layout.
  unsigned index() const noexcept { return m_binding->binding; }

private:
  friend class ReflectInfoIteratorTraitsCommon<ReflectDescriptorBindingTraits>;
  explicit SPIRVDescriptorBindingInfo(
      const SpvReflectDescriptorBinding *binding) noexcept
      : m_binding{binding} {}
  const SpvReflectDescriptorBinding *m_binding;
};

class SPIRVPushConstantInfo;

struct PushConstantTraits {
  using value_type = SPIRVPushConstantInfo;
  using super_type = const SpvReflectShaderModule;
};

class SPIRVPushConstantInfo {
public:
  unsigned size() const { return m_constant->size; }

  unsigned offset() const { return m_constant->offset; }

private:
  friend class ReflectInfoIteratorTraitsCommon<PushConstantTraits>;
  explicit SPIRVPushConstantInfo(
      const SpvReflectBlockVariable *constant) noexcept
      : m_constant{constant} {}
  const SpvReflectBlockVariable *m_constant;
};

class SPIRVDescriptorSetInfo;

struct ReflectDescriptorSetTraits {
  using value_type = SPIRVDescriptorSetInfo;
  using super_type = const SpvReflectEntryPoint;
};

struct ReflectDescriptorSetModuleTraits {
  using value_type = SPIRVDescriptorSetInfo;
  using super_type = const SpvReflectShaderModule;
};

class SPIRVDescriptorSetInfo {
public:
  using Bindings = ReflectInfoRange<ReflectDescriptorBindingTraits>;

  /// Set index from layout.
  unsigned index() const noexcept { return m_set->set; }

  /// Range of bindings in set.
  auto bindings() const noexcept { return Bindings{m_set}; }

private:
  friend class ReflectInfoIteratorTraitsCommon<ReflectDescriptorSetTraits>;
  friend class ReflectInfoIteratorTraitsCommon<
      ReflectDescriptorSetModuleTraits>;
  explicit SPIRVDescriptorSetInfo(const SpvReflectDescriptorSet *set) noexcept
      : m_set{set} {}
  const SpvReflectDescriptorSet *m_set;
};

class SPIRVEntryPointInfo;

struct ReflectEntryPointTraits {
  using value_type = SPIRVEntryPointInfo;
  using super_type = const SpvReflectShaderModule;
};

class SPIRVEntryPointInfo final {
public:
  using InputVariables = ReflectInfoRange<ReflectInputVariableTraits>;
  using OutputVariables = ReflectInfoRange<ReflectOutputVariableTraits>;
  using InterfaceVariables = ReflectInfoRange<ReflectInterfaceVariableTraits>;
  using DescriptorSets = ReflectInfoRange<ReflectDescriptorSetTraits>;

  std::string_view name() const noexcept { return m_handle->name; }
  unsigned id() const noexcept { return m_handle->id; }

  VkShaderStageFlagBits stage() const noexcept {
    // VkShaderStageFlagBits <-> SpvReflectShaderStageFlagBits are mapped one to
    // one.
    return VkShaderStageFlagBits(m_handle->shader_stage);
  }

  /// Input shader attributes used by this kernel. In GLSL are marked as 'in'.
  /// @note they are NOT sorted by their location.
  auto inputVariables() const noexcept { return InputVariables(m_handle); }

  /// Output shader attributes used by this kernel. In GLSL are marked as 'out'.
  /// @note they are NOT sorted by their location.
  auto outputVariables() const noexcept { return OutputVariables(m_handle); }

  /// Combined shader attributes used by this kernel (both input and output).
  /// @note they are NOT sorted by their location.
  auto interfaceVariables() const noexcept {
    return InterfaceVariables(m_handle);
  }

  /// Descriptors sets used by this entry point.
  auto sets() const noexcept { return DescriptorSets(m_handle); }

private:
  friend class ReflectInfoIteratorTraitsCommon<ReflectEntryPointTraits>;
  explicit SPIRVEntryPointInfo(const SpvReflectEntryPoint *handle) noexcept
      : m_handle(handle) {}
  const SpvReflectEntryPoint *m_handle;
};

class SPIRVModuleInfo final {
public:
  explicit SPIRVModuleInfo(SPIRVModule const &module) noexcept(
      ExceptionsDisabled)
      : m_info([&]() {
          auto ret = std::make_unique<SpvReflectShaderModule>();
          auto code = module.code();
          auto result = vkw_spvReflectCreateShaderModule(
              sizeof(uint32_t) * code.size(), code.data(), ret.get());
          if (result != SPV_REFLECT_RESULT_SUCCESS)
            postError(
                SPIRVReflectError{result, "vkw_spvReflectCreateShaderModule"});
          return ret.release();
        }()) {}

  using EntryPoints = ReflectInfoRange<ReflectEntryPointTraits>;
  using DescriptorSets = ReflectInfoRange<ReflectDescriptorSetModuleTraits>;
  using PushConstants = ReflectInfoRange<PushConstantTraits>;

  /// List of all entry points defined by that module.
  auto entryPoints() const noexcept { return EntryPoints(m_info.get()); }

  /// Descriptors sets used in the first entry point. If there is no
  /// entry points it shows all descriptors for this module.
  auto sets() const noexcept { return DescriptorSets(m_info.get()); }

  /// List of all push constants defined in this module.
  auto pushConstants() const noexcept { return PushConstants(m_info.get()); }

private:
  struct ModuleDestructor {
    void operator()(SpvReflectShaderModule *module) {
      if (module) {
        vkw_spvReflectDestroyShaderModule(module);
        delete module;
      }
    }
  };
  std::unique_ptr<SpvReflectShaderModule, ModuleDestructor> m_info;
};

template <>
inline SPIRVEntryPointInfo
ReflectInfoIteratorTraitsCommon<ReflectEntryPointTraits>::get_value(
    const SpvReflectShaderModule *s, unsigned idx) noexcept {
  return SPIRVEntryPointInfo{s->entry_points + idx};
}

template <>
inline unsigned
ReflectInfoIteratorTraitsCommon<ReflectEntryPointTraits>::get_size(
    const SpvReflectShaderModule *s) noexcept {
  return s->entry_point_count;
}

template <>
inline SPIRVInterfaceVariable
ReflectInfoIteratorTraitsCommon<ReflectInterfaceVariableTraits>::get_value(
    const SpvReflectEntryPoint *s, unsigned idx) noexcept {
  return SPIRVInterfaceVariable{s->interface_variables + idx};
}

template <>
inline unsigned
ReflectInfoIteratorTraitsCommon<ReflectInterfaceVariableTraits>::get_size(
    const SpvReflectEntryPoint *s) noexcept {
  return s->interface_variable_count;
}

template <>
inline SPIRVInterfaceVariable
ReflectInfoIteratorTraitsCommon<ReflectInputVariableTraits>::get_value(
    const SpvReflectEntryPoint *s, unsigned idx) noexcept {
  return SPIRVInterfaceVariable{s->input_variables[idx]};
}

template <>
inline unsigned
ReflectInfoIteratorTraitsCommon<ReflectInputVariableTraits>::get_size(
    const SpvReflectEntryPoint *s) noexcept {
  return s->input_variable_count;
}

template <>
inline SPIRVInterfaceVariable
ReflectInfoIteratorTraitsCommon<ReflectOutputVariableTraits>::get_value(
    const SpvReflectEntryPoint *s, unsigned idx) noexcept {
  return SPIRVInterfaceVariable{s->output_variables[idx]};
}

template <>
inline unsigned
ReflectInfoIteratorTraitsCommon<ReflectOutputVariableTraits>::get_size(
    const SpvReflectEntryPoint *s) noexcept {
  return s->output_variable_count;
}

template <>
inline SPIRVDescriptorBindingInfo
ReflectInfoIteratorTraitsCommon<ReflectDescriptorBindingTraits>::get_value(
    const SpvReflectDescriptorSet *s, unsigned idx) noexcept {
  return SPIRVDescriptorBindingInfo{s->bindings[idx]};
}

template <>
inline unsigned
ReflectInfoIteratorTraitsCommon<ReflectDescriptorBindingTraits>::get_size(
    const SpvReflectDescriptorSet *s) noexcept {
  return s->binding_count;
}

template <>
inline SPIRVDescriptorSetInfo
ReflectInfoIteratorTraitsCommon<ReflectDescriptorSetTraits>::get_value(
    const SpvReflectEntryPoint *s, unsigned idx) noexcept {
  return SPIRVDescriptorSetInfo{s->descriptor_sets + idx};
}

template <>
inline unsigned
ReflectInfoIteratorTraitsCommon<ReflectDescriptorSetTraits>::get_size(
    const SpvReflectEntryPoint *s) noexcept {
  return s->descriptor_set_count;
}

template <>
inline SPIRVDescriptorSetInfo
ReflectInfoIteratorTraitsCommon<ReflectDescriptorSetModuleTraits>::get_value(
    const SpvReflectShaderModule *s, unsigned idx) noexcept {
  return SPIRVDescriptorSetInfo{s->descriptor_sets + idx};
}

template <>
inline unsigned
ReflectInfoIteratorTraitsCommon<ReflectDescriptorSetModuleTraits>::get_size(
    const SpvReflectShaderModule *s) noexcept {
  return s->descriptor_set_count;
}

template <>
inline SPIRVPushConstantInfo
ReflectInfoIteratorTraitsCommon<PushConstantTraits>::get_value(
    const SpvReflectShaderModule *s, unsigned idx) noexcept {
  return SPIRVPushConstantInfo{s->push_constant_blocks + idx};
}

template <>
inline unsigned ReflectInfoIteratorTraitsCommon<PushConstantTraits>::get_size(
    const SpvReflectShaderModule *s) noexcept {
  return s->push_constant_block_count;
}

} // namespace vkw
#endif // VKWRAPPER_SPIRVMODULE_HPP
