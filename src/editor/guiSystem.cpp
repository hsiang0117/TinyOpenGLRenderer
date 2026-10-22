#include "guiSystem.hpp"
#include "componentEditors.hpp"
#include "scene/component.hpp"
#include "scene/gameObject.hpp"
#include "scene/selectionModel.hpp"
#include "assets/model.hpp"
#include "rendering/camera.hpp"
#include "scene/gameObjectFactory.hpp"
#include "assets/resourceManager.hpp"
#include "assets/shaderCatalog.hpp"
#include "platform/windowSystem.hpp"
#include "rendering/renderSystem.hpp"
#include "rendering/renderLimits.hpp"
#include "rendering/opengl/shader.hpp"
#include "core/logger.hpp"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_internal.h"
#include "imgui/ImGuiFileDialog.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <vector>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#endif

namespace togr {

namespace {
    constexpr const char* kModelImportDialogKey = "ChooseFileDlgKey";

    std::filesystem::path defaultFontPath() {
#ifdef _WIN32
        std::wstring directory(MAX_PATH, L'\0');
        UINT length = GetWindowsDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
        if (length >= directory.size()) {
            directory.resize(length);
            length = GetWindowsDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
        }
        if (length == 0 || length >= directory.size()) return {};
        directory.resize(length);
        const auto path = std::filesystem::path(directory) / "Fonts" / "msyh.ttc";
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error)) return path;
#endif
        return {};
    }

    // Scene-tree classification predicates (extracted from the former inline lambdas).
    bool isLightObject(const GameObjectPtr& object) {
        return object->has<PointLightComponent>() ||
            object->has<DirectionalLightComponent>() ||
            object->has<SpotLightComponent>();
    }

    bool isSceneObject(const GameObjectPtr& object) {
        return object->has<ModelRenderComponent>() || object->has<CloudVolumeComponent>() ||
            (object->has<StaticMeshComponent>() && !object->has<PointLightComponent>() && !object->has<SkyboxComponent>());
    }

    bool isSkyboxObject(const GameObjectPtr& object) {
        return object->has<SkyboxComponent>();
    }

    void drawSceneCategory(const char* label, bool (*predicate)(const GameObjectPtr&),
        ResourceManager& resources, SelectionModel* selection, GameObjectPtr& selected) {
        if (ImGui::TreeNode(label)) {
            auto gameObjects = resources.getGameObjects();
            for (int i = 0; i < gameObjects.size(); i++) {
                GameObjectPtr object = gameObjects[i];
                if (predicate(object)) {
                    ImGui::PushID(i);
                    if (ImGui::Selectable(object->getName().c_str(), object == selected)) {
                        if (selection) selection->set(object);
                        selected = object;
                    }
                    if (ImGui::BeginPopupContextItem("ObjectButtonContext")) {
                        if (ImGui::MenuItem(u8"remove")) {
                            resources.removeGameObject(i);
                            // Only clear selection if the removed object was selected;
                            // removing a different object leaves the highlight intact.
                            if (object == selected) {
                                if (selection) selection->clear();
                                selected = nullptr;
                            }
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }
            }
            ImGui::TreePop();
        }
    }

    // Data-driven Add menu catalog. Adding a creatable object = one row here; no
    // hand-written menu code. canAdd (optional) gates + greys out the item.
    struct SpawnEntry {
        const char* category;
        const char* label;
        GameObjectPtr(*create)();
        bool(*canAdd)(ResourceManager&);   // nullptr => always allowed
    };

    const SpawnEntry kSpawnCatalog[] = {
        { "Light", "Point Light", &GameObjectFactory::createPointLight,
            [](ResourceManager& rm) { return rm.getPointLightCount() < RenderLimits::PointLights; } },
        { "Light", "Directional Light", &GameObjectFactory::createDirectionalLight,
            [](ResourceManager& rm) { return rm.getDirectionalLightCount() < RenderLimits::DirectionalLights; } },
        { "Light", "Spot Light", &GameObjectFactory::createSpotLight,
            [](ResourceManager& rm) { return rm.getSpotLightCount() < RenderLimits::SpotLights; } },
        { "Static Mesh", "Cube", &GameObjectFactory::createCube, nullptr },
        { "Static Mesh", "Plane", &GameObjectFactory::createPlane, nullptr },
        { "Static Mesh", "Sphere", &GameObjectFactory::createSphere, nullptr },
        { "Other", "Skybox", &GameObjectFactory::createSkybox, nullptr },
        { "Other", "Cloud Layer", &GameObjectFactory::createCloudVolume, nullptr },
    };
}

void GuiSystem::init(GLFWwindow* window) {
    window_ = window;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    const auto fontPath = defaultFontPath();
    if (!fontPath.empty()) {
        if (auto font = io.Fonts->AddFontFromFileTTF(fontPath.u8string().c_str(), 24.0f, NULL, io.Fonts->GetGlyphRangesChineseFull())) {
            io.FontDefault = font;
        }
    }
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");
    ImGui::StyleColorsDark();
}

void GuiSystem::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GuiSystem::buildDefaultLayoutOnce(ImGuiID dockspaceId) {
    // Runs once on first launch (no imgui.ini). If a node already exists (ini
    // restored or built last frame), leave the user's layout alone.
    if (ImGui::DockBuilderGetNode(dockspaceId) != nullptr) return;

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID center = dockspaceId;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);

    ImGui::DockBuilderDockWindow("Scene", left);
    ImGui::DockBuilderDockWindow("Inspector", left);          // stacked tab under Scene
    ImGui::DockBuilderDockWindow("Render Settings", right);
    ImGui::DockBuilderDockWindow("Assets", bottom);
    ImGui::DockBuilderDockWindow("Viewport", center);         // central node
    ImGui::DockBuilderFinish(dockspaceId);
}

void GuiSystem::render(double deltaTime) {
    showDockspace();
    showScenePanel();
    showInspectorPanel();
    showRenderSettingsPanel(deltaTime);
    showAssetsPanel();
    showViewportPanel(deltaTime);
    processModelImportDialog();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GuiSystem::showDockspace() {
    // Fullscreen, non-dockable host window that owns the dockspace.
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    buildDefaultLayoutOnce(dockspaceId);
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f));
    ImGui::End();
}

void GuiSystem::processModelImportDialog() {
    // Display the file dialog as a top-level (modal) window, after the panels so it
    // layers above them and receives input.
    if (ImGuiFileDialog::Instance()->Display(kModelImportDialogKey)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
            rm_->queueModelLoad(filePath);
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void GuiSystem::shutdown() {
    if (flying_ && window_) {
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    flying_ = false;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    window_ = nullptr;
}

void GuiSystem::showScenePanel() {
    ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_MenuBar);
    if (ImGui::BeginMenuBar()) {
        drawAddMenu();
        ImGui::EndMenuBar();
    }
    drawSceneTree();
    ImGui::End();
}

void GuiSystem::showInspectorPanel() {
    ImGui::Begin("Inspector");
    drawInspector();
    ImGui::End();
}

void GuiSystem::showViewportPanel(double deltaTime) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport");
    ImVec2 avail = ImGui::GetContentRegionAvail();
    int w = (int)avail.x;
    if (w < 1) w = 1;
    int h = (int)avail.y;
    if (h < 1) h = 1;
    if (renderSystem_) {
        // Report panel size (drives the scene FBO size on the next frame) and show
        // the rendered scene. UV flipped vertically: GL textures are bottom-up.
        renderSystem_->setViewportSize(w, h);
        ImTextureID texId = (ImTextureID)(intptr_t)renderSystem_->getSceneColorTextureId();
        ImGui::Image(texId, avail, ImVec2(0, 1), ImVec2(1, 0));

        const bool hovered = ImGui::IsItemHovered();
        updateViewportCamera(hovered, deltaTime);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void GuiSystem::updateViewportCamera(bool hovered, double deltaTime) {
    // Editor fly camera: hold RMB over the viewport to look (mouse) + move
    // (WASD/Space/LeftCtrl); wheel zooms the FOV. Latched so the look keeps working
    // even if the cursor drifts off the image; releasing RMB anywhere exits.
    const bool wasFlying = flying_;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) flying_ = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) flying_ = false;
    // A modal popup (e.g. the file dialog) owns input exclusively: never fly under it.
    if (ImGui::GetTopMostPopupModal() != nullptr) flying_ = false;

    // Lock + hide the OS cursor while flying so it cannot leave the window and stop
    // the rotation at a screen edge. GLFW_CURSOR_DISABLED reports unbounded virtual
    // coordinates, so we drive the look from our own per-frame cursor delta (not
    // io.MouseDelta, which is clamped to the window). The ImGui GLFW backend
    // early-returns from cursor management while DISABLED, so it won't fight us.
    if (window_ && flying_ && !wasFlying) {
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwGetCursorPos(window_, &lastCursorX_, &lastCursorY_);
    }
    if (window_ && !flying_ && wasFlying) {
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    if (flying_ && camera_ && window_) {
        double cx, cy;
        glfwGetCursorPos(window_, &cx, &cy);
        camera_->lookDelta((float)(cx - lastCursorX_), (float)(cy - lastCursorY_));
        lastCursorX_ = cx;
        lastCursorY_ = cy;
        if (ImGui::IsKeyDown(ImGuiKey_W)) camera_->processKeyboard(FRONT, deltaTime);
        if (ImGui::IsKeyDown(ImGuiKey_S)) camera_->processKeyboard(BACK, deltaTime);
        if (ImGui::IsKeyDown(ImGuiKey_A)) camera_->processKeyboard(LEFT, deltaTime);
        if (ImGui::IsKeyDown(ImGuiKey_D)) camera_->processKeyboard(RIGHT, deltaTime);
        if (ImGui::IsKeyDown(ImGuiKey_Space)) camera_->processKeyboard(UP, deltaTime);
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) camera_->processKeyboard(DOWN, deltaTime);
        ImGui::SetNextFrameWantCaptureMouse(true);
    }

    // Wheel zooms FOV when hovering the viewport, or while flying (RMB held), where
    // the cursor is locked so IsItemHovered() no longer reports true.
    if ((hovered || flying_) && camera_) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) camera_->zoom(wheel);
    }
}

void GuiSystem::drawAddMenu() {
    if (ImGui::BeginMenu("Add")) {
        // Distinct categories in first-seen order.
        std::vector<const char*> categories;
        for (const auto& e : kSpawnCatalog) {
            bool seen = false;
            for (const char* c : categories) {
                if (strcmp(c, e.category) == 0) {
                    seen = true;
                    break;
                }
            }
            if (!seen) categories.push_back(e.category);
        }
        for (const char* cat : categories) {
            if (ImGui::BeginMenu(cat)) {
                for (const auto& e : kSpawnCatalog) {
                    if (strcmp(e.category, cat) != 0) continue;
                    bool ok = !e.canAdd || e.canAdd(*rm_);
                    if (ImGui::MenuItem(e.label, nullptr, false, ok)) {
                        rm_->addGameObject(e.create());
                    }
                }
                ImGui::EndMenu();
            }
        }
        ImGui::EndMenu();
    }
}

void GuiSystem::drawSceneTree() {
    // Snapshot the current selection once; highlight is decided by identity.
    GameObjectPtr selected = selection_ ? selection_->get() : nullptr;
    drawSceneCategory(u8"Light", isLightObject, *rm_, selection_, selected);
    drawSceneCategory(u8"Object", isSceneObject, *rm_, selection_, selected);
    drawSceneCategory(u8"Skybox", isSkyboxObject, *rm_, selection_, selected);
}

void GuiSystem::drawInspector() {
    if (auto selected = selection_ ? selection_->get() : nullptr) {
        auto cloud = selected->getComponent<CloudVolumeComponent>();
        for (auto& component : selected->getAllComponents()) {
            if (cloud && cloud->mode == CloudVolumeComponent::Mode::SkyLayer &&
                std::dynamic_pointer_cast<Transform>(component)) continue;
            ComponentEditors::draw(*component);
        }
    }
}

void GuiSystem::showRenderSettingsPanel(double deltaTime) {
    ImGui::Begin("Render Settings");
    drawFrameRateControls(deltaTime);
    drawRenderOptions();
    drawProfilingControls();
    ImGui::End();
}

void GuiSystem::drawFrameRateControls(double deltaTime) {
    frameRate_.elapsedSeconds += deltaTime;
    frameRate_.frameCount++;

    if (frameRate_.elapsedSeconds >= 1.0) {
        frameRate_.framesPerSecond = frameRate_.frameCount / frameRate_.elapsedSeconds;
        frameRate_.elapsedSeconds = 0.0;
        frameRate_.frameCount = 0;
    }

    if (ImGui::Checkbox("VSync", &vsyncChecked_)) {
        WindowSystem::setVsync(vsyncChecked_);
    }
    ImGui::SameLine();
    ImGui::Text("FPS: %d", (int)frameRate_.framesPerSecond);
    ImGui::Separator();
}

void GuiSystem::drawRenderOptions() {
    // Each checkbox reflects the program that is actually rendering, including
    // restored local settings and failed edits that kept the previous program.
    auto forward = rm_->getShader(ShaderCatalog::forward.key);
    auto composite = rm_->getShader(ShaderCatalog::composite.key);
    drawShaderOption("Soft Shadow", "PCF_SHADOW", *forward);
    ImGui::Separator();
    drawShaderOption("HDR", "USE_HDR", *composite);
    if (ImGui::SliderFloat("Exposure", &exposure_, 0.1f, 10.0f)) composite->setFloat("exposure", exposure_);
    drawShaderOption("Bloom", "USE_BLOOM", *composite);
    ImGui::Separator();
    drawShaderOption("Environment Mapping", "USE_ENVIRONMENT_MAPPING", *forward);
    if (!renderSettingsError_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("Setting change failed. The previous setting remains active. See Console for details.");
    }
    ImGui::Separator();
}

void GuiSystem::drawShaderOption(const char* label, const char* name, Shader& shader) {
    bool enabled = shader.settingEnabled(name);
    if (ImGui::Checkbox(label, &enabled)) {
        if (shader.applySetting(name, enabled)) {
            renderSettingsError_.clear();
            if (&shader == rm_->getShader(ShaderCatalog::composite.key).get()) {
                shader.setFloat("exposure", exposure_);
            }
        } else {
            renderSettingsError_ = shader.lastError();
        }
    }
}

void GuiSystem::drawProfilingControls() {
    if (renderSystem_ && ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool enabled = renderSystem_->profilingEnabled();
        if (ImGui::Checkbox("Collect timings", &enabled)) renderSystem_->setProfilingEnabled(enabled);
        if (enabled) {
            const auto& profile = renderSystem_->getRenderProfile();
            if (!profile.valid) ImGui::TextDisabled("Waiting for GPU results...");
            else {
                ImGui::Text("Sample #%llu | %llu frames old", static_cast<unsigned long long>(profile.frame),
                    static_cast<unsigned long long>(renderSystem_->profileAgeFrames()));
                ImGui::Text("Viewport: %d x %d | Opaque objects: %zu | Clouds: %zu", profile.width, profile.height,
                    profile.opaqueObjectCount, profile.cloudVolumeCount);
                ImGui::Text("Lights: %zu point / %zu directional / %zu spot", profile.pointLightCount,
                    profile.directionalLightCount, profile.spotLightCount);
                if (ImGui::BeginTable("PassTimings", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("CPU ms", ImGuiTableColumnFlags_WidthFixed, 65.0f);
                    ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthFixed, 65.0f);
                    ImGui::TableHeadersRow();
                    auto row = [](const char* name, double cpu, double gpu) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(name);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.3f", cpu);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.3f", gpu);
                    };
                    row("Scene render total", profile.cpuMs, profile.gpuMs);
                    row("Prepare", profile.prepareCpuMs, profile.prepareGpuMs);
                    for (const auto& pass : profile.passes) row(pass.name.c_str(), pass.cpuMs, pass.gpuMs);
                    ImGui::EndTable();
                }
            }
            ImGui::TextDisabled("Samples skipped: %llu", static_cast<unsigned long long>(renderSystem_->skippedProfileFrames()));
            ImGui::TextWrapped("Latest completed sample; excludes UI and VSync wait. CPU and GPU overlap, so their times are not added.");
        }
    }
}

void GuiSystem::showAssetsPanel() {
    ImGui::Begin("Assets");

    if (ImGui::BeginTabBar("BottomTabs")) {
        if (ImGui::BeginTabItem("Models")) {
            drawModelsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Console")) {
            drawConsoleTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void GuiSystem::drawModelsTab() {
    // Per-tab toolbar: importing a model belongs to the Models tab, not a
    // panel-level File menu shared with the Console tab.
    if (ImGui::Button("Open...") && !ImGuiFileDialog::Instance()->IsOpened(kModelImportDialogKey)) {
        IGFD::FileDialogConfig config;
        config.path = ".";
        config.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(kModelImportDialogKey, "Choose File", ".obj,.fbx,.ply", config);
    }
    ImGui::Separator();

    constexpr int itemsPerRow = 4;
    const float itemWidth = 80.0f;
    const float itemHeight = 80.0f;
    const float padding = 10.0f;

    auto models = rm_->getAllModels();
    for (size_t i = 0; i < models.size(); i++) {
        auto& model = models[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::Button(model->getName().c_str(), ImVec2(itemWidth, itemHeight));
        if (ImGui::BeginPopupContextItem("ModelButtonContext")) {
            if (ImGui::MenuItem("Add to Scene")) {
                rm_->addGameObject(GameObjectFactory::createFromModel(model));
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();

        if ((i + 1) % itemsPerRow != 0) {
            ImGui::SameLine(0.0f, padding);
        }
    }
}

void GuiSystem::drawConsoleTab() {
    if (ImGui::Button("Clear")) Logger::getInstance().clear();
    ImGui::SameLine();
    ImGui::Checkbox("Info", &console_.showInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warn", &console_.showWarn);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &console_.showErr);
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &console_.autoScroll);
    ImGui::SameLine();
    console_.textFilter.Draw("Filter", 160.0f);
    ImGui::Separator();

    std::vector<LogEntry> entries = Logger::getInstance().snapshot();
    unsigned long long rev = Logger::getInstance().revision();

    ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& e : entries) {
        if (e.level == LogLevel::Info && !console_.showInfo) continue;
        if (e.level == LogLevel::Warning && !console_.showWarn) continue;
        if (e.level == LogLevel::Error && !console_.showErr) continue;

        std::string line = "[" + e.time + "] " + e.text;
        if (!console_.textFilter.PassFilter(line.c_str())) continue;

        ImVec4 col;
        switch (e.level) {
        case LogLevel::Error:
            col = ImVec4(1.0f, 0.40f, 0.40f, 1.0f);
            break;
        case LogLevel::Warning:
            col = ImVec4(1.0f, 0.85f, 0.30f, 1.0f);
            break;
        default:
            col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
    }

    // Auto-scroll to bottom when new entries arrived this frame.
    if (console_.autoScroll && rev != console_.lastSeenRevision) {
        ImGui::SetScrollHereY(1.0f);
    }
    console_.lastSeenRevision = rev;

    ImGui::EndChild();
}
} // namespace togr
