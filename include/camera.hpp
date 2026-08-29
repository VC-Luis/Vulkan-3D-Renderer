#include <glm/glm.hpp>

#ifndef CAMERA_H
#define CAMERA_H

class Camera
{
public:
	const glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);

	//The position of the camera in world space
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);

	//The direction the camera is facing
	glm::vec3 direction = glm::vec3(0.0f, 0.0f, 0.0f);

	glm::vec3 camUp = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 camRight = glm::vec3(0.0f, 0.0f, 0.0f);

	float pitch = 0.0f; //Looking up and down
	float yaw = 0.0f; //Looking left and right
	float fov = 1.0f;
	float sensitivity = 0.1f;

	float maxFOV = 60.0f;
	float minFOV = 1.0f;

	float nearPlane = 0.1f;
	float farPlane = 100.0f;

	Camera(glm::vec3 initialPosition, glm::vec3 initialDirection, float initialFOV, float minimumFOV, float maximumFOV, float mouseSensitivity, float nearClip, float farClip);

    void updateCameraParameters();
};

#endif