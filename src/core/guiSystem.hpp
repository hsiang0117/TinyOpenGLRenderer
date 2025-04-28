#ifndef GUISYSTEM_HPP
#define GUISYSTEM_HPP
#pragma once

#include "../Input.hpp"
#include "../component.hpp"
#include "../meshGenerator.hpp"
#include "resourceManager.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/ImGuiFileDialog.h"
#include <GLFW/glfw3.h>

#define LEFT_SIDEBAR_WIDTH 300.0f
#define RIGHT_SIDEBAR_WIDTH 250.0f
#define BOTTOM_SIDEBAR_HEIGHT 250.0f

// 组件对应渲染方法，接收一个组件指针，返回void
using ComponentWidgetFunc = std::function<void(ComponentPtr)>;

class GuiSystem {
public:
	GuiSystem() = default;
	~GuiSystem() = default;
	void init(GLFWwindow* window);
	void beginFrame();
	void render();
	void shutDown();

	// 注册组件的渲染方法，传入组件名称和渲染函数
	template<typename T>
	void registerComponentWidget(const std::string& componentName, std::function<void(std::shared_ptr<T>)> func);

	// 根据组件渲染对应的widget
	void showComponentWidget(std::shared_ptr<Component> comp);
private:
	// 组件名和对应渲染函数的注册表
	static std::unordered_map<std::string, ComponentWidgetFunc>& getWidgetRegistry() {
		static std::unordered_map<std::string, ComponentWidgetFunc> registry;
		return registry;
	}

	void showLeftSideBar();
	void showRightSideBar();
	void showBottomSideBar();
	void registComponents();
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
	registComponents();
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
	static std::weak_ptr<GameObject> objectSelected;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(ImVec2(LEFT_SIDEBAR_WIDTH, viewport->Size.y - BOTTOM_SIDEBAR_HEIGHT));

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	ImGui::Begin("LeftSidebar", nullptr, windowFlags);

	ImVec2 availableSize = ImGui::GetContentRegionAvail();

	ImGui::BeginChild(u8"scenePanel", ImVec2(0, availableSize.y / 2), false, ImGuiWindowFlags_MenuBar);

	if (ImGui::BeginMenuBar()) {
		ImGui::Text(u8"场景");
		if (ImGui::BeginMenu(u8"添加")) {
			if (ImGui::BeginMenu(u8"光源")) {
				if (ImGui::MenuItem(u8"点光源")) {
					auto gameObject = std::make_shared<PointLightObject>("PointLight");
					gameObject->addComponent<Transform>();
					gameObject->addComponent<PointLightComponent>();
					ResourceManager::getInstance().gameObjects.push_back(gameObject);
					ResourceManager::getInstance().pointLightNum++;
				}
				if (ImGui::MenuItem(u8"平行光")) {
					if (ResourceManager::getInstance().directionLightNum == 0) {
						auto gameObject = std::make_shared<DirectionLightObject>("DirectionLight");
						gameObject->addComponent<Transform>();
						gameObject->addComponent<DirectionLightComponent>();
						gameObject->addComponent<ShadowCaster2D>();
						ResourceManager::getInstance().gameObjects.push_back(gameObject);
						ResourceManager::getInstance().directionLightNum++;
					}
				}
				if (ImGui::MenuItem(u8"聚光灯")) {
					auto gameObject = std::make_shared<SpotLightObject>("SpotLight");
					gameObject->addComponent<Transform>();
					gameObject->addComponent<SpotLightComponent>();
					ResourceManager::getInstance().gameObjects.push_back(gameObject);
					ResourceManager::getInstance().spotLightNum++;
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(u8"静态网格体")) {
				if (ImGui::MenuItem(u8"立方体")) {
					auto gameObject = std::make_shared<StaticMeshObject>("Cube");
					gameObject->addComponent<Transform>();
					gameObject->addComponent<StaticMeshComponent>();
					gameObject->getComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateCube());
					gameObject->addComponent<DynamicMaterialComponent>();
					ResourceManager::getInstance().gameObjects.push_back(gameObject);
				}
				if (ImGui::MenuItem(u8"平面")) {
					auto gameObject = std::make_shared<StaticMeshObject>("Plane");
					gameObject->addComponent<Transform>();
					gameObject->addComponent<StaticMeshComponent>();
					gameObject->getComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generatePlane());
					gameObject->addComponent<DynamicMaterialComponent>();
					ResourceManager::getInstance().gameObjects.push_back(gameObject);
				}
				if (ImGui::MenuItem(u8"球体")) {
					auto gameObject = std::make_shared<StaticMeshObject>("Sphere");
					gameObject->addComponent<Transform>();
					gameObject->addComponent<StaticMeshComponent>();
					gameObject->getComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateSphere());
					gameObject->addComponent<DynamicMaterialComponent>();
					ResourceManager::getInstance().gameObjects.push_back(gameObject);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(u8"其他")) {
				if (ImGui::MenuItem(u8"天空盒")) {
					auto gameObject = std::make_shared<SkyBoxObject>("SkyBox");
					gameObject->addComponent<SkyBoxComponent>();
					ResourceManager::getInstance().gameObjects.push_back(gameObject);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::BeginChild(u8"sceneRegion", ImVec2(0, 0), true);
	static int selected = -1;

	auto renderObjectTreeNode = [&](const char* label, std::function<bool(GameObjectPtr)> predicate) {
		if (ImGui::TreeNode(label)) {
			for (int i = 0; i < ResourceManager::getInstance().gameObjects.size(); i++) {
				GameObjectPtr object = ResourceManager::getInstance().gameObjects[i];
				if (predicate(object)) {
					ImGui::PushID(i);
					if (ImGui::Selectable(object->getName().c_str(), selected == i)) {
						selected = i;
						objectSelected = object;
					}
					if (ImGui::BeginPopupContextItem("ObjectButtonContext")) {
						if (ImGui::MenuItem(u8"remove")) {
							ResourceManager::getInstance().removeQueue.push(i);
							selected = -1;
						}
						ImGui::EndPopup();
					}
					ImGui::PopID();
				}
			}
			ImGui::TreePop();
		}
		};

	renderObjectTreeNode(u8"Light", [](GameObjectPtr object) {
		return object->getType() == GameObject::Type::POINTLIGHTOBJECT ||
			object->getType() == GameObject::Type::DIRECTIONLIGHTOBJECT ||
			object->getType() == GameObject::Type::SPOTLIGHTOBJECT;
		});

	renderObjectTreeNode(u8"Object", [](GameObjectPtr object) {
		return object->getType() == GameObject::Type::RENDEROBJECT;
		});

	renderObjectTreeNode(u8"SkyBox", [](GameObjectPtr object) {
		return object->getType() == GameObject::Type::SKYBOXOBJECT;
		});
	ImGui::EndChild();
	ImGui::EndChild();

	ImGui::BeginChild(u8"detailPanel", ImVec2(0, 0), false, ImGuiWindowFlags_MenuBar);
	if (ImGui::BeginMenuBar()) {
		ImGui::Text(u8"细节");
		ImGui::EndMenuBar();
	}
	ImGui::BeginChild(u8"detailRegion", ImVec2(0, 0), true);
	if (objectSelected.lock()) {
		for (auto& component : objectSelected.lock()->getAllComponents()) {
			showComponentWidget(component);
		}
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
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	ImGui::Begin("RightSidebar", nullptr, windowFlags);
	ImGui::Text(u8"渲染设置");

	ImGui::End();
}

void GuiSystem::showBottomSideBar()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - BOTTOM_SIDEBAR_HEIGHT));
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, BOTTOM_SIDEBAR_HEIGHT));
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar;
	ImGui::Begin("BottomSidebar", nullptr, windowFlags);

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu(u8"文件")) {
			if (ImGui::MenuItem(u8"打开")) {
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
				std::shared_ptr<RenderObject>gameObject = std::make_shared<RenderObject>(it->second->getName());
				gameObject->addComponent<Transform>();
				gameObject->addComponent<RenderComponent>();
				gameObject->getComponent<RenderComponent>()->setModel(it->second);
				ResourceManager::getInstance().gameObjects.push_back(gameObject);
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

void GuiSystem::registComponents()
{
	registerComponentWidget<Transform>("Transform", [](std::shared_ptr<Transform> transform) {
		ImGui::Text(u8"Transform");
		ImGui::Separator();
		ImGui::DragFloat3(u8"Translate", glm::value_ptr(transform->translate));
		ImGui::DragFloat3(u8"Scale", glm::value_ptr(transform->scale));
		ImGui::DragFloat3(u8"Rotate", glm::value_ptr(transform->rotate));
		ImGui::Separator();
		});

	registerComponentWidget<RenderComponent>("RenderComponent", [](std::shared_ptr<RenderComponent> renderComponent) {
		});

	registerComponentWidget<PointLightComponent>("PointLightComponent", [](std::shared_ptr<PointLightComponent> pointLightComponent) {
		ImGui::Text(u8"PointLight");
		ImGui::Separator();
		ImGui::ColorPicker3(u8"Color", glm::value_ptr(pointLightComponent->color));
		ImGui::DragFloat(u8"Constant", &pointLightComponent->constant);
		ImGui::DragFloat(u8"Linear", &pointLightComponent->linear);
		ImGui::DragFloat(u8"Quadratic", &pointLightComponent->quadratic);
		ImGui::Separator();
		});

	registerComponentWidget<DirectionLightComponent>("DirectionLightComponent", [](std::shared_ptr<DirectionLightComponent> directionLightComponent) {
		ImGui::Text(u8"DirectionLight");
		ImGui::Separator();
		ImGui::ColorPicker3(u8"Color", glm::value_ptr(directionLightComponent->color));
		ImGui::Separator();
		});

	registerComponentWidget<SpotLightComponent>("SpotLightComponent", [](std::shared_ptr<SpotLightComponent> spotLightComponent) {
		ImGui::Text(u8"SpotLight");
		ImGui::Separator();
		ImGui::ColorPicker3(u8"Color", glm::value_ptr(spotLightComponent->color));
		ImGui::DragFloat(u8"CutOff", &spotLightComponent->cutOff);
		ImGui::DragFloat(u8"OuterCutOff", &spotLightComponent->outerCutOff);
		ImGui::Separator();
		});

	registerComponentWidget<SkyBoxComponent>("SkyBoxComponent", [](std::shared_ptr<SkyBoxComponent> skyboxComponent) {
		ImGui::Text(u8"SkyBox");
		ImGui::Separator();
		static char buf[255];
		ImGui::InputText(u8"路径", buf, sizeof(buf)); ImGui::SameLine();
		if (ImGui::Button(u8"确认")) {
			skyboxComponent->setSkyBox(std::string(buf));
		}
		});

	registerComponentWidget<StaticMeshComponent>("StaticMeshComponent", [](std::shared_ptr<StaticMeshComponent> staticMeshComponent) {
		});

	registerComponentWidget<DynamicMaterialComponent>("DynamicMaterialComponent", [](std::shared_ptr<DynamicMaterialComponent> dynamicMaterialComponent) {
		ImGui::Text(u8"Material");
		ImGui::Separator();
		ImGui::InputText(u8"albedo", dynamicMaterialComponent->albedoPath, 255);
		ImGui::InputText(u8"specular", dynamicMaterialComponent->specularPath, 255);
		if (ImGui::Button(u8"确认")) {
			dynamicMaterialComponent->setMaterial();
		}
		});

	registerComponentWidget<ShadowCaster2D>("ShadowCaster2D", [](std::shared_ptr<ShadowCaster2D> shadowCaster2D) {
		ImGui::Text(u8"ShadowCaster");
		ImGui::Separator();
		ImGui::Checkbox(u8"Enabled", &shadowCaster2D->enabled);
		});
}


// 注册组件对应的UI渲染方法，传入组件名称和渲染函数
template<typename T>
void GuiSystem::registerComponentWidget(const std::string& componentName, std::function<void(std::shared_ptr<T>)> func) {
	getWidgetRegistry()[componentName] = [func](ComponentPtr comp) {
		std::shared_ptr<T> specificComp = std::dynamic_pointer_cast<T>(comp); // 将shared_ptr<T>转换为实际的组件指针
		if (specificComp) {
			func(specificComp);
		}
		};
}

// 根据组件渲染对应的widget
void GuiSystem::showComponentWidget(std::shared_ptr<Component> comp) {
	std::string key = comp->getName();
	auto& registry = getWidgetRegistry();
	if (registry.find(key) != registry.end()) {
		registry[key](comp);
	}
	else {
		ImGui::Text(u8"没有注册 %s 的编辑器", key.c_str());
	}
}
#endif