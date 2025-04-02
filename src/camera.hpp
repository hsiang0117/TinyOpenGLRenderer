#ifndef CAMERA_HPP
#define CAMERA_HPP
#pragma once

#include "input.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

class Camera {
public:
	Camera();
	Camera(glm::vec3 cameraPos);
	glm::vec3 getPos();
	glm::vec3 getFront();
	glm::mat4 getViewMat();
	glm::mat4 getProjectionMat(const float scrWidth, const float scrHeight);
	void processKeyboard(Direction d);
	void processMouseMovement(const float xPos, const float yPos);
	void processMouseScroll(const float yOffset);
	void update();
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

void Camera::processKeyboard(Direction d) {
	float cameraSpeed = 2.5f;
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

void Camera::update()
{

}

#endif // !CAMERA_HPP