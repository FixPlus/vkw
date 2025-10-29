#ifndef VKWRAPPER_LAYERS_HPP
#define VKWRAPPER_LAYERS_HPP

#include <vkw/Instance.hpp>

namespace vkw {

template <layer id> class Layer {
public:
  explicit Layer(Instance const &instance) noexcept(ExceptionsDisabled) {
    if (!instance.isLayerEnabled(id))
      postError(LayerMissing(id, Library::LayerName(id)));
  }
};

} // namespace vkw
#endif // VKWRAPPER_LAYERS_HPP
