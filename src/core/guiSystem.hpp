#ifndef GUISYSTEM_HPP
#define GUISYSTEM_HPP
#pragma once

#include "../Input.hpp"
#include "resourceManager.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

class GuiSystem {
    friend class WindowSystem;
public:
    GuiSystem() {};
    void init(GLFWwindow* window);
    void beginFrame();
    void render();
    void shutDown();
private:
    void showLeftSideBar();
};

void GuiSystem::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();    
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 24.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
    if (font) {
        io.FontDefault = font;
    }
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");
    ImGui::StyleColorsDark();
}

void GuiSystem::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GuiSystem::render() {
    showLeftSideBar();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GuiSystem::shutDown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void GuiSystem::showLeftSideBar()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float sidebarWidth = 250.0f;
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, viewport->Size.y));

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("LeftSidebar", nullptr, window_flags);

    ImGui::Text(u8"场景");
    if (ImGui::Button(u8"添加模型"))
    {

    }
    ImGui::Separator();

    ImGui::BeginChild(u8"ScrollingRegion", ImVec2(0, 0), true);
    for (auto it = ResourceManager::getInstance().modelCache.begin(); it != ResourceManager::getInstance().modelCache.end(); ++it) {
		ImGui::Text(u8"%s", it->second->getName().c_str());
    }
    ImGui::EndChild();

    ImGui::End();
}

#endif