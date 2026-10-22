#ifndef TOGR_GUISYSTEM_HPP
#define TOGR_GUISYSTEM_HPP
#pragma once

#include "imgui/imgui.h"
#include <string>

struct GLFWwindow;

namespace togr {

class Camera;
class RenderSystem;
class ResourceManager;
class SelectionModel;
class Shader;

class GuiSystem {
public:
    // Lifecycle: main thread with this window's GL context current and entry
    // points loaded. The window is required and borrowed through shutdown().
    // Call shutdown() once after init() returns, before the context is destroyed;
    // it releases any fly-camera cursor capture and the window borrow. The default
    // destructor does not shut down the ImGui backends/context.
    GuiSystem() = default;
    ~GuiSystem() = default;
    void init(GLFWwindow* window);
    void shutdown();

    // Non-owning dependencies; injected owners must outlive their GUI use.
    // ResourceManager and its built-in shaders are required before render().
    void setResourceManager(ResourceManager* rm) { rm_ = rm; }

    // Optional renderer: null omits the viewport image and profiling controls.
    // Optional selection: null disables selection/inspector access.
    // Optional camera: null omits movement/zoom; a renderer is also needed for
    // the viewport controls. The initialized window still serves the GUI backend.
    void setRenderSystem(RenderSystem* rs) { renderSystem_ = rs; }
    void setSelectionModel(SelectionModel* s) { selection_ = s; }
    void setCamera(Camera* c) { camera_ = c; }

    // Frame operations: after init(), call beginFrame() then render() on the main
    // thread with current GL. Engine renders the scene between these calls, so
    // panel edits, viewport size reports and camera changes affect its next draw.
    void beginFrame();
    void render(double deltaTime);

private:
    void showDockspace();
    void processModelImportDialog();

    // Dockable panels and their content; each show*Panel() is an ImGui window.
    void showScenePanel();
    void showInspectorPanel();
    void showRenderSettingsPanel(double deltaTime);
    void showAssetsPanel();
    void showViewportPanel(double deltaTime);
    void drawAddMenu();
    void drawSceneTree();
    void drawInspector();
    void drawFrameRateControls(double deltaTime);
    void drawRenderOptions();
    void drawShaderOption(const char* label, const char* name, Shader& shader);
    void drawProfilingControls();
    void drawModelsTab();
    void drawConsoleTab();
    void updateViewportCamera(bool hovered, double deltaTime);

    // Default dock layout is created once when no saved imgui.ini exists.
    void buildDefaultLayoutOnce(ImGuiID dockspaceId);

    struct FrameRateState {
        double elapsedSeconds = 0.0;
        int frameCount = 0;
        double framesPerSecond = 0.0;
    };

    struct ConsoleState {
        bool showInfo = true;
        bool showWarn = true;
        bool showErr = true;
        bool autoScroll = true;
        ImGuiTextFilter textFilter;
        unsigned long long lastSeenRevision = 0;
    };

    // Borrowed systems (see injection contracts above). SelectionModel itself
    // observes the selected object weakly; GUI does not own the selection model.
    SelectionModel* selection_ = nullptr;
    RenderSystem* renderSystem_ = nullptr;
    ResourceManager* rm_ = nullptr;
    Camera* camera_ = nullptr;

    // Viewport interaction and settings shown by the panels.
    bool flying_ = false; // RMB held after pressing over the Viewport.
    bool vsyncChecked_ = true;
    float exposure_ = 1.0f;
    std::string renderSettingsError_;
    FrameRateState frameRate_;
    // Filters and revision advance only when the Console tab is actually drawn.
    ConsoleState console_;
    GLFWwindow* window_ = nullptr; // Borrowed window; also used for cursor lock.
    // Prior cursor position while flying.
    double lastCursorX_ = 0.0;
    double lastCursorY_ = 0.0;
};

} // namespace togr

#endif
