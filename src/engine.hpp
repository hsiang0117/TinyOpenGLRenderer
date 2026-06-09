#ifndef ENGINE_HPP
#define ENGINE_HPP
#pragma once

#include "camera.hpp"
#include "input.hpp"
#include "core/windowSystem.hpp"
#include "core/guiSystem.hpp"
#include "core/resourceManager.hpp"
#include "core/renderSystem.hpp"

class Engine {
public:
	Engine() = default;
	~Engine() = default;
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
	double getDeltaTime();
};

#endif // !ENGINE_HPP
