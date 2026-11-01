#pragma once

#include <boost/mp11.hpp>

/**
 * Creates std::variant from index given in runtime.
 */
template<typename V>
inline auto createVariant(size_t index) -> V
{
    return boost::mp11::mp_with_index<boost::mp11::mp_size<V>>(
      index, [](auto I) { return V(std::in_place_index<I>); });
}
