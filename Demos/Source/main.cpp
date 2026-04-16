#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include <cstring>
#include <memory>

#include <SDL3/SDL.h>
#include <tracy/Tracy.hpp>

#include "DemoBase.hpp"
#include "DemoRegistry.hpp"

namespace {

constexpr int DefaultWidth = 1280;
constexpr int DefaultHeight = 720;

void PrintUsage() {
    SDL_Log("Usage: Lucky.Demos.exe [--demo <name>] [--list]");
    SDL_Log("");
    SDL_Log("Available demos:");
    for (const auto &demo : LuckyDemos::DemoRegistry::All()) {
        SDL_Log("  %-24s %s", demo.name, demo.description);
    }
}

int FindDemoIndexByName(const char *name) {
    const auto &demos = LuckyDemos::DemoRegistry::All();
    for (size_t i = 0; i < demos.size(); i++) {
        if (std::strcmp(demos[i].name, name) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void SetWindowTitle(SDL_Window *window, const LuckyDemos::Demo &demo) {
    char title[256];
    SDL_snprintf(title, sizeof(title), "Lucky.Demos - %s", demo.name);
    SDL_SetWindowTitle(window, title);
}

} // namespace

int main(int argc, char *argv[]) {
#if defined(_WIN32) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    const auto &demos = LuckyDemos::DemoRegistry::All();
    if (demos.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "No demos registered. Make sure at least one demo .cpp file is linked "
            "into Lucky.Demos.exe.");
        return 1;
    }

    // Parse command line.
    int initialDemoIndex = 0;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0 ||
            std::strcmp(arg, "--list") == 0) {
            PrintUsage();
            return 0;
        } else if (std::strcmp(arg, "--demo") == 0 && i + 1 < argc) {
            int found = FindDemoIndexByName(argv[i + 1]);
            if (found < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown demo: %s", argv[i + 1]);
                PrintUsage();
                return 1;
            }
            initialDemoIndex = found;
            i++; // skip the value we just consumed
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown argument: %s", arg);
            PrintUsage();
            return 1;
        }
    }

    // Initialize SDL and create the window. The graphics device is created
    // by each individual demo so they can serve as self-contained examples of
    // how to use Lucky.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window =
        SDL_CreateWindow("Lucky.Demos", DefaultWidth, DefaultHeight, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int currentDemoIndex = initialDemoIndex;
    std::unique_ptr<LuckyDemos::DemoBase> currentDemo = demos[currentDemoIndex].create(window);
    SetWindowTitle(window, demos[currentDemoIndex]);

    auto switchDemo = [&](int newIndex) {
        currentDemo.reset();
        currentDemoIndex = newIndex;
        currentDemo = demos[currentDemoIndex].create(window);
        SetWindowTitle(window, demos[currentDemoIndex]);
    };

    uint64_t previousTicks = SDL_GetPerformanceCounter();
    const double ticksPerSecond = static_cast<double>(SDL_GetPerformanceFrequency());

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN &&
                       (event.key.key == SDLK_UP || event.key.key == SDLK_DOWN)) {
                // Launcher-level demo navigation: Up = previous, Down = next.
                // Intercepted before the demo sees the event so every demo
                // gets navigation without duplicating the binding.
                const int count = static_cast<int>(demos.size());
                int delta = (event.key.key == SDLK_DOWN) ? 1 : -1;
                switchDemo((currentDemoIndex + count + delta) % count);
            } else {
                currentDemo->HandleEvent(event);
            }
        }

        uint64_t currentTicks = SDL_GetPerformanceCounter();
        float deltaSeconds = static_cast<float>((currentTicks - previousTicks) / ticksPerSecond);
        previousTicks = currentTicks;

        currentDemo->Update(deltaSeconds);

        // Apply any request the demo signaled during HandleEvent or Update.
        LuckyDemos::DemoBase::Request request = currentDemo->GetPendingRequest();
        if (request != LuckyDemos::DemoBase::Request::None) {
            currentDemo->ClearPendingRequest();
            switch (request) {
            case LuckyDemos::DemoBase::Request::Exit:
                running = false;
                break;
            case LuckyDemos::DemoBase::Request::NextDemo: {
                const int count = static_cast<int>(demos.size());
                switchDemo((currentDemoIndex + 1) % count);
                break;
            }
            case LuckyDemos::DemoBase::Request::PrevDemo: {
                const int count = static_cast<int>(demos.size());
                switchDemo((currentDemoIndex + count - 1) % count);
                break;
            }
            case LuckyDemos::DemoBase::Request::None:
                break;
            }

            if (!running) {
                break;
            }
        }

        currentDemo->Draw();

        FrameMark;
    }

    currentDemo.reset();

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
