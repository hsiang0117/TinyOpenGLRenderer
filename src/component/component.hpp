#ifndef COMPONENT_HPP
#define COMPONENT_HPP
#pragma once

class Component {
public:

	Component() {};
	virtual ~Component() {};
	virtual void init() = 0;
	virtual void update() = 0;
	virtual void render() = 0;
	virtual void shutDown() = 0;
};

#endif