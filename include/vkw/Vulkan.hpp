#ifndef VKWRAPPER_SYMBOLTABLE_HPP
#define VKWRAPPER_SYMBOLTABLE_HPP

#include <concepts>

#include <vulkan/vulkan.h>

#include <memory>
#include <vkw/Exception.hpp>
#include <vkw/ReferenceGuard.hpp>

namespace vkw {

class Instance;
class Device;

template <typename Level>
  requires std::same_as<Level, VkInstance> || std::same_as<Level, VkDevice>
class SymbolTableBase {
public:
  using PFN_getProcAddr = PFN_vkVoidFunction (*)(Level, const char *);

  SymbolTableBase(PFN_getProcAddr getProcAddr, Level level)
      : m_level(level), m_getProcAddr(getProcAddr){};

  virtual ~SymbolTableBase() = default;

protected:
  template <typename FuncType> FuncType getProcAddrT(const char *funcName) {
    return reinterpret_cast<FuncType>(m_getProcAddr(m_level, funcName));
  }

private:
  PFN_getProcAddr m_getProcAddr;
  Level m_level;
};

template <uint32_t major, uint32_t minor> class InstanceCore {};

template <uint32_t major, uint32_t minor> class DeviceCore {};

#define VKW_DUMP_CORE_CLASSES
#include "vkw/SymbolTable.inc"
#undef VKW_DUMP_CORE_CLASSES

enum class ext {
#define VKW_DUMP_EXTENSION_MAP
#define VKW_EXTENSION_ENTRY(X) X,
#include "vkw/SymbolTable.inc"
#undef VKW_EXTENSION_ENTRY
#undef VKW_DUMP_EXTENSION_MAP
};

class ExtensionError : public Error {
public:
  const char *extName() const noexcept { return what(); }

  ext id() const noexcept { return m_id; }

protected:
  ExtensionError(ext id, std::string_view extName) noexcept
      : Error(extName), m_id(id) {}

private:
  ext m_id;
};

class ExtensionMissing final : public ExtensionError {
public:
  ExtensionMissing(ext id, std::string_view extName) noexcept
      : ExtensionError(id, extName) {}
  std::string_view codeString() const noexcept override {
    return "Extension missing";
  }
};

class ExtensionUnsupported : public ExtensionError {
public:
  ExtensionUnsupported(ext id, std::string_view extName) noexcept
      : ExtensionError(id, extName) {}
  std::string_view codeString() const noexcept override {
    return "Extension unsupported";
  }
};

enum class layer {
#define VKW_LAYER_MAP_ENTRY(X) X,
#include "vkw/LayerMap.inc"
#undef VKW_LAYER_MAP_ENTRY
};

class LayerError : public Error {
public:
  const char *layerName() const noexcept { return what(); }

  layer id() const noexcept { return m_id; }

protected:
  LayerError(layer id, std::string_view layerName) noexcept
      : Error(layerName), m_id(id) {}

private:
  layer m_id;
};

class LayerMissing : public LayerError {
public:
  LayerMissing(layer id, std::string_view layerName) noexcept
      : LayerError(id, layerName) {}
  std::string_view codeString() const noexcept override {
    return "Layer missing";
  }
};

class LayerUnsupported : public LayerError {
public:
  LayerUnsupported(layer id, std::string_view layerName) noexcept
      : LayerError(id, layerName) {}
  std::string_view codeString() const noexcept override {
    return "Layer unsupported";
  }
};

class VulkanError final : public PositionalError {
public:
  static inline const char *errorString(VkResult errorCode) noexcept {
    switch (errorCode) {
#define STR(r)                                                                 \
  case VK_##r:                                                                 \
    return #r
      STR(NOT_READY);
      STR(TIMEOUT);
      STR(EVENT_SET);
      STR(EVENT_RESET);
      STR(INCOMPLETE);
      STR(ERROR_OUT_OF_HOST_MEMORY);
      STR(ERROR_OUT_OF_DEVICE_MEMORY);
      STR(ERROR_OUT_OF_POOL_MEMORY);
      STR(ERROR_INITIALIZATION_FAILED);
      STR(ERROR_DEVICE_LOST);
      STR(ERROR_MEMORY_MAP_FAILED);
      STR(ERROR_LAYER_NOT_PRESENT);
      STR(ERROR_EXTENSION_NOT_PRESENT);
      STR(ERROR_FEATURE_NOT_PRESENT);
      STR(ERROR_INCOMPATIBLE_DRIVER);
      STR(ERROR_TOO_MANY_OBJECTS);
      STR(ERROR_FORMAT_NOT_SUPPORTED);
      STR(ERROR_SURFACE_LOST_KHR);
      STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
      STR(SUBOPTIMAL_KHR);
      STR(ERROR_OUT_OF_DATE_KHR);
      STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
      STR(ERROR_VALIDATION_FAILED_EXT);
      STR(ERROR_INVALID_SHADER_NV);
#undef STR
    default:
      return "UNKNOWN_ERROR";
    }
  }

  VulkanError(VkResult error, std::string const &filename,
              uint32_t line) noexcept
      : PositionalError(std::string("Vulkan function call returned VkResult: ")
                            .append(errorString(error)),
                        filename, line),
        m_result(error) {}

  VkResult result() const noexcept { return m_result; }
  std::string_view codeString() const noexcept override {
    return "Vulkan error";
  }

private:
  VkResult m_result;
};
#ifdef VK_CHECK_RESULT
#error "redefinition of VK_CHECK_RESULT"
#endif
#define VK_CHECK_RESULT(...)                                                   \
  {                                                                            \
    VkResult res = (__VA_ARGS__);                                              \
    if (res != VK_SUCCESS)                                                     \
      postError(VulkanError(res, __FILE__, __LINE__));                         \
  }

template <typename T> struct VulkanTypeTraits {};

#define VKW_GENERATE_TYPE_DEFINITIONS
#include "vkw/VulkanTypeTraits.inc"
#undef VKW_GENERATE_TYPE_DEFINITIONS

/**
 * @class UniqueDeleter<T>
 *
 * @brief This template is used by Unique<> to create, destroy and store parent
 * reference of an object T. It uses VulkanTypeTraits<> to fetch constructors
 * and destructors. In some cases explicit specialization is needed (e.g.
 * VkDevice, see vkw/Device.hpp file). In this case, specialization must provide
 * following methods:
 *
 *  // 1. call to object destructor.
 *  void operator()(T handle) noexcept;
 *  // 2. calls to object constructor. Must throw/terminate if unsuccessful.
 *  // Accepts CreatorType and CreateInfoType as defined in VulkanTypeTraits<>
 *  static T create(CreatorType const &, CreateInfoType const&);
 *  // 3. returns reference to a parent that was passed in deleter's
 *  // constructor.
 *  CreatorType const &parent() const noexcept;
 *
 *  Specialization must also provide constructor that accepts single argument
 * that is constant reference to creator object. Reference should be kept as it
 * is used in parent() method.
 *
 * @tparam T is raw vulkan handle(VkInstance, VkDevice, etc.)
 */
template <typename T> class UniqueDeleter {
public:
  using TypeTraits = VulkanTypeTraits<T>;

  UniqueDeleter(typename TypeTraits::CreatorType const &creator) noexcept(
      ExceptionsDisabled)
      : m_creator(creator){};

  void operator()(T handle) noexcept {
    std::invoke(TypeTraits::getDestructor(m_creator.get()), m_creator.get(),
                handle, HostAllocator::get());
  }

  static T create(typename TypeTraits::CreatorType const &creator,
                  typename TypeTraits::CreateInfoType const
                      &createInfo) noexcept(ExceptionsDisabled) {
    T ret;
    VK_CHECK_RESULT(std::invoke(TypeTraits::getConstructor(creator), creator,
                                &createInfo, HostAllocator::get(), &ret))
    return ret;
  }

  typename TypeTraits::CreatorType const &parent() const noexcept {
    return m_creator.get();
  }

private:
  StrongReference<typename TypeTraits::CreatorType const> m_creator;
};

template <typename T> struct VulkanTypeDeleter {
  using Type = UniqueDeleter<T>;
};

/**
 * @class Unique
 *
 * @brief Base class template for many vulkan created objects. Some objects
 * have their VulkanTypeTraits<> class specialization auto-generated and
 * that's why can use main template. But there are exceptions like VkDevice
 * when it is needed to define custom Deleter object.
 *
 * @tparam T is raw vulkan handle(VkInstance, VkDevice, etc.)
 */
template <typename T>
class Unique : private std::unique_ptr<std::remove_pointer_t<T>,
                                       typename VulkanTypeDeleter<T>::Type>,
               public ReferenceGuard {
public:
  using Deleter = typename VulkanTypeDeleter<T>::Type;
  using Base = std::unique_ptr<std::remove_pointer_t<T>, Deleter>;
  using TypeTraits = VulkanTypeTraits<T>;

  Unique(typename TypeTraits::CreatorType const &creator,
         typename TypeTraits::CreateInfoType const
             &createInfo) noexcept(ExceptionsDisabled)
      : std::unique_ptr<std::remove_pointer_t<T>, Deleter>(
            Deleter::create(creator, createInfo), Deleter(creator)) {}

  operator T() const noexcept { return Base::get(); }

  auto &parent() const noexcept { return Base::get_deleter().parent(); }

protected:
  auto handle() const noexcept { return Base::get(); }
};

#define VKW_GENERATE_ALIAS_TYPES
#include "vkw/VulkanTypeTraits.inc"
#undef VKW_GENERATE_ALIAS_TYPES

// Special cases, not generated.
namespace vk {
using Instance = Unique<VkInstance>;
using Device = Unique<VkDevice>;
} // namespace vk
} // namespace vkw
#endif // VKWRAPPER_SYMBOLTABLE_HPP
