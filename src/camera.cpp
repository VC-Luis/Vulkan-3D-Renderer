#include "../include/camera.hpp"

Camera::Camera(glm::vec3 initialPosition, glm::vec3 initialDirection, float initialFOV, float minimumFOV, float maximumFOV, float mouseSensitivity, float nearClip, float farClip)
{
    position = initialPosition;
    direction = initialDirection;
    fov = initialFOV;
    minFOV = minimumFOV;
    maxFOV = maximumFOV;
    sensitivity = mouseSensitivity;
    nearPlane = nearClip;
    farPlane = farClip;
}

void Camera::updateCameraParameters()
{
    //Clamp the pitch value between the interval (-90,90)
    if (pitch > 89.99f) pitch = 89.99f;
    if (pitch < -89.99f) pitch = -89.99f;

    //Clamp the fov values
    if (fov > maxFOV) fov = maxFOV;
    if (fov < minFOV) fov = minFOV;

    //Calculate the direction vector based on the yaw and pitch
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = -1 * sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.z = sin(glm::radians(pitch));


    //Calculate the local up and right vectors for the camera
    camRight = glm::normalize(glm::cross(direction, up));
    camUp = glm::normalize(glm::cross(direction, camRight));
}