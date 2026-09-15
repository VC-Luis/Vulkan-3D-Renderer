#include <window.hpp>
#include <Renderer3D.hpp>
#include <vertex.hpp>
#include <camera.hpp>
#include "../include/mesh.hpp"

#include <chrono>

Camera cam(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 60.0f, 1.0f, 90.0f, 0.1f, 0.1f, 1000.0f);
bool firstMouse = true;
Window window(800, 600, "3D Renderer Test");
auto [width, height] = window.getSize();
float lastX = (float)width / 2;
float lastY = (float)height / 2;

void mouse_callback(GLFWwindow* window, double xPos, double yPos)
{
    if (firstMouse == true)
    {
        lastX = (float)xPos;
        lastY = (float)yPos;
        firstMouse = false;
    }
    float xOffset = (float)xPos - (float)lastX;
    float yOffset = (float)yPos - (float)lastY;
    lastX = (float)xPos;
    lastY = (float)yPos;

    xOffset *= cam.getSensitivity();
    yOffset *= cam.getSensitivity();

    cam.setYaw(cam.getYaw() + xOffset);
    cam.setPitch(cam.getPitch() - yOffset);
}

int main(int argc, char const *argv[])
{
    glfwSetCursorPosCallback(window.getHandle(), mouse_callback);
	glfwSetInputMode(window.getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    Renderer3D renderer(window, true);

    renderer.createGraphicsPipeline("assets/shader.spv", "vertMain", "assets/shader.spv", "fragMain");
    
    Texture boatTexture("assets/colormap.png");
    Texture ocreanLinerTexture("assets/inverted-colormap.png");
    Texture boatHouseTexture("assets/colorlessmap.png");

    Mesh shipMesh("assets/ship-large.obj");
    Model boatTestModel(shipMesh, glm::vec3(0.0f, 0.0f, 0.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f), boatTexture);
    renderer.drawModel(&boatTestModel);

    Mesh oceanLinerMesh("assets/ship-ocean-liner.obj");
    Model oceanModel(oceanLinerMesh, glm::vec3(0.0f, 10.0f, 0.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f), ocreanLinerTexture);
    renderer.drawModel(&oceanModel);

    Mesh boatHouseMesh("assets/boat-house-b.obj");
    Model boatHouseModel(boatHouseMesh, glm::vec3(0.0f, 0.0f, 10.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f), boatHouseTexture);
    renderer.drawModel(&boatHouseModel);

    renderer.createBuffers();

    renderer.loadTexture(boatTexture);
    renderer.loadTexture(ocreanLinerTexture);
    renderer.loadTexture(boatHouseTexture);

    renderer.createDescriptors(sizeof(CameraUBO));
    renderer.createSyncObjects();

    double angle = 0;


    while(!window.windowShouldClose())
    {
        cam.updateCameraParameters();

        angle += 0.01;

        boatTestModel.rotationAngle += 0.01f;
        boatTestModel.position = glm::vec3{0.0f, 0.0f, sin(angle) * 10};

        if(glfwGetKey(window.getHandle(), GLFW_KEY_W) == GLFW_PRESS)
        {
            cam.move(cam.getDirection() * 0.1f);
        }
        if(glfwGetKey(window.getHandle(), GLFW_KEY_S) == GLFW_PRESS)
        {
            cam.move(-cam.getDirection() * 0.1f);
        }
        if(glfwGetKey(window.getHandle(), GLFW_KEY_A) == GLFW_PRESS)
        {
            cam.move(-cam.getRight() * 0.1f);
        }
        if(glfwGetKey(window.getHandle(), GLFW_KEY_D) == GLFW_PRESS)
        {
            cam.move(+cam.getRight() * 0.1f);
        }
        if(glfwGetKey(window.getHandle(), GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            cam.move(glm::vec3(0.0f, 0.0f, 0.1f));
        }
        if(glfwGetKey(window.getHandle(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        {
            cam.move(glm::vec3(0.0f, 0.0f, -0.1f));
        }

        //First, we have to wait for the previous frame to finish rendering
        renderer.waitForFrame();

        //After that, we fetch the next image from the swap chain
        renderer.fetchNewImage(window, cam);
    }

    renderer.cleanUpSwapchain();

    return 0;
}
