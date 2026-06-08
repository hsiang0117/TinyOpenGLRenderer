#include "guiSystem.hpp"
#include "../Input.hpp"
#include "../gameObjectFactory.hpp"
#include "resourceManager.hpp"
#include "windowSystem.hpp"
#include "renderSystem.hpp"
#include "logger.hpp"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_internal.h"
#include "imgui/ImGuiFileDialog.h"
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cstring>

namespace {
	// Scene-tree classification predicates (extracted from the former inline lambdas).
	bool isLightObject(const GameObjectPtr& object) {
		return object->has<PointLightComponent>() ||
			object->has<DirectionLightComponent>() ||
			object->has<SpotLightComponent>();
	}

	bool isSceneObject(const GameObjectPtr& object) {
		return object->has<RenderComponent>() ||
			(object->has<StaticMeshComponent>() && !object->has<PointLightComponent>() && !object->has<SkyBoxComponent>());
	}

	bool isSkyBoxObject(const GameObjectPtr& object) {
		return object->has<SkyBoxComponent>();
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
		{ "Light", "Point Light", &GameObjectFactory::createPointLight, nullptr },
		{ "Light", "Direction Light", &GameObjectFactory::createDirectionLight,
			[](ResourceManager& rm) { return rm.getDirectionLightCount() == 0; } },
		{ "Light", "Spot Light", &GameObjectFactory::createSpotLight, nullptr },
		{ "Static Mesh", "Cube", &GameObjectFactory::createCube, nullptr },
		{ "Static Mesh", "Plane", &GameObjectFactory::createPlane, nullptr },
		{ "Static Mesh", "Sphere", &GameObjectFactory::createSphere, nullptr },
		{ "Other", "SkyBox", &GameObjectFactory::createSkyBox, nullptr },
		{ "Other", "Volume Cloud", &GameObjectFactory::createVolumeCloud, nullptr },
	};
}

void GuiSystem::init(GLFWwindow* window) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImFont* font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 24.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
	if (font) {
		io.FontDefault = font;
	}
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
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

void GuiSystem::buildDefaultLayoutOnce(ImGuiID dockspaceId)
{
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

	showScenePanel();
	showInspectorPanel();
	showRenderSettingsPanel(deltaTime);
	showAssetsPanel();
	showViewportPanel();

	// Display the file dialog as a top-level (modal) window, after the panels so it
	// layers above them and receives input.
	if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
			rm_->queueModelLoad(filePath);
		}
		ImGuiFileDialog::Instance()->Close();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GuiSystem::shutDown() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void GuiSystem::showScenePanel()
{
	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_MenuBar);
	if (ImGui::BeginMenuBar()) {
		drawAddMenu();
		ImGui::EndMenuBar();
	}
	drawSceneTree();
	ImGui::End();
}

void GuiSystem::showInspectorPanel()
{
	ImGui::Begin("Inspector");
	drawInspector();
	ImGui::End();
}

