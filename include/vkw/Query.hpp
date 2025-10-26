#ifndef VKWRAPPER_QUERY_HPP
#define VKWRAPPER_QUERY_HPP

#include <vkw/Device.hpp>

#include <span>

#undef min

namespace vkw {

class QueryPool : public vk::QueryPool {
public:
  QueryPool(
      vkw::Device &device,
      const VkQueryPoolCreateInfo &createInfo) noexcept(ExceptionsDisabled)
      : vk::QueryPool{device, createInfo}, m_size(createInfo.queryCount) {}

  void getResults(std::span<uint64_t> results) noexcept(ExceptionsDisabled) {
    m_getResultsImpl(results.data(), 0u, std::min(m_size, results.size()),
                     sizeof(uint64_t) * results.size(), sizeof(uint64_t),
                     VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  }

  void getResults(std::span<uint32_t> results) noexcept(ExceptionsDisabled) {
    m_getResultsImpl(results.data(), 0u, std::min(m_size, results.size()),
                     sizeof(uint32_t) * results.size(), sizeof(uint32_t),
                     VK_QUERY_RESULT_WAIT_BIT);
  }

  auto size() const { return m_size; }

private:
  void m_getResultsImpl(void *pData, uint32_t firstQuery, uint32_t queryCount,
                        size_t dataSize, size_t stride,
                        VkQueryResultFlags flags) noexcept(ExceptionsDisabled) {
    parent().core<1, 0>().vkGetQueryPoolResults(parent(), *this, firstQuery,
                                                queryCount, dataSize, pData,
                                                stride, flags);
  }
  size_t m_size;
};

class OcclusionQuery : public QueryPool {
public:
  OcclusionQuery(vkw::Device &device,
                 uint32_t count) noexcept(ExceptionsDisabled)
      : QueryPool(device, [&]() {
          VkQueryPoolCreateInfo createInfo{};
          createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
          createInfo.pNext = nullptr;
          createInfo.flags = 0u;
          createInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
          createInfo.queryCount = count;
          return createInfo;
        }()){};
};

} // namespace vkw
#endif // VKWRAPPER_QUERY_HPP
