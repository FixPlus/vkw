# vkw

vkw - Vulkan API Wrapper. It is lightweight C++ wrapper over Vulkan library. This project is work in progress.

## Prerequisites

* Compiler: gcc 12.0+ / msvc 19.44 (core c++20 minimum)
* CMake: 3.30+
* Boost: 1.80+
* Python 3.10+
* Vulkan SDK 1.4.328+: https://vulkan.lunarg.com/

Older versions of components may work, but are not tested with.

## Download & Build

First, install all dependencies listed above.

### bash:

```bash
git clone https://github.com/FixPlus/VkWrapper.git --recursive
cd VkWrapper
cmake -B build -S .
cmake --build build
cmake --install build
```

### IDEs and other platforms:

Open it as a cmake project and follow instructions from your IDE.

## Usage

### Import in cmake projects

Use find package

```cmake
find_package(vkw <version>)
```

Link your project to it:

```cmake
target_link_libraries(<target name> PUBLIC vkw::vkwrt)
```

### Use in code

API is divided in a few headers, you can include what you need and use:

```c++
#include <vkw/Library.hpp>

int main(){
    auto lib = vkw::Library();
    std::cout << "Loaded vulkan: " << lib.instanceAPIVersion() << "\n";
}
```

## Features

* ### Dynamic vulkan loader
    No hard linking to system vulkan loader (just like in volk). Symbols are fetched according to requested API version. Drivers symbols are fetched directly from driver, skipping instance level dispatch. Extensions and layer's symbols are fetched automatically when used.
* ### RAII encapsulated vulkan objects
    All objects are wrapped in unique_ptr like structures that manage objects' lifetime.
* ### Reduced boiler-plate code
    Vulkan countless structure filling routines are wrapped into methods with easy api.
* ### Extendability
    Although this library encapsulates many manual work with Vulkan C API, it still exposes it for users to extend current implementation by writing new derived classes and methods using C API.
* ### Embedded SPIRV-Link and SPIRV-Reflect
    Tools for linking and inspecting spirv shader modules are included in this library api, allowing to build and manage shader libraries.
* ### C ABI shared library
    Majority of this library's code is c-plus-plus headers, however some functionality requires to be wrapped into shared library. It's binary interface is full C ABI and it can be safely re-used and distributed. 