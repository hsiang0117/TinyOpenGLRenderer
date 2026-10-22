#ifndef INPUT_HPP
#define INPUT_HPP
#pragma once

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

#endif // !INPUT_HPP
