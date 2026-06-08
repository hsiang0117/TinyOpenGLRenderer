#include "engine.hpp"
#include "input.hpp"
#include "sceneFactory.hpp"
#include <GLFW/glfw3.h>

Engine& Engine::getInstance() {
	static Engine instance;
	return instance;
}

void Engine::init() {
	Input::getInstance().init();
	windowSystem.init(1280, 720);
	renderSystem.init();
	ResourceManager::getInstance().init();
	guiSystem.init(windowSystem.getWindow());
	guiSystem.setRenderSystem(&renderSystem);
	ResourceManager::getInstance().setCurrentScene(SceneFactory::createDefaultScene());
}

void Engine::run() {
	while (!windowSystem.getShouldClose()) {
		double deltaTime = getDeltaTime();
		Input::getInstance().update();
		windowSystem.update();
		ResourceManager::getInstance().update();
		camera.update(deltaTime);
		renderSystem.update(deltaTime);
		guiSystem.beginFrame();
		renderSystem.render(camera);
		guiSystem.render(deltaTime);
		windowSystem.swapBuffers();
	}
	guiSystem.shutDown();
	ResourceManager::getInstance().shutdown();
	windowSystem.shutDown();
}

double Engine::getDeltaTime()
{
	static double lastFrame = 0.0;
	double currentFrame = glfwGetTime();
	double deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;
	return deltaTime;
}
