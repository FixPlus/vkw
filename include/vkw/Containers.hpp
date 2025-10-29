#pragma once

#include <memory>
#include <vector>

#include <boost/container/small_vector.hpp>

namespace vkw::cntr {

template <typename T, int Amount, typename Allocator = std::allocator<T>>
using vector = boost::container::small_vector<T, Amount, Allocator>;

}