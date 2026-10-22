#include "engine.hpp"
#include "scene/sceneFactory.hpp"
#include <GLFW/glfw3.h>

namespace togr {

Engine::~Engine() {
	shutdown();
}

void Engine::init() {
	input.init();
	windowSystem.init(1280, 720, &input);
	windowCleanupRequired_ = true;
	try {
		renderSystem.init(); // Cleans its own partial allocations if it throws.
		renderCleanupRequired_ = true;
		renderSystem.setResourceManager(&resourceManager);
		renderSystem.setSelectionModel(&selectionModel);
		// Shader registration can fail after some programs have been created.
		resourcesCleanupRequired_ = true;
		resourceManager.init();
		guiSystem.init(windowSystem.getWindow());
		guiCleanupRequired_ = true;
		guiSystem.setRenderSystem(&renderSystem);
		guiSystem.setResourceManager(&resourceManager);
		guiSystem.setSelectionModel(&selectionModel);
		guiSystem.setCamera(&camera);
		resourceManager.setCurrentScene(SceneFactory::createDefaultScene());
	} catch (...) {
		shutdown();
		throw;
	}
}

void Engine::run() {
	while (!windowSystem.getShouldClose()) {
		double deltaTime = getDeltaTime();
		input.update();
		windowSystem.update();
		resourceManager.update();
		renderSystem.update(deltaTime);
		guiSystem.beginFrame();
		renderSystem.render(camera);
		guiSystem.render(deltaTime);   // viewport panel drives the camera (applies next frame)
		windowSystem.swapBuffers();
	}
	shutdown();
}

void Engine::shutdown() {
	if (!windowCleanupRequired_) return;
	glfwMakeContextCurrent(windowSystem.getWindow());
	if (guiCleanupRequired_) {
		guiSystem.shutdown();
		guiCleanupRequired_ = false;
	}
	selectionModel.clear();
	if (renderCleanupRequired_) {
		renderSystem.shutdown();
		renderCleanupRequired_ = false;
	}
	if (resourcesCleanupRequired_) {
		resourceManager.shutdown();
		resourcesCleanupRequired_ = false;
	}
	windowSystem.shutdown();
	windowCleanupRequired_ = false;
}

double Engine::getDeltaTime()
{
	static double lastFrame = 0.0;
	double currentFrame = glfwGetTime();
	double deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;
	return deltaTime;
}
} // namespace togr
