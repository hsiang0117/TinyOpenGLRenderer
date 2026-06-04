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

#define SPLITTER_THICKNESS 4

enum class SplitterAxis { Vertical, Horizontal };

using ComponentWidgetFunc = std::function<void(ComponentPtr)>;

class GuiSystem {
public:
	GuiSystem() = default;
	~GuiSystem() = default;
	void init(GLFWwindow* window);
	void beginFrame();
	void render(double deltaTime);
	void shutDown();

	// Register a detail-panel widget for a component type (keyed by type, no string name).
	template<typename T>
	void registerComponentWidget(std::function<void(std::shared_ptr<T>)> func);

	void showComponentWidget(std::shared_ptr<Component> comp);

	static float leftSideBarWidth;
	static float rightSideBarWidth;
	static float bottomSideBarHeight;
private:
	// component type -> detail-panel widget. Keyed by type_index (decided at compile time
	// from the template parameter), removing the old "registration string must match
	// getName() exactly" footgun.
	static std::unordered_map<std::type_index, ComponentWidgetFunc>& getWidgetRegistry() {
		static std::unordered_map<std::type_index, ComponentWidgetFunc> registry;
		return registry;
	}

	void showLeftSideBar();
	void drawAddMenu();
	void drawSceneTree();
	void drawInspector();
	void showRightSideBar(double deltaTime);
	void showBottomSideBar();
	void drawConsoleTab();
	void registComponents();

	// Current selection, shared by the scene tree (writes) and inspector (reads).
	// Still two fields (index + identity) as before; unifying them is a later step.
	std::weak_ptr<GameObject> objectSelected_;
	int selectedIndex_ = -1;

	// Shared draggable splitter. pos = top-left in screen space, length = the
	// extent along the bar, sizeRef = the panel dimension it resizes, sign = +1/-1
	// for which side the drag grows, clamped to [minSize, maxSize].
	void drawSplitter(const char* id, SplitterAxis axis, ImVec2 pos, float length,
	                  float& sizeRef, float sign, float minSize, float maxSize);

	float clamp(float value, float min, float max) { return std::max(min, std::min(value, max)); }
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
