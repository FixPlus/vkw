#ifndef VKWRAPPER_DESCRIPTORPOOL_HPP
#define VKWRAPPER_DESCRIPTORPOOL_HPP

#include <vkw/Containers.hpp>
#include <vkw/Device.hpp>

#include <span>

namespace vkw {

class DescriptorSetLayout;
class DescriptorSet;

class DescriptorPoolInfo {
public:
  DescriptorPoolInfo(
      uint32_t maxSets, std::span<const VkDescriptorPoolSize> poolSizes,
      VkDescriptorPoolCreateFlags flags = 0) noexcept(ExceptionsDisabled) {
    std::copy(poolSizes.begin(), poolSizes.end(),
              std::back_inserter(m_poolSizes));
    m_createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    m_createInfo.pNext = nullptr;
    m_createInfo.flags = flags;
    m_createInfo.maxSets = maxSets;
    m_createInfo.poolSizeCount = m_poolSizes.size();
    m_createInfo.pPoolSizes = m_poolSizes.data();
  }

  uint32_t maxSets() const noexcept { return m_createInfo.maxSets; }

  auto &info() const noexcept { return m_createInfo; }

private:
  cntr::vector<VkDescriptorPoolSize, 3> m_poolSizes;
  VkDescriptorPoolCreateInfo m_createInfo{};
};

class DescriptorPool : public DescriptorPoolInfo, public vk::DescriptorPool {
public:
  DescriptorPool(
      Device const &device, uint32_t maxSets,
      std::span<const VkDescriptorPoolSize> poolSizes,
      VkDescriptorPoolCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : DescriptorPoolInfo(maxSets, poolSizes, flags), vk::DescriptorPool(
                                                           device, info()) {}

  uint32_t currentSetsCount() const noexcept { return m_setCount; }

private:
  VkDescriptorSet
  allocateSet(VkDescriptorSetLayout layout) noexcept(ExceptionsDisabled) {
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.descriptorPool = handle();
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    VK_CHECK_RESULT(parent().core<1, 0>().vkAllocateDescriptorSets(
        parent(), &allocateInfo, &set))

    m_setCount++;

    return set;
  }

  void freeSet(VkDescriptorSet set) noexcept {
    if (!(info().flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT))
      return;

    // This function is meant to be called inside the DescriptorSet destructor.
    // That means it is not allowed to pass any exceptions out.
    // That's why irrecoverableError() is issued instead.
    try {
      VK_CHECK_RESULT(parent().core<1, 0>().vkFreeDescriptorSets(
          parent(), handle(), 1, &set))
    } catch (VulkanError &e) {
      irrecoverableError(e);
    }
    m_setCount--;
  }

  uint32_t m_setCount = 0;

  friend class DescriptorSet;
};

} // namespace vkw
#endif // VKWRAPPER_DESCRIPTORPOOL_HPP
