#include "guiSystem.hpp"
#include "../Input.hpp"
#include "../meshGenerator.hpp"
#include "../gameObjectFactory.hpp"
#include "resourceManager.hpp"
#include "windowSystem.hpp"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/ImGuiFileDialog.h"
#include <glm/gtc/type_ptr.hpp>

float GuiSystem::leftSideBarWidth = 300.0f;
float GuiSystem::rightSideBarWidth = 300.0f;
float GuiSystem::bottomSideBarHeight = 200.0f;

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
	ImGui_ImplOpenGL3_Init("#version 450");
	ImGui::StyleColorsDark();
	registComponents();
}

void GuiSystem::beginFrame() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void GuiSystem::render(double deltaTime) {
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	ImGui::Begin("MainViewport", nullptr, windowFlags);
	showLeftSideBar();
	showRightSideBar(deltaTime);
	showBottomSideBar();
	ImGui::End();
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

	ImGui::SetCursorPos({ 0, 0 });
	ImGui::BeginChild("LeftSidebar", ImVec2(leftSideBarWidth, viewport->Size.y - bottomSideBarHeight - SPLITTER_THICKNESS), true, ImGuiWindowFlags_NoMove);

	ImVec2 avalableSize = ImGui::GetContentRegionAvail();
	ImGui::BeginChild(u8"scenePanel", ImVec2(0, avalableSize.y/2), false, ImGuiWindowFlags_MenuBar);

	if (ImGui::BeginMenuBar()) {
		ImGui::Text("Scene");
		if (ImGui::BeginMenu("Add")) {
			if (ImGui::BeginMenu("Light")) {
				if (ImGui::MenuItem("Point Light")) {
					ResourceManager::getInstance().addGameObject(GameObjectFactory::createPointLight());
				}
				if (ImGui::MenuItem("Direction Light")) {
					if (ResourceManager::getInstance().getDirectionLightCount() == 0) {
						ResourceManager::getInstance().addGameObject(GameObjectFactory::createDirectionLight());
					}
				}
				if (ImGui::MenuItem("Spot Light")) {
					ResourceManager::getInstance().addGameObject(GameObjectFactory::createSpotLight());
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Static Mesh")) {
				if (ImGui::MenuItem("Cube")) {
					ResourceManager::getInstance().addGameObject(GameObjectFactory::createCube());
				}
				if (ImGui::MenuItem("Plane")) {
					ResourceManager::getInstance().addGameObject(GameObjectFactory::createPlane());
				}
				if (ImGui::MenuItem("Sphere")) {
					ResourceManager::getInstance().addGameObject(GameObjectFactory::createSphere());
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Other")) {
				if (ImGui::MenuItem("SkyBox")) {
					ResourceManager::getInstance().addGameObject(GameObjectFactory::createSkyBox());
				}
				if (ImGui::MenuItem("Volume Cloud")) {
					ResourceManager::getInstance().addGameObject(GameObjectFactory::createVolumeCloud());
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
			auto gameObjects = ResourceManager::getInstance().getGameObjects();
			for (int i = 0; i < gameObjects.size(); i++) {
				GameObjectPtr object = gameObjects[i];
				if (predicate(object)) {
					ImGui::PushID(i);
					if (ImGui::Selectable(object->getName().c_str(), selected == i)) {
						selected = i;
						objectSelected = object;
					}
					if (ImGui::BeginPopupContextItem("ObjectButtonContext")) {
						if (ImGui::MenuItem(u8"remove")) {
							ResourceManager::getInstance().removeGameObject(i);
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
		return object->has<PointLightComponent>() ||
			object->has<DirectionLightComponent>() ||
			object->has<SpotLightComponent>();
		});

	renderObjectTreeNode(u8"Object", [](GameObjectPtr object) {
		return object->has<RenderComponent>() ||
			(object->has<StaticMeshComponent>() && !object->has<PointLightComponent>() && !object->has<SkyBoxComponent>());
		});

	renderObjectTreeNode(u8"SkyBox", [](GameObjectPtr object) {
		return object->has<SkyBoxComponent>();
		});
	ImGui::EndChild();
	ImGui::EndChild();

	ImGui::BeginChild(u8"detailPanel", ImVec2(0, 0), false, ImGuiWindowFlags_MenuBar);
	if (ImGui::BeginMenuBar()) {
		ImGui::Text("Detail");
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

	ImGui::EndChild();

	ImGui::SetCursorScreenPos({ leftSideBarWidth, 0 });
	float height = viewport->Size.y - bottomSideBarHeight - SPLITTER_THICKNESS;
	if (height > 0) {
		ImGui::InvisibleButton("##SplitterLeft", ImVec2(SPLITTER_THICKNESS, height));
	}
	if (ImGui::IsItemActive()) {
		leftSideBarWidth = leftSideBarWidth + ImGui::GetIO().MouseDelta.x;
		leftSideBarWidth = clamp(leftSideBarWidth, 100, viewport->Size.x / 3);
		Input::getInstance().onUiResized();
	}
	ImDrawList* draw = ImGui::GetWindowDrawList();
	float x0 = leftSideBarWidth;
	float x1 = x0 + SPLITTER_THICKNESS;
	draw->AddRectFilled(ImVec2(x0, 0), ImVec2(x1, viewport->Size.y - bottomSideBarHeight - SPLITTER_THICKNESS), IM_COL32(200, 200, 200, 180));
}

void GuiSystem::showRightSideBar(double deltaTime)
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::SetCursorScreenPos({ viewport->Size.x - rightSideBarWidth, 0 });
	ImGui::BeginChild("RightSidebar", ImVec2(rightSideBarWidth, viewport->Size.y - bottomSideBarHeight - SPLITTER_THICKNESS), true, ImGuiWindowFlags_NoMove);
	ImGui::BeginChild(u8"renderSettingsPanel", ImVec2(0, 0), false, ImGuiWindowFlags_MenuBar);
	if (ImGui::BeginMenuBar()) {
		ImGui::Text("Render Settings");
		ImGui::EndMenuBar();
	}

	static double fpsTimer = 0.0;
	static int frameCount = 0;
	static double fps = 0.0;

	fpsTimer += deltaTime;
	frameCount++;

	if (fpsTimer >= 1.0) {
		fps = frameCount / fpsTimer;
		fpsTimer = 0.0;
		frameCount = 0;
	}

	static bool vsyncChecked = true;
	if (ImGui::Checkbox("VSync", &vsyncChecked)) {
		WindowSystem::setVsync(vsyncChecked);
	}
	ImGui::SameLine();
	ImGui::Text("FPS: %d", (int)fps);
	ImGui::Separator();

	static bool softShadowChecked = true;
	if (ImGui::Checkbox("Soft Shadow", &softShadowChecked)) {
		if (softShadowChecked) {
			Shader::changeSettings("PCF_SHADOW", true);
		}
		else {
			Shader::changeSettings("PCF_SHADOW", false);
		}
		ResourceManager::getInstance().getShader("default")->reCompile();
	}
	ImGui::Separator();

	static bool hdrChecked = true;
	static float exposure = 1.0;
	static bool bloomChecked = false;
	if (ImGui::Checkbox(u8"HDR", &hdrChecked)) {
		if (hdrChecked) {
			Shader::changeSettings("USE_HDR", true);
		}
		else {
			Shader::changeSettings("USE_HDR", false);
		}
		ResourceManager::getInstance().getShader("screenQuad")->reCompile();
	}
	if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.0f)) {
		ResourceManager::getInstance().getShader("screenQuad")->setFloat("exposure", exposure);
	}
	if (ImGui::Checkbox(u8"Bloom", &bloomChecked)) {
		if (bloomChecked) {
			Shader::changeSettings("USE_BLOOM", true);
		}
		else {
			Shader::changeSettings("USE_BLOOM", false);
		}
		ResourceManager::getInstance().getShader("screenQuad")->reCompile();
	}
	ImGui::Separator();

	static bool environmentMappingChecked = false;
	if (ImGui::Checkbox("Environment Mapping", &environmentMappingChecked)) {
		if (environmentMappingChecked) {
			Shader::changeSettings("USE_ENVIRONMENT_MAPPING", true);
		}
		else {
			Shader::changeSettings("USE_ENVIRONMENT_MAPPING", false);
		}
		ResourceManager::getInstance().getShader("default")->reCompile();
	}
	ImGui::Separator();

	ImGui::EndChild();
	ImGui::EndChild();

	ImGui::SetCursorScreenPos({ viewport->Size.x - rightSideBarWidth - SPLITTER_THICKNESS, 0 });
	float height = viewport->Size.y - bottomSideBarHeight - SPLITTER_THICKNESS;
	if (height > 0) {
		ImGui::InvisibleButton("##SplitterRight", ImVec2(SPLITTER_THICKNESS, height));
	}
	if (ImGui::IsItemActive()) {
		rightSideBarWidth = rightSideBarWidth - ImGui::GetIO().MouseDelta.x;
		rightSideBarWidth = clamp(rightSideBarWidth, 100, viewport->Size.x / 3);
		Input::getInstance().onUiResized();
	}
	ImDrawList* draw = ImGui::GetWindowDrawList();
	float x0 = viewport->Size.x - rightSideBarWidth - SPLITTER_THICKNESS;
	float x1 = x0 + SPLITTER_THICKNESS;
	draw->AddRectFilled(ImVec2(x0, 0), ImVec2(x1, viewport->Size.y - bottomSideBarHeight - SPLITTER_THICKNESS), IM_COL32(200, 200, 200, 180));
}

void GuiSystem::showBottomSideBar()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::SetCursorScreenPos({ 0, viewport->Size.y - bottomSideBarHeight});
	ImGui::BeginChild("BottomSidebar", ImVec2(viewport->Size.x, bottomSideBarHeight), true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open")) {
				ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".obj,.fbx,.ply");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	static int itemsPerRow = 4;
	const float itemWidth = 80.0f;
	const float itemHeight = 80.0f;
	const float padding = 10.0f;

	auto models = ResourceManager::getInstance().getAllModels();
	for (size_t i = 0; i < models.size(); i++) {
		auto& model = models[i];
		ImGui::PushID(static_cast<int>(i));
		ImGui::Button(model->getName().c_str(), ImVec2(itemWidth, itemHeight));
		if (ImGui::BeginPopupContextItem("ModelButtonContext"))
		{
			if (ImGui::MenuItem("Add to Scene"))
			{
				ResourceManager::getInstance().addGameObject(GameObjectFactory::createFromModel(model));
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();

		if ((i + 1) % itemsPerRow != 0)
		{
			ImGui::SameLine(0.0f, padding);
		}
	}

	ImGui::EndChild();

	ImGui::SetCursorScreenPos({ 0, viewport->Size.y - bottomSideBarHeight - SPLITTER_THICKNESS });
	float width = viewport->Size.x;
	if(width > 0) {
		ImGui::InvisibleButton("##SplitterBottom", ImVec2(width, SPLITTER_THICKNESS));
	}
	if (ImGui::IsItemActive()) {
		bottomSideBarHeight = bottomSideBarHeight - ImGui::GetIO().MouseDelta.y;
		bottomSideBarHeight = clamp(bottomSideBarHeight, 100, viewport->Size.y / 2);
		Input::getInstance().onUiResized();
	}
	ImDrawList* draw = ImGui::GetWindowDrawList();
	float y0 = viewport->Size.y - bottomSideBarHeight - SPLITTER_THICKNESS;
	float y1 = y0 + SPLITTER_THICKNESS;
	draw->AddRectFilled(ImVec2(0, y0), ImVec2(viewport->Size.x, y1), IM_COL32(200, 200, 200, 180));

	if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
			ResourceManager::getInstance().queueModelLoad(filePath);
		}
		ImGuiFileDialog::Instance()->Close();
	}
}

void GuiSystem::registComponents()
{
	registerComponentWidget<Transform>([](std::shared_ptr<Transform> transform) {
		ImGui::Text(u8"Transform");
		ImGui::Separator();
		ImGui::DragFloat3(u8"Translate", glm::value_ptr(transform->translate));
		ImGui::DragFloat3(u8"Scale", glm::value_ptr(transform->scale));
		ImGui::DragFloat3(u8"Rotate", glm::value_ptr(transform->rotate));
		ImGui::Separator();
		});

	registerComponentWidget<RenderComponent>([](std::shared_ptr<RenderComponent> renderComponent) {
		});

	registerComponentWidget<PointLightComponent>([](std::shared_ptr<PointLightComponent> pointLightComponent) {
		ImGui::Text(u8"PointLight");
		ImGui::Separator();
		ImGui::ColorEdit3(u8"Color", glm::value_ptr(pointLightComponent->color));
		ImGui::DragFloat(u8"Brightness", &pointLightComponent->brightness, 0.1f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat(u8"Constant", &pointLightComponent->constant, 0.01f, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat(u8"Linear", &pointLightComponent->linear, 0.001f, 0.0f, 1.0f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat(u8"Quadratic", &pointLightComponent->quadratic, 0.0005f, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::Separator();
		});

	registerComponentWidget<DirectionLightComponent>([](std::shared_ptr<DirectionLightComponent> directionLightComponent) {
		ImGui::Text(u8"DirectionLight");
		ImGui::Separator();
		ImGui::ColorEdit3(u8"Color", glm::value_ptr(directionLightComponent->color));
		ImGui::DragFloat(u8"Brightness", &directionLightComponent->brightness, 0.1f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::Separator();
		});

	registerComponentWidget<SpotLightComponent>([](std::shared_ptr<SpotLightComponent> spotLightComponent) {
		ImGui::Text(u8"SpotLight");
		ImGui::Separator();
		ImGui::ColorEdit3(u8"Color", glm::value_ptr(spotLightComponent->color));
		ImGui::DragFloat(u8"Brightness", &spotLightComponent->brightness, 0.1f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat(u8"CutOff", &spotLightComponent->cutOff, 0.5f, 0.0f, 89.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat(u8"OuterCutOff", &spotLightComponent->outerCutOff, 0.5f, 0.0f, 90.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::Separator();
		});

	registerComponentWidget<SkyBoxComponent>([](std::shared_ptr<SkyBoxComponent> skyboxComponent) {
		ImGui::Text(u8"SkyBox");
		ImGui::Separator();
		ImGui::InputText("Path", skyboxComponent->folderPath, sizeof(skyboxComponent->folderPath)); ImGui::SameLine();
		if (ImGui::Button("OK")) {
			skyboxComponent->setSkyBox(std::string(skyboxComponent->folderPath));
		}
		});

	registerComponentWidget<StaticMeshComponent>([](std::shared_ptr<StaticMeshComponent> staticMeshComponent) {
		});

	registerComponentWidget<DynamicMaterialComponent>([](std::shared_ptr<DynamicMaterialComponent> dynamicMaterialComponent) {
		ImGui::Text(u8"Material");
		ImGui::Separator();
		ImGui::InputText(u8"albedo", dynamicMaterialComponent->albedoPath, 255);
		ImGui::InputText(u8"specular", dynamicMaterialComponent->specularPath, 255);
		ImGui::InputText(u8"normal", dynamicMaterialComponent->normalPath, 255);
		if (ImGui::Button("OK")) {
			dynamicMaterialComponent->setMaterial();
		}
		});

	registerComponentWidget<ShadowCaster2D>([](std::shared_ptr<ShadowCaster2D> shadowCaster2D) {
		ImGui::Text(u8"ShadowCaster");
		ImGui::Separator();
		ImGui::Checkbox(u8"Enabled", &shadowCaster2D->enabled);
		});

	registerComponentWidget<ShadowCasterCube>([](std::shared_ptr<ShadowCasterCube> shadowCasterCube) {
		ImGui::Text(u8"ShadowCaster");
		ImGui::Separator();
		ImGui::Checkbox(u8"Enabled", &shadowCasterCube->enabled);
		ImGui::DragFloat(u8"FarPlane", &shadowCasterCube->farPlane, 0.5f, 0.1f, 10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		});

	registerComponentWidget<AnimatorComponent>([](std::shared_ptr<AnimatorComponent> animatorComponent) {
		ImGui::Text(u8"Animator");
		ImGui::Separator();
		if(ImGui::BeginCombo(u8"##AnimatorCombo", animatorComponent->getCurrentAnimation().c_str())) {
			for (const auto& name : animatorComponent->getAnimationsNames()) {
				bool isSelected = (animatorComponent->getCurrentAnimation() == name);
				if (ImGui::Selectable(name.c_str(), isSelected)) {
					animatorComponent->playAnimation(name);
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (animatorComponent->playing) {
			if (ImGui::Button("Pause")) {
				animatorComponent->playing = false;
			}
		}
		else {
			if (ImGui::Button("Play")) {
				animatorComponent->playing = true;
			}
		}
		});

	registerComponentWidget<SkeletonViewerComponent>([](std::shared_ptr<SkeletonViewerComponent> skeletonViewerComponent) {
		ImGui::Text(u8"SkeletonViewer");
		ImGui::Separator();
		ImGui::Checkbox(u8"Show", &skeletonViewerComponent->show);
		ImGui::Separator();
		});
}

void GuiSystem::showComponentWidget(std::shared_ptr<Component> comp) {
	auto& registry = getWidgetRegistry();
	auto it = registry.find(std::type_index(typeid(*comp)));
	if (it != registry.end()) {
		it->second(comp);
	}
	else {
		ImGui::Text("No editor registered for %s", comp->getName().c_str());
	}
}
