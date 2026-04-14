#pragma once

#include <memory>
#include <vector>

#include <SDL3/SDL_video.h>

#include "DemoBase.hpp"

namespace LuckyDemos {

/**
 * A single registered demo.
 *
 * `create` is a factory function that instantiates the demo class, given a
 * host SDL window. The launcher calls this when the user picks the demo.
 */
struct Demo {
    using Factory = std::unique_ptr<DemoBase> (*)(SDL_Window *);

    const char *name;
    const char *description;
    Factory create;
};

/**
 * Global registry of demos populated at static-initialization time.
 *
 * Each demo translation unit registers itself via the LUCKY_DEMO macro.
 * The launcher calls All() to enumerate demos and dispatches to a chosen
 * demo's factory.
 */
class DemoRegistry {
  public:
    static void Add(const Demo &demo);
    static const std::vector<Demo> &All();
};

namespace detail {

// Helper called by LUCKY_DEMO to register a demo at static-init time. Returns
// a bool so it can be used as the initializer of a static const variable.
inline bool RegisterDemo(const Demo &demo) {
    DemoRegistry::Add(demo);
    return true;
}

} // namespace detail

} // namespace LuckyDemos

/**
 * Registers a demo with the global DemoRegistry at static-initialization.
 *
 * Usage at the bottom of a demo's .cpp file:
 *
 *     class MyDemo : public LuckyDemos::DemoBase { ... };
 *     LUCKY_DEMO("MyDemo", "Demonstrates something", MyDemo);
 *
 * The macro expands to a static const variable whose initializer runs before
 * main(), adding the demo to the registry.
 */
#define LUCKY_DEMO(NAME, DESCRIPTION, CLASS)                                                       \
    static const bool _luckyAutoReg_##CLASS = LuckyDemos::detail::RegisterDemo(LuckyDemos::Demo{   \
        NAME, DESCRIPTION, [](SDL_Window *window) -> std::unique_ptr<LuckyDemos::DemoBase> {       \
            return std::make_unique<CLASS>(window);                                                \
        }})
