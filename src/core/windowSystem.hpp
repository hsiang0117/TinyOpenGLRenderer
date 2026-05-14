#ifndef WINDOWSYSTEM_HPP
#define WINDOWSYSTEM_HPP
#pragma once

#include "../Input.hpp"
#include <iostream>
#include <GLFW/glfw3.h>

class WindowSystem {
public:
	WindowSystem() = default;
	~WindowSystem() = default;
	void init(int width, int height);
	void swapBuffers();
	void update();
	GLFWwindow* getWindow();
	bool getCursor();
	void setCursor(bool enable);
	bool getShouldClose();
	void setShouldClose(bool value);
	void shutDown();
	static void setVsync(bool enable);
private:
	void connectInputToWindow(GLFWwindow* window);
	GLFWwindow* window;
};

#endif // !WINDOWSYSTEM_HPP
