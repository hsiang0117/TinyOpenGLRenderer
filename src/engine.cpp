#include "engine.hpp"
#include "input.hpp"
#include "sceneFactory.hpp"
#include <GLFW/glfw3.h>

void Engine::init() {
	input.init();
	windowSystem.init(1280, 720, &input);
	renderSystem.init();
	renderSystem.setResourceManager(&resourceManager);
	resourceManager.init();
	guiSystem.init(windowSystem.getWindow());
	guiSystem.setRenderSystem(&renderSystem);
	guiSystem.setResourceManager(&resourceManager);
	resourceManager.setCurrentScene(SceneFactory::createDefaultScene());
}

void Engine::run() {
	while (!windowSystem.getShouldClose()) {
		double deltaTime = getDeltaTime();
		input.update();
		windowSystem.update();
		resourceManager.update();
		camera.update(deltaTime, input);
		renderSystem.update(deltaTime);
		guiSystem.beginFrame();
		renderSystem.render(camera);
		guiSystem.render(deltaTime);
		windowSystem.swapBuffers();
	}
	guiSystem.shutDown();
	resourceManager.shutdown();
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
