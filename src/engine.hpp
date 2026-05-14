#ifndef ENGINE_HPP
#define ENGINE_HPP
#pragma once

#include "camera.hpp"
#include "core/windowSystem.hpp"
#include "core/guiSystem.hpp"
#include "core/resourceManager.hpp"
#include "core/renderSystem.hpp"

class Engine {
public:
	static Engine& getInstance();
	void init();
	void run();
private:
	Engine() = default;
	~Engine() = default;
	Camera camera;
	WindowSystem windowSystem;
	RenderSystem renderSystem;
	GuiSystem guiSystem;
	double getDeltaTime();
};

#endif // !ENGINE_HPP
