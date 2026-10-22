#ifndef TOGR_ENGINE_HPP
#define TOGR_ENGINE_HPP
#pragma once

#include "assets/resourceManager.hpp"
#include "editor/guiSystem.hpp"
#include "platform/input.hpp"
#include "platform/windowSystem.hpp"
#include "rendering/camera.hpp"
#include "rendering/renderSystem.hpp"
#include "scene/selectionModel.hpp"

namespace togr {

class Engine {
public:
    // Lifecycle: use on the main thread. init() creates the window/context before
    // GL resources; call it once before run(). Errors propagate after shutdown()
    // releases the systems whose cleanup obligations have been registered.
    Engine() = default;
    ~Engine();
    void init();

    // Requires successful init(). Runs frames until close, then shuts down.
    // GUI edits, camera controls and viewport sizing affect the next scene draw.
    void run();

private:
    // Cleanup keeps the window's context current, then releases GUI, renderer,
    // resources and finally the window. Safe after run() or a prior cleanup.
    void shutdown();
    double getDeltaTime();

    // Owned systems; declaration order is retained for member destruction.
    Input input;
    Camera camera;
    WindowSystem windowSystem;
    ResourceManager resourceManager;
    SelectionModel selectionModel;
    RenderSystem renderSystem;
    GuiSystem guiSystem;

    // Cleanup obligations, not success states. Resource cleanup is armed BEFORE
    // init() because shader registration can leave partial resources on failure.
    // The other flags are armed after their init() returns; RenderSystem handles
    // its own partial initialization failure before control returns to Engine.
    bool windowCleanupRequired_ = false;
    bool renderCleanupRequired_ = false;
    bool resourcesCleanupRequired_ = false;
    bool guiCleanupRequired_ = false;
};

} // namespace togr

#endif // !TOGR_ENGINE_HPP
