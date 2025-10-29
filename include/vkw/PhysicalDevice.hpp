#ifndef VKWRAPPER_PHYSICALDEVICE_HPP
#define VKWRAPPER_PHYSICALDEVICE_HPP

#include <vkw/Containers.hpp>
#include <vkw/Instance.hpp>

#include <ranges>

namespace vkw {

class QueueFamily {
public:
  class NotEnoughQueues final : public Error {
  public:
    NotEnoughQueues(unsigned index, unsigned requested, unsigned max)
        : Error([&]() {
            std::stringstream ss;
            ss << "Requested for " << requested
               << " queues in queue family index " << index
               << ", when it only supports at max " << max;
            return ss.str();
          }()) {}

    std::string_view codeString() const noexcept override {
      return "Not enough device queues";
    }
  };
  enum { GRAPHICS = 0x1, TRANSFER = 0x2, COMPUTE = 0x4 };
  struct Type {
    unsigned value : 3;
    Type(unsigned val) noexcept : value(val) {}
  };

  QueueFamily(VkQueueFamilyProperties family, unsigned index) noexcept
      : m_family(family), m_index(index) {}

  bool strictly(Type type) const noexcept {
    VkQueueFlags flags = 0;
    if (type.value & GRAPHICS)
      flags |= VK_QUEUE_GRAPHICS_BIT;
    if (type.value & TRANSFER)
      flags |= VK_QUEUE_TRANSFER_BIT;
    if (type.value & COMPUTE)
      flags |= VK_QUEUE_COMPUTE_BIT;

    auto result = flags ^ m_family.queueFlags;
    return !(result & VK_QUEUE_GRAPHICS_BIT) &&
           !(result & VK_QUEUE_TRANSFER_BIT) &&
           !(result & VK_QUEUE_COMPUTE_BIT);
  }
  bool graphics() const noexcept {
    return m_family.queueFlags & VK_QUEUE_GRAPHICS_BIT;
  }

  bool transfer() const noexcept {
    return m_family.queueFlags & VK_QUEUE_TRANSFER_BIT;
  }

  bool compute() const noexcept {
    return m_family.queueFlags & VK_QUEUE_COMPUTE_BIT;
  }

  unsigned queueCount() const noexcept { return m_family.queueCount; }
  unsigned queueRequestedCount() const noexcept {
    return m_queuesRequested.size();
  }
  float queuePriority(unsigned id) const noexcept(ExceptionsDisabled) {
    return m_queuesRequested.at(id);
  }

  bool hasRequestedQueues() const noexcept {
    return !m_queuesRequested.empty();
  }
  float const *queuePrioritiesRaw() const noexcept {
    return m_queuesRequested.data();
  }

  unsigned index() const noexcept { return m_index; }

  void requestQueue(float priority = 0.0f) noexcept(ExceptionsDisabled) {
    if (m_queuesRequested.size() == m_family.queueCount)
      postError(NotEnoughQueues{m_index, m_family.queueCount + 1,
                                m_family.queueCount});
    m_queuesRequested.emplace_back(priority);
  }

private:
  cntr::vector<float, 4> m_queuesRequested;
  unsigned m_index;
  VkQueueFamilyProperties m_family;
};

class PhysicalDevice final {
public:
  static cntr::vector<PhysicalDevice, 2>
  enumerate(const Instance &instance) noexcept(ExceptionsDisabled) {
    cntr::vector<VkPhysicalDevice, 2> devs;
    uint32_t deviceCount = 0;
    instance.core<1, 0>().vkEnumeratePhysicalDevices(instance, &deviceCount,
                                                     nullptr);
    if (deviceCount == 0)
      return {};

    devs.resize(deviceCount);

    instance.core<1, 0>().vkEnumeratePhysicalDevices(instance, &deviceCount,
                                                     devs.data());

    cntr::vector<PhysicalDevice, 2> ret;
    ret.reserve(devs.size());

    std::transform(devs.begin(), devs.end(), std::back_inserter(ret),
                   [&instance](VkPhysicalDevice device) {
                     return PhysicalDevice{instance, device};
                   });
    return ret;
  }

  enum class feature {
#define VKW_DUMP_FEATURES
#define VKW_FEATURE_ENTRY(X) X,
#include "vkw/DeviceFeatures.inc"
#undef VKW_FEATURE_ENTRY
#undef VKW_DUMP_FEATURES
  };
  enum class feature_v11 {
#define VKW_DUMP_VULKAN11_FEATURES
#define VKW_FEATURE_ENTRY(X) X,
#include "vkw/DeviceFeatures.inc"
#undef VKW_FEATURE_ENTRY
#undef VKW_DUMP_VULKAN11_FEATURES
  };
private:
  static const char *m_featureNameMap(PhysicalDevice::feature feature) {
    switch (feature) {
#define VKW_DUMP_FEATURES
#define VKW_FEATURE_ENTRY(X)                                                   \
  case PhysicalDevice::feature::X:                                             \
    return #X;
#include "vkw/DeviceFeatures.inc"
#undef VKW_FEATURE_ENTRY
#undef VKW_DUMP_FEATURES
    default:
      assert(0 && "feature value not in list");
      return nullptr;
    }
  }

#ifdef VK_VERSION_1_2
  static const char *m_featureNameMap(PhysicalDevice::feature_v11 feature) {
    switch (feature) {
#define VKW_DUMP_VULKAN11_FEATURES
#define VKW_FEATURE_ENTRY(X)                                                   \
  case PhysicalDevice::feature_v11::X:                                         \
    return #X;
#include "vkw/DeviceFeatures.inc"
#undef VKW_FEATURE_ENTRY
#undef VKW_DUMP_VULKAN11_FEATURES
    default:
      assert(0 && "feature value not in list");
      return nullptr;
    }
  }
#endif

public:
  template <typename T> class FeatureUnsupported : public Error {
  public:
    FeatureUnsupported(T feature, std::string_view featureName) noexcept
        : Error(featureName), m_feature(feature) {}
    T feature() const noexcept { return m_feature; }
    std::string_view codeString() const noexcept override {
      return "Device feature unsupported";
    }

  private:
    T m_feature;
  };

  operator VkPhysicalDevice() const noexcept { return m_physicalDevice; }

  VkPhysicalDeviceProperties const &properties() const noexcept {
    return m_properties;
  }

  VkPhysicalDeviceMemoryProperties const &memoryProperties() const noexcept {
    return m_memoryProperties;
  }

  VkPhysicalDeviceFeatures const &supportedFeatures() const noexcept {
    return m_features;
  }

  VkPhysicalDeviceFeatures const &enabledFeatures() const noexcept {
    return m_enabledFeatures;
  }
#ifdef VK_VERSION_1_2
  VkPhysicalDeviceVulkan11Features const &
  enabledVulkan11Features() const noexcept {
    return m_enabledVulkan11Features;
  }
#endif

  auto supportedExtensions() const noexcept {
    return std::ranges::subrange(m_supportedExtensions.begin(),
                                 m_supportedExtensions.end());
  }

  auto enabledExtensions() const noexcept {
    return std::ranges::subrange(m_enabledExtensions.begin(),
                                 m_enabledExtensions.end());
  }

  bool isExtensionEnabled(vkw::ext id) const noexcept {
    return std::ranges::count(enabledExtensions(), id);
  }

  ApiVersion supportedApiVersion() const noexcept {
    return m_properties.apiVersion;
  }

  void requestApiVersion(ApiVersion version) noexcept(ExceptionsDisabled) {
    if (version > supportedApiVersion())
      postError(
          ApiVersionUnsupported("Cannot create device with requested version",
                                supportedApiVersion(), version));
    m_requestedApiVersion = version;
  }

  ApiVersion requestedApiVersion() const noexcept {
    return m_requestedApiVersion;
  }

  bool isFeatureSupported(feature feature) const noexcept {
    switch (feature) {
#define VKW_DUMP_FEATURES
#define VKW_FEATURE_ENTRY(X)                                                   \
  case PhysicalDevice::feature::X:                                             \
    return m_features.X;
#include "vkw/DeviceFeatures.inc"
#undef VKW_FEATURE_ENTRY
#undef VKW_DUMP_FEATURES
    default:
      assert(0 && "feature value not in list");
      return false;
    }
  }
  void enableFeature(feature feature) noexcept(ExceptionsDisabled) {
    if (!isFeatureSupported(feature))
      postError(FeatureUnsupported(feature, m_featureNameMap(feature)));

    switch (feature) {
#define VKW_DUMP_FEATURES
#define VKW_FEATURE_ENTRY(X)                                                   \
  case PhysicalDevice::feature::X:                                             \
    m_enabledFeatures.X = true;                                                \
    break;
#include "vkw/DeviceFeatures.inc"
#undef VKW_FEATURE_ENTRY
#undef VKW_DUMP_FEATURES
    default:
      assert(0 && "feature value not in list");
    };
  }
#ifdef VK_VERSION_1_2
  bool isFeatureSupported(feature_v11 feature) const noexcept {
    switch (feature) {
#define VKW_DUMP_VULKAN11_FEATURES
#define VKW_FEATURE_ENTRY(X)                                                   \
  case PhysicalDevice::feature_v11::X:                                         \
    return m_vulkan11Features.X;
#include "vkw/DeviceFeatures.inc"
#undef VKW_FEATURE_ENTRY
#undef VKW_DUMP_VULKAN11_FEATURES
    default:
      assert(0 && "feature value not in list");
      return false;
    }
  }
  void enableFeature(feature_v11 feature) noexcept(ExceptionsDisabled) {
    if (!isFeatureSupported(feature))
      postError(FeatureUnsupported(feature, m_featureNameMap(feature)));

    switch (feature) {
#define VKW_DUMP_VULKAN11_FEATURES
#define VKW_FEATURE_ENTRY(X)                                                   \
  case PhysicalDevice::feature_v11::X:                                         \
    m_enabledVulkan11Features.X = true;                                        \
    break;
#include "vkw/DeviceFeatures.inc"
#undef VKW_FEATURE_ENTRY
#undef VKW_DUMP_VULKAN11_FEATURES
    default:
      assert(0 && "feature value not in list");
    };
  }
#endif

  bool extensionSupported(ext extension) const noexcept {
    return std::find(m_supportedExtensions.begin(), m_supportedExtensions.end(),
                     extension) != m_supportedExtensions.end();
  }

  void enableExtension(ext extension) noexcept(ExceptionsDisabled) {
    if (!extensionSupported(extension))
      postError(
          ExtensionUnsupported(extension, Library::ExtensionName(extension)));

    if (std::find(m_enabledExtensions.begin(), m_enabledExtensions.end(),
                  extension) != m_enabledExtensions.end())
      return;

    m_enabledExtensions.emplace_back(extension);
  }

  auto queueFamilies() noexcept {
    return std::ranges::subrange(m_queueFamilyProperties.begin(),
                                 m_queueFamilyProperties.end());
  }

  auto queueFamilies() const noexcept {
    return std::ranges::subrange(m_queueFamilyProperties.begin(),
                                 m_queueFamilyProperties.end());
  }

private:
  PhysicalDevice(Instance const &instance,
                 VkPhysicalDevice device) noexcept(ExceptionsDisabled)
      : m_physicalDevice(device) {

    // Store Properties features, limits and properties of the physical device
    // for later use Device properties also contain limits and sparse properties
    instance.core<1, 0>().vkGetPhysicalDeviceProperties(m_physicalDevice,
                                                        &m_properties);
    // Features should be checked by the examples before using them
    instance.core<1, 0>().vkGetPhysicalDeviceFeatures(m_physicalDevice,
                                                      &m_features);
#ifdef VK_VERSION_1_2
    if (instance.apiVersion() >= ApiVersion(1, 1, 0)) {
      m_vulkan11Features.pNext = nullptr;
      m_vulkan11Features.sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
      m_enabledVulkan11Features.pNext = nullptr;
      m_enabledVulkan11Features.sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

      VkPhysicalDeviceFeatures2 feats{};
      feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
      feats.pNext = &m_vulkan11Features;
      instance.core<1, 1>().vkGetPhysicalDeviceFeatures2(m_physicalDevice,
                                                         &feats);
    }
#endif
    // Memory properties are used regularly for creating all kinds of buffers
    instance.core<1, 0>().vkGetPhysicalDeviceMemoryProperties(
        m_physicalDevice, &m_memoryProperties);
    // Queue family properties, used for setting up requested queues upon device
    // creation
    uint32_t queueFamilyCount;
    instance.core<1, 0>().vkGetPhysicalDeviceQueueFamilyProperties(
        m_physicalDevice, &queueFamilyCount, nullptr);

    cntr::vector<VkQueueFamilyProperties, 4> rawQueueProps;
    rawQueueProps.resize(queueFamilyCount);
    instance.core<1, 0>().vkGetPhysicalDeviceQueueFamilyProperties(
        m_physicalDevice, &queueFamilyCount, rawQueueProps.data());
    unsigned indexAcc = 0;
    std::transform(rawQueueProps.begin(), rawQueueProps.end(),
                   std::back_inserter(m_queueFamilyProperties),
                   [&indexAcc](auto rawProp) {
                     return QueueFamily{rawProp, indexAcc++};
                   });

    // Get list of supported extensions
    uint32_t extCount = 0;
    instance.core<1, 0>().vkEnumerateDeviceExtensionProperties(
        m_physicalDevice, nullptr, &extCount, nullptr);
    if (extCount > 0) {
      cntr::vector<VkExtensionProperties, 10> extensions(extCount);
      if (instance.core<1, 0>().vkEnumerateDeviceExtensionProperties(
              m_physicalDevice, nullptr, &extCount, &extensions.front()) ==
          VK_SUCCESS) {

        for (auto &props : extensions) {
          if (!Library::ValidExtensionName(props.extensionName))
            continue;
          m_supportedExtensions.emplace_back(
              Library::ExtensionId(props.extensionName));
        }
      }
    }
  }

  /** @brief Properties of the physical device including limits that the
   * application can check against */
  VkPhysicalDeviceProperties m_properties{};
  /** @brief Features of the physical device that an application can use to
   * check if a feature is supported */
  VkPhysicalDeviceFeatures m_features{};
#ifdef VK_VERSION_1_2
  VkPhysicalDeviceVulkan11Features m_vulkan11Features{};
#endif
  /** @brief Features that have been enabled for use on the physical device */
  VkPhysicalDeviceFeatures m_enabledFeatures{};

#ifdef VK_VERSION_1_2
  VkPhysicalDeviceVulkan11Features m_enabledVulkan11Features{};
#endif

  /** @brief Memory types and heaps of the physical device */
  VkPhysicalDeviceMemoryProperties m_memoryProperties{};
  /** @brief Queue family properties of the physical device */
  cntr::vector<QueueFamily, 4> m_queueFamilyProperties{};
  /** @brief List of extensions supported by the device */
  cntr::vector<ext, 5> m_supportedExtensions{};

  cntr::vector<ext, 5> m_enabledExtensions{};

  VkPhysicalDevice m_physicalDevice{};
  ApiVersion m_requestedApiVersion = ApiVersion{1, 0, 0};
};

} // namespace vkw
#endif // VKWRAPPER_PHYSICALDEVICE_HPP
