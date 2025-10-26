#ifndef VKRENDERER_IMAGE_HPP
#define VKRENDERER_IMAGE_HPP

#include <vkw/Allocation.hpp>

namespace vkw {

class ImageInterface : public ReferenceGuard {
public:
  explicit ImageInterface(VkImageUsageFlags usage = 0) noexcept {
    m_createInfo.usage = usage;
    m_createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    m_createInfo.pNext = nullptr;
    m_createInfo.arrayLayers =
        1; // default parameter, maybe overridden by child classes
  }

  VkImageUsageFlags usage() const noexcept { return m_createInfo.usage; }

  VkFormat format() const noexcept { return m_createInfo.format; }

  VkExtent3D rawExtents() const noexcept { return m_createInfo.extent; }

  VkImageType type() const noexcept { return m_createInfo.imageType; }

  uint32_t mipLevels() const noexcept { return m_createInfo.mipLevels; }

  VkImageSubresourceRange completeSubresourceRange() const noexcept {
    VkImageSubresourceRange ret{};
    ret.baseMipLevel = 0;
    ret.levelCount = m_createInfo.mipLevels;
    ret.aspectMask = isDepthFormat(format()) ? VK_IMAGE_ASPECT_DEPTH_BIT
                                             : VK_IMAGE_ASPECT_COLOR_BIT;
    ret.baseArrayLayer = 0;
    ret.layerCount = m_createInfo.arrayLayers;
    return ret;
  }

  virtual operator VkImage() const noexcept = 0;

  static bool isDepthFormat(VkFormat format) noexcept {
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_S8_UINT:
      return true;
    default:
      return false;
    }
  }

  static bool isColorFormat(VkFormat format) noexcept {
    return !isDepthFormat(format);
  }

protected:
  VkImageCreateInfo m_createInfo{};
};

inline bool operator==(VkImageSubresourceRange const &lhs,
                       VkImageSubresourceRange const &rhs) noexcept {
  return lhs.levelCount == rhs.levelCount && lhs.layerCount == rhs.layerCount &&
         lhs.baseMipLevel == rhs.baseMipLevel &&
         lhs.baseArrayLayer == rhs.baseArrayLayer &&
         lhs.aspectMask == rhs.aspectMask;
}

inline bool operator==(VkComponentMapping const &lhs,
                       VkComponentMapping const &rhs) noexcept {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

inline bool operator==(VkImageViewCreateInfo const &lhs,
                       VkImageViewCreateInfo const &rhs) noexcept {
  return lhs.image == rhs.image &&
         lhs.subresourceRange == rhs.subresourceRange &&
         lhs.flags == rhs.flags && lhs.format == rhs.format &&
         lhs.viewType == rhs.viewType && lhs.components == rhs.components;
}

class ImageViewBase : public ReferenceGuard {
public:
  bool operator==(ImageViewBase const &another) const noexcept {
    return m_createInfo == another.m_createInfo;
  }

  ImageInterface const *image() const noexcept { return &m_parent.get(); }

  virtual operator VkImageView() const noexcept = 0;

  VkFormat format() const noexcept { return m_createInfo.format; };

protected:
  explicit ImageViewBase(ImageInterface const *image = nullptr,
                         VkFormat format = VK_FORMAT_MAX_ENUM,
                         uint32_t baseMipLevel = 0, uint32_t levelCount = 1,
                         VkComponentMapping componentMapping = {},
                         VkImageViewCreateFlags flags = 0) noexcept
      : m_parent(*image) {
    assert(image && "incomplete view is constructed");
    m_createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    m_createInfo.pNext = nullptr;
    m_createInfo.subresourceRange.baseMipLevel = baseMipLevel;
    m_createInfo.subresourceRange.levelCount = levelCount;
    m_createInfo.components = componentMapping;
    m_createInfo.flags = flags;
    m_createInfo.image = image->operator VkImage_T *();
    m_createInfo.format = format;
  }
  VkImageViewCreateInfo m_createInfo{};

private:
  StrongReference<ImageInterface const> m_parent;
};

class ImageViewCreator : virtual public ImageViewBase {
protected:
  explicit ImageViewCreator(Device const &device) noexcept(ExceptionsDisabled)
      : m_imageView(
            [&]() {
              VkImageView ret{};
              VK_CHECK_RESULT(device.core<1, 0>().vkCreateImageView(
                  device, &m_createInfo, HostAllocator::get(), &ret));
              return ret;
            }(),
            ViewDestructor{device}) {}

public:
  operator VkImageView() const noexcept override { return m_imageView.get(); };

private:
  struct ViewDestructor {
    StrongReference<Device const> device;
    void operator()(VkImageView view) {
      if (!view)
        return;
      device.get().core<1, 0>().vkDestroyImageView(device.get(), view,
                                                   HostAllocator::get());
    }
  };
  std::unique_ptr<VkImageView_T, ViewDestructor> m_imageView;
};

class AllocatedImage : public Allocation<VkImage>,
                       virtual public ImageInterface {
public:
  AllocatedImage(
      DeviceAllocator &allocator,
      const AllocationCreateInfo &allocCreateInfo) noexcept(ExceptionsDisabled)
      : Allocation<VkImage>(allocator, allocCreateInfo, m_createInfo) {}

  operator VkImage() const noexcept override { return handle(); }
};

class NonOwingImage : virtual public ImageInterface {
public:
  NonOwingImage(VkImage image) noexcept : m_image(image) {}

  operator VkImage() const noexcept override { return m_image; }

private:
  VkImage m_image;
};

class ImageArray : virtual public ImageInterface {
public:
  ImageArray(unsigned arrayLayers) noexcept {
    m_createInfo.arrayLayers = arrayLayers;
  }

  unsigned layers() const noexcept { return m_createInfo.arrayLayers; }
};

class ImageSingle : virtual public ImageInterface {
public:
  ImageSingle(unsigned arrayLayers = 1) noexcept {
    m_createInfo.arrayLayers = 1;
  }
};

enum ImageArrayness { SINGLE, ARRAY };

template <ImageArrayness iarr> struct ImageArraynessT {};

template <> struct ImageArraynessT<SINGLE> {
  using Type = ImageSingle;
};

template <> struct ImageArraynessT<ARRAY> {
  using Type = ImageArray;
};

template <typename T>
concept ImageArrayOrSingle =
    std::is_same_v<T, ImageArray>() || std::is_same_v<T, ImageSingle>();

enum ImagePixelType { COLOR, DEPTH, DEPTH_STENCIL };

template <ImagePixelType ptype> struct ImageAspectVal {};

template <> struct ImageAspectVal<COLOR> {
  static constexpr const VkImageAspectFlags value = VK_IMAGE_ASPECT_COLOR_BIT;
};

template <> struct ImageAspectVal<DEPTH> {
  static constexpr const VkImageAspectFlags value = VK_IMAGE_ASPECT_DEPTH_BIT;
};

template <> struct ImageAspectVal<DEPTH_STENCIL> {
  static constexpr const VkImageAspectFlags value =
      VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;
};

enum ImageType { I1D, I2D, I3D, ICUBE };

template <ImageType itype> struct ImageTypeVal {};

template <> struct ImageTypeVal<I1D> {
  static constexpr const VkImageType value = VK_IMAGE_TYPE_1D;
};

template <> struct ImageTypeVal<I2D> {
  static constexpr const VkImageType value = VK_IMAGE_TYPE_2D;
};

template <> struct ImageTypeVal<I3D> {
  static constexpr const VkImageType value = VK_IMAGE_TYPE_3D;
};

enum ImageViewType {
  V1D,
  V1DA,
  V2D,
  V2DA,
  VCUBE,
  VCUBEA,
  V3D,
};

template <ImageType itype, ImageViewType vtype> struct CompatibleViewType {
  constexpr static const bool value = false;
};

template <> struct CompatibleViewType<I1D, V1D> {
  constexpr static const bool value = true;
};

template <> struct CompatibleViewType<I1D, V1DA> {
  constexpr static const bool value = true;
};

template <> struct CompatibleViewType<I2D, V2D> {
  constexpr static const bool value = true;
};

template <> struct CompatibleViewType<I2D, V2DA> {
  constexpr static const bool value = true;
};

template <> struct CompatibleViewType<I3D, V3D> {
  constexpr static const bool value = true;
};
template <ImageType itype, ImageViewType vtype>
concept CompatibleViewTypeC = CompatibleViewType<itype, vtype>::value;

template <ImageViewType vtype> struct ImageViewTypeVal {};

template <> struct ImageViewTypeVal<V1D> {
  static constexpr const VkImageViewType value = VK_IMAGE_VIEW_TYPE_1D;
};

template <> struct ImageViewTypeVal<V1DA> {
  static constexpr const VkImageViewType value = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
};

template <> struct ImageViewTypeVal<V2D> {
  static constexpr const VkImageViewType value = VK_IMAGE_VIEW_TYPE_2D;
};

template <> struct ImageViewTypeVal<V2DA> {
  static constexpr const VkImageViewType value = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
};

template <> struct ImageViewTypeVal<VCUBE> {
  static constexpr const VkImageViewType value = VK_IMAGE_VIEW_TYPE_CUBE;
};
template <> struct ImageViewTypeVal<VCUBEA> {
  static constexpr const VkImageViewType value = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
};
template <> struct ImageViewTypeVal<V3D> {
  static constexpr const VkImageViewType value = VK_IMAGE_VIEW_TYPE_3D;
};

template <ImagePixelType ptype>
class ImageViewIPT : virtual public ImageViewBase {
protected:
  ImageViewIPT(VkFormat format) noexcept {
    m_createInfo.subresourceRange.aspectMask = ImageAspectVal<ptype>::value;
    m_createInfo.format = format;
  }
};

template <ImageViewType vtype>
class ImageViewVT : virtual public ImageViewBase {
public:
  unsigned layers() const noexcept {
    return m_createInfo.subresourceRange.layerCount;
  }
  unsigned baseLayer() const noexcept {
    return m_createInfo.subresourceRange.baseArrayLayer;
  }

  unsigned levels() const noexcept {
    return m_createInfo.subresourceRange.levelCount;
  }
  unsigned baseLevel() const noexcept {
    return m_createInfo.subresourceRange.baseMipLevel;
  }

protected:
  ImageViewVT() noexcept {
    m_createInfo.viewType = ImageViewTypeVal<vtype>::value;
  }
};

class ImageViewSubRange : virtual public ImageViewBase {
protected:
  ImageViewSubRange(unsigned baseLayer, unsigned layerCount,
                    unsigned baseMipLevel, unsigned mipLevelCount) noexcept {
    m_createInfo.subresourceRange.baseArrayLayer = baseLayer;
    m_createInfo.subresourceRange.layerCount = layerCount;
    m_createInfo.subresourceRange.baseMipLevel = baseMipLevel;
    m_createInfo.subresourceRange.levelCount = mipLevelCount;
  }
};

template <ImagePixelType ptype> class ImageIPT : virtual public ImageInterface {
public:
  ImageIPT(ImageIPT &&another) noexcept : ImageInterface(std::move(another)) {}
  ImageIPT &operator=(ImageIPT &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
  }

protected:
  ImageIPT(VkFormat format) noexcept { m_createInfo.format = format; }
};
namespace __detail {

inline unsigned m_FormatRedBits(VkFormat format) noexcept {
  switch (format) {
  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_R8G8B8A8_SNORM:
  case VK_FORMAT_R8G8B8A8_SSCALED:
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SINT:
  case VK_FORMAT_R8G8B8A8_USCALED:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_R8G8B8_SINT:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_R8G8B8_SRGB:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_R8G8B8_USCALED:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SSCALED:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_SRGB:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_USCALED:
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SSCALED:
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_SRGB:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_USCALED:
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SSCALED:
    return 8;
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_SSCALED:
  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R16G16B16A16_USCALED:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_USCALED:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SSCALED:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_USCALED:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SSCALED:
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_USCALED:
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SSCALED:
    return 16;
  case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    return 11;
  default:
    assert("Format Not Handled yet");
  }
  return 0;
}
inline unsigned m_FormatGreenBits(VkFormat format) noexcept {
  switch (format) {
  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_R8G8B8A8_SNORM:
  case VK_FORMAT_R8G8B8A8_SSCALED:
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SINT:
  case VK_FORMAT_R8G8B8A8_USCALED:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_R8G8B8_SINT:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_R8G8B8_SRGB:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_R8G8B8_USCALED:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SSCALED:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_SRGB:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_USCALED:
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SSCALED:
    return 8;
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_SSCALED:
  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R16G16B16A16_USCALED:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_USCALED:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SSCALED:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_USCALED:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SSCALED:
    return 16;
  case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    return 11;
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_SRGB:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_USCALED:
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SSCALED:
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_USCALED:
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SSCALED:
    return 0;

  default:
    assert("Format Not Handled yet");
  }
  return 0;
}
inline unsigned m_FormatBlueBits(VkFormat format) noexcept {
  switch (format) {
  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_R8G8B8A8_SNORM:
  case VK_FORMAT_R8G8B8A8_SSCALED:
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SINT:
  case VK_FORMAT_R8G8B8A8_USCALED:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_R8G8B8_SINT:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_R8G8B8_SRGB:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_R8G8B8_USCALED:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SSCALED:
    return 8;

  case VK_FORMAT_R8_SSCALED:
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_SSCALED:
  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R16G16B16A16_USCALED:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_USCALED:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SSCALED:
    return 16;
  case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    return 10;
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_SRGB:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_USCALED:
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SSCALED:
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_SRGB:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_USCALED:
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_USCALED:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SSCALED:
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_USCALED:
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SSCALED:
    return 0;
  default:
    assert("Format Not Handled yet");
  }
  return 0;
}
inline unsigned m_FormatAlphaBits(VkFormat format) noexcept {
  switch (format) {
  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_R8G8B8A8_SNORM:
  case VK_FORMAT_R8G8B8A8_SSCALED:
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SINT:
  case VK_FORMAT_R8G8B8A8_USCALED:
  case VK_FORMAT_R8G8B8A8_SRGB:
    return 8;
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_SSCALED:
  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R16G16B16A16_USCALED:
    return 16;
  case VK_FORMAT_R8G8B8_SINT:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_R8G8B8_SRGB:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_R8G8B8_USCALED:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SSCALED:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_SRGB:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_USCALED:
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SSCALED:
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_SRGB:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_USCALED:
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SSCALED:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_USCALED:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SSCALED:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_USCALED:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SSCALED:
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_USCALED:
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SSCALED:
  case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    return 0;
  default:
    assert("Format Not Handled yet");
  }
  return 0;
}
inline unsigned m_FormatDepthBits(VkFormat format) noexcept {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_D16_UNORM_S8_UINT:
    return 16;
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D24_UNORM_S8_UINT:
    return 24;
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
  case VK_FORMAT_D32_SFLOAT:
    return 32;
  case VK_FORMAT_S8_UINT:
    return 0;
  default:
    assert("Format Not Handled yet");
  }
  return 0;
}
inline unsigned m_FormatStencilBits(VkFormat format) noexcept {
  switch (format) {

  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return 8;
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_D32_SFLOAT:
    return 0;
  default:
    assert("Format Not Handled yet");
  }
  return 0;
}

} // namespace __detail
template <> class ImageIPT<COLOR> : virtual public ImageInterface {
public:
  unsigned redBits() const noexcept {
    return __detail::m_FormatRedBits(m_createInfo.format);
  }
  unsigned greenBits() const noexcept {
    return __detail::m_FormatGreenBits(m_createInfo.format);
  }
  unsigned blueBits() const noexcept {
    return __detail::m_FormatBlueBits(m_createInfo.format);
  }
  unsigned alphaBits() const noexcept {
    return __detail::m_FormatAlphaBits(m_createInfo.format);
  }

protected:
  ImageIPT(VkFormat format) noexcept { m_createInfo.format = format; }
};

template <> class ImageIPT<DEPTH> : virtual public ImageInterface {
public:
  unsigned dBits() const noexcept {
    return __detail::m_FormatDepthBits(m_createInfo.format);
  }

protected:
  ImageIPT(VkFormat format) noexcept { m_createInfo.format = format; }
};

template <> class ImageIPT<DEPTH_STENCIL> : virtual public ImageInterface {
public:
  unsigned dBits() const noexcept {
    return __detail::m_FormatDepthBits(m_createInfo.format);
  }
  unsigned sBits() const noexcept {
    return __detail::m_FormatStencilBits(m_createInfo.format);
  }

protected:
  ImageIPT(VkFormat format) noexcept { m_createInfo.format = format; }
};

template <ImageType itype> class ImageIT : virtual public ImageInterface {
public:
  ImageIT() noexcept { m_createInfo.imageType = ImageTypeVal<itype>::value; }
};

template <> class ImageIT<I1D> : virtual public ImageInterface {
public:
  ImageIT(unsigned width, unsigned = 0, unsigned = 0) noexcept {
    m_createInfo.imageType = ImageTypeVal<I1D>::value;
    m_createInfo.extent.width = width;
    m_createInfo.extent.height = 1;
    m_createInfo.extent.depth = 1;
  }

  unsigned width() const noexcept { return m_createInfo.extent.width; }
};

template <> class ImageIT<I2D> : virtual public ImageInterface {
public:
  ImageIT(unsigned width, unsigned height, unsigned = 0) noexcept {
    m_createInfo.imageType = ImageTypeVal<I2D>::value;
    m_createInfo.extent.width = width;
    m_createInfo.extent.height = height;
    m_createInfo.extent.depth = 1;
  }

  unsigned width() const noexcept { return m_createInfo.extent.width; }

  unsigned height() const noexcept { return m_createInfo.extent.height; }
};

template <> class ImageIT<I3D> : virtual public ImageInterface {
public:
  ImageIT(unsigned width, unsigned height, unsigned depth) noexcept {
    m_createInfo.imageType = ImageTypeVal<I3D>::value;
    m_createInfo.extent.width = width;
    m_createInfo.extent.height = height;
    m_createInfo.extent.depth = depth;
  }

  unsigned width() const noexcept { return m_createInfo.extent.width; }

  unsigned height() const noexcept { return m_createInfo.extent.height; }

  unsigned depth() const noexcept { return m_createInfo.extent.depth; }
};

template <ImagePixelType ptype, ImageType itype>
class ImageTypeInterface : public ImageIPT<ptype>, public ImageIT<itype> {
public:
  ImageTypeInterface(VkFormat format, unsigned width, unsigned height,
                     unsigned depth) noexcept
      : ImageIPT<ptype>(format), ImageIT<itype>(width, height, depth) {}
};

template <ImagePixelType ptype, ImageType itype, ImageArrayness iarr>
class BasicImage : public ImageTypeInterface<ptype, itype>,
                   public ImageArraynessT<iarr>::Type {
public:
  BasicImage(VkFormat format, unsigned width, unsigned height, unsigned depth,
             unsigned layers) noexcept
      : ImageTypeInterface<ptype, itype>(format, width, height, depth),
        ImageArraynessT<iarr>::Type(layers) {}
};

template <ImagePixelType ptype, ImageViewType vtype>
class ImageView : public ImageViewIPT<ptype>,
                  public ImageViewVT<vtype>,
                  public ImageViewSubRange,
                  public ImageViewCreator {
public:
  template <ImageType itype>
    requires CompatibleViewTypeC<itype, vtype>
  ImageView(Device const &device, BasicImage<ptype, itype, ARRAY> const &image,
            VkFormat format, unsigned baseLayer = 0, unsigned layerCount = 1,
            unsigned baseMipLevel = 0, unsigned mipLevels = 1,
            VkComponentMapping mapping =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            VkImageViewCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : ImageViewBase(&image, format, 0, 0, mapping, flags),
        ImageViewIPT<ptype>(format),
        ImageViewSubRange(baseLayer, layerCount, baseMipLevel, mipLevels),
        ImageViewVT<vtype>(), ImageViewCreator(device) {}

  template <ImageType itype>
    requires CompatibleViewTypeC<itype, vtype>
  ImageView(Device const &device, BasicImage<ptype, itype, SINGLE> const &image,
            VkFormat format, unsigned baseMipLevel = 0, unsigned mipLevels = 1,
            VkComponentMapping mapping =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            VkImageViewCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : ImageViewBase(&image, format, 0, 0, mapping, flags),
        ImageViewIPT<ptype>(format),
        ImageViewSubRange(0, 1, baseMipLevel, mipLevels), ImageViewVT<vtype>(),
        ImageViewCreator(device) {}
};

class ImageRestInterface : virtual public ImageInterface {
protected:
  ImageRestInterface(VkSampleCountFlagBits samples, uint32_t mipLevels,
                     VkImageUsageFlags usage, VkImageCreateFlags flags,
                     VkImageLayout initialLayout, VkImageTiling tiling,
                     SharingInfo const &sharingInfo) noexcept {
    m_createInfo.usage = usage;
    m_createInfo.flags = flags;
    m_createInfo.initialLayout = initialLayout;
    m_createInfo.samples = samples;
    m_createInfo.tiling = tiling;
    m_createInfo.mipLevels = mipLevels;
    m_createInfo.sharingMode = sharingInfo.sharingMode();
    m_createInfo.queueFamilyIndexCount = sharingInfo.queueFamilies().size();
    if (sharingInfo.sharingMode() != VK_SHARING_MODE_EXCLUSIVE)
      m_createInfo.pQueueFamilyIndices = sharingInfo.queueFamilies().data();
  }
};

/** ********************** Complete Image Types. ****************************/

/**
 *                            Swap chain Image
 *     This class represents the image retrieved from swap chain.
 *
 */

class SwapChainImage : public BasicImage<COLOR, I2D, ARRAY>,
                       public NonOwingImage {
public:
private:
  friend class SwapChain;
  SwapChainImage(VkImage swapChainImage, VkFormat surfaceFormat, uint32_t width,
                 uint32_t height, uint32_t layers,
                 VkImageUsageFlags usage) noexcept
      : ImageInterface(), BasicImage<COLOR, I2D, ARRAY>(surfaceFormat, width,
                                                        height, 1, layers),
        NonOwingImage(swapChainImage) {
    m_createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    m_createInfo.mipLevels = 1;
    m_createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    m_createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    m_createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_createInfo.usage = usage;
  }
};

/**
 *                            Staging Image
 *     This class represents the image with linear tiling layout.
 *     In Vulkan, images with such tiling are strongly limited and are
 *     capable only of transfer in/out operations. However this is the only
 *     type of images that can be mapped to host memory and be filled directly.
 *
 */
class StagingImage : public BasicImage<COLOR, I2D, SINGLE>,
                     public ImageRestInterface,
                     private AllocatedImage {
public:
  StagingImage(DeviceAllocator& allocator, VkFormat colorFormat, uint32_t width,
               uint32_t height) noexcept(ExceptionsDisabled)
      : BasicImage<COLOR, I2D, SINGLE>(colorFormat, width, height, 1, 1),
        ImageRestInterface(VK_SAMPLE_COUNT_1_BIT, 1,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                           0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_LINEAR,
                           SharingInfo{}),
        AllocatedImage(allocator,
                       AllocationCreateInfo{
                        /// TODO: re-fill
                        /*.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
                        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT*/}) {
  }

  std::span<unsigned char> mapped() noexcept {
    return AllocatedImage::mapped<unsigned char>();
  }

  void flush() noexcept(ExceptionsDisabled) {
    AllocatedImage::flush(0, VK_WHOLE_SIZE);
  }

  void invalidate() noexcept(ExceptionsDisabled) {
    AllocatedImage::invalidate(0, VK_WHOLE_SIZE);
  }
};

template <ImagePixelType ptype, ImageType itype, ImageArrayness iarr = SINGLE>
class Image : public BasicImage<ptype, itype, iarr>,
              public ImageRestInterface,
              public AllocatedImage {
public:
  Image(DeviceAllocator &allocator, const AllocationCreateInfo &allocCreateInfo,
        VkFormat format, uint32_t width, uint32_t height, uint32_t depth,
        uint32_t layers, uint32_t mipLevels, VkImageUsageFlags usage,
        VkImageCreateFlags flags = 0,
        SharingInfo const &sharingInfo = {}) noexcept(ExceptionsDisabled)
      : BasicImage<ptype, itype, iarr>(format, width, height, depth, layers),
        ImageRestInterface(VK_SAMPLE_COUNT_1_BIT, mipLevels, usage, flags,
                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL,
                           sharingInfo),
        AllocatedImage(allocator, allocCreateInfo) {}
};

} // namespace vkw
#endif // VKRENDERER_IMAGE_HPP
