#include "editor/componentEditors.hpp"
#include "editor/guiSystem.hpp"
#include "assets/model.hpp"
#include "assets/resourceManager.hpp"
#include "assets/shaderCatalog.hpp"
#include "scene/component.hpp"
#include "rendering/opengl/shader.hpp"
#include "rendering/skybox.hpp"
#include "imgui/imgui.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace togr::tests {

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Item {
    ImGuiID id;
    ImVec2 minimum, maximum;
};

Item lastItem() {
    return {ImGui::GetItemID(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
}

void drawEditor(Component& component) {
    const auto scope = ImGui::GetID("scope sentinel");
    ComponentEditors::draw(component);
    check(ImGui::GetID("scope sentinel") == scope, "Component editor did not balance its ImGui ID scope");
}

// Build draw data and feed input events; no platform or renderer backend is used.
class ComponentFrames {
public:
    ComponentFrames() : previous(ImGui::GetCurrentContext()), context(ImGui::CreateContext()) {
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(800, 600);
        io.DeltaTime = 1.0f / 60.0f;
        io.ConfigInputTrickleEventQueue = false;
        io.Fonts->AddFontDefault();
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        check(pixels && width > 0 && height > 0, "Headless ImGui font atlas did not build");
        io.Fonts->SetTexID(static_cast<ImTextureID>(1));
    }
    ~ComponentFrames() {
        ImGui::DestroyContext(context);
        ImGui::SetCurrentContext(previous);
    }
    ComponentFrames(const ComponentFrames&) = delete;
    ComponentFrames& operator=(const ComponentFrames&) = delete;

    template<typename Draw>
    void frame(Draw& draw) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(800, 600));
        ImGui::Begin("Component checks", nullptr, ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
        draw();
        ImGui::End();
        ImGui::Render();
    }

    template<typename Draw>
    void click(Item item, Draw& draw) {
        check(item.id != 0 && item.maximum.x > item.minimum.x && item.maximum.y > item.minimum.y,
            "Component interaction did not identify a real widget");
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent((item.minimum.x + item.maximum.x) * 0.5f, (item.minimum.y + item.maximum.y) * 0.5f);
        frame(draw);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        frame(draw);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        frame(draw);
    }

    void replaceFirstInput(Component& component, const std::string& replacement) {
        bool focus = true;
        auto draw = [&] {
            if (focus) ImGui::SetKeyboardFocusHere();
            drawEditor(component);
        };
        frame(draw);
        focus = false;
        frame(draw);
        check(ImGui::IsAnyItemActive(), "Component path input did not acquire keyboard focus");
        auto& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_A, true);
        frame(draw);
        io.AddKeyEvent(ImGuiKey_A, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        frame(draw);
        io.AddInputCharactersUTF8(replacement.c_str());
        frame(draw);
    }

private:
    ImGuiContext* previous;
    ImGuiContext* context;
};

void writeImage(const std::filesystem::path& path) {
    const unsigned char header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 0x28};
    const unsigned char pixel[4] = {149, 67, 31, 255};
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(header), sizeof(header));
    output.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
    check(bool(output), "GUI resource fixture could not be written");
}

void checkBorrowedDispatch(ComponentFrames& frames) {
    ShadowCaster2D first, second;
    Item firstCheckbox{}, secondCheckbox{};
    auto draw = [&] {
        drawEditor(first); firstCheckbox = lastItem();
        drawEditor(second); secondCheckbox = lastItem();
    };
    frames.frame(draw);
    check(firstCheckbox.id != 0 && secondCheckbox.id != 0 && firstCheckbox.id != secondCheckbox.id,
        "Same-label component controls share an ImGui ID");
    frames.click(secondCheckbox, draw);
    check(first.enabled && !second.enabled, "Editing one borrowed component changed another instance");
    frames.click(firstCheckbox, draw);
    check(!first.enabled && !second.enabled, "The first component control lost its own state");

    struct UnknownShadow : ShadowCaster2D {} unknown;
    auto drawUnknown = [&] {
        drawEditor(unknown);
        check(ImGui::GetItemID() == 0, "Unknown dynamic type was edited as its registered base class");
    };
    frames.frame(drawUnknown);
    check(unknown.enabled, "Unknown component fallback changed component values");
}

void checkResourceDrafts(ComponentFrames& frames) {
    namespace fs = std::filesystem;
    const fs::path directory = "x64/RenderChecks/gui";
    const auto skyDirectory = directory / "skybox";
    fs::create_directories(skyDirectory);
    for (const char* face : {"right.jpg", "left.jpg", "top.jpg", "bottom.jpg", "front.jpg", "back.jpg"}) {
        writeImage(skyDirectory / face);
    }
    const auto imagePath = directory / "albedo.tga";
    writeImage(imagePath);
    const std::string missing = u8"x64/RenderChecks/gui/\u4e0d\u5b58\u5728/" + std::string(280, 'x') + ".tga";

    SkyboxComponent skybox;
    check(skybox.setSkybox(skyDirectory.u8string()), "GUI skybox fixture did not initialize");
    const auto activeSkybox = skybox.getSkybox();
    frames.replaceFirstInput(skybox, missing);
    check(skybox.draftFolderPath == missing && skybox.getSkybox() == activeSkybox && skybox.lastError().empty(),
        "Long UTF-8 skybox editing truncated the draft or automatically committed it");
    Item skyboxButton{};
    auto drawSkybox = [&] { drawEditor(skybox); skyboxButton = lastItem(); };
    frames.frame(drawSkybox);
    frames.click(skyboxButton, drawSkybox); // click() caches the button before error text changes the last item.
    check(!skybox.lastError().empty() && skybox.getSkybox() == activeSkybox && activeSkybox->isReady() &&
        skybox.draftFolderPath == missing, "Failed skybox OK discarded the applied resource or editable draft");

    DynamicMaterialComponent material;
    material.draft.albedoPath = imagePath.u8string();
    check(material.applyMaterial(), "GUI material fixture did not initialize");
    auto shader = std::make_shared<Shader>(ShaderCatalog::forward.vertexPath, ShaderCatalog::forward.fragmentPath);
    check(shader->isValid(), "GUI material fixture shader did not compile");
    material.bindMaterial(shader); glActiveTexture(GL_TEXTURE0);
    GLint activeTexture = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &activeTexture);
    check(activeTexture != 0, "GUI material fixture has no applied albedo texture");
    frames.replaceFirstInput(material, missing);
    check(material.draft.albedoPath == missing && material.getMaterial().albedoPath == imagePath.u8string() &&
        material.lastError().empty(), "Material path editing automatically applied an invalid draft");
    Item materialButton{};
    auto drawMaterial = [&] { drawEditor(material); materialButton = lastItem(); };
    frames.frame(drawMaterial);
    frames.click(materialButton, drawMaterial);
    material.bindMaterial(shader); glActiveTexture(GL_TEXTURE0);
    GLint retainedTexture = 0; glGetIntegerv(GL_TEXTURE_BINDING_2D, &retainedTexture);
    check(!material.lastError().empty() && retainedTexture == activeTexture &&
        material.getMaterial().albedoPath == imagePath.u8string() && material.draft.albedoPath == missing,
        "Failed material OK discarded the active texture or editable draft");
    glUseProgram(0);
}

void checkPlaybackButton(ComponentFrames& frames) {
    AnimatorComponent animator(std::make_shared<Model>());
    Item button{};
    auto draw = [&] { drawEditor(animator); button = lastItem(); };
    frames.frame(draw);
    frames.click(button, draw);
    check(animator.playing, "Animator Play control did not update playback state");
    frames.click(button, draw);
    check(!animator.playing && animator.getCurrentAnimation().empty() && animator.getPose().nodePositions.empty(),
        "Animator Pause control changed an empty model's clip or pose");
}

void checkBackendFrames() {
    ResourceManager resources;
    resources.init();
    GuiSystem gui;
    gui.setResourceManager(&resources);
    gui.init(glfwGetCurrentContext());
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().LogFilename = nullptr;
    try {
        for (int frame = 0; frame < 2; ++frame) {
            gui.beginFrame();
            gui.render(1.0 / 60.0);
            check(ImGui::GetDrawData() && ImGui::GetDrawData()->TotalVtxCount > 0,
                "GUI backend frame did not submit panel draw data");
        }
    } catch (...) {
        gui.shutdown();
        resources.shutdown();
        throw;
    }
    gui.shutdown();
    resources.shutdown();
    check(ImGui::GetCurrentContext() == nullptr, "GUI shutdown retained its ImGui context");
}
}

void runGuiChecks() {
    check(glfwGetCurrentContext() != nullptr, "GUI checks require the existing hidden GL context");
    check(ImGui::GetCurrentContext() == nullptr, "GUI checks require no active application ImGui context");
    {
        ComponentFrames frames;
        checkBorrowedDispatch(frames);
        checkResourceDrafts(frames);
        checkPlaybackButton(frames);
    }
    checkBackendFrames();
    check(glGetError() == GL_NO_ERROR, "GUI checks produced an OpenGL error");
    std::cout << "PASS: borrowed component controls, ID isolation, UTF-8 drafts, failed commits, playback buttons, and GUI backend frames\n";
}

} // namespace togr::tests
