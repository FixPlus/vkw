#include <filesystem>
#include <fstream>
#include <iostream>
#include <vkw/Device.hpp>
#include <vkw/SPIRVModule.hpp>

class ShaderModuleLoadError : public std::runtime_error {
public:
  ShaderModuleLoadError(const std::filesystem::path &path,
                        std::string_view what)
      : std::runtime_error([&]() {
          std::stringstream ss;
          ss << "Failed to load shader module " << path << ": " << what;
          return ss.str();
        }()) {}
};
namespace {
vkw::SPIRVModule loadModule(std::filesystem::path shaderPath) {
  std::ifstream shaderFile{shaderPath, std::ios::binary | std::ios::ate};
  if (!shaderFile)
    throw ShaderModuleLoadError(shaderPath, "cannot read file");
  size_t fileSize = shaderFile.tellg();
  if (fileSize % sizeof(uint32_t)) {
    throw ShaderModuleLoadError(shaderPath,
                                "file must have size aligned to WORD");
  }
  shaderFile.seekg(0);
  std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
  shaderFile.read(reinterpret_cast<char *>(code.data()), fileSize);
  if (!shaderFile)
    throw ShaderModuleLoadError(shaderPath, "read operation failed");
  return vkw::SPIRVModule{code};
}

vkw::Instance createInstance(vkw::Library &library) {
  vkw::InstanceCreateInfo createInfo{};
  createInfo.applicationName = "hello_world";
  createInfo.engineName = "hello_world";
  createInfo.applicationVersion = vkw::ApiVersion{1, 0, 0};
  createInfo.engineVersion = vkw::ApiVersion{1, 0, 0};
  createInfo.requestApiVersion(vkw::ApiVersion{1, 3, 0});

  return vkw::Instance{library, createInfo};
}

vkw::Device createDevice(vkw::Instance &instance) {
  auto phDevs = vkw::PhysicalDevice::enumerate(instance);
  if (phDevs.empty())
    throw std::runtime_error(
        "Cannot create device: there is no physical devices");
  for (auto &&phDev : vkw::PhysicalDevice::enumerate(instance)) {
    auto &props = phDev.properties();
    std::cout << props.deviceName << " : " << vkw::ApiVersion{props.apiVersion}
              << std::endl;
  }
  auto &chosenDevice = phDevs.front();
  return vkw::Device(instance, chosenDevice);
}

} // namespace
int main() try {
  vkw::Library library;
  std::cout << "vkw runtime version: " << library.runtimeVersion() << std::endl;
  std::cout << "Vulkan loader version: " << library.instanceAPIVersion()
            << std::endl;

  for (auto &&ext : library.extensions()) {
    std::cout << ext.extensionName << " : " << ext.specVersion << std::endl;
  }

  auto instance = createInstance(library);
  auto device = createDevice(instance);

  auto my_module = loadModule("shader.spv");
  vkw::SPIRVModuleInfo info{my_module};
  for (auto &&ep : info.entryPoints()) {
    std::cout << "Entry point: " << ep.name() << std::endl;
    std::cout << "interface vars: " << std::endl;
    for (auto &&iv : ep.interfaceVariables()) {
      std::cout << iv.name() << " loc = " << iv.location() << std::endl;
    }
    for (auto &&set : ep.sets()) {
      std::cout << "Set at index #" << set.index() << ":" << std::endl;
      for (auto &&binding : set.bindings()) {
        std::cout << "  bnd #" << binding.index() << ": '" << binding.name()
                  << "'" << std::endl;
      }
    }
  }

  std::cout << "Press any key to close..." << std::endl;
  char any;
  std::cin >> any;
  return 0;
} catch (vkw::Error &e) {
  std::cerr << "vkw: " << e.what() << std::endl;
  return 1;
} catch (std::runtime_error &e) {
  std::cerr << "std::runtime_error: " << e.what() << std::endl;
  return 1;
}