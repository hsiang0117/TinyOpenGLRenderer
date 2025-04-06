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
    // �����������
    float sidebarWidth = 250.0f;
    // ������һ�����ڵ�λ�úʹ�С���̶������
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, viewport->Size.y));

    // ʹ��һЩ���ڱ�־ȥ�����������ƶ������ŵȹ��ܣ�������Ҫѡ��
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    // ��ʼһ�����ڣ��������ֿ��Ը�����Ҫ�Զ���
    ImGui::Begin("Sidebar", nullptr, window_flags);

    // �����������Ĳ�������ݣ����簴ť���б�������Ϣ��
    ImGui::Text("���ǲ����");
    if (ImGui::Button("��ť1"))
    {
        // ����ť1�ĵ���¼�
    }
    ImGui::Separator();
    ImGui::Text("����ѡ��...");

    // ����ʹ�� ImGui::BeginChild �������ɹ����������ݳ���ʱ����ֹ�����
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true);
    for (int i = 0; i < 50; i++)
    {
        ImGui::Text("ѡ�� %d", i);
    }
    ImGui::EndChild();

    ImGui::End(); // �������������
}

#endif