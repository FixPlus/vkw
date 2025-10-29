#ifndef VKRENDERER_VERTEXBUFFER_HPP
#define VKRENDERER_VERTEXBUFFER_HPP

#include <vkw/Buffer.hpp>

namespace vkw {

enum class VertexAttributeType {
  VEC4F,
  VEC3F,
  VEC2F,
  FLOAT,
  RGBA8_UNORM,
  VEC4U,
  VEC3U,
  VEC2U,
  VEC4U16,
  VEC3U16,
  VEC2U16,
  VEC4U8,
  VEC3U8,
  VEC2U8,
  UINT,
  UINT16,
  UINT8
};

constexpr uint64_t size_of(VertexAttributeType attrType) {

  switch (attrType) {
  case VertexAttributeType::VEC4F:
    return 4 * sizeof(float);
  case VertexAttributeType::VEC3F:
    return 3 * sizeof(float);
  case VertexAttributeType::VEC2F:
    return 2 * sizeof(float);
  case VertexAttributeType::FLOAT:
    return 1 * sizeof(float);
  case VertexAttributeType::RGBA8_UNORM:
    return 1 * sizeof(uint32_t);
  case VertexAttributeType::VEC4U:
    return 4 * sizeof(uint32_t);
  case VertexAttributeType::VEC3U:
    return 3 * sizeof(uint32_t);
  case VertexAttributeType::VEC2U:
    return 2 * sizeof(uint32_t);
  case VertexAttributeType::VEC4U16:
    return 4 * sizeof(uint16_t);
  case VertexAttributeType::VEC3U16:
    return 3 * sizeof(uint16_t);
  case VertexAttributeType::VEC2U16:
    return 2 * sizeof(uint16_t);
  case VertexAttributeType::VEC4U8:
    return 4 * sizeof(uint8_t);
  case VertexAttributeType::VEC3U8:
    return 3 * sizeof(uint8_t);
  case VertexAttributeType::VEC2U8:
    return 2 * sizeof(uint8_t);
  case VertexAttributeType::UINT:
    return sizeof(uint32_t);
  case VertexAttributeType::UINT16:
    return sizeof(uint16_t);
  case VertexAttributeType::UINT8:
    return sizeof(uint8_t);
  }
  return 0;
}

constexpr VkFormat format_of(VertexAttributeType attrType) {
  switch (attrType) {
  case VertexAttributeType::VEC4F:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case VertexAttributeType::VEC3F:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case VertexAttributeType::VEC2F:
    return VK_FORMAT_R32G32_SFLOAT;
  case VertexAttributeType::FLOAT:
    return VK_FORMAT_R32_SFLOAT;
  case VertexAttributeType::RGBA8_UNORM:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case VertexAttributeType::VEC4U:
    return VK_FORMAT_R32G32B32A32_UINT;
  case VertexAttributeType::VEC3U:
    return VK_FORMAT_R32G32B32_UINT;
  case VertexAttributeType::VEC2U:
    return VK_FORMAT_R32G32_UINT;
  case VertexAttributeType::VEC4U16:
    return VK_FORMAT_R16G16B16A16_UINT;
  case VertexAttributeType::VEC3U16:
    return VK_FORMAT_R16G16B16_UINT;
  case VertexAttributeType::VEC2U16:
    return VK_FORMAT_R16G16_UINT;
  case VertexAttributeType::VEC4U8:
    return VK_FORMAT_R8G8B8A8_UINT;
  case VertexAttributeType::VEC3U8:
    return VK_FORMAT_R8G8B8_UINT;
  case VertexAttributeType::VEC2U8:
    return VK_FORMAT_R8G8_UINT;
  case VertexAttributeType::UINT:
    return VK_FORMAT_R32_UINT;
  case VertexAttributeType::UINT16:
    return VK_FORMAT_R16_UINT;
  case VertexAttributeType::UINT8:
    return VK_FORMAT_R8_UINT;
  }

  return VK_FORMAT_MAX_ENUM;
}

constexpr uint32_t locations_hold(VertexAttributeType attrType) {
  switch (attrType) {
  case VertexAttributeType::VEC4F:
  case VertexAttributeType::VEC3F:
  case VertexAttributeType::VEC2F:
  case VertexAttributeType::FLOAT:
  case VertexAttributeType::RGBA8_UNORM:
  case VertexAttributeType::VEC4U:
  case VertexAttributeType::VEC3U:
  case VertexAttributeType::VEC2U:
  case VertexAttributeType::VEC4U16:
  case VertexAttributeType::VEC3U16:
  case VertexAttributeType::VEC2U16:
  case VertexAttributeType::VEC4U8:
  case VertexAttributeType::VEC3U8:
  case VertexAttributeType::VEC2U8:
  case VertexAttributeType::UINT:
  case VertexAttributeType::UINT16:
  case VertexAttributeType::UINT8:
    return 1u;
  }
  return 0xFFFFFFFF;
}

/** @Concept: Any struct used as attribute holder for Vertex Buffer should be
 * AttributeArrayLike */
template <typename T>
concept AttributeArrayLike = requires(T array) {
                               // Static constexpr member required to query
                               // attribute count
                               { T::count() } -> std::same_as<uint32_t>;
                               // Static constexpr member required to query
                               // every attribute type
                               {
                                 (T::getAttrType(0))
                                 } -> std::same_as<VertexAttributeType>;
                             };

template <AttributeArrayLike T> struct is_attribute_array_valid_size {
  /**
   * @template: is_attribute_array_valid_size<T>
   *
   * Checks if actual structure size (sizeof(T)) matches sum of every attribute
   * size
   *
   */
private:
  constexpr static uint64_t sumSize(uint64_t elem) {
    auto size = size_of(T::getAttrType(elem));
    if (elem == 0)
      return size;

    return sumSize(elem - 1) + size;
  }

  constexpr static bool validate() {
    return sizeof(T) == sumSize(T::count() - 1);
  }

public:
  constexpr static const bool value = validate();
};

/** @Concept: Any struct used by Vertex Buffer should satisfy AttributeArray
 * restrictions */
template <typename T>
concept AttributeArray = is_attribute_array_valid_size<T>::value;

template <vkw::VertexAttributeType... attrs> class AttributeBase {
public:
  /**
   * Base class for any vertex attribute placeholders
   *
   * e.g.:
   *    struct MyAttributes: public AttributeBase<VEC4F, VEC3F>{
   *       glm::vec4 pos;
   *       float color[3];
   *    };
   *
   *    VertexBuffer<MyAttributes> v_buf{...}; //OK
   *
   */

  constexpr static uint32_t count() { return sizeof...(attrs); }
  constexpr static vkw::VertexAttributeType getAttrType(uint32_t elem) {
    return m_attrs[elem];
  }

private:
  constexpr static const vkw::VertexAttributeType m_attrs[sizeof...(attrs)] = {
      attrs...};
};

template <AttributeArray T> class VertexBuffer : public Buffer<T> {
public:
  VertexBuffer(DeviceAllocator &allocator, uint64_t count,
               AllocationCreateInfo const &createInfo,
               VkBufferUsageFlags usage = 0,
               SharingInfo const &sharingInfo = {}) noexcept(ExceptionsDisabled)
      : Buffer<T>(allocator, count, usage | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  createInfo, sharingInfo) {}

private:
};

/** @template: type map VkIndexType<->(unsigned int sized type) */
template <VkIndexType type> struct vkr_index_type {};

template <> struct vkr_index_type<VK_INDEX_TYPE_UINT32> {
  typedef uint32_t Type;
};

template <> struct vkr_index_type<VK_INDEX_TYPE_UINT16> {
  typedef uint16_t Type;
};

template <> struct vkr_index_type<VK_INDEX_TYPE_UINT8_EXT> {
  typedef uint8_t Type;
};

template <VkIndexType type>
class IndexBuffer : public Buffer<typename vkr_index_type<type>::Type> {
public:
  IndexBuffer(DeviceAllocator &allocator, uint64_t count,
              AllocationCreateInfo const &createInfo,
              VkBufferUsageFlags usage = 0,
              SharingInfo const &sharingInfo = {}) noexcept(ExceptionsDisabled)
      : Buffer<typename vkr_index_type<type>::Type>(
            allocator, count, usage | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            createInfo, sharingInfo) {}
};
} // namespace vkw
#endif // VKRENDERER_VERTEXBUFFER_HPP
