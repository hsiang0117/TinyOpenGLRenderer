#include "input.hpp"

namespace togr {

void Input::init()
{
	for (int i = 0; i < 1024; i++) {
		keys[i] = false;
		prevKeys[i] = false;
	}
}

void Input::update() {
	for (int i = 0; i < 1024; i++) {
		prevKeys[i] = keys[i];
	}
}

bool Input::isKeyPressed(int key) {
	return keys[key] && !prevKeys[key];
}
} // namespace togr
