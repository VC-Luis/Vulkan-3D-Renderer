#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include "window.hpp"
#include "vertex.hpp"
#include "camera.hpp"
#include "mesh.hpp"
#include "model.hpp"
#include "texture.hpp"

#ifndef RENDERER3D_H
#define RENDERER3D_H

struct CameraUBO
{
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
};
    
int dGPUBiascoringFunction(vk::raii::PhysicalDevice GPU);

class Renderer3D
{
public:
    Renderer3D(Window& showWindow, bool enableValidationLayers);
    /**
     * @brief Sets up the Vulkan instance and associates it with the given window.
     * 
     * @param showWindow The window to associate with the instance
     * @param enableValidationLayers Determines if the validation layers, which show debug messages, are enabled
     */
    void engineSetup(Window& showWindow, bool enableValidationLayers);
    /**
     * @brief Picks a graphics card and links it to Vulkan
     * 
     * @param GPUScoringFunction The function that gives a score to each graphics card. The one with the biggest score will be picked.
     */
    void setupGPU(int (*GPUScoringFunction) (vk::raii::PhysicalDevice GPU) = dGPUBiascoringFunction);
    /**
     * @brief Generates all Vulkan resources related to the final image output
     * 
     * @param showWindow The window that will be rendered to
     */
    void generateImageManagement(Window& showWindow);
    /**
     * @brief Creates all necessary objects to handle sending commands to the GPU.
     */
    void generateCommandInfrastructure();
    /**
     * @brief Fills the vertex and index buffers of all models, which will later be sent to the GPU.
     */
    void createBuffers();
    /**
     * @brief Create the descriptor sets that will send camera information to the GPU.
     * 
     * @param UBOSize The size (in bytes) of the data block to be sent
     */
    void createDescriptors(size_t UBOSize = sizeof(CameraUBO));

    /**
     * @brief Generate texture resources and load them into the rendering pipeline.
     * 
     * @param texture The texture to be loaded
     */
    void loadTexture(Texture& texture);

    /**
     * @brief Wait for the previous frame to end rendering fully.
     */
    void waitForFrame();
    /**
     * @brief Updates all data being sent to the GPU and creates the new frame.
     * 
     * @param showWindow the window to render to
     * @param cam The camera from which to render
     */
    void fetchNewImage(Window& showWindow, Camera cam);
    /**
     * @brief Cleans up the swap chain and its images
     */
    void cleanUpSwapchain();
    /**
     * @brief Sends a model to be rendered
     * 
     * @param model The model to be rendered.
     */
    void drawModel(Model* model);
    /**
     * @brief Creates the descriptor sets for a texture for transfer to the GPU.
     * 
     * @param texture The texture that will have the descriptor sets created
     */
    void createTextureDescriptorSets(Texture& texture);
    /**
     * @brief Creates the descriptor layout for textures
     */
    void createTextureDescriptorLayout();
    /**
     * @brief Creates the descriptor pools and adds more as needed.
     */
    void createTextureDescriptorPool();
    /**
     * @brief Generates the graphics pipeline, which defines the way everything to do with rendering is going to work.
     * 
     * @param vertShaderPath The location of the vertex shader file
     * @param vertStartPoint The name of the function that starts the vertex shader
     * @param fragShaderPath The location of the fragment shader file
     * @param fragStartPoint The name of the function that starts the fragment shader
     * 
     * @note The vertex and fragment shaders can be in a single file
     */
    void createGraphicsPipeline(const std::string& vertShaderPath, const char* vertStartpoint, const std::string& fragShaderPath, const char* fragStartpoint);
    /**
     * @brief Creates the commnad pool, which holds the commands that will later be sent to the GPU.
     */
    void createCommandPool();
    /**
     * @brief Creates the vertex buffer for the given mesh, which holds the vertices' data for the GPU
     * 
     * @param mesh The mesh holding the vertex buffer to be generated.
     */
    void createVertexBuffer(Mesh& mesh);
    /**
     * @brief Creates the index buffer for the given mesh, which holds the indices' data for the GPU
     * 
     * @param mesh The mesh holding the index buffer to be generated.
     */
    void createIndexBuffer(Mesh& mesh);
    /**
     * @brief Creates objects to manage synchronization between frames.
     */
    void createSyncObjects();
    /**
     * @brief Creates all needed resources to manage depth mapping
     */
    void createDepthResources();

    //The maximum number of frames that are to be rendered at once
    int MAX_FRAMES_IN_FLIGHT = 2;
    //The number of textures that fit inside a texture descriptor pool
    int TEXTURES_IN_POOL = 2;

private:
    const uint8_t engineMajorVersion = 0;
    const uint8_t engineMinorVersion = 1;
    const uint8_t enginePatchVersion = 0;

    const char* engineName = "Bedrock Engine";

    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities, Window& window);
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;
    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size);
    void recreateSwapchain(Window& showWindow);
    void updateUniformBuffer(uint32_t currentImage, Camera cam);
    void recordCommandBuffer(uint32_t imageIndex);
    void transitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask, vk::ImageAspectFlags imageAspectFlags);
    std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);
    vk::raii::CommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(vk::raii::CommandBuffer&& commandBuffer);
    vk::raii::ImageView createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags);
    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
    vk::Format findDepthFormat();
    void createInstance(Window& showWindow, bool enableValidationLayers);
    void setupDebugMessenger(bool enableValidationLayers);
    void createSurface(Window& window);
    void pickPhysicalDevice(int (*scoringFunction) (vk::raii::PhysicalDevice GPU));
    void createLogicalDevice();
    void createSwapchain(Window& showWindow);
    void createImageViews();
    void createDescriptorSetLayout();
    void createUniformBuffers(size_t UBOSize = sizeof(CameraUBO));
    void createDescriptorPool();
    void createDescriptorSets(size_t UBOSize = sizeof(CameraUBO));
    void createCommandBuffers();
    void createTextureImage(Texture& texture);
    void createTextureImageView(Texture& texture);
    void createTextureSampler(Texture& texture);

    vk::raii::Context context; //The Vulkan context provides a way to access Vulkan functions, which will be used by an instance
    vk::raii::Instance instance = nullptr; //The Vulkan instance is the connection between this application and the Vulkan library

    const std::vector<char const*> validationLayers = {"VK_LAYER_KHRONOS_validation"}; //Validation layers can show Vulkan-related errors the compiler would not catch
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr; //This object handles sending the messages from the validation layers

    vk::raii::PhysicalDevice physicalDevice = nullptr; //The actual hardware that will do the computation we code later
    vk::raii::Device logicalDevice = nullptr; //The logical device that will interface with the hardware
    vk::raii::Queue graphicsQueue = nullptr; //This queue will hold the information necessary to send images to the screen
    uint32_t queueIndex = UINT32_MAX; //Holds the index for the queue family for descriptor pools

    vk::raii::SurfaceKHR surface = nullptr; //The surface is an abstraction of a window

    vk::raii::SwapchainKHR swapChain = nullptr; //The swapchain holds several images so as to maximise usage rates and avoid screen tearing
    std::vector<vk::Image> swapChainImages; //The actual images that are in the swapchain
    vk::SurfaceFormatKHR swapChainSurfaceFormat; //The format of the images
    vk::Extent2D swapChainExtent; //The size of the images of the swapchian

    std::vector<vk::raii::ImageView> swapChainImageViews; //Image views describe how to access and image and what part of it

    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout pipelineLayout = nullptr; //Holds uniform values, which can be changed at drawing time without regenerating the entire graphics pipeline
    vk::raii::Pipeline graphicsPipeline = nullptr; //The actual graphics pipeline itself

    vk::raii::CommandPool commandPool = nullptr; //The command pool manages the memory used to store the buffers and it allocates the command buffers
    std::vector<vk::raii::CommandBuffer> commandBuffers; //The command buffers hold the commands we will send to the GPU

    std::vector<vk::raii::Semaphore> presentCompleteSemaphores; //Semaphores for controlling when the image is presented to the screen
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores; //Semaphores for controlling when the image is rendered
    std::vector<vk::raii::Fence> inFlightFences; //Fences to ensure synchronization between the images of the swapchain

    uint32_t frameIndex = 0; //The index of the frame that is being currentry worked on
    bool framebufferResized = false; //Whether the framebuffer was resized for synching reasons
    
    std::vector<Model*> renderingObjects; //The list of objects to be rendered

    std::vector<vk::raii::Buffer> uniformBuffers; //The uniform buffers hold the camera information for rendering
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory; //The memory of the uniform buffers
    std::vector<void*> uniformBuffersMapped; //Mapping of the uniform buffers' memory onto CPU-readable addresses

    vk::raii::DescriptorSetLayout samplerLayout = nullptr; //Descriptor set layout for textures
    std::vector<vk::raii::DescriptorPool> samplerPools; //This vector holds all memory allocated to textures

    vk::raii::DescriptorPool descriptorPool = nullptr; //This descriptor pool holds the uniform buffers in memory for the GPU
    std::vector<vk::raii::DescriptorSet> descriptorSets; //This vector holds the actual uniform buffer data to

    vk::raii::Image depthImage = nullptr; //The depth image. Note that depth values range from 0 to 1
    vk::raii::DeviceMemory depthImageMemory = nullptr; //The memory holding the depth image
    vk::raii::ImageView depthImageView = nullptr; //This object describes how and where to access the depth image

};

#endif