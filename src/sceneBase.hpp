#ifndef SCENEBASE_HPP
#define SCENEBASE_HPP
#pragma once

#include <iostream>
#include "item.hpp"

class SceneBase {
public:
	void init();
private:
	std::vector<Item> itemList;
};

#endif