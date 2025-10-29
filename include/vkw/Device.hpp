#ifndef VKRENDERER_DEVICE_HPP
#define VKRENDERER_DEVICE_HPP

#include <vkw/Containers.hpp>
#include <vkw/PhysicalDevice.hpp>

#include <cstdint>

namespace vkw {

template <> struct VulkanTypeTraits<VkDevice> {
  using CreatorType = vkw::Instance;
  using CreateInfoType = std::pair<VkPhysicalDevice, VkDeviceCreateInfo>;
  static PFN_vkCreateDevice getConstructor(vkw::Instance const &creator);
  static PFN_vkDestroyDevice getDestructor(vkw::Instance const &creator);
};

inline PFN_vkCreateDevice
VulkanTypeTraits<VkDevice>::getConstructor(vkw::Instance const &creator) {
  return creator.core<1, 0>().vkCreateDevice;
}
inline PFN_vkDestroyDevice
VulkanTypeTraits<VkDevice>::getDestructor(vkw::Instance const &creator) {
  return reinterpret_cast<PFN_vkDestroyDevice>(
      creator.parent().vkGetInstanceProcAddr(creator, "vkDestroyDevice"));
}

namespace __detail {
class DeviceDestructor {
public:
  using TypeTraits = VulkanTypeTraits<VkDevice>;

  DeviceDestructor(Instance const &creator) noexcept(ExceptionsDisabled)
      : m_creator(creator){};

  void operator()(VkDevice handle) noexcept {
    std::invoke(TypeTraits::getDestructor(m_creator.get()), handle,
                HostAllocator::get());
  }
  static VkDevice create(vkw::Instance const &instance,
                         std::pair<VkPhysicalDevice, VkDeviceCreateInfo> const
                             &createInfo) noexcept(ExceptionsDisabled) {
    VkDevice ret;
    VK_CHECK_RESULT(std::invoke(TypeTraits::getConstructor(instance),
                                createInfo.first, &createInfo.second,
                                HostAllocator::get(), &ret))
    return ret;
  }

  const Instance &parent() const noexcept { return m_creator.get(); }

private:
  StrongReference<Instance const> m_creator;
};

} // namespace __detail

template <> struct VulkanTypeDeleter<VkDevice> {
  using Type = __detail::DeviceDestructor;
};

class DeviceInfo {
public:
  DeviceInfo(Instance const &parent,
             PhysicalDevice phDevice) noexcept(ExceptionsDisabled)
      : m_ph_device(std::move(phDevice)) {

    auto const &queueFamilies = m_ph_device.queueFamilies();
    m_queueCreateInfo.reserve(queueFamilies.size());

    std::for_each(
        queueFamilies.begin(), queueFamilies.end(), [this](auto const &family) {
          if (!family.hasRequestedQueues())
            return;

          VkDeviceQueueCreateInfo queueInfo{};
          queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
          queueInfo.queueFamilyIndex = family.index();
          queueInfo.queueCount = family.queueRequestedCount();
          queueInfo.pQueuePriorities = family.queuePrioritiesRaw();
          m_queueCreateInfo.push_back(queueInfo);
        });

    m_createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    m_createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(m_queueCreateInfo.size());
    m_createInfo.pQueueCreateInfos = m_queueCreateInfo.data();
    m_createInfo.pEnabledFeatures = &m_ph_device.enabledFeatures();

    // Add memory VK_EXT_memory_budget if possible.
    if (parent.isExtensionEnabled(ext::KHR_get_physical_device_properties2) &&
        m_ph_device.extensionSupported(ext::EXT_memory_budget) &&
        std::ranges::none_of(m_ph_device.enabledExtensions(),
                             [](auto &extension) {
                               return extension == ext::EXT_memory_budget;
                             })) {
      m_ph_device.enableExtension(ext::EXT_memory_budget);
    }

    m_enabledExtensionsRaw.reserve(m_ph_device.enabledExtensions().size());
    std::transform(
        m_ph_device.enabledExtensions().begin(),
        m_ph_device.enabledExtensions().end(),
        std::back_inserter(m_enabledExtensionsRaw),
        [](ext extension) { return Library::ExtensionName(extension); });

    m_createInfo.enabledExtensionCount = m_enabledExtensionsRaw.size();
    m_createInfo.ppEnabledExtensionNames = m_enabledExtensionsRaw.data();
#ifdef VK_VERSION_1_2
    if (phDevice.requestedApiVersion() >= ApiVersion(1, 1, 0))
      m_createInfo.pNext = &m_ph_device.enabledVulkan11Features();
    else
      m_createInfo.pNext = nullptr;
#endif

    m_apiVer = m_ph_device.requestedApiVersion();
  }

  auto &info() const noexcept { return m_createInfo; }

  PhysicalDevice const &physicalDevice() const noexcept { return m_ph_device; }

  auto &apiVersion() const noexcept { return m_apiVer; }

private:
  VkDeviceCreateInfo m_createInfo{};
  cntr::vector<VkDeviceQueueCreateInfo, 2> m_queueCreateInfo;
  cntr::vector<const char *, 4> m_enabledExtensionsRaw;

  PhysicalDevice m_ph_device;
  ApiVersion m_apiVer;
};

class Device : public DeviceInfo, public vk::Device {
public:
  Device(Instance const &parent,
         PhysicalDevice phDevice) noexcept(ExceptionsDisabled);

  template <uint32_t major, uint32_t minor>
  DeviceCore<major, minor> const &core() const noexcept(ExceptionsDisabled) {
    constexpr auto requested = ApiVersion{major, minor, 0};
    if (apiVersion() < requested)
      postError(SymbolsMissing{apiVersion(), requested});
    return *static_cast<DeviceCore<major, minor> const *>(
        m_coreDeviceSymbols.get());
  }

  void waitIdle() noexcept(ExceptionsDisabled) {
    VK_CHECK_RESULT(core<1, 0>().vkDeviceWaitIdle(handle()))
  }

private:
  template <unsigned major = 1, unsigned minor = 0>
  static std::unique_ptr<DeviceCore<1, 0>>
  loadDeviceSymbols(vkw::Instance const &instance, VkDevice device,
                    ApiVersion version) noexcept(ExceptionsDisabled) {
    // Here template magic is being used to automatically generate load of
    // every available DeviceCore<major, minor> classes from SymbolTable.inc
    if (version >
        ApiVersion{major, minor, std::numeric_limits<unsigned>::max()}) {
      if constexpr (std::derived_from<DeviceCore<major, minor + 1>,
                                      SymbolTableBase<VkDevice>>)
        return loadDeviceSymbols<major, minor + 1>(instance, device, version);
      else if constexpr (std::derived_from<DeviceCore<major + 1, 0>,
                                           SymbolTableBase<VkDevice>>)
        return loadDeviceSymbols<major + 1, 0>(instance, device, version);
      else
        postError(ApiVersionUnsupported(
            "Could not load device symbols for requested api version",
            ApiVersion{major, minor, 0}, version));
    } else {
      return std::make_unique<DeviceCore<major, minor>>(
          instance.core<1, 0>().vkGetDeviceProcAddr, device);
    }
  }

  std::unique_ptr<DeviceCore<1, 0>> m_coreDeviceSymbols;
};

inline Device::Device(Instance const &instance,
                      PhysicalDevice phDevice) noexcept(ExceptionsDisabled)
    : DeviceInfo(instance, std::move(phDevice)),
      vk::Device(instance, std::pair<VkPhysicalDevice, VkDeviceCreateInfo>(
                               physicalDevice(), info())),
      m_coreDeviceSymbols(loadDeviceSymbols(
          parent(), handle(), physicalDevice().requestedApiVersion())) {}

#define VKW_GENERATE_TYPE_FUNC_IMPL
#include "vkw/VulkanTypeTraits.inc"
#undef VKW_GENERATE_TYPE_FUNC_IMPL

} // namespace vkw
#endif // VKRENDERER_DEVICE_HPP
