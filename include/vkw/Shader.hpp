#ifndef VKRENDERER_SHADER_HPP
#define VKRENDERER_SHADER_HPP

#include <vkw/Device.hpp>
#include <vkw/SPIRVModule.hpp>

#include <functional>

namespace vkw {

class BadShaderModule : public Error {
public:
  BadShaderModule(std::string_view what) noexcept : Error(what) {}
  std::string_view codeString() const noexcept override {
    return "Bad shader module";
  }
};
class ShaderBase : public vk::ShaderModule {
private:
  static std::string_view shaderStageStr(VkShaderStageFlagBits stage) {
    switch (stage) {
#define CASE(X)                                                                \
  case (X):                                                                    \
    return #X;
      CASE(VK_SHADER_STAGE_VERTEX_BIT)
      CASE(VK_SHADER_STAGE_FRAGMENT_BIT)
      CASE(VK_SHADER_STAGE_COMPUTE_BIT)
    default:
      return "BAD_STAGE_ID";
    }
  }

public:
  ShaderBase(Device const &device, SPIRVModule const &module,
             VkShaderStageFlagBits stage,
             VkShaderModuleCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : vk::ShaderModule(device,
                         [&]() {
                           VkShaderModuleCreateInfo createInfo{};
                           createInfo.sType =
                               VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                           createInfo.pNext = nullptr;
                           createInfo.flags = flags;
                           createInfo.codeSize = module.code().size() * 4;
                           createInfo.pCode = module.code().data();
                           return createInfo;
                         }()),
        m_stage(stage) {}

  VkShaderStageFlagBits stage() const noexcept { return m_stage; }

  /// @brief Checks that module has only one entry point and its stage
  /// is expectedStage.
  /// @param module to verify.
  /// @param expectedStage shader stage thi module should define.
  /// @throw BadShaderModule if conditions are unmet.
  static SPIRVModule const &checkModule(
      SPIRVModule const &module,
      VkShaderStageFlagBits expectedStage) noexcept(ExceptionsDisabled) {
    auto info = SPIRVModuleInfo{module};
    if (info.entryPoints().size() != 1)
      postError(BadShaderModule{[&]() {
        std::stringstream ss;
        ss << "Bad shader module: unexpected entry point count("
           << info.entryPoints().size() << "). Expected 1.";
        return ss.str();
      }()});

    auto moduleStage = (*info.entryPoints().begin()).stage();
    if (moduleStage != expectedStage)
      postError(BadShaderModule{[&]() {
        std::stringstream ss;
        ss << "Bad shader module: shader stage mismatch.\n";
        ss << "  Expected: " << shaderStageStr(expectedStage) << "\n";
        ss << "  Got: " << shaderStageStr(moduleStage) << "\n";
        return ss.str();
      }()});
    return module;
  }

private:
  VkShaderStageFlagBits m_stage;
};

template <VkShaderStageFlagBits STAGE> class Shader : public ShaderBase {
public:
  Shader(Device const &device, SPIRVModule const &module,
         VkShaderModuleCreateFlags flags = 0) noexcept(ExceptionsDisabled)
      : ShaderBase(device, ShaderBase::checkModule(module, STAGE), STAGE,
                   flags){};
};

using FragmentShader = Shader<VK_SHADER_STAGE_FRAGMENT_BIT>;
using VertexShader = Shader<VK_SHADER_STAGE_VERTEX_BIT>;
using ComputeShader = Shader<VK_SHADER_STAGE_COMPUTE_BIT>;

} // namespace vkw
#endif // VKRENDERER_SHADER_HPP
