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
	double getDeltaTime();
};

Engine& Engine::getInstance() {
	static Engine instance;
	return instance;
}

void Engine::init() {
	Input::getInstance();
	windowSystem.init(1280, 720);
	renderSystem.init();
}

void Engine::run() {
	while (!windowSystem.getShouldClose()) {
		double deltaTime = getDeltaTime();
		Input::getInstance().update();
		windowSystem.update();
		camera.update(deltaTime);
		renderSystem.update();
		renderSystem.render(camera);
		windowSystem.swapBuffers();
	}
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