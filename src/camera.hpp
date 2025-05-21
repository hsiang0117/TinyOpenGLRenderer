#ifndef CAMERA_HPP
#define CAMERA_HPP
#pragma once

#include "input.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
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

Camera::Camera() {
	pos = DEFAULT_CAMERA_POS;
	front = DEFAULT_CAMERA_FRONT;
	up = DEFAULT_CAMERA_UP;
	pitch = DEFAULT_PITCH;
	yaw = DEFAULT_YAW;
	fov = DEFAULT_FOV;
	sensitivity = DEFAULT_SENSITIVITY;
}

Camera::Camera(glm::vec3 cameraPos) {
	pos = cameraPos;
	front = DEFAULT_CAMERA_FRONT;
	up = DEFAULT_CAMERA_UP;
	pitch = DEFAULT_PITCH;
	yaw = DEFAULT_YAW;
	fov = DEFAULT_FOV;
	sensitivity = DEFAULT_SENSITIVITY;
}

glm::vec3 Camera::getPos() {
	return pos;
}

glm::vec3 Camera::getFront() {
	return front;
}

glm::mat4 Camera::getViewMat() {
	return glm::lookAt(pos, pos + front, up);
}

glm::mat4 Camera::getProjectionMat(const float scrWidth, const float scrHeight) {
	return glm::perspective(glm::radians(fov), scrWidth / scrHeight, 0.1f, 100.0f);
}

Frustum Camera::getFrustum(const float scrWidth, const float scrHeight) {
	glm::mat4 proj = getProjectionMat(scrWidth, scrHeight);
	glm::mat4 view = getViewMat();
	glm::mat4 vp = proj * view;

	Frustum frustum;

	// 提取平面
	// 左
	frustum.leftPlane.x = vp[0][3] + vp[0][0];
	frustum.leftPlane.y = vp[1][3] + vp[1][0];
	frustum.leftPlane.z = vp[2][3] + vp[2][0];
	frustum.leftPlane.w = vp[3][3] + vp[3][0];
	// 右
	frustum.rightPlane.x = vp[0][3] - vp[0][0];
	frustum.rightPlane.y = vp[1][3] - vp[1][0];
	frustum.rightPlane.z = vp[2][3] - vp[2][0];
	frustum.rightPlane.w = vp[3][3] - vp[3][0];
	// 下
	frustum.bottomPlane.x = vp[0][3] + vp[0][1];
	frustum.bottomPlane.y = vp[1][3] + vp[1][1];
	frustum.bottomPlane.z = vp[2][3] + vp[2][1];
	frustum.bottomPlane.w = vp[3][3] + vp[3][1];
	// 上
	frustum.topPlane.x = vp[0][3] - vp[0][1];
	frustum.topPlane.y = vp[1][3] - vp[1][1];
	frustum.topPlane.z = vp[2][3] - vp[2][1];
	frustum.topPlane.w = vp[3][3] - vp[3][1];
	// 近
	frustum.nearPlane.x = vp[0][3] + vp[0][2];
	frustum.nearPlane.y = vp[1][3] + vp[1][2];
	frustum.nearPlane.z = vp[2][3] + vp[2][2];
	frustum.nearPlane.w = vp[3][3] + vp[3][2];
	// 远
	frustum.farPlane.x = vp[0][3] - vp[0][2];
	frustum.farPlane.y = vp[1][3] - vp[1][2];
	frustum.farPlane.z = vp[2][3] - vp[2][2];
	frustum.farPlane.w = vp[3][3] - vp[3][2];

	// 归一化每个平面
	auto normalizePlane = [](glm::vec4& plane) {
		float len = glm::length(glm::vec3(plane));
		plane /= len;
		};
	normalizePlane(frustum.leftPlane);
	normalizePlane(frustum.rightPlane);
	normalizePlane(frustum.bottomPlane);
	normalizePlane(frustum.topPlane);
	normalizePlane(frustum.nearPlane);
	normalizePlane(frustum.farPlane);

	return frustum;
}

void Camera::processKeyboard(Direction d, double deltaTime) {
	float cameraSpeed = 2.5f * deltaTime;
	switch (d) {
	case FRONT:
		pos += cameraSpeed * front;
		break;
	case BACK:
		pos -= cameraSpeed * front;
		break;
	case LEFT:
		pos -= glm::normalize(glm::cross(front, up)) * cameraSpeed;
		break;
	case RIGHT:
		pos += glm::normalize(glm::cross(front, up)) * cameraSpeed;
		break;
	case UP:
		pos += glm::normalize(up) * cameraSpeed;
		break;
	case DOWN:
		pos -= glm::normalize(up) * cameraSpeed;
		break;
	}
}

void Camera::processMouseMovement(float xPos, float yPos) {
	if (firstMouse)
	{
		lastX = xPos;
		lastY = yPos;
		firstMouse = false;
	}

	float xoffset = xPos - lastX;
	float yoffset = lastY - yPos;
	lastX = xPos;
	lastY = yPos;

	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	this->front = glm::normalize(front);
}

void Camera::processMouseScroll(float yOffset) {
	if (fov >= 1.0f && fov <= 70.0f)
		fov -= yOffset;
	if (fov <= 1.0f)
		fov = 1.0f;
	if (fov >= 70.0f)
		fov = 70.0f;
}

void Camera::update(double deltaTime)
{
	static bool movable = false;
	if (Input::getInstance().isKeyPressed(GLFW_KEY_LEFT_ALT)) {
		movable = !movable;
		if (!movable) {
			firstMouse = true;
		}
	}

	if (!movable) {
		return;
	}

	// Movement
	if (Input::getInstance().isKeyHeld(GLFW_KEY_W)) {
		processKeyboard(FRONT, deltaTime);
	}
	if (Input::getInstance().isKeyHeld(GLFW_KEY_S)) {
		processKeyboard(BACK, deltaTime);
	}
	if (Input::getInstance().isKeyHeld(GLFW_KEY_A)) {
		processKeyboard(LEFT, deltaTime);
	}
	if (Input::getInstance().isKeyHeld(GLFW_KEY_D)) {
		processKeyboard(RIGHT, deltaTime);
	}
	if (Input::getInstance().isKeyHeld(GLFW_KEY_SPACE)) {
		processKeyboard(UP, deltaTime);
	}
	if (Input::getInstance().isKeyHeld(GLFW_KEY_LEFT_CONTROL)) {
		processKeyboard(DOWN, deltaTime);
	}

	// Scroll
	if (Input::getInstance().isMouseMoved()) {
		double xPos = Input::getInstance().getMouseX();
		double yPos = Input::getInstance().getMouseY();
		processMouseMovement((float)xPos, (float)yPos);
	}

	// Fov change
	if (Input::getInstance().isScrollMoved()) {
		double yOffset = Input::getInstance().getScrollY();
		processMouseScroll((float)yOffset);
	}
}

#endif // !CAMERA_HPP