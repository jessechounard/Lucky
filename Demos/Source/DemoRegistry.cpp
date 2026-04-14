#include <algorithm>
#include <cstring>

#include "DemoRegistry.hpp"

namespace LuckyDemos {

namespace {

// Using a function-local static guarantees the vector exists before any
// LUCKY_DEMO static initializer runs, regardless of translation-unit init
// order.
std::vector<Demo> &MutableRegistry() {
    static std::vector<Demo> demos;
    return demos;
}

} // namespace

void DemoRegistry::Add(const Demo &demo) {
    auto &demos = MutableRegistry();
    demos.push_back(demo);
    // Sort by name so the launcher presents demos in a stable order
    // regardless of the linker-dependent static-init order.
    std::sort(demos.begin(), demos.end(), [](const Demo &a, const Demo &b) {
        return std::strcmp(a.name, b.name) < 0;
    });
}

const std::vector<Demo> &DemoRegistry::All() {
    return MutableRegistry();
}

} // namespace LuckyDemos
