#include "engine.hpp"
#include "scene/sceneFactory.hpp"
#include <GLFW/glfw3.h>

Engine::~Engine() {
	shutdown();
}

void Engine::init() {
	input.init();
	windowSystem.init(1280, 720, &input);
	windowReady_ = true;
	renderSystem.init();
	renderReady_ = true;
	renderSystem.setResourceManager(&resourceManager);
	renderSystem.setSelectionModel(&selectionModel);
	resourceManager.init();
	resourcesReady_ = true;
	guiSystem.init(windowSystem.getWindow());
	guiReady_ = true;
	guiSystem.setRenderSystem(&renderSystem);
	guiSystem.setResourceManager(&resourceManager);
	guiSystem.setSelectionModel(&selectionModel);
	guiSystem.setCamera(&camera);
	resourceManager.setCurrentScene(SceneFactory::createDefaultScene());
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
	if (!windowReady_) return;
	glfwMakeContextCurrent(windowSystem.getWindow());
	if (guiReady_) {
		guiSystem.shutdown();
		guiReady_ = false;
	}
	selectionModel.clear();
	if (renderReady_) {
		renderSystem.shutdown();
		renderReady_ = false;
	}
	if (resourcesReady_) {
		resourceManager.shutdown();
		resourcesReady_ = false;
	}
	windowSystem.shutdown();
	windowReady_ = false;
}

double Engine::getDeltaTime()
{
	static double lastFrame = 0.0;
	double currentFrame = glfwGetTime();
	double deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;
	return deltaTime;
}