void GuiSystem::showViewportPanel()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Viewport");
	ImVec2 avail = ImGui::GetContentRegionAvail();
	int w = (int)avail.x; if (w < 1) w = 1;
	int h = (int)avail.y; if (h < 1) h = 1;
	if (renderSystem_) {
		// Report panel size (drives the scene FBO size on the next frame) and show
		// the rendered scene. UV flipped vertically: GL textures are bottom-up.
		renderSystem_->setViewportSize(w, h);
		ImTextureID texId = (ImTextureID)(intptr_t)renderSystem_->getSceneColorTextureID();
		ImGui::Image(texId, avail, ImVec2(0, 1), ImVec2(1, 0));
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void GuiSystem::drawAddMenu()
{
	if (ImGui::BeginMenu("Add")) {
		// Distinct categories in first-seen order.
		std::vector<const char*> categories;
		for (const auto& e : kSpawnCatalog) {
			bool seen = false;
			for (const char* c : categories) if (strcmp(c, e.category) == 0) { seen = true; break; }
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

void GuiSystem::drawSceneTree()
{
	// Snapshot the current selection once; highlight is decided by identity.
	GameObjectPtr selected = objectSelected_.lock();

	auto renderObjectTreeNode = [&](const char* label, std::function<bool(GameObjectPtr)> predicate) {
		if (ImGui::TreeNode(label)) {
			auto gameObjects = rm_->getGameObjects();
			for (int i = 0; i < gameObjects.size(); i++) {
				GameObjectPtr object = gameObjects[i];
				if (predicate(object)) {
					ImGui::PushID(i);
					if (ImGui::Selectable(object->getName().c_str(), object == selected)) {
						objectSelected_ = object;
						selected = object;
					}
					if (ImGui::BeginPopupContextItem("ObjectButtonContext")) {
						if (ImGui::MenuItem(u8"remove")) {
							rm_->removeGameObject(i);
							// Only clear selection if the removed object was selected;
							// removing a different object leaves the highlight intact.
							if (object == selected) {
								objectSelected_.reset();
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
		};

	renderObjectTreeNode(u8"Light", isLightObject);
	renderObjectTreeNode(u8"Object", isSceneObject);
	renderObjectTreeNode(u8"SkyBox", isSkyBoxObject);
}

void GuiSystem::drawInspector()
{
	if (auto selected = objectSelected_.lock()) {
		for (auto& component : selected->getAllComponents()) {
			showComponentWidget(component);
		}
	}
}

void GuiSystem::showRenderSettingsPanel(double deltaTime)
{
	ImGui::Begin("Render Settings");

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
		rm_->getShader("default")->reCompile();
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
		rm_->getShader("screenQuad")->reCompile();
	}
	if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.0f)) {
		rm_->getShader("screenQuad")->setFloat("exposure", exposure);
	}
	if (ImGui::Checkbox(u8"Bloom", &bloomChecked)) {
		if (bloomChecked) {
			Shader::changeSettings("USE_BLOOM", true);
		}
		else {
			Shader::changeSettings("USE_BLOOM", false);
		}
		rm_->getShader("screenQuad")->reCompile();
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
		rm_->getShader("default")->reCompile();
	}
	ImGui::Separator();

	ImGui::End();
}

void GuiSystem::showAssetsPanel()
{
	ImGui::Begin("Assets");

	if (ImGui::BeginTabBar("BottomTabs")) {
		if (ImGui::BeginTabItem("Models")) {
			// Per-tab toolbar: importing a model belongs to the Models tab, not a
			// panel-level File menu shared with the Console tab.
			if (ImGui::Button("Open...") && !ImGuiFileDialog::Instance()->IsOpened("ChooseFileDlgKey")) {
				IGFD::FileDialogConfig config;
				config.path = ".";
				config.flags = ImGuiFileDialogFlags_Modal;
				ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".obj,.fbx,.ply", config);
			}
			ImGui::Separator();

			static int itemsPerRow = 4;
			const float itemWidth = 80.0f;
			const float itemHeight = 80.0f;
			const float padding = 10.0f;

			auto models = rm_->getAllModels();
			for (size_t i = 0; i < models.size(); i++) {
				auto& model = models[i];
				ImGui::PushID(static_cast<int>(i));
				ImGui::Button(model->getName().c_str(), ImVec2(itemWidth, itemHeight));
				if (ImGui::BeginPopupContextItem("ModelButtonContext"))
				{
					if (ImGui::MenuItem("Add to Scene"))
					{
						rm_->addGameObject(GameObjectFactory::createFromModel(model));
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();

				if ((i + 1) % itemsPerRow != 0)
				{
					ImGui::SameLine(0.0f, padding);
				}
			}
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

void GuiSystem::drawConsoleTab()
{
	static bool showInfo = true, showWarn = true, showErr = true;
	static bool autoScroll = true;
	static ImGuiTextFilter textFilter;
	static unsigned long long lastSeenRev = 0;

	if (ImGui::Button("Clear")) Logger::getInstance().clear();
	ImGui::SameLine(); ImGui::Checkbox("Info", &showInfo);
	ImGui::SameLine(); ImGui::Checkbox("Warn", &showWarn);
	ImGui::SameLine(); ImGui::Checkbox("Error", &showErr);
	ImGui::SameLine(); ImGui::Checkbox("Auto-scroll", &autoScroll);
	ImGui::SameLine(); textFilter.Draw("Filter", 160.0f);
	ImGui::Separator();

	std::vector<LogEntry> entries = Logger::getInstance().snapshot();
	unsigned long long rev = Logger::getInstance().revision();

	ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
	for (const auto& e : entries) {
		if (e.level == LogLevel::Info && !showInfo) continue;
		if (e.level == LogLevel::Warning && !showWarn) continue;
		if (e.level == LogLevel::Error && !showErr) continue;

		std::string line = "[" + e.time + "] " + e.text;
		if (!textFilter.PassFilter(line.c_str())) continue;

		ImVec4 col;
		switch (e.level) {
			case LogLevel::Error:   col = ImVec4(1.0f, 0.40f, 0.40f, 1.0f); break;
			case LogLevel::Warning: col = ImVec4(1.0f, 0.85f, 0.30f, 1.0f); break;
			default:                col = ImGui::GetStyleColorVec4(ImGuiCol_Text); break;
		}
		ImGui::PushStyleColor(ImGuiCol_Text, col);
		ImGui::TextUnformatted(line.c_str());
		ImGui::PopStyleColor();
	}

	// Auto-scroll to bottom when new entries arrived this frame.
	if (autoScroll && rev != lastSeenRev) {
		ImGui::SetScrollHereY(1.0f);
	}
	lastSeenRev = rev;

	ImGui::EndChild();
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
