#ifndef INPUT_HPP
#define INPUT_HPP
#pragma once

class Input {
public:
	Input() = default;
	~Input() = default;
	static Input& getInstance();
	void init();
	void update();
	bool isKeyPressed(int key);
	bool isKeyHeld(int key);
	bool isMouseMoved() { return mouseMoved; }
	double getMouseX() { return mouseX; }
	double getMouseY() { return mouseY; }
	bool isScrollMoved() { return scrollMoved; }
	double getScrollX() { return scrollX; }
	double getScrollY() { return scrollY; }
	bool isWindowResized() { return windowResized; }
	int getWindowWidth() { return windowWidth; }
	int getWindowHeight() { return windowHeight; }
	bool isUiResized() { return uiResized; }
	void onKeyPressed(int key) { keys[key] = true; }
	void onKeyReleased(int key) { keys[key] = false; }
	void onMouseMoved(double x, double y) { mouseMoved = true; mouseX = x; mouseY = y; }
	void onScrollMoved(double x, double y) { scrollMoved = true; scrollX = x; scrollY = y; }
	void onWindowResized(int width, int height) { windowResized = true; windowWidth = width; windowHeight = height; }
	void onUiResized() { uiResized = true; }
private:
	bool keys[1024], prevKeys[1024];
	bool mouseMoved;
	double mouseX, mouseY;
	bool scrollMoved;
	double scrollX, scrollY;
	bool windowResized;
	int windowWidth = 1280, windowHeight = 720;
	bool uiResized;
};

Input& Input::getInstance() {
	static Input instance;
	return instance;
}

void Input::init()
{
	for (int i = 0; i < 1024; i++) {
		keys[i] = false;
		prevKeys[i] = false;
	}
	mouseMoved = false;
	scrollMoved = false;
	windowResized = false;
}

void Input::update() {
	for (int i = 0; i < 1024; i++) {
		prevKeys[i] = keys[i];
	}
	mouseMoved = false;
	scrollMoved = false;
	windowResized = false;
}

bool Input::isKeyPressed(int key) {
	return keys[key] && !prevKeys[key];
}

bool Input::isKeyHeld(int key) {
	return keys[key];
}

#endif // !INPUT_HPP