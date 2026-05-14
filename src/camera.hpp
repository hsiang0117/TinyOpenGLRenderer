#ifndef CAMERA_HPP
#define CAMERA_HPP
#pragma once

#include "input.hpp"
#include <glm/glm.hpp>

#define DEFAULT_CAMERA_POS glm::vec3(0.0f,0.0f,5.0f)
#define DEFAULT_CAMERA_FRONT glm::vec3(0.0f,0.0f,-1.0f)
#define DEFAULT_CAMERA_UP glm::vec3(0.0f,1.0f,0.0f)
#define DEFAULT_PITCH 0.0f
#define DEFAULT_YAW -90.0f
#define DEFAULT_FOV 55.0f
#define DEFAULT_SENSITIVITY 0.05f

enum Direction {
	FRONT,
	BACK,
	LEFT,
	RIGHT,
	UP,
	DOWN
};

struct Frustum {
	glm::vec4 leftPlane;
	glm::vec4 rightPlane;
	glm::vec4 bottomPlane;
	glm::vec4 topPlane;
	glm::vec4 nearPlane;
	glm::vec4 farPlane;
};

class Camera {
public:
	Camera();
	Camera(glm::vec3 cameraPos);
	glm::vec3 getPos();
	glm::vec3 getFront();
	glm::mat4 getViewMat();
	glm::mat4 getProjectionMat(const float scrWidth, const float scrHeight);
	Frustum getFrustum(const float scrWidth, const float scrHeight);
	void processKeyboard(Direction d, double deltaTime);
	void processMouseMovement(const float xPos, const float yPos);
	void processMouseScroll(const float yOffset);
	void update(double deltaTime);
private:
	glm::vec3 pos;
	glm::vec3 front;
	glm::vec3 up;
	float pitch;
	float yaw;
	float fov;
	float sensitivity;
	bool firstMouse = true;
	float lastX;
	float lastY;
};

#endif // !CAMERA_HPP
