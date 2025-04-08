#ifndef COMPONENT_HPP
#define COMPONENT_HPP
#pragma once

#include <iostream>
#include <glm/glm.hpp>

class Component {
public:
	virtual ~Component() {}
	virtual std::string getName() const = 0;
};

using ComponentPtr = std::shared_ptr<Component>;

class Transform : public Component {
public:
    Transform()
        : translate(0.0f), scale(1.0f), rotate(0.0f)
    {
        name = "Transform";
    }

    virtual std::string getName() const override { return name; }

    glm::vec3 translate;
    glm::vec3 scale;
    glm::vec3 rotate;

private:
    std::string name;
};
#endif