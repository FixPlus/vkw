#ifndef VKRENDERER_SURFACE_HPP
#define VKRENDERER_SURFACE_HPP

#include <vkw/Containers.hpp>
#include <vkw/Extensions.hpp>
#include <vkw/Instance.hpp>

namespace vkw {

class Surface : public ReferenceGuard {
public:
  Surface(Instance const &parent,
          VkSurfaceKHR surface) noexcept(ExceptionsDisabled)
      : m_surface(surface, SurfaceDestructor(parent)) {}
#ifdef VK_USE_PLATFORM_WIN32_KHR
  Surface(Instance const &parent, HINSTANCE hinstance,
          HWND hwnd) noexcept(ExceptionsDisabled)
      : m_surface(nullptr, SurfaceDestructor(parent)) {
    Extension<ext::KHR_win32_surface> win32SurfaceExt{parent};

    VkWin32SurfaceCreateInfoKHR createInfo{};

    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.hinstance = hinstance;
    createInfo.hwnd = hwnd;
    VkSurfaceKHR tmpSurface = nullptr;

    VK_CHECK_RESULT(win32SurfaceExt.vkCreateWin32SurfaceKHR(
        parent, &createInfo, HostAllocator::get(), &tmpSurface));
    m_surface.reset(tmpSurface);
  }
#elif VK_USE_PLATFORM_XLIB_KHR
  Surface(Instance const &parent, Display *display,
          Window window) noexcept(ExceptionsDisabled)
      : m_surface(nullptr, SurfaceDestructor(parent)) {
    Extension<ext::KHR_xlib_surface> xlibSurfaceExt{parent};

    VkXlibSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.dpy = display;
    createInfo.window = window;
    VkSurfaceKHR tmpSurface = nullptr;

    VK_CHECK_RESULT(xlibSurfaceExt.vkCreateXlibSurfaceKHR(
        parent, &createInfo, HostAllocator::get(), &tmpSurface));
    m_surface.reset(tmpSurface);
  }
#elif defined VK_USE_PLATFORM_XCB_KHR
  Surface(Instance const &parent, xcb_connection_t *connection,
          xcb_window_t window) noexcept(ExceptionsDisabled)
      : m_surface(nullptr, SurfaceDestructor(parent)) {
    Extension<ext::KHR_xcb_surface> xcbSurfaceExt{parent};

    VkXcbSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.connection = connection;
    createInfo.window = window;
    VkSurfaceKHR tmpSurface = nullptr;

    VK_CHECK_RESULT(xcbSurfaceExt.vkCreateXCBSurfaceKHR(
        parent, &createInfo, HostAllocator::get(), &tmpSurface));
    m_surface.reset(tmpSurface);
  }
#elif defined VK_USE_PLATFORM_WAYLAND_KHR
  Surface(Instance const &parent, wl_display *display,
          wl_surface *surface) noexcept(ExceptionsDisabled)
      : m_surface(nullptr, SurfaceDestructor(parent)) {
    Extension<ext::KHR_wayland_surface> waylandSurfaceExt{parent};

    VkWaylandSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.display = display;
    createInfo.surface = surface;
    VkSurfaceKHR tmpSurface = nullptr;

    VK_CHECK_RESULT(waylandSurfaceExt.vkCreateWaylandSurfaceKHR(
        parent, &createInfo, HostAllocator::get(), &tmpSurface));
    m_surface.reset(tmpSurface);
  }
#endif

  Instance const &parent() const noexcept {
    return m_surface.get_deleter().parent.get();
  };

  const Extension<ext::KHR_surface> &ext() const noexcept {
    return m_surface.get_deleter().surfExt;
  }

  cntr::vector<VkPresentModeKHR, 4>
  getAvailablePresentModes(VkPhysicalDevice device) const
      noexcept(ExceptionsDisabled) {
    uint32_t modeCount = 0;

    cntr::vector<VkPresentModeKHR, 4> ret{};
    VK_CHECK_RESULT(ext().vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, m_surface.get(), &modeCount, nullptr));

    if (modeCount == 0)
      return ret;

    ret.resize(modeCount);
    VK_CHECK_RESULT(ext().vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, m_surface.get(), &modeCount, ret.data()));

    return ret;
  }
  cntr::vector<VkSurfaceFormatKHR, 4>
  getAvailableFormats(VkPhysicalDevice device) const
      noexcept(ExceptionsDisabled) {
    uint32_t modeCount = 0;

    cntr::vector<VkSurfaceFormatKHR, 4> ret{};
    VK_CHECK_RESULT(ext().vkGetPhysicalDeviceSurfaceFormatsKHR(
        device, m_surface.get(), &modeCount, nullptr))

    if (modeCount == 0)
      return ret;

    ret.resize(modeCount);
    VK_CHECK_RESULT(ext().vkGetPhysicalDeviceSurfaceFormatsKHR(
        device, m_surface.get(), &modeCount, ret.data()))

    return ret;
  }
  VkSurfaceCapabilitiesKHR getSurfaceCapabilities(VkPhysicalDevice device) const
      noexcept(ExceptionsDisabled) {
    VkSurfaceCapabilitiesKHR ret{};
    VK_CHECK_RESULT(ext().vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device, m_surface.get(), &ret))
    return ret;
  }
  cntr::vector<uint32_t, 4>
  getQueueFamilyIndicesThatSupportPresenting(VkPhysicalDevice device) const
      noexcept(ExceptionsDisabled) {
    uint32_t queueFamilyCount;
    parent().core<1, 0>().vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queueFamilyCount, nullptr);

    cntr::vector<uint32_t, 4> ret;

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
      VkBool32 supported;
      VK_CHECK_RESULT(ext().vkGetPhysicalDeviceSurfaceSupportKHR(
          device, i, m_surface.get(), &supported))
      if (supported)
        ret.push_back(i);
    }

    return ret;
  }

  operator VkSurfaceKHR() const noexcept { return m_surface.get(); }

private:
  struct SurfaceDestructor {
    SurfaceDestructor(Instance const &inst) noexcept(ExceptionsDisabled)
        : parent(inst), surfExt(inst) {}
    void operator()(VkSurfaceKHR surface) const noexcept {
      if (!surface)
        return;
      surfExt.vkDestroySurfaceKHR(parent.get(), surface, HostAllocator::get());
    }
    StrongReference<Instance const> parent;
    Extension<ext::KHR_surface> surfExt;
  };
  std::unique_ptr<VkSurfaceKHR_T, SurfaceDestructor> m_surface;
};
} // namespace vkw

#endif // VKRENDERER_SURFACE_HPP
