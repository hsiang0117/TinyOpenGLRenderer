#ifndef WINDOWSYSTEM_HPP
#define WINDOWSYSTEM_HPP
#pragma once

#include "../Input.hpp"
#include <iostream>
#include <GLFW/glfw3.h>

class WindowSystem {
public:
	WindowSystem() {};
	void init(int width, int height);
	void swapBuffers();
	void update();
	GLFWwindow* getWindow();
	bool getCursor();
	void setCursor(bool enable);
	void setVsync(bool enable);
	bool getShouldClose();
	void setShouldClose(bool value);
	void shutDown();
private:
	void connectInputToWindow(GLFWwindow* window);
	GLFWwindow* window;
};

void WindowSystem::init(int width, int height) {
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW" << std::endl;
		exit(EXIT_FAILURE);
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	window = glfwCreateWindow(width, height, "TinyOpenglRenderer", NULL, NULL);
	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwFocusWindow(window);
	setCursor(true);
	setVsync(true);
	connectInputToWindow(window);
}

void WindowSystem::swapBuffers() {
	glfwSwapBuffers(window);
}

void WindowSystem::update() {
	glfwPollEvents();

	if (Input::getInstance().isKeyPressed(GLFW_KEY_ESCAPE)) {
		setShouldClose(true);
	}
	if (Input::getInstance().isKeyPressed(GLFW_KEY_LEFT_ALT)) {
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
	const auto keyboardCallback = [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		if (action == GLFW_PRESS) {
			Input::getInstance().onKeyPressed(key);
		}
		else if (action == GLFW_RELEASE) {
			Input::getInstance().onKeyReleased(key);
		}
	};
	glfwSetKeyCallback(window, keyboardCallback);

	const auto mouseCallback = [](GLFWwindow* window, double xpos, double ypos) {
		Input::getInstance().onMouseMoved(xpos, ypos);
	};
	glfwSetCursorPosCallback(window, mouseCallback);

	const auto scrollCallback = [](GLFWwindow* window, double xoffset, double yoffset) {
		Input::getInstance().onScrollMoved(xoffset, yoffset);
	};
	glfwSetScrollCallback(window, scrollCallback);

	const auto windowResizeCallback = [](GLFWwindow* window, int width, int height) {
		Input::getInstance().onWindowResized(width, height);
	};
	glfwSetFramebufferSizeCallback(window, windowResizeCallback);
}
#endif // !WINDOWSYSTEM_HPP