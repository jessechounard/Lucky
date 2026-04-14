#pragma once

#include <SDL3/SDL_events.h>

namespace LuckyDemos {

/**
 * Abstract base class for all demos.
 *
 * Each demo subclass constructs its own GraphicsDevice, BatchRenderer,
 * shaders, and any other engine resources it needs. The launcher owns only
 * the SDL window and event loop, and forwards events / update / draw calls
 * to the currently active demo.
 *
 * To signal that the app should switch demos or exit, demos call the
 * protected helpers Request*(). The launcher checks the pending request
 * after each Update call and acts on it.
 */
class DemoBase {
  public:
    virtual ~DemoBase() = default;

    virtual void HandleEvent(const SDL_Event &event) = 0;
    virtual void Update(float deltaSeconds) = 0;
    virtual void Draw() = 0;

    enum class Request {
        None,
        Exit,
        NextDemo,
        PrevDemo,
    };

    Request GetPendingRequest() const {
        return pendingRequest;
    }

    void ClearPendingRequest() {
        pendingRequest = Request::None;
    }

  protected:
    void RequestExit() {
        pendingRequest = Request::Exit;
    }

    void RequestNextDemo() {
        pendingRequest = Request::NextDemo;
    }

    void RequestPrevDemo() {
        pendingRequest = Request::PrevDemo;
    }

  private:
    Request pendingRequest = Request::None;
};

} // namespace LuckyDemos
