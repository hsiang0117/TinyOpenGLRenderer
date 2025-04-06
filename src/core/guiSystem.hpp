#ifndef GUISYSTEM_HPP
#define GUISYSTEM_HPP

#include "../Input.hpp"
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
    // 定义侧边栏宽度
    float sidebarWidth = 250.0f;
    // 设置下一个窗口的位置和大小，固定在左侧
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, viewport->Size.y));

    // 使用一些窗口标志去掉标题栏、移动和缩放等功能（根据需要选择）
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    // 开始一个窗口，窗口名字可以根据需要自定义
    ImGui::Begin("Sidebar", nullptr, window_flags);

    // 在这里添加你的侧边栏内容，比如按钮、列表、调试信息等
    ImGui::Text("这是侧边栏");
    if (ImGui::Button("按钮1"))
    {
        // 处理按钮1的点击事件
    }
    ImGui::Separator();
    ImGui::Text("更多选项...");

    // 可以使用 ImGui::BeginChild 来创建可滚动区域，内容超过时会出现滚动条
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true);
    for (int i = 0; i < 50; i++)
    {
        ImGui::Text("选项 %d", i);
    }
    ImGui::EndChild();

    ImGui::End(); // 结束侧边栏窗口
}

#endif