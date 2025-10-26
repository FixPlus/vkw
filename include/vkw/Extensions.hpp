#ifndef VKWRAPPER_EXTENSIONS_HPP
#define VKWRAPPER_EXTENSIONS_HPP

#include <vkw/Device.hpp>

namespace vkw {

namespace __detail {
inline bool isExtensionEnabled(Instance const &instance,
                               vkw::ext id) noexcept(ExceptionsDisabled) {
  return instance.isExtensionEnabled(id);
}
inline bool isExtensionEnabled(Device const &device,
                               vkw::ext id) noexcept(ExceptionsDisabled) {
  return device.physicalDevice().isExtensionEnabled(id);
}

template <typename T> struct NativeHandlerOf {};
template <> struct NativeHandlerOf<Instance> {
  using Handle = VkInstance;
};
template <> struct NativeHandlerOf<Device> {
  using Handle = VkDevice;
};

template <typename T>
using PFN_getProcAddr =
    PFN_vkVoidFunction (*)(typename NativeHandlerOf<T>::Handle, const char *);

inline PFN_getProcAddr<Device> getProcAddrOf(Device const &device) noexcept {
  return device.parent().core<1, 0>().vkGetDeviceProcAddr;
}

inline PFN_getProcAddr<Instance>
getProcAddrOf(Instance const &instance) noexcept {
  return instance.parent().vkGetInstanceProcAddr;
}

} // namespace __detail

template <ext id, typename T>
class ExtensionBase
    : public SymbolTableBase<typename __detail::NativeHandlerOf<T>::Handle> {
public:
  explicit ExtensionBase(T const &handle) noexcept(ExceptionsDisabled)
      : SymbolTableBase<typename __detail::NativeHandlerOf<T>::Handle>(
            __detail::getProcAddrOf(handle), handle) {
    if (!__detail::isExtensionEnabled(handle, id))
      postError(ExtensionMissing(id, Library::ExtensionName(id)));
  }
};

template <ext name> class Extension {};

#define VKW_DUMP_EXTENSION_CLASSES
#include "SymbolTable.inc"
#undef VKW_DUMP_EXTENSION_CLASSES

} // namespace vkw
#endif // VKWRAPPER_EXTENSIONS_HPP
