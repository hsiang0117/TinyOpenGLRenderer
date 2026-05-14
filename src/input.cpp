#include "input.hpp"

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
