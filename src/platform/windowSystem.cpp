#include "windowSystem.hpp"
#include "input.hpp"
#include <GLFW/glfw3.h>
#include "core/logger.hpp"
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>

namespace togr {

void WindowSystem::showError(const std::string& message) {
	MessageBoxA(nullptr, message.c_str(), "TinyOpenGLRenderer - startup/render failure", MB_OK | MB_ICONERROR);
}

void WindowSystem::init(int width, int height, Input* input) {
	input_ = input;
	if (!glfwInit()) {
		TOGR_LOG_ERROR("Failed to initialize GLFW");
		throw std::runtime_error("Failed to initialize GLFW");
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
	window = glfwCreateWindow(width, height, "TinyOpenglRenderer", NULL, NULL);
	if (!window) {
		const char* description = nullptr;
		glfwGetError(&description);
		std::string message = "Cannot create an OpenGL 4.5 window: ";
		message += description ? description : "unknown GLFW error";
		glfwTerminate();
		throw std::runtime_error(message);
	}
	glfwMakeContextCurrent(window);
	glfwFocusWindow(window);
	// Store the injected Input on the window so the C-style GLFW callbacks can reach
	// it (they are capture-less function pointers). ImGui's GLFW backend deliberately
	// does NOT use the window user pointer, so this slot is free.
	glfwSetWindowUserPointer(window, input_);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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
}

GLFWwindow* WindowSystem::getWindow()
{
	return window;
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

void WindowSystem::shutdown(){
	glfwDestroyWindow(window);
	glfwTerminate();
}

void WindowSystem::connectInputToWindow(GLFWwindow* window)
{
	// This capture-less GLFW callback reaches Input through the window user pointer.
	// ImGui's GLFW backend handles mouse, scroll and display-size updates.
	const auto keyboardCallback = [](GLFWwindow* window, int key, int, int action, int) {
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
}
} // namespace togr
