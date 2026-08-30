#include <window.hpp>
#include <Renderer3D.hpp>
#include <vertex.hpp>
#include <camera.hpp>
#include "../include/mesh.hpp"

Camera cam(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 60.0f, 1.0f, 90.0f, 0.1f, 0.1f, 1000.0f);
bool firstMouse = true;
Window window(800, 600, "3D Renderer Test");
float lastX = (float)window.width / 2;
float lastY = (float)window.height / 2;

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

    xOffset *= cam.sensitivity;
    yOffset *= cam.sensitivity;


    cam.yaw += xOffset;
    cam.pitch += -yOffset;
}

int main(int argc, char const *argv[])
{
    Renderer3D renderer;

    glfwSetCursorPosCallback(window.GLWindow, mouse_callback);
	glfwSetInputMode(window.GLWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    renderer.engineSetup("Test", window, true, {0, 1, 0});
    renderer.setupGPU();
    renderer.generateImageManagement(window);
    renderer.createDepthResources();
    renderer.generateCommandInfrastructure();

     renderer.createGraphicsPipeline("assets/shader.spv", "vertMain", "assets/shader.spv", "fragMain");

    Mesh shipMesh("assets/ship-large.obj");
    renderer.loadModel(shipMesh);

    renderer.createBuffers();

    renderer.createTextureImage("assets/colormap.png");
    renderer.createTextureImageView();
    renderer.createTextureSampler();


    renderer.createDescriptors(sizeof(UniformBufferObject));
    renderer.createSyncObjects();

    while(!window.windowShouldClose())
    {
        glfwPollEvents();
        cam.updateCameraParameters();

        if(glfwGetKey(window.GLWindow, GLFW_KEY_W) == GLFW_PRESS)
        {
            cam.position += cam.direction * 0.1f;
        }
        if(glfwGetKey(window.GLWindow, GLFW_KEY_S) == GLFW_PRESS)
        {
            cam.position -= cam.direction * 0.1f;
        }
        if(glfwGetKey(window.GLWindow, GLFW_KEY_A) == GLFW_PRESS)
        {
            cam.position -= cam.camRight * 0.1f;
        }
        if(glfwGetKey(window.GLWindow, GLFW_KEY_D) == GLFW_PRESS)
        {
            cam.position += cam.camRight * 0.1f;
        }
        if(glfwGetKey(window.GLWindow, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            cam.position += cam.up * 0.1f;
        }
        if(glfwGetKey(window.GLWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        {
            cam.position -= cam.up * 0.1f;
        }

        //First, we have to wait for the previous frame to finish rendering
        renderer.waitForFrame();

        //After that, we fetch the next image from the swap chain
        renderer.fetchNewImage(window, cam);
    }

    std::cout << "RENDERING OVER" << std::endl;

    renderer.cleanUpSwapchain();

    return 0;
}


