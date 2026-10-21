#ifndef ENGINE_HPP
#define ENGINE_HPP
#pragma once

#include "rendering/camera.hpp"
#include "platform/input.hpp"
#include "scene/gameObject.hpp"
#include "platform/windowSystem.hpp"
#include "editor/guiSystem.hpp"
#include "assets/resourceManager.hpp"
#include "rendering/renderSystem.hpp"

class Engine {
public:
	Engine() = default;
	~Engine();
	void init();
	void run();
private:
	Input input;
	Camera camera;
	WindowSystem windowSystem;
	ResourceManager resourceManager;
	SelectionModel selectionModel;
	RenderSystem renderSystem;
	GuiSystem guiSystem;
	bool windowReady_ = false, renderReady_ = false, resourcesReady_ = false, guiReady_ = false;
	void shutdown();
	double getDeltaTime();
};

#endif // !ENGINE_HPP
