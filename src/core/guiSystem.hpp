#ifndef GUISYSTEM_HPP
#define GUISYSTEM_HPP
#pragma once

#include "../Input.hpp"
#include "../component.hpp"
#include "resourceManager.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/ImGuiFileDialog.h"
#include <GLFW/glfw3.h>

#define LEFT_SIDEBAR_WIDTH 300.0f
#define RIGHT_SIDEBAR_WIDTH 250.0f
#define BOTTOM_SIDEBAR_HEIGHT 250.0f

class GuiSystem {
public:
	GuiSystem() = default;
	~GuiSystem() = default;
	void init(GLFWwindow* window);
	void beginFrame();
	void render();
	void shutDown();
private:
	void showLeftSideBar();
	void showRightSideBar();
	void showBottomSideBar();
	void showTransformWidget(std::shared_ptr<Transform> transform);
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
	showRightSideBar();
	showBottomSideBar();

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
	static GameObjectPtr objectSelected;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(ImVec2(LEFT_SIDEBAR_WIDTH, viewport->Size.y - BOTTOM_SIDEBAR_HEIGHT));

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	ImGui::Begin("LeftSidebar", nullptr, window_flags);

	ImVec2 availableSize = ImGui::GetContentRegionAvail();

	ImGui::BeginChild(u8"scenePanel", ImVec2(0, availableSize.y / 2), false);
	ImGui::Text(u8"场景"); 
	ImGui::BeginChild(u8"sceneRegion", ImVec2(0, 0), true);
	for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++)
	{
		static int selected = -1;
		ImGui::PushID(i);
		if (ImGui::Selectable(ResourceManager::getInstance().gameObjects[i]->getName().c_str(), selected == i)) {
			selected = i;
			objectSelected = ResourceManager::getInstance().gameObjects[i];
		}
		ImGui::PopID();
	}
	ImGui::EndChild();
	ImGui::EndChild();

	availableSize = ImGui::GetContentRegionAvail();

	ImGui::BeginChild(u8"detailPanel", ImVec2(0, availableSize.y), false);
	ImGui::Text(u8"细节");
	ImGui::BeginChild(u8"detailRegion", ImVec2(0, 0), true);
	if (objectSelected) {
		showTransformWidget(objectSelected->getComponent<Transform>());
	}
	else {
		ImGui::Text(u8"未选择物体");
	}
	ImGui::EndChild();
	ImGui::EndChild();

	ImGui::End();
}

void GuiSystem::showRightSideBar()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x - RIGHT_SIDEBAR_WIDTH, viewport->Pos.y));
	ImGui::SetNextWindowSize(ImVec2(RIGHT_SIDEBAR_WIDTH, viewport->Size.y - BOTTOM_SIDEBAR_HEIGHT));
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	ImGui::Begin("RightSidebar", nullptr, window_flags);
	ImGui::Text(u8"渲染设置");

	ImGui::End();
}

void GuiSystem::showBottomSideBar()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - BOTTOM_SIDEBAR_HEIGHT));
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, BOTTOM_SIDEBAR_HEIGHT));
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar;
	ImGui::Begin("BottomSidebar", nullptr, window_flags);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(u8"文件"))
		{
			if (ImGui::MenuItem(u8"打开"))
			{
				ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", u8"选择文件", ".obj\0*.obj\0\0");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	static int itemsPerRow = 4;
	const float itemWidth = 80.0f;
	const float itemHeight = 80.0f;
	const float padding = 10.0f;

	int i = 0;
	for (auto it = ResourceManager::getInstance().modelCache.begin();
		it != ResourceManager::getInstance().modelCache.end(); it++, i++) {
		ImGui::PushID(i);
		ImGui::Button(it->second->getName().c_str(), ImVec2(itemWidth, itemHeight));
		if (ImGui::BeginPopupContextItem("ModelButtonContext"))
		{
			if (ImGui::MenuItem(u8"添加到场景"))
			{
				ResourceManager::getInstance().gameObjects.push_back(std::make_shared<GameObject>(it->second->getName(), it->second));
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();

		if ((i + 1) % itemsPerRow != 0)
		{
			ImGui::SameLine(0.0f, padding);
		}

	}

	ImGui::End();

	if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
			ResourceManager::getInstance().modelQueue.push(filePath.c_str());
		}
		ImGuiFileDialog::Instance()->Close();
	}
}

void GuiSystem::showTransformWidget(std::shared_ptr<Transform> transform){
	ImGui::DragFloat3(u8"平移", glm::value_ptr(transform->translate));
	ImGui::DragFloat3(u8"缩放", glm::value_ptr(transform->scale));
	ImGui::DragFloat3(u8"旋转", glm::value_ptr(transform->rotate));
}

#endif