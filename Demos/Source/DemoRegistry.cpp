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
    MutableRegistry().push_back(demo);
}

void DemoRegistry::Sort() {
    auto &demos = MutableRegistry();
    std::sort(demos.begin(), demos.end(), [](const Demo &a, const Demo &b) {
        return std::strcmp(a.name, b.name) < 0;
    });
}

const std::vector<Demo> &DemoRegistry::All() {
    return MutableRegistry();
}

} // namespace LuckyDemos
