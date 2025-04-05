#ifndef RESOURCEMANAGER_HPP
#define RESOURCEMANAGER_HPP
#pragma once

#include "../model.hpp"
#include <iostream>

class ResourceManager {
public:
	ResourceManager() {};
	static ResourceManager& getInstance();
	void init();
private:
	
};

ResourceManager& ResourceManager::getInstance() {
	static ResourceManager instance;
	return instance;
}

void ResourceManager::init() {
	
}

#endif // !RESOURCEMANAGER_HPP