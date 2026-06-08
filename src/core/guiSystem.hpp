#ifndef GUISYSTEM_HPP
#define GUISYSTEM_HPP
#pragma once

#include "../component.hpp"
#include "../gameObject.hpp"
#include "imgui/imgui.h"
#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <memory>

using ComponentWidgetFunc = std::function<void(ComponentPtr)>;

class RenderSystem;

class GuiSystem {
public:
	GuiSystem() = default;
	~GuiSystem() = default;
	void init(GLFWwindow* window);
	void setRenderSystem(RenderSystem* rs) { renderSystem_ = rs; }
	void beginFrame();
	void render(double deltaTime);
	void shutDown();

	// Register a detail-panel widget for a component type (keyed by type, no string name).
	template<typename T>
	void registerComponentWidget(std::function<void(std::shared_ptr<T>)> func);

	void showComponentWidget(std::shared_ptr<Component> comp);
private:
	// component type -> detail-panel widget. Keyed by type_index (decided at compile time
	// from the template parameter), removing the old "registration string must match
	// getName() exactly" footgun.
	static std::unordered_map<std::type_index, ComponentWidgetFunc>& getWidgetRegistry() {
		static std::unordered_map<std::type_index, ComponentWidgetFunc> registry;
		return registry;
	}

	// Dockable panels (each a top-level ImGui window).
	void showScenePanel();
	void showInspectorPanel();
	void showRenderSettingsPanel(double deltaTime);
	void showAssetsPanel();
	void showViewportPanel();
	void drawAddMenu();
	void drawSceneTree();
	void drawInspector();
	void drawConsoleTab();
	void registComponents();

	// Build the default dock layout once, on first run when no imgui.ini exists.
	void buildDefaultLayoutOnce(ImGuiID dockspaceId);

	// Current selection: a single identity-based source of truth shared by the
	// scene tree (highlight + writes) and the inspector (reads). Highlight is by
	// object identity, so removing a different object never disturbs it, and the
	// weak_ptr auto-expires if the selected object is destroyed elsewhere.
	std::weak_ptr<GameObject> objectSelected_;

	// Non-owning; set by Engine. The Viewport panel reports its size here and reads
	// the scene color texture to display.
	RenderSystem* renderSystem_ = nullptr;
};

template<typename T>
void GuiSystem::registerComponentWidget(std::function<void(std::shared_ptr<T>)> func) {
	getWidgetRegistry()[std::type_index(typeid(T))] = [func](ComponentPtr comp) {
		std::shared_ptr<T> specificComp = std::dynamic_pointer_cast<T>(comp);
		if (specificComp) {
			func(specificComp);
		}
	};
}

#endif
