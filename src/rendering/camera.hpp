#ifndef TOGR_CAMERA_HPP
#define TOGR_CAMERA_HPP
#pragma once

#include <glm/glm.hpp>

#define TOGR_DEFAULT_CAMERA_POS glm::vec3(0.0f,0.0f,5.0f)
#define TOGR_DEFAULT_CAMERA_FRONT glm::vec3(0.0f,0.0f,-1.0f)
#define TOGR_DEFAULT_CAMERA_UP glm::vec3(0.0f,1.0f,0.0f)
#define TOGR_DEFAULT_PITCH 0.0f
#define TOGR_DEFAULT_YAW -90.0f
#define TOGR_DEFAULT_FOV 55.0f
#define TOGR_DEFAULT_SENSITIVITY 0.05f

namespace togr {

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
	// Editor fly-camera primitives (driven by the Viewport panel via ImGui input):
	// lookDelta applies a per-frame pixel delta to yaw/pitch; zoom adjusts the FOV.
	void lookDelta(float dxPixels, float dyPixels);
	void zoom(float wheelSteps);
private:
	glm::vec3 pos;
	glm::vec3 front;
	glm::vec3 up;
	float pitch;
	float yaw;
	float fov;
	float sensitivity;
};

} // namespace togr

#endif // !TOGR_CAMERA_HPP
