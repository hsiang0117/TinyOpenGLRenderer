#ifndef ENGINE_HPP
#define ENGINE_HPP
#pragma once

#include "camera.hpp"
#include "core/windowSystem.hpp"
#include "core/renderSystem.hpp"
#include "core/guiSystem.hpp"
#include "core/resourceManager.hpp"
#include <GLFW/glfw3.h>

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

Engine& Engine::getInstance() {
	static Engine instance;
	return instance;
}

void Engine::init() {
	Input::getInstance().init();
	windowSystem.init(1280, 720);
	renderSystem.init();
	guiSystem.init(windowSystem.getWindow());
	ResourceManager::getInstance().init();
}

void Engine::run() {
	while (!windowSystem.getShouldClose()) {
		double deltaTime = getDeltaTime();
		Input::getInstance().update();
		windowSystem.update();
		camera.update(deltaTime);
		renderSystem.update();
		ResourceManager::getInstance().update();
		guiSystem.beginFrame();
		renderSystem.render(camera);
		guiSystem.render();
		windowSystem.swapBuffers();
	}
	guiSystem.shutDown();
	windowSystem.shutDown();
}

double Engine::getDeltaTime()
{
	static float lastFrame = 0.0f;
	double currentFrame = glfwGetTime();
	double deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;
	return deltaTime;
}

#endif // !ENGINE_HPP