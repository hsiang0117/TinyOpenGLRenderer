#include "windowSystem.hpp"
#include "logger.hpp"

void WindowSystem::init(int width, int height, Input* input) {
	input_ = input;
	if (!glfwInit()) {
		LOG_ERROR("Failed to initialize GLFW");
		exit(EXIT_FAILURE);
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
	window = glfwCreateWindow(width, height, "TinyOpenglRenderer", NULL, NULL);
	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwFocusWindow(window);
	// Store the injected Input on the window so the C-style GLFW callbacks can reach
	// it (they are capture-less function pointers). ImGui's GLFW backend deliberately
	// does NOT use the window user pointer, so this slot is free.
	glfwSetWindowUserPointer(window, input_);
	setCursor(true);
	setVsync(true);
	connectInputToWindow(window);
}

void WindowSystem::swapBuffers() {
	glfwSwapBuffers(window);
}

void WindowSystem::update() {
	glfwPollEvents();

	if (input_->isKeyPressed(GLFW_KEY_ESCAPE)) {
		setShouldClose(true);
	}
	if (input_->isKeyPressed(GLFW_KEY_LEFT_ALT)) {
		setCursor(!getCursor());
	}
}

GLFWwindow* WindowSystem::getWindow()
{
	return window;
}

bool WindowSystem::getCursor()
{
	return glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL ? true : false;
}

void WindowSystem::setCursor(bool enable) {
	if (enable) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

void WindowSystem::setVsync(bool enable) {
	if (enable) {
		glfwSwapInterval(1);
	}
	else {
		glfwSwapInterval(0);
	}
}

bool WindowSystem::getShouldClose() {
	return glfwWindowShouldClose(window);
}

void WindowSystem::setShouldClose(bool value) {
	glfwSetWindowShouldClose(window, value);
}

void WindowSystem::shutDown(){
	glfwDestroyWindow(window);
	glfwTerminate();
}

void WindowSystem::connectInputToWindow(GLFWwindow* window)
{
	// Callbacks are C-style function pointers (capture-less). They retrieve the
	// injected Input via the window user pointer set in init().
	const auto keyboardCallback = [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
		if (!input) return;
		if (action == GLFW_PRESS) {
			input->onKeyPressed(key);
		}
		else if (action == GLFW_RELEASE) {
			input->onKeyReleased(key);
		}
	};
	glfwSetKeyCallback(window, keyboardCallback);

	const auto mouseCallback = [](GLFWwindow* window, double xpos, double ypos) {
		Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
		if (input) input->onMouseMoved(xpos, ypos);
	};
	glfwSetCursorPosCallback(window, mouseCallback);

	const auto scrollCallback = [](GLFWwindow* window, double xoffset, double yoffset) {
		Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
		if (input) input->onScrollMoved(xoffset, yoffset);
	};
	glfwSetScrollCallback(window, scrollCallback);

	const auto windowResizeCallback = [](GLFWwindow* window, int width, int height) {
		Input* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
		if (input) input->onWindowResized(width, height);
	};
	glfwSetFramebufferSizeCallback(window, windowResizeCallback);
}
