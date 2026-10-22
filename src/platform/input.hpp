#ifndef TOGR_INPUT_HPP
#define TOGR_INPUT_HPP
#pragma once

namespace togr {

class Input {
public:
	Input() = default;
	~Input() = default;
	void init();
	void update();
	bool isKeyPressed(int key);
	void onKeyPressed(int key) { keys[key] = true; }
	void onKeyReleased(int key) { keys[key] = false; }
private:
	bool keys[1024], prevKeys[1024];
};

} // namespace togr

#endif // !TOGR_INPUT_HPP
