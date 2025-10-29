#ifndef VKWRAPPER_LIBRARY_HPP
#define VKWRAPPER_LIBRARY_HPP

#include <vkw/Containers.hpp>
#include <vkw/HostAllocator.hpp>
#include <vkw/Runtime.h>
#include <vkw/Vulkan.hpp>

#include <algorithm>
#include <cassert>
#include <memory>
#include <sstream>

namespace vkw {

#include <vkw/LibraryVersion.inc>

struct ApiVersion {
  uint32_t major;
  uint32_t minor;
  uint32_t revision;

  constexpr ApiVersion(uint32_t maj, uint32_t min, uint32_t rev)
      : major(maj), minor(min), revision(rev) {}
  constexpr ApiVersion(uint32_t encoded)
      : major(VK_API_VERSION_MAJOR(encoded)),
        minor(VK_API_VERSION_MINOR(encoded)),
        revision(VK_API_VERSION_PATCH(encoded)) {}
  constexpr ApiVersion() = default;

  operator std::string() const;

  constexpr operator uint32_t() const {
    return VK_MAKE_API_VERSION(0, major, minor, revision);
  }
  constexpr auto operator<=>(ApiVersion const &another) const = default;
};

inline std::ostream &operator<<(std::ostream &os, const ApiVersion &ver) {
  os << ver.major << "." << ver.minor << "." << ver.revision;
  return os;
}

inline ApiVersion::operator std::string() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

class ApiVersionUnsupported final : public Error {
public:
  enum class CompatibilityFactor { SEM_VERSION, EXACT_VERSION };
  ApiVersionUnsupported(std::string_view details, ApiVersion lastSupported,
                        ApiVersion unsupported,
                        CompatibilityFactor compatibility =
                            CompatibilityFactor::SEM_VERSION) noexcept
      : Error([&]() {
          std::stringstream msg;
          msg << details << ": " << unsupported << " is unsupported ("
              << (compatibility == CompatibilityFactor::EXACT_VERSION
                      ? "supported: =="
                      : "supported: >=")
              << lastSupported << ")";
          return msg.str();
        }()),
        m_unsupported(unsupported), m_lastSupported(lastSupported),
        m_compatibility(compatibility) {}

  auto &supported() const noexcept { return m_lastSupported; }
  auto &unsupported() const noexcept { return m_unsupported; }
  auto compatibility() const noexcept { return m_compatibility; }
  std::string_view codeString() const noexcept override {
    return "API version unsupported";
  }

private:
  ApiVersion m_unsupported;
  ApiVersion m_lastSupported;
  CompatibilityFactor m_compatibility;
};

class SymbolsMissing final : public Error {
public:
  SymbolsMissing(ApiVersion loaded, ApiVersion requested)
      : Error([&]() {
          std::stringstream ss;
          ss << "vulkan symbols for " << requested
             << " version are unavailable. Most recent version loaded: "
             << loaded;
          return ss.str();
        }()) {}
  std::string_view codeString() const noexcept override {
    return "Symbols missing";
  }
};

class VulkanLibraryLoader {
public:
  virtual PFN_vkGetInstanceProcAddr getInstanceProcAddr() = 0;

  virtual ~VulkanLibraryLoader() = default;
};

class VulkanLoadError : public Error {
public:
  VulkanLoadError(std::string_view details) noexcept : Error(details){};

  std::string_view codeString() const noexcept override {
    return "Vulkan load error";
  }
};

/// TODO: remove this copy-paste
class ExtensionNameError : public Error {
public:
  ExtensionNameError(std::string_view name) : Error(name) {}

  const char *name() const { return what(); }
  std::string_view codeString() const noexcept override {
    return "Bad extension name";
  }
};

class LayerNameError : public Error {
public:
  LayerNameError(std::string_view name) : Error(name) {}

  const char *name() const { return what(); }
  std::string_view codeString() const noexcept override {
    return "Bad layer name";
  }
};

namespace __detail {
class VulkanDefaultLoader final : public VulkanLibraryLoader {
public:
  VulkanDefaultLoader()
      : m_handle([]() {
          VKW_RTLoader handle = nullptr;
          auto result = vkw_loadVulkan(&handle);
          if (!handle || result != VKW_OK)
            postError(VulkanLoadError{vkw_lastError()});
          return handle;
        }()) {}
  PFN_vkGetInstanceProcAddr getInstanceProcAddr() override {
    return vkw_vkGetInstanceProcAddr(m_handle.get());
  }

private:
  struct Closer {
    void operator()(VKW_RTLoader handle) { vkw_closeVulkan(handle); };
  };
  std::unique_ptr<VKW_RTLoader_T, Closer> m_handle;
};
} // namespace __detail

class Library final : public ReferenceGuard {
public:
  /**
   *    @param loader
   *    Application may create their own vulkan loader
   *    using interface VulkanLibraryLoader.
   *    Pass nullptr to use embedded loader.
   *
   * */
  Library(std::unique_ptr<VulkanLibraryLoader> loader = nullptr) noexcept(
      ExceptionsDisabled)
      : m_loader(loader ? std::move(loader)
                        : std::make_unique<__detail::VulkanDefaultLoader>()) {
    auto rtVersion = runtimeVersion();
    auto hrVersion = headerVersion();
    if (hrVersion.major != rtVersion.major ||
        hrVersion.minor > rtVersion.minor) {
      postError(ApiVersionUnsupported{
          "vkw runtime version mismatch", rtVersion, hrVersion,
          ApiVersionUnsupported::CompatibilityFactor::SEM_VERSION});
    }

    vkGetInstanceProcAddr = m_loader->getInstanceProcAddr();
    if (!vkGetInstanceProcAddr)
      postError(VulkanLoadError{"vkGetInstanceProcAddr is null"});
#ifdef VKW_GET_SYMBOL
#error "VKW_GET_SYMBOL must not be defined here"
#endif
#define VKW_GET_SYMBOL(X)                                                      \
  X = reinterpret_cast<decltype(X)>(vkGetInstanceProcAddr(nullptr, #X));       \
  if (!X)                                                                      \
    postError(VulkanLoadError{#X " symbol not found"});

    VKW_GET_SYMBOL(vkCreateInstance)
    VKW_GET_SYMBOL(vkEnumerateInstanceExtensionProperties)
    VKW_GET_SYMBOL(vkEnumerateInstanceLayerProperties)
    VKW_GET_SYMBOL(vkEnumerateInstanceVersion)
#undef VKW_GET_SYMBOL
    uint32_t layerCount;

    // enumerate all instance layers

    VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
    m_layer_properties.resize(layerCount);
    VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(
        &layerCount, m_layer_properties.data()));

    // enumerate all instance-level extensions

    uint32_t extensionCount;

    VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(
        nullptr, &extensionCount, nullptr));
    m_instance_extension_properties.resize(extensionCount);
    VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(
        nullptr, &extensionCount, m_instance_extension_properties.data()));

    uint32_t extensionAccumulated = extensionCount;

    for (auto &layer : m_layer_properties) {
      VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(
          layer.layerName, &extensionCount, nullptr));
      m_instance_extension_properties.resize(extensionAccumulated +
                                             extensionCount);
      VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(
          layer.layerName, &extensionCount,
          m_instance_extension_properties.data() + extensionAccumulated));
      extensionAccumulated += extensionCount;
    }
  }

  bool hasLayer(layer layerId) const noexcept {
    std::string_view layerName = LayerName(layerId);
    return std::any_of(m_layer_properties.begin(), m_layer_properties.end(),
                       [&layerName](VkLayerProperties const &layer) {
                         return !layerName.compare(layer.layerName);
                       });
  }

  VkLayerProperties layerProperties(layer layerId) const
      noexcept(ExceptionsDisabled) {
    std::string_view layerName = LayerName(layerId);
    auto found =
        std::find_if(m_layer_properties.begin(), m_layer_properties.end(),
                     [&layerName](VkLayerProperties const &layer) {
                       return !layerName.compare(layer.layerName);
                     });

    if (found == m_layer_properties.end())
      postError(vkw::LayerMissing(layerId, layerName));

    return *found;
  }

  bool hasInstanceExtension(ext extensionId) const noexcept {

    std::string_view name = ExtensionName(extensionId);

    return std::any_of(m_instance_extension_properties.begin(),
                       m_instance_extension_properties.end(),
                       [&name](VkExtensionProperties const &layer) {
                         return !name.compare(layer.extensionName);
                       });
  }
  VkExtensionProperties instanceExtensionProperties(ext extensionId) const
      noexcept(ExceptionsDisabled) {
    std::string_view name = ExtensionName(extensionId);

    auto found = std::find_if(m_instance_extension_properties.begin(),
                              m_instance_extension_properties.end(),
                              [&name](VkExtensionProperties const &layer) {
                                return !name.compare(layer.extensionName);
                              });

    if (found == m_instance_extension_properties.end())
      postError(vkw::ExtensionMissing(extensionId, name));

    return *found;
  }

  PFN_vkCreateInstance vkCreateInstance;
  PFN_vkEnumerateInstanceExtensionProperties
      vkEnumerateInstanceExtensionProperties;
  PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;

  ApiVersion instanceAPIVersion() const noexcept {
    if (this->vkEnumerateInstanceVersion == nullptr)
      return {1, 0, 0};

    uint32_t encoded;
    this->vkEnumerateInstanceVersion(&encoded);
    return ApiVersion{encoded};
  }

  static const char *ExtensionName(ext id) noexcept {
    auto index = static_cast<unsigned>(id);
    assert(index < sizeof(ExtensionNames) / sizeof(const char *));

    return ExtensionNames[static_cast<unsigned>(id)];
  }

  static ext
  ExtensionId(std::string_view extensionName) noexcept(ExceptionsDisabled) {
    auto *begin = ExtensionNames;
    auto *end =
        ExtensionNames + (sizeof(ExtensionNames) / sizeof(const char *));
    auto *found = std::find_if(begin, end, [extensionName](const char *entry) {
      return !extensionName.compare(entry);
    });
    if (found == end)
      postError(ExtensionNameError(extensionName));

    return static_cast<ext>(found - begin);
  }

  static bool ValidExtensionName(std::string_view extensionName) noexcept {
    auto *begin = ExtensionNames;
    auto *end =
        ExtensionNames + (sizeof(ExtensionNames) / sizeof(const char *));
    auto *found = std::find_if(begin, end, [extensionName](const char *entry) {
      return !extensionName.compare(entry);
    });

    return found != end;
  }

  static const char *LayerName(layer id) noexcept {
    auto index = static_cast<unsigned>(id);
    assert(index < sizeof(LayerNames) / sizeof(const char *));
    return LayerNames[static_cast<unsigned>(id)];
  }

  static layer
  LayerId(std::string_view layerName) noexcept(ExceptionsDisabled) {
    auto *begin = LayerNames;
    auto *end = LayerNames + (sizeof(LayerNames) / sizeof(const char *));
    auto *found = std::find_if(begin, end, [layerName](const char *entry) {
      return !layerName.compare(entry);
    });
    if (found == end)
      postError(LayerNameError(layerName));

    return static_cast<layer>(found - begin);
  }

  static bool ValidLayerName(std::string_view layerName) noexcept {
    auto *begin = LayerNames;
    auto *end = LayerNames + (sizeof(LayerNames) / sizeof(const char *));
    auto *found = std::find_if(begin, end, [layerName](const char *entry) {
      return !layerName.compare(entry);
    });

    return found != end;
  }

  static ApiVersion runtimeVersion() noexcept {
    ApiVersion ret{};
    vkw_getRuntimeVersion(&ret.major, &ret.minor, &ret.revision);
    return ret;
  }

  static ApiVersion headerVersion() noexcept {
    return ApiVersion(MajorVersion, MinorVersion, RevVersion);
  }

  auto layers() const {
    return std::ranges::subrange(m_layer_properties.begin(),
                                 m_layer_properties.end());
  }

  auto extensions() const {
    return std::ranges::subrange(m_instance_extension_properties.begin(),
                                 m_instance_extension_properties.end());
  }

private:
  static constexpr inline const char *ExtensionNames[] = {
#define STRINGIFY(X) #X
#define VKW_DUMP_EXTENSION_MAP
#define VKW_EXTENSION_ENTRY(X) STRINGIFY(VK_##X),
#include "vkw/SymbolTable.inc"
#undef VKW_EXTENSION_ENTRY
#undef VKW_DUMP_EXTENSION_MAP
  };

  static constexpr inline const char *LayerNames[] = {
#define VKW_LAYER_MAP_ENTRY(X) STRINGIFY(VK_LAYER_##X),
#include "vkw/LayerMap.inc"
#undef VKW_EXTENSION_ENTRY
  };

  std::unique_ptr<VulkanLibraryLoader> m_loader;
  cntr::vector<VkLayerProperties, 10> m_layer_properties;
  cntr::vector<VkExtensionProperties, 10> m_instance_extension_properties;
};
} // namespace vkw
#endif // VKWRAPPER_LIBRARY_HPP
