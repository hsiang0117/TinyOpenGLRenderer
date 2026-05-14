#ifndef GUISYSTEM_HPP
#define GUISYSTEM_HPP
#pragma once

#include "../component.hpp"
#include "imgui/imgui.h"
#include <GLFW/glfw3.h>
#include <functional>
#include <string>
#include <unordered_map>

#define SPLITTER_THICKNESS 4

using ComponentWidgetFunc = std::function<void(ComponentPtr)>;

class GuiSystem {
public:
	GuiSystem() = default;
	~GuiSystem() = default;
	void init(GLFWwindow* window);
	void beginFrame();
	void render(double deltaTime);
	void shutDown();

	template<typename T>
	void registerComponentWidget(const std::string& componentName, std::function<void(std::shared_ptr<T>)> func);

	void showComponentWidget(std::shared_ptr<Component> comp);

	static float leftSideBarWidth;
	static float rightSideBarWidth;
	static float bottomSideBarHeight;
private:
	static std::unordered_map<std::string, ComponentWidgetFunc>& getWidgetRegistry() {
		static std::unordered_map<std::string, ComponentWidgetFunc> registry;
		return registry;
	}

	void showLeftSideBar();
	void showRightSideBar(double deltaTime);
	void showBottomSideBar();
	void registComponents();

	float clamp(float value, float min, float max) { return std::max(min, std::min(value, max)); }
};

template<typename T>
void GuiSystem::registerComponentWidget(const std::string& componentName, std::function<void(std::shared_ptr<T>)> func) {
	getWidgetRegistry()[componentName] = [func](ComponentPtr comp) {
		std::shared_ptr<T> specificComp = std::dynamic_pointer_cast<T>(comp);
		if (specificComp) {
			func(specificComp);
		}
	};
}

#endif
