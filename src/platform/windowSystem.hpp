#ifndef WINDOWSYSTEM_HPP
#define WINDOWSYSTEM_HPP
#pragma once

#include <string>

class Input;
struct GLFWwindow;

class WindowSystem {
public:
	WindowSystem() = default;
	~WindowSystem() = default;
	void init(int width, int height, Input* input);
	void swapBuffers();
	void update();
	GLFWwindow* getWindow();
	bool getShouldClose();
	void setShouldClose(bool value);
	void shutdown();
	static void setVsync(bool enable);
	static void showError(const std::string& message);
private:
	void connectInputToWindow(GLFWwindow* window);
	GLFWwindow* window;
	Input* input_ = nullptr;   // non-owning; injected via init()
};

#endif // !WINDOWSYSTEM_HPP
