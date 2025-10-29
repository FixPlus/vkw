#ifndef VKRENDERER_INSTANCE_HPP
#define VKRENDERER_INSTANCE_HPP

#include <vkw/Containers.hpp>
#include <vkw/Library.hpp>

#include <functional>
#include <set>
#include <unordered_map>

#undef max
#undef min

namespace vkw {

struct InstanceCreateInfo {
  void requestApiVersion(ApiVersion version) noexcept { apiVersion = version; }
  void requestExtension(ext ext) noexcept(ExceptionsDisabled) {
    requestedExtensions.push_back(ext);
  }

  void requestLayer(layer layer) noexcept(ExceptionsDisabled) {
    requestedLayers.push_back(layer);
  }

  cntr::vector<ext, 4> requestedExtensions;
  cntr::vector<layer, 2> requestedLayers;
  ApiVersion apiVersion = ApiVersion{1, 0, 0};
  std::string_view applicationName = "APITest";
  std::string_view engineName = "APITest";
  ApiVersion applicationVersion = ApiVersion{1, 0, 0};
  ApiVersion engineVersion = ApiVersion{1, 0, 0};
};

class CompiledInstanceCreateInfo final {
public:
  CompiledInstanceCreateInfo(const vkw::Library &library,
                             const InstanceCreateInfo &CI) {
    if (library.instanceAPIVersion() < CI.apiVersion)
      postError(ApiVersionUnsupported(
          "Cannot create instance with requested api version",
          library.instanceAPIVersion(), CI.apiVersion));

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = CI.applicationName.data();
    appInfo.applicationVersion = CI.applicationVersion;
    appInfo.pEngineName = CI.engineName.data();
    appInfo.engineVersion = CI.engineVersion;
    appInfo.apiVersion = CI.apiVersion;

    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Check presence of all required layers and extensions

    std::for_each(CI.requestedLayers.begin(), CI.requestedLayers.end(),
                  [&library](layer id) {
                    if (!library.hasLayer(id))
                      postError(LayerUnsupported{id, Library::LayerName(id)});
                  });

    auto requestedExtensions = CI.requestedExtensions;
    std::for_each(
        CI.requestedExtensions.begin(), CI.requestedExtensions.end(),
        [&library](ext id) {
          if (!library.hasInstanceExtension(id))
            postError(ExtensionUnsupported{id, Library::ExtensionName(id)});
        });

    // Enable VK_KHR_get_physical_device_properties2 if possible.
    if (library.hasInstanceExtension(
            ext::KHR_get_physical_device_properties2) &&
        std::ranges::none_of(requestedExtensions, [](auto &extension) {
          return extension == ext::KHR_get_physical_device_properties2;
        })) {
      requestedExtensions.emplace_back(
          ext::KHR_get_physical_device_properties2);
    }

    std::transform(requestedExtensions.begin(), requestedExtensions.end(),
                   std::back_inserter(reqExtensionsNames),
                   [](auto id) { return Library::ExtensionName(id); });
    std::transform(CI.requestedLayers.begin(), CI.requestedLayers.end(),
                   std::back_inserter(reqLayerNames),
                   [](auto id) { return Library::LayerName(id); });

    createInfo.enabledExtensionCount = reqExtensionsNames.size();
    createInfo.ppEnabledExtensionNames = reqExtensionsNames.data();
    createInfo.enabledLayerCount = reqLayerNames.size();
    createInfo.ppEnabledLayerNames = reqLayerNames.data();
  }

  CompiledInstanceCreateInfo(CompiledInstanceCreateInfo &&) = delete;
  CompiledInstanceCreateInfo &operator=(CompiledInstanceCreateInfo &&) = delete;
  CompiledInstanceCreateInfo(const CompiledInstanceCreateInfo &) = delete;
  CompiledInstanceCreateInfo &
  operator=(const CompiledInstanceCreateInfo &) = delete;
  ~CompiledInstanceCreateInfo() = default;
  const VkInstanceCreateInfo *get() const { return &createInfo; }

private:
  VkApplicationInfo appInfo{};
  cntr::vector<const char *, 5> reqExtensionsNames{};
  cntr::vector<const char *, 5> reqLayerNames{};
  VkInstanceCreateInfo createInfo{};
};

namespace __detail {

class InstanceDestructor final {
public:
  using TypeTraits = VulkanTypeTraits<VkInstance>;

  InstanceDestructor(vkw::Library const &creator) noexcept(ExceptionsDisabled)
      : m_creator(creator){};

  void operator()(VkInstance handle) noexcept {
    auto destroyFn = reinterpret_cast<PFN_vkDestroyInstance>(
        m_creator.get().vkGetInstanceProcAddr(handle, "vkDestroyInstance"));
    if (!destroyFn)
      irrecoverableError(VulkanError{VK_ERROR_UNKNOWN, __FILE__, __LINE__});
    std::invoke(destroyFn, handle, HostAllocator::get());
  }
  static VkInstance create(vkw::Library const &library,
                           const CompiledInstanceCreateInfo
                               &createInfo) noexcept(ExceptionsDisabled) {
    VkInstance ret;
    VK_CHECK_RESULT(std::invoke(library.vkCreateInstance, createInfo.get(),
                                HostAllocator::get(), &ret))
    return ret;
  }

  const vkw::Library &parent() const noexcept { return m_creator.get(); }

private:
  StrongReference<vkw::Library const> m_creator;
};

} // namespace __detail

template <> struct VulkanTypeTraits<VkInstance> {
  using CreatorType = vkw::Library;
  using CreateInfoType = CompiledInstanceCreateInfo;
};

template <> struct VulkanTypeDeleter<VkInstance> {
  using Type = __detail::InstanceDestructor;
};

class Instance : public vk::Instance {
public:
  Instance(Library const &library,
           InstanceCreateInfo const &createInfo) noexcept(ExceptionsDisabled);

  bool isExtensionEnabled(ext extension) const noexcept {
    return m_enabledExtensions.contains(extension);
  }

  bool isLayerEnabled(layer layer) const noexcept {
    return m_enabledLayers.contains(layer);
  }

  auto &apiVersion() const noexcept { return m_apiVer; }

  template <uint32_t major, uint32_t minor>
  InstanceCore<major, minor> const &core() const noexcept(ExceptionsDisabled) {
    constexpr auto requested = ApiVersion{major, minor, 0};
    if (m_apiVer < requested)
      postError(SymbolsMissing{m_apiVer, requested});

    auto *ptr = static_cast<InstanceCore<major, minor> const *>(
        m_coreInstanceSymbols.get());
    return *ptr;
  }

private:
  template <unsigned major = 1, unsigned minor = 0>
  static std::unique_ptr<InstanceCore<1, 0>>
  loadInstanceSymbols(vkw::Library const &library, VkInstance instance,
                      ApiVersion version) noexcept(ExceptionsDisabled) {
    // Here template magic is being used to automatically generate load of
    // every available InstanceCore<major, minor> classes from SymbolTable.inc
    if (version >
        ApiVersion{major, minor, std::numeric_limits<unsigned>::max()}) {
      if constexpr (std::derived_from<InstanceCore<major, minor + 1>,
                                      SymbolTableBase<VkInstance>>)
        return loadInstanceSymbols<major, minor + 1>(library, instance,
                                                     version);
      else if constexpr (std::derived_from<InstanceCore<major + 1, 0>,
                                           SymbolTableBase<VkInstance>>)
        return loadInstanceSymbols<major + 1, 0>(library, instance, version);
      else
        postError(ApiVersionUnsupported(
            "Could not load instance symbols for requested api version",
            ApiVersion{major, minor, 0}, version));
    } else {
      return std::make_unique<InstanceCore<major, minor>>(
          library.vkGetInstanceProcAddr, instance);
    }
  }
  ApiVersion m_apiVer;
  std::unique_ptr<InstanceCore<1, 0>> m_coreInstanceSymbols;
  std::set<ext> m_enabledExtensions;
  std::set<layer> m_enabledLayers;
};

inline Instance::Instance(
    Library const &library,
    InstanceCreateInfo const &createInfo) noexcept(ExceptionsDisabled)
    : vk::Instance(library, CompiledInstanceCreateInfo(library, createInfo)),
      m_apiVer(createInfo.apiVersion),
      m_coreInstanceSymbols(
          loadInstanceSymbols(library, *this, createInfo.apiVersion)) {

  std::ranges::copy(
      createInfo.requestedExtensions,
      std::inserter(m_enabledExtensions, m_enabledExtensions.end()));
  std::ranges::copy(createInfo.requestedLayers,
                    std::inserter(m_enabledLayers, m_enabledLayers.end()));
}

} // namespace vkw
#endif // VKRENDERER_INSTANCE_HPP
