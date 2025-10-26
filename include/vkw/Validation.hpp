#ifndef VKWRAPPER_VALIDATION_HPP
#define VKWRAPPER_VALIDATION_HPP

#include <vkw/Extensions.hpp>
#include <vkw/Instance.hpp>
#include <vkw/Layers.hpp>

#include <cassert>
#include <concepts>
#include <functional>

namespace vkw::debug {

enum class MsgSeverity : unsigned {
  Verbose = 0x1,
  Info = 0x2,
  Warning = 0x4,
  Error = 0x8,
};
using MsgSeverityFlags = unsigned;

enum class MsgType : unsigned {
  General = 0x1,
  Validation = 0x2,
  Performance = 0x4
};
using MsgTypeFlags = unsigned;

inline unsigned operator|(MsgSeverity a, MsgSeverity b) {
  return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

inline unsigned operator|(unsigned a, MsgSeverity b) {
  return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

inline unsigned operator|(MsgSeverity a, unsigned b) {
  return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

inline unsigned operator&(MsgSeverity a, MsgSeverity b) {
  return static_cast<unsigned>(a) & static_cast<unsigned>(b);
}

inline unsigned operator&(unsigned a, MsgSeverity b) {
  return static_cast<unsigned>(a) & static_cast<unsigned>(b);
}

inline unsigned operator&(MsgSeverity a, unsigned b) {
  return static_cast<unsigned>(a) & static_cast<unsigned>(b);
}

inline unsigned operator|(MsgType a, MsgType b) {
  return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

inline unsigned operator|(unsigned a, MsgType b) {
  return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

inline unsigned operator|(MsgType a, unsigned b) {
  return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}

inline unsigned operator&(MsgType a, MsgType b) {
  return static_cast<unsigned>(a) & static_cast<unsigned>(b);
}

inline unsigned operator&(unsigned a, MsgType b) {
  return static_cast<unsigned>(a) & static_cast<unsigned>(b);
}

inline unsigned operator&(MsgType a, unsigned b) {
  return static_cast<unsigned>(a) & static_cast<unsigned>(b);
}

namespace __detail {

VkDebugUtilsMessageSeverityFlagsEXT severityConvert(MsgSeverityFlags flags) {
  VkDebugUtilsMessageSeverityFlagsEXT ret{};
  if (flags & MsgSeverity::Error)
    ret |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  if (flags & MsgSeverity::Warning)
    ret |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
  if (flags & MsgSeverity::Info)
    ret |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
  if (flags & MsgSeverity::Verbose)
    ret |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
  return ret;
}

VkDebugUtilsMessageTypeFlagsEXT typeConvert(MsgTypeFlags flags) {
  VkDebugUtilsMessageTypeFlagsEXT ret{};
  if (flags & MsgType::General)
    ret |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
  if (flags & MsgType::Validation)
    ret |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
  if (flags & MsgType::Performance)
    ret |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  return ret;
}

MsgSeverity toSeverity(VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    return MsgSeverity::Verbose;
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    return MsgSeverity::Info;
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    return MsgSeverity::Warning;
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    return MsgSeverity::Error;
  } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    return MsgSeverity::Error;
  } else {
    return MsgSeverity::Verbose;
  }
}

MsgType toType(VkDebugUtilsMessageTypeFlagsEXT type) {
  switch (type) {
  default:
  case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
    return MsgType::General;
  case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
    return MsgType::Validation;
  case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
    return MsgType::Performance;
  }
}
} // namespace __detail

struct Message {
  // This corresponds to VkDebugUtilsMessengerCallbackDataEXT
  int id;
  std::string_view name;
  std::string_view what;
};

class Validation {
public:
  template <std::invocable<MsgSeverity, MsgType, Message const &> Fn>
  explicit Validation(Instance const &instance, Fn &&callback,
                      MsgSeverityFlags severityFilter = MsgSeverity::Warning |
                                                        MsgSeverity::Error,
                      MsgTypeFlags typeFilter =
                          MsgType::General |
                          MsgType::Validation) noexcept(ExceptionsDisabled)
      : m_handler(std::make_unique<MessageHandler>(std::forward<Fn>(callback))),
        m_messenger(nullptr, MessengerDestructor{instance}) {
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
    debugUtilsMessengerCI.sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugUtilsMessengerCI.messageSeverity =
        __detail::severityConvert(severityFilter);
    debugUtilsMessengerCI.messageType = __detail::typeConvert(typeFilter);
    debugUtilsMessengerCI.pUserData = m_handler.get();
    debugUtilsMessengerCI.pfnUserCallback = &MessageHandler::CallbackEntry;
    VkDebugUtilsMessengerEXT tmpMsg = nullptr;

    VK_CHECK_RESULT(m_ext().vkCreateDebugUtilsMessengerEXT(
        instance, &debugUtilsMessengerCI, HostAllocator::get(), &tmpMsg));
    m_messenger.reset(tmpMsg);
  }

  virtual ~Validation() = default;

private:
  using CallbackFn = std::function<void(MsgSeverity severity, MsgType type,
                                        Message const &message)>;
  struct MessageHandler {
    CallbackFn callback;
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    CallbackEntry(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                  void *pUserData) noexcept(ExceptionsDisabled) {
      assert(pUserData);
      MessageHandler &handler = *reinterpret_cast<MessageHandler *>(pUserData);
      return handler.runCallbacks(messageSeverity, messageType, pCallbackData);
    }
    VkBool32
    runCallbacks(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                 VkDebugUtilsMessageTypeFlagsEXT messageType,
                 const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData) {
      auto severity = __detail::toSeverity(messageSeverity);
      auto msgType = __detail::toType(messageType);
      Message message{pCallbackData->messageIdNumber,
                      pCallbackData->pMessageIdName, pCallbackData->pMessage};
      std::invoke(callback, severity, msgType, message);
      return severity == MsgSeverity::Error;
    }
  };

  const Instance &m_instance() const noexcept {
    return m_messenger.get_deleter().instance;
  }

  const Layer<layer::KHRONOS_validation> &m_layer() const noexcept {
    return m_messenger.get_deleter().validLayer;
  }
  const Extension<ext::EXT_debug_utils> &m_ext() const noexcept {
    return m_messenger.get_deleter().dbgUtils;
  }

  struct MessengerDestructor {
    MessengerDestructor(const Instance &inst) noexcept(ExceptionsDisabled)
        : instance(inst), validLayer(inst), dbgUtils(inst) {}
    void operator()(VkDebugUtilsMessengerEXT messenger) const noexcept {
      if (!messenger)
        return;
      dbgUtils.vkDestroyDebugUtilsMessengerEXT(instance.get(), messenger,
                                               HostAllocator::get());
    }
    StrongReference<const Instance> instance;
    Layer<layer::KHRONOS_validation> validLayer;
    Extension<ext::EXT_debug_utils> dbgUtils;
  };
  std::unique_ptr<MessageHandler> m_handler;
  std::unique_ptr<VkDebugUtilsMessengerEXT_T, MessengerDestructor> m_messenger;
};

} // namespace vkw::debug
#endif // VKWRAPPER_VALIDATION_HPP
