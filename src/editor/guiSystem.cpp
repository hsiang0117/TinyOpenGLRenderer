#include "guiSystem.hpp"
#include "scene/component.hpp"
#include "scene/gameObject.hpp"
#include "rendering/geometry/mesh.hpp"
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
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cstring>
#include <filesystem>
#include <string>
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

namespace {
	int resizePathInput(ImGuiInputTextCallbackData* data) {
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
			auto& text = *static_cast<std::string*>(data->UserData);
			text.resize(static_cast<size_t>(data->BufTextLen));
			data->Buf = text.data();
		}
		return 0;
	}

	void inputPath(const char* label, std::string& text) {
		ImGui::InputText(label, text.data(), text.capacity() + 1,
			ImGuiInputTextFlags_CallbackResize, resizePathInput, &text);
	}

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
	registerComponentWidgets();
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
	showViewportPanel(deltaTime);

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

void GuiSystem::shutdown() {
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

void GuiSystem::showViewportPanel(double deltaTime)
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
		ImTextureID texId = (ImTextureID)(intptr_t)renderSystem_->getSceneColorTextureId();
		ImGui::Image(texId, avail, ImVec2(0, 1), ImVec2(1, 0));

		// Editor fly camera: hold RMB over the viewport to look (mouse) + move
		// (WASD/Space/LeftCtrl); wheel zooms the FOV. Latched so the look keeps working
		// even if the cursor drifts off the image; releasing RMB anywhere exits.
		const bool wasFlying = flying_;
		const bool hovered = ImGui::IsItemHovered();
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
	GameObjectPtr selected = selection_ ? selection_->get() : nullptr;

	auto renderObjectTreeNode = [&](const char* label, std::function<bool(GameObjectPtr)> predicate) {
		if (ImGui::TreeNode(label)) {
			auto gameObjects = rm_->getGameObjects();
			for (int i = 0; i < gameObjects.size(); i++) {
				GameObjectPtr object = gameObjects[i];
				if (predicate(object)) {
					ImGui::PushID(i);
					if (ImGui::Selectable(object->getName().c_str(), object == selected)) {
						if (selection_) selection_->set(object);
						selected = object;
					}
					if (ImGui::BeginPopupContextItem("ObjectButtonContext")) {
						if (ImGui::MenuItem(u8"remove")) {
							rm_->removeGameObject(i);
							// Only clear selection if the removed object was selected;
							// removing a different object leaves the highlight intact.
							if (object == selected) {
								if (selection_) selection_->clear();
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
	renderObjectTreeNode(u8"Skybox", isSkyboxObject);
}

void GuiSystem::drawInspector()
{
	if (auto selected = selection_ ? selection_->get() : nullptr) {
		auto cloud = selected->getComponent<CloudVolumeComponent>();
		for (auto& component : selected->getAllComponents()) {
			if (cloud && cloud->mode == CloudVolumeComponent::Mode::SkyLayer &&
				std::dynamic_pointer_cast<Transform>(component)) continue;
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

	bool& vsyncChecked = vsyncChecked_;
	if (ImGui::Checkbox("VSync", &vsyncChecked)) {
		WindowSystem::setVsync(vsyncChecked);
	}
	ImGui::SameLine();
	ImGui::Text("FPS: %d", (int)fps);
	ImGui::Separator();

	// Each checkbox reflects the program that is actually rendering, including
	// restored local settings and failed edits that kept the previous program.
	auto option = [&](const char* label, const char* name, const ShaderPtr& shader) {
		bool enabled = shader->settingEnabled(name);
		if (ImGui::Checkbox(label, &enabled)) {
			if (shader->applySetting(name, enabled)) {
				renderSettingsError_.clear();
				if (shader == rm_->getShader(ShaderCatalog::composite.key)) shader->setFloat("exposure", exposure_);
			} else renderSettingsError_ = shader->lastError();
		}
	};
	auto forward = rm_->getShader(ShaderCatalog::forward.key), composite = rm_->getShader(ShaderCatalog::composite.key);
	option("Soft Shadow", "PCF_SHADOW", forward);
	ImGui::Separator();
	option("HDR", "USE_HDR", composite);
	if (ImGui::SliderFloat("Exposure", &exposure_, 0.1f, 10.0f)) composite->setFloat("exposure", exposure_);
	option("Bloom", "USE_BLOOM", composite);
	ImGui::Separator();
	option("Environment Mapping", "USE_ENVIRONMENT_MAPPING", forward);
	if (!renderSettingsError_.empty()) {
		ImGui::Separator();
		ImGui::TextWrapped("Setting change failed. The previous setting remains active. See Console for details.");
	}
	ImGui::Separator();

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
						ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(name);
						ImGui::TableNextColumn(); ImGui::Text("%.3f", cpu);
						ImGui::TableNextColumn(); ImGui::Text("%.3f", gpu);
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

void GuiSystem::registerComponentWidgets()
{
	registerComponentWidget<Transform>([](std::shared_ptr<Transform> transform) {
		ImGui::Text(u8"Transform");
		ImGui::Separator();
		ImGui::DragFloat3(u8"Translate", glm::value_ptr(transform->translate));
		ImGui::DragFloat3(u8"Scale", glm::value_ptr(transform->scale));
		ImGui::DragFloat3(u8"Rotate", glm::value_ptr(transform->rotate));
		ImGui::Separator();
		});

	// Storage-only components intentionally have no editable fields.
	registerComponentWidget<ModelRenderComponent>([](std::shared_ptr<ModelRenderComponent>) {});

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

	registerComponentWidget<DirectionalLightComponent>([](std::shared_ptr<DirectionalLightComponent> directionalLightComponent) {
		ImGui::Text(u8"DirectionalLight");
		ImGui::Separator();
		ImGui::ColorEdit3(u8"Color", glm::value_ptr(directionalLightComponent->color));
		ImGui::DragFloat(u8"Brightness", &directionalLightComponent->brightness, 0.1f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
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

	registerComponentWidget<CloudVolumeComponent>([](std::shared_ptr<CloudVolumeComponent> c) {
		ImGui::Text(u8"Clouds");
		const char* modes[] = { "Sky Layer", "Local Volume" };
		int mode = c->mode == CloudVolumeComponent::Mode::SkyLayer ? 0 : 1;
		if (ImGui::Combo("Mode", &mode, modes, 2)) {
			c->mode = mode == 0 ? CloudVolumeComponent::Mode::SkyLayer : CloudVolumeComponent::Mode::LocalVolume;
		}
		if (ImGui::BeginCombo("Apply preset", "Choose appearance...")) {
			const char* presets[] = { "Scattered Cumulus", "Broken Stratocumulus", "Overcast" };
			for (int i = 0; i < 3; ++i) {
				if (ImGui::Selectable(presets[i])) c->applyPreset(static_cast<CloudVolumeComponent::Preset>(i));
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply shape and lighting settings. Keeps layer bounds, wind and quality settings.");
		if (c->mode == CloudVolumeComponent::Mode::SkyLayer) {
			ImGui::DragFloat("LayerBottom", &c->layerBottom, 1.0f, -10000.0f, 10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("LayerThickness", &c->layerThickness, 1.0f, 0.0f, 10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			if (ImGui::DragFloat("MaxTraceDistance", &c->maxTraceDistance, 10.0f, 1.0f, 100000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
				c->distanceFadeStart = glm::clamp(c->distanceFadeStart, 0.0f, c->maxTraceDistance);
			}
			ImGui::DragFloat("DistanceFadeStart", &c->distanceFadeStart, 10.0f, 0.0f, c->maxTraceDistance, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		}
		ImGui::Separator();

		ImGui::Text(u8"Shape");
		ImGui::SliderFloat(u8"CloudType", &c->cloudType, 0.0f, 1.0f, "%.2f");
		ImGui::Separator();

		ImGui::Text(u8"Coverage");
		if (c->mode == CloudVolumeComponent::Mode::SkyLayer) {
			ImGui::DragFloat("RegionScale", &c->regionScale, 0.00001f, 0.00001f, 0.01f, "%.5f", ImGuiSliderFlags_AlwaysClamp);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Broad cloud banks and clear sky. Region width is approximately 1 / scale in world units.");
			ImGui::SliderFloat("RegionCoverage", &c->regionCoverage, 0.0f, 1.0f, "%.2f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lower values open broad clear-sky regions. 1 fills all regions; local weather still shapes individual clouds.");
			ImGui::SliderFloat("DensityVariation", &c->densityVariation, 0.0f, 1.0f, "%.2f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Varies cloud density between regions. 0 uses the same density multiplier everywhere.");
			ImGui::SliderFloat("HeightVariation", &c->heightVariation, 0.0f, 0.8f, "%.2f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Varies cloud thickness in XZ. Keeps the layer bottom and maximum top altitude fixed.");
			ImGui::DragFloat("WeatherScale", &c->weatherScale, 0.00001f, 0.00001f, 0.05f, "%.5f", ImGuiSliderFlags_AlwaysClamp);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Coverage within a weather region. Smaller values make larger groups of individual clouds.");
		}
		ImGui::DragFloat(u8"CoverageLow", &c->coverageLow, 0.005f, 0.0f, c->coverageHigh, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat(u8"CoverageHigh", &c->coverageHigh, 0.005f, c->coverageLow, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Weather maps this interval into cloud coverage. Lower values create clear sky gaps.");
		ImGui::Separator();

		ImGui::Text(u8"Noise Scales");
		ImGui::DragFloat(u8"BaseNoiseScale", &c->baseNoiseScale, 0.0005f, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Smaller values make larger cloud structures.");
		ImGui::DragFloat(u8"WarpScale", &c->warpScale, 0.0002f, 0.0001f, 0.05f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat(u8"WarpStrength", &c->warpStrength, 0.5f, 0.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Maximum displacement per horizontal axis, in world units.");
		ImGui::DragFloat(u8"DetailScale", &c->detailScale, 0.0005f, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat(u8"DetailErosion", &c->detailErosionStrength, 0.005f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::Separator();

		ImGui::Text(u8"Density and Lighting");
		ImGui::DragFloat("DensityMultiplier", &c->densityMultiplier, 0.01f, 0.0f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scales opacity and self-shadowing together. Zero removes the cloud medium.");
		ImGui::DragFloat("ExtinctionCoefficient", &c->extinctionCoefficient, 0.001f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Inverse world units at density 1. Shared by view and shadow rays; typical sky values: 0.02-0.1.");
		ImGui::SliderFloat("ScatteringAlbedo", &c->scatteringAlbedo, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("AmbientStrength", &c->ambientStrength, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("SunStrength", &c->sunStrength, 0.0f, 10.0f, "%.2f");
		ImGui::SliderFloat("PhaseForwardG", &c->phaseForwardG, 0.0f, 0.85f, "%.2f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward scattering. Higher values brighten the cloud when looking toward the sun.");
		ImGui::SliderFloat("MultipleScattering", &c->multipleScattering, 0.0f, 1.0f, "%.2f");
		ImGui::Separator();

		ImGui::Text(u8"Animation");
		ImGui::DragFloat3(u8"WindVelocity", glm::value_ptr(c->windVelocity), 0.05f, -10.0f, 10.0f, "%.2f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("World units/sec. All cloud patterns move together in this direction.");
		ImGui::Separator();

		ImGui::Text(u8"Quality");
		if (c->mode == CloudVolumeComponent::Mode::LocalVolume) {
			ImGui::DragFloat(u8"EdgeFadeDistance", &c->edgeFadeDistance, 0.5f, 0.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		}
		ImGui::DragFloat(u8"ViewStepWorldSize", &c->viewStepWorldSize, 0.05f, 0.25f, 16.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SliderInt("ViewMaxSteps", &c->viewMaxSteps, 32, 256);
		ImGui::SliderInt("LightSteps", &c->lightSteps, 4, 24);
		ImGui::SliderInt("EmptyStepMultiplier", &c->emptyStepMultiplier, 1, 8);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Skip certified empty blocks without changing the fine sampling grid. 1 disables block skipping.");
		ImGui::Checkbox("TemporalAccumulation", &c->temporalAccumulation);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reproject cloud history with wind compensation. Sky layers must share horizontal wind. Edits reset history.");
		ImGui::Separator();
		});

	registerComponentWidget<SkyboxComponent>([](std::shared_ptr<SkyboxComponent> skyboxComponent) {
		ImGui::Text(u8"Skybox");
		ImGui::Separator();
		inputPath("Path", skyboxComponent->folderPath); ImGui::SameLine();
		if (ImGui::Button("OK")) {
			skyboxComponent->setSkybox(skyboxComponent->folderPath);
		}
		if (!skyboxComponent->lastError().empty())
			ImGui::TextWrapped("%s", skyboxComponent->lastError().c_str());
		});

	registerComponentWidget<StaticMeshComponent>([](std::shared_ptr<StaticMeshComponent>) {});

	registerComponentWidget<DynamicMaterialComponent>([](std::shared_ptr<DynamicMaterialComponent> dynamicMaterialComponent) {
		ImGui::Text(u8"Material");
		ImGui::Separator();
		inputPath(u8"albedo", dynamicMaterialComponent->albedoPath);
		inputPath(u8"specular", dynamicMaterialComponent->specularPath);
		inputPath(u8"normal", dynamicMaterialComponent->normalPath);
		if (ImGui::Button("OK")) {
			dynamicMaterialComponent->setMaterial();
		}
		if (!dynamicMaterialComponent->lastError().empty())
			ImGui::TextWrapped("%s", dynamicMaterialComponent->lastError().c_str());
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
