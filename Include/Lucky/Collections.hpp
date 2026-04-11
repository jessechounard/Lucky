#include <map>

namespace Lucky {

template <typename Map> bool Contains(const Map &map, const typename Map::key_type &key) {
    auto it = map.find(key);
    return it != map.end();
}

} // namespace Lucky