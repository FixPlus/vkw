#ifndef VKRENDERER_IMAGE_HPP
#define VKRENDERER_IMAGE_HPP

#include <vkw/Allocation.hpp>
#include <vkw/Device.hpp>

namespace vkw {

class ImageInterface : public ReferenceGuard {
public:
  explicit ImageInterface(VkImageUsageFlags usage = 0) noexcept;

  ImageInterface(ImageInterface &&another) = default;
  ImageInterface(ImageInterface const &another) = delete;

  ImageInterface &operator=(ImageInterface &&another) = default;
  ImageInterface &operator=(ImageInterface const &another) = delete;

  virtual ~ImageInterface() = default;

  VkImageUsageFlags usage() const noexcept { return m_createInfo.usage; }

  VkFormat format() const noexcept { return m_createInfo.format; }

  VkExtent3D rawExtents() const noexcept { return m_createInfo.extent; }

  VkImageType type() const noexcept { return m_createInfo.imageType; }

  uint32_t mipLevels() const noexcept { return m_createInfo.mipLevels; }

  VkImageSubresourceRange completeSubresourceRange() const noexcept;

  virtual operator VkImage() const noexcept = 0;

  static bool isDepthFormat(VkFormat format) noexcept;

  static bool isColorFormat(VkFormat format) noexcept;

protected:
  VkImageCreateInfo m_createInfo{};
};

class ImageViewBase : public ReferenceGuard {
public:
  bool operator==(ImageViewBase const &another) const noexcept;

  ImageInterface const *image() const noexcept { return &m_parent.get(); }

  ImageViewBase(ImageViewBase &&another) noexcept
      : ReferenceGuard(std::move(another)),
        m_parent(std::move(another.m_parent)),
        m_createInfo(another.m_createInfo) {
    another.m_moved_out = true;
  }
  ImageViewBase(ImageViewBase const &another) = delete;

  ImageViewBase &operator=(ImageViewBase &&another) noexcept {
    ReferenceGuard::operator=(std::move(another));
    std::swap(m_parent, another.m_parent);
    m_createInfo = another.m_createInfo;
    another.m_moved_out = true;
    return *this;
  }
  ImageViewBase &operator=(ImageViewBase const &another) = delete;

  virtual operator VkImageView() const noexcept = 0;

  VkFormat format() const noexcept { return m_createInfo.format; };

  virtual ~ImageViewBase() = default;

protected:
  explicit ImageViewBase(ImageInterface const *image = nullptr,
                         VkFormat format = VK_FORMAT_MAX_ENUM,
                         uint32_t baseMipLevel = 0, uint32_t levelCount = 1,
                         VkComponentMapping componentMapping = {},
                         VkImageViewCreateFlags flags = 0) noexcept;
  VkImageViewCreateInfo m_createInfo{};
  auto isMovedOut() const noexcept { return m_moved_out; }

private:
  bool m_moved_out = false;
  StrongReference<ImageInterface const> m_parent;
};

class ImageViewCreator : virtual public ImageViewBase {
public:
  ~ImageViewCreator() override;

protected:
  explicit ImageViewCreator(Device const &device) noexcept(ExceptionsDisabled);

public:
  ImageViewCreator(ImageViewCreator const &another) = delete;
  ImageViewCreator(ImageViewCreator &&another) noexcept
      : m_device(another.m_device), m_imageView(another.m_imageView) {
    if (!another.isMovedOut())
      ImageViewBase::operator=(std::move(another));
    another.m_imageView = VK_NULL_HANDLE;
  };

  ImageViewCreator &operator=(ImageViewCreator const &another) = delete;
  ImageViewCreator &operator=(ImageViewCreator &&another) noexcept {
    if (!another.isMovedOut())
      ImageViewBase::operator=(std::move(another));
    std::swap(m_device, another.m_device);
    std::swap(m_imageView, another.m_imageView);
    return *this;
  }

  operator VkImageView() const noexcept override { return m_imageView; };

private:
  VkImageView m_imageView{};
  StrongReference<Device const> m_device;
};

class AllocatedImage : public Allocation, virtual public ImageInterface {
public:
  AllocatedImage(
      VmaAllocator allocator,
      VmaAllocationCreateInfo allocCreateInfo) noexcept(ExceptionsDisabled);
  AllocatedImage(AllocatedImage &&another) noexcept
      : Allocation(std::move(another)), ImageInterface(std::move(another)),
        m_image(another.m_image) {
    another.m_image = VK_NULL_HANDLE;
  }

  AllocatedImage(AllocatedImage const &another) = delete;
  AllocatedImage const &operator=(AllocatedImage const &another) = delete;
  AllocatedImage &operator=(AllocatedImage &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    std::swap(m_image, another.m_image);
    return *this;
  }

  operator VkImage() const noexcept override { return m_image; }

  ~AllocatedImage() override;

private:
  VkImage m_image = VK_NULL_HANDLE;
};

class NonOwingImage : virtual public ImageInterface {
public:
  NonOwingImage(NonOwingImage &&another) noexcept
      : ImageInterface(std::move(another)), m_image(another.m_image) {}
  NonOwingImage &operator=(NonOwingImage &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    m_image = another.m_image;
    return *this;
  }

  operator VkImage() const noexcept override { return m_image; }

protected:
  explicit NonOwingImage(VkImage image) noexcept : m_image(image) {}

private:
  VkImage m_image;
};

class ImageArray : virtual public ImageInterface {
public:
  ImageArray(unsigned arrayLayers) noexcept {
    m_createInfo.arrayLayers = arrayLayers;
  }

  ImageArray(ImageArray &&another) noexcept
      : ImageInterface(std::move(another)) {}
  ImageArray &operator=(ImageArray &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
  }

  unsigned layers() const noexcept { return m_createInfo.arrayLayers; }
};

class ImageSingle : virtual public ImageInterface {
public:
  ImageSingle(unsigned arrayLayers = 1) noexcept {
    m_createInfo.arrayLayers = 1;
  }

  ImageSingle(ImageSingle &&another) noexcept
      : ImageInterface(std::move(another)) {}
  ImageSingle &operator=(ImageSingle &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
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
    std::is_same<T, ImageArray>() || std::is_same<T, ImageSingle>();

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
public:
  ImageViewIPT(ImageViewIPT &&another) noexcept {
    if (!another.isMovedOut())
      ImageViewBase::operator=(std::move(another));
  }
  ImageViewIPT &operator=(ImageViewIPT &&another) noexcept {
    if (!another.isMovedOut())
      ImageViewBase::operator=(std::move(another));
    return *this;
  }

protected:
  ImageViewIPT(VkFormat format) noexcept {
    m_createInfo.subresourceRange.aspectMask = ImageAspectVal<ptype>::value;
    m_createInfo.format = format;
  }
};

template <ImageViewType vtype>
class ImageViewVT : virtual public ImageViewBase {
public:
  ImageViewVT(ImageViewVT &&another) noexcept {
    if (!another.isMovedOut())
      ImageViewBase::operator=(std::move(another));
  }
  ImageViewVT &operator=(ImageViewVT &&another) noexcept {
    if (!another.isMovedOut())
      ImageViewBase::operator=(std::move(another));
    return *this;
  }
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
public:
  ImageViewSubRange(ImageViewSubRange &&another) noexcept {
    if (!another.isMovedOut())
      ImageViewBase::operator=(std::move(another));
  }
  ImageViewSubRange &operator=(ImageViewSubRange &&another) noexcept {
    if (!another.isMovedOut())
      ImageViewBase::operator=(std::move(another));
    return *this;
  }

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

unsigned m_FormatRedBits(VkFormat) noexcept;
unsigned m_FormatGreenBits(VkFormat) noexcept;
unsigned m_FormatBlueBits(VkFormat) noexcept;
unsigned m_FormatAlphaBits(VkFormat) noexcept;
unsigned m_FormatDepthBits(VkFormat) noexcept;
unsigned m_FormatStencilBits(VkFormat) noexcept;

template <> class ImageIPT<COLOR> : virtual public ImageInterface {
public:
  ImageIPT(ImageIPT &&another) noexcept : ImageInterface(std::move(another)) {}
  ImageIPT &operator=(ImageIPT &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
  }

  unsigned redBits() const noexcept {
    return m_FormatRedBits(m_createInfo.format);
  }
  unsigned greenBits() const noexcept {
    return m_FormatGreenBits(m_createInfo.format);
  }
  unsigned blueBits() const noexcept {
    return m_FormatBlueBits(m_createInfo.format);
  }
  unsigned alphaBits() const noexcept {
    return m_FormatAlphaBits(m_createInfo.format);
  }

protected:
  ImageIPT(VkFormat format) noexcept { m_createInfo.format = format; }
};

template <> class ImageIPT<DEPTH> : virtual public ImageInterface {
public:
  ImageIPT(ImageIPT &&another) noexcept : ImageInterface(std::move(another)) {}
  ImageIPT &operator=(ImageIPT &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
  }

  unsigned dBits() const noexcept {
    return m_FormatDepthBits(m_createInfo.format);
  }

protected:
  ImageIPT(VkFormat format) noexcept { m_createInfo.format = format; }
};

template <> class ImageIPT<DEPTH_STENCIL> : virtual public ImageInterface {
public:
  ImageIPT(ImageIPT &&another) noexcept : ImageInterface(std::move(another)) {}
  ImageIPT &operator=(ImageIPT &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
  }

  unsigned dBits() const noexcept {
    return m_FormatDepthBits(m_createInfo.format);
  }
  unsigned sBits() const noexcept {
    return m_FormatStencilBits(m_createInfo.format);
  }

protected:
  ImageIPT(VkFormat format) noexcept { m_createInfo.format = format; }
};

template <ImageType itype> class ImageIT : virtual public ImageInterface {
public:
  ImageIT(ImageIT &&another) noexcept : ImageInterface(std::move(another)) {}
  ImageIT &operator=(ImageIT &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
  }

  ImageIT() noexcept { m_createInfo.imageType = ImageTypeVal<itype>::value; }
};

template <> class ImageIT<I1D> : virtual public ImageInterface {
public:
  ImageIT(ImageIT &&another) noexcept : ImageInterface(std::move(another)) {}
  ImageIT &operator=(ImageIT &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
  }

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
  ImageIT(ImageIT &&another) noexcept : ImageInterface(std::move(another)) {}
  ImageIT &operator=(ImageIT &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
  }

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
  ImageIT(ImageIT &&another) noexcept : ImageInterface(std::move(another)) {}
  ImageIT &operator=(ImageIT &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
  }

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
public:
  ImageRestInterface(ImageRestInterface &&another) noexcept
      : ImageInterface(std::move(another)) {}
  ImageRestInterface &operator=(ImageRestInterface &&another) noexcept {
    ImageInterface::operator=(std::move(another));
    return *this;
  }

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
  StagingImage(VmaAllocator allocator, VkFormat colorFormat, uint32_t width,
               uint32_t height) noexcept(ExceptionsDisabled)
      : BasicImage<COLOR, I2D, SINGLE>(colorFormat, width, height, 1, 1),
        ImageRestInterface(VK_SAMPLE_COUNT_1_BIT, 1,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                           0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_LINEAR,
                           SharingInfo{}),
        AllocatedImage(allocator,
                       {.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
                        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT}) {
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
  Image(VmaAllocator allocator, VmaAllocationCreateInfo allocCreateInfo,
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
