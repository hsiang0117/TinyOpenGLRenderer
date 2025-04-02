#ifndef ENGINE_HPP
#define ENGINE_HPP
#pragma once

#include "camera.hpp"
#include "core/windowSystem.hpp"
#include "core/renderSystem.hpp"
#include <GLFW/glfw3.h>

class Engine {
public:
	Engine() {};
	static Engine& getInstance();
	void init();
	void run();
private:
	Camera camera;
	WindowSystem windowSystem;
	RenderSystem renderSystem;
};

Engine& Engine::getInstance() {
	static Engine instance;
	return instance;
}

void Engine::init() {
	Input::getInstance();
	windowSystem.init(1920, 1080);
	renderSystem.init();
}

void Engine::run() {
	while (!windowSystem.getShouldClose()) {
		Input::getInstance().update();
		windowSystem.update();
		renderSystem.update();
		renderSystem.render(camera);
		windowSystem.swapBuffers();
	}
	windowSystem.shutDown();
}

#endif // !ENGINE_HPP