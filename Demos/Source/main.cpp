#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include <cstring>
#include <memory>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <tracy/Tracy.hpp>

#include "DemoBase.hpp"
#include "DemoRegistry.hpp"

struct Constants {
    static constexpr int WindowWidth = 1280;
    static constexpr int WindowHeight = 720;
};

struct Context {
    SDL_Window *window = nullptr;
    std::unique_ptr<LuckyDemos::DemoBase> currentDemo;
    int currentDemoIndex = 0;
    uint64_t previousTicks = 0;
    double ticksPerSecond = 0;
};

void SwitchDemo(Context *context, int newIndex) {
    const auto &demos = LuckyDemos::DemoRegistry::All();
    context->currentDemo.reset();
    context->currentDemoIndex = newIndex;
    context->currentDemo = demos[newIndex].create(context->window);
}

SDL_AppResult SDL_AppInit(void **state, int argc, char *argv[]) {
#if defined(_WIN32) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    LuckyDemos::DemoRegistry::Sort();
    const auto &demos = LuckyDemos::DemoRegistry::All();
    if (demos.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No demos registered.");
        return SDL_APP_FAILURE;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    Context *context = new Context();
    context->window = SDL_CreateWindow(
        "Lucky Demos", Constants::WindowWidth, Constants::WindowHeight, SDL_WINDOW_RESIZABLE);
    if (!context->window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        delete context;
        return SDL_APP_FAILURE;
    }

    context->currentDemo = demos[context->currentDemoIndex].create(context->window);
    context->previousTicks = SDL_GetPerformanceCounter();
    context->ticksPerSecond = static_cast<double>(SDL_GetPerformanceFrequency());

    *state = context;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *state, SDL_Event *event) {
    Context *context = static_cast<Context *>(state);
    const auto &demos = LuckyDemos::DemoRegistry::All();

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat &&
        (event->key.key == SDLK_PAGEUP || event->key.key == SDLK_PAGEDOWN)) {
        const int count = static_cast<int>(demos.size());
        int delta = (event->key.key == SDLK_PAGEDOWN) ? 1 : -1;
        SwitchDemo(context, (context->currentDemoIndex + count + delta) % count);
    } else {
        context->currentDemo->HandleEvent(*event);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *state) {
    Context *context = static_cast<Context *>(state);
    const auto &demos = LuckyDemos::DemoRegistry::All();

    uint64_t currentTicks = SDL_GetPerformanceCounter();
    float deltaSeconds =
        static_cast<float>((currentTicks - context->previousTicks) / context->ticksPerSecond);
    context->previousTicks = currentTicks;

    context->currentDemo->Update(deltaSeconds);

    LuckyDemos::DemoBase::Request request = context->currentDemo->GetPendingRequest();
    if (request != LuckyDemos::DemoBase::Request::None) {
        context->currentDemo->ClearPendingRequest();
        switch (request) {
        case LuckyDemos::DemoBase::Request::Exit:
            return SDL_APP_SUCCESS;
        case LuckyDemos::DemoBase::Request::NextDemo: {
            const int count = static_cast<int>(demos.size());
            SwitchDemo(context, (context->currentDemoIndex + 1) % count);
            break;
        }
        case LuckyDemos::DemoBase::Request::PrevDemo: {
            const int count = static_cast<int>(demos.size());
            SwitchDemo(context, (context->currentDemoIndex + count - 1) % count);
            break;
        }
        case LuckyDemos::DemoBase::Request::None:
            break;
        }
    }

    context->currentDemo->Draw();

    FrameMark;
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *state, SDL_AppResult result) {
    (void)result;
    Context *context = static_cast<Context *>(state);
    if (context) {
        context->currentDemo.reset();
        SDL_DestroyWindow(context->window);
        delete context;
    }
}
