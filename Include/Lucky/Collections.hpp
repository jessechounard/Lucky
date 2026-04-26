#pragma once

#include <map>

namespace Lucky {

/**
 * Returns true if `map` contains `key`.
 *
 * Wraps the find-then-compare idiom for any associative container that
 * exposes `find`, `end`, and `key_type` (std::map, std::unordered_map,
 * std::set, etc.). C++20's `map.contains(key)` makes this redundant; the
 * helper exists because Lucky targets C++17.
 */
template <typename Map> bool Contains(const Map &map, const typename Map::key_type &key) {
    auto it = map.find(key);
    return it != map.end();
}

} // namespace Lucky
