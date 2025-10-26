#ifndef VKWRAPPER_SAMPLER_HPP
#define VKWRAPPER_SAMPLER_HPP

#include <vkw/Device.hpp>

namespace vkw {

class Sampler : public vk::Sampler {
public:
  Sampler(Device const &device,
          VkSamplerCreateInfo createInfo) noexcept(ExceptionsDisabled)
      : vk::Sampler(device, createInfo), m_createInfo(createInfo) {}

  auto &info() const noexcept { return m_createInfo; }

private:
  VkSamplerCreateInfo m_createInfo{};
};
} // namespace vkw
#endif // VKWRAPPER_SAMPLER_HPP
