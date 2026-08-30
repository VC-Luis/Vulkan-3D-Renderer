#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include "window.hpp"
#include "vertex.hpp"
#include "camera.hpp"
#include "mesh.hpp"

#ifndef RENDERER3D_H
#define RENDERER3D_H

struct EngineVersion
{
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
};

struct UniformBufferObject
{
    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
};
    
int myScoringFunction(vk::raii::PhysicalDevice GPU);

class Renderer3D
{
public:
    void engineSetup(std::string engineName, Window& showWindow, bool enableValidationLayers, EngineVersion version);
    void setupGPU(int (*GPUScoringFunction) (vk::raii::PhysicalDevice GPU) = myScoringFunction);
    void generateImageManagement(Window& showWindow);
    void generateCommandInfrastructure();
    void createBuffers();
    void createDescriptors(size_t UBOSize);

    void waitForFrame();
    void fetchNewImage(Window& showWindow, Camera cam);

    void cleanUpSwapchain();

    void createInstance(std::string engineName, Window& showWindow, bool enableValidationLayers, EngineVersion version);
    void setupDebugMessenger(bool enableValidationLayers);
    void createSurface(Window& window);
    void pickPhysicalDevice(int (*scoringFunction) (vk::raii::PhysicalDevice GPU));
    void createLogicalDevice();
    void createSwapchain(Window& showWindow);
    void createImageViews();
    void createDescriptorSetLayout();
    void createGraphicsPipeline(const std::string& vertShaderPath, const char* vertStartpoint, const std::string& fragShaderPath, const char* fragStartpoint);
    void createCommandPool();
    void loadModel(Mesh mesh);
    void createVertexBuffer(std::vector<Vertex> vertices);
    void createIndexBuffer(std::vector<uint32_t> indices);
    void createUniformBuffers(size_t UBOSize);
    void createDescriptorPool();
    void createDescriptorSets(size_t UBOSize);
    void createCommandBuffers();
    void createSyncObjects();
    void createTextureImage(std::string textureFile);
    void createTextureImageView();
    void createTextureSampler();
    void createDepthResources();

    int MAX_FRAMES_IN_FLIGHT;

private:

    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities, Window& window);
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;
    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size);
    void recreateSwapchain(Window& showWindow);
    void updateUniformBuffer(uint32_t currentImage, Camera cam);
    void recordCommandBuffer(uint32_t imageIndex, uint32_t numIndices);
    void transitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask, vk::ImageAspectFlags imageAspectFlags);
    std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);
    vk::raii::CommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(vk::raii::CommandBuffer&& commandBuffer);
    vk::raii::ImageView createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags);
    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
    vk::Format findDepthFormat();

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr; //The Vulkan instance is the connection between this application and the Vulkan library

    const std::vector<char const*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

    vk::raii::PhysicalDevice physicalDevice = nullptr; //The actual hardware that will do the computation we code later
    vk::raii::Device logicalDevice = nullptr; //The logical device that will interface with the hardware
    vk::raii::Queue graphicsQueue = nullptr;
    uint32_t queueIndex = UINT32_MAX;

    vk::raii::SurfaceKHR surface = nullptr;

    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::SurfaceFormatKHR swapChainSurfaceFormat;
    vk::Extent2D swapChainExtent;

    std::vector<vk::raii::ImageView> swapChainImageViews; //Image views describe how to access and image and what part of it

    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout pipelineLayout = nullptr; //Holds uniform values, which can be changed at drawing time without regenerating the entire graphics pipeline
    vk::raii::Pipeline graphicsPipeline = nullptr; //The actual graphics pipeline itself

    vk::raii::CommandPool commandPool = nullptr; //The command pool manages the memory used to store the buffers and it allocates the command buffers
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;

    uint32_t frameIndex = 0;
    bool framebufferResized = false;

    std::vector<Vertex> vertices;
    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;

    std::vector<uint32_t> indices;
    vk::raii::Buffer indexBuffer = nullptr;
    vk::raii::DeviceMemory indexBufferMemory = nullptr;

    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    vk::raii::DescriptorPool descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;

    vk::raii::Image textureImage = nullptr;
    vk::raii::DeviceMemory textureImageMemory = nullptr;
    vk::raii::ImageView textureImageView = nullptr;
    vk::raii::Sampler textureSampler = nullptr;

    vk::raii::Image depthImage = nullptr;
    vk::raii::DeviceMemory depthImageMemory = nullptr;
    vk::raii::ImageView depthImageView = nullptr;
};

#endif