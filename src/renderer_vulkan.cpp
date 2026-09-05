#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <map>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <fstream>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define BOLD    "\033[1m"
#define UNDERLINE "\033[4m"
#define BRIGHT_RED "\033[91m"
#define BRIGHT_YELLOW "\33[93m"
#define BRIGHT_WHITE "\033[97m"

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
    constexpr bool enableValidationLayers = false;
#else
    constexpr bool enableValidationLayers = true;
#endif

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        vk::VertexInputBindingDescription description;
        description.binding = 0; //The number of bytes between data entries
        description.stride = sizeof(Vertex); //The size of each entry
        description.inputRate = vk::VertexInputRate::eVertex;
        return description;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescription()
    {
        vk::VertexInputAttributeDescription posDescription;
        posDescription.location = 0;
        posDescription.binding = 0;
        posDescription.format = vk::Format::eR32G32Sfloat;
        posDescription.offset = offsetof(Vertex, pos);
        vk::VertexInputAttributeDescription colorDescription;
        colorDescription.location = 1;
        colorDescription.binding = 0;
        colorDescription.format = vk::Format::eR32G32B32Sfloat;
        colorDescription.offset = offsetof(Vertex, color);

        return {posDescription, colorDescription};
    }
};

//The uniform buffer object will be used to send frequently changing values to shaders 
//(also, using GLM allows us to just do a memcpy because its binary-compatible with the shader types)
struct CameraUBO
{
    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
};

const std::vector<Vertex> vertices = 
{
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = 
{
    0, 1, 2, 2, 3, 0
};

static std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file;
    file.open(filename, std::ios::ate | std::ios::binary);

    if(!file.is_open())
    {
        throw std::runtime_error("Failed to open file");
    }

    std::vector<char> buffer(file.tellg()); //This creates a buffer the size of the file 

    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    
    //Now that we have the data, we can just close the file and retrieve the buffer we created
    file.close();
    return buffer;
}

class RendererAplication
{
private:
    GLFWwindow* window = nullptr;
    const uint32_t windowWidth = 800;
    const uint32_t windowHeight = 600;

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

    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;

    vk::raii::Buffer indexBuffer = nullptr;
    vk::raii::DeviceMemory indexBufferMemory = nullptr;

    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    vk::raii::DescriptorPool descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;



public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanUp();
    }

private:
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
    {
        vk::ShaderModuleCreateInfo shaderCreateInfo{};

        shaderCreateInfo.codeSize = code.size() * sizeof(char);
        shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        vk::raii::ShaderModule shaderModule {logicalDevice, shaderCreateInfo};
        return shaderModule;
    }

    void initWindow()
    {
        //Initialise the GLFW library and set the hints such that no OpenGL context is created
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(windowWidth, windowHeight, "3D Renderer", nullptr, nullptr);

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetWindowCloseCallback(window, windowCloseCallback);
        
        if(window == nullptr || window == NULL)
        {
            throw std::runtime_error("The GLFW window could not be created properly");
        }
        else
        {
            std::cout << GREEN << "GLFW window was successfully created!" << RESET << std::endl;
        }
    }

    static void windowCloseCallback(GLFWwindow* window)
    {
        std::cout << "THE WINDOW WAS CLOSED!!!" << std::endl;
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<RendererAplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        //First, we get the properties of the memory of our GPU
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

        for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        throw std::runtime_error("Failed to find a suitable memory type");
    }

    void cleanUpSwapChain()
    {
        swapChainImageViews.clear();
        swapChain = nullptr;
    }

    void recreateSwapchain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);

        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
            std::cout << YELLOW << "Window was either minimised or has no size, so we'll stop until it is visible again" << RESET << std::endl;
        }

        logicalDevice.waitIdle();

        cleanUpSwapChain();

        createSwapchain();
        createImageViews();
    }
    
    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();

        pickPhysicalDevice();
        createLogicalDevice();

        createSwapchain();
        createImageViews();

        createDescriptorSetLayout();
        
        createGraphicsPipeline();
        
        createCommandPool();
        
        createCommandBuffers();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        
        createDescriptorPool();
        createDescriptorSets();
        
        createSyncObjects();
    }

    void createDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);

        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets = logicalDevice.allocateDescriptorSets(allocInfo);

        //Now that we have created the descriptor sets, we need to actually populate them
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorBufferInfo bufferInfo;
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(CameraUBO);

            vk::WriteDescriptorSet descriptorWrite;
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptorWrite.pBufferInfo = &bufferInfo;

            logicalDevice.updateDescriptorSets(descriptorWrite, {});
        }
    }

    void createDescriptorPool()
    {
        vk::DescriptorPoolSize poolSize;
        poolSize.type = vk::DescriptorType::eUniformBuffer;
        poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        descriptorPool = vk::raii::DescriptorPool(logicalDevice, poolInfo);
    }

    void createUniformBuffers()
    {
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DeviceSize bufferSize = sizeof(CameraUBO);

            auto [buffer, bufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            uniformBuffers.emplace_back(std::move(buffer));
            uniformBuffersMemory.emplace_back(std::move(bufferMemory));
            uniformBuffersMapped.emplace_back(uniformBuffersMemory.back().mapMemory(0, bufferSize));
        }
    }

    void createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding UBOLayoutBinding;
        UBOLayoutBinding.binding = 0;
        UBOLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
        UBOLayoutBinding.descriptorCount = 1;
        UBOLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutCreateInfo layoutInfo;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &UBOLayoutBinding;

        descriptorSetLayout = vk::raii::DescriptorSetLayout(logicalDevice, layoutInfo);

        if(descriptorSetLayout == nullptr)
        {
            throw std::runtime_error("The descriptor set layout could not be made");
        }
    }

    void createIndexBuffer()
    {
        vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void *data = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(data, indices.data(), bufferSize);
		stagingBufferMemory.unmapMemory();

        std::tie(indexBuffer, indexBufferMemory) = createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);
    }

    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
    {
        //First, we generate the actual vertex buffer
        vk::BufferCreateInfo bufferInfo;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;

        vk::raii::Buffer buffer = vk::raii::Buffer(logicalDevice, bufferInfo);

        //Now we need to get the memory requirements for the buffer so that we can check if we have the right type of memory
        vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

        vk::MemoryAllocateInfo memAllocateInfo;
        memAllocateInfo.allocationSize = memRequirements.size;
        memAllocateInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        //After that, we can actually give the memory to the buffer
        vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(logicalDevice, memAllocateInfo);
        buffer.bindMemory(*bufferMemory, 0);

        return {std::move(buffer), std::move(bufferMemory)};
    }

    void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size)
    {
        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = commandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;

        vk::raii::CommandBuffer commandCopyBuffer = std::move(logicalDevice.allocateCommandBuffers(allocInfo).front());

        vk::CommandBufferBeginInfo beginInfo;
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        commandCopyBuffer.begin(beginInfo);

        commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));

        commandCopyBuffer.end();


        vk::SubmitInfo submitInfo;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &*commandCopyBuffer;
        graphicsQueue.submit(submitInfo, nullptr);
        graphicsQueue.waitIdle();
    }

    void createVertexBuffer()
    {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, vertices.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        std::tie(vertexBuffer, vertexBufferMemory) = createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
        
        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
    }

    void createSyncObjects()
    {
        assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());
        
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
        
        for(size_t i = 0; i < swapChainImages.size(); i++)
        {
            renderFinishedSemaphores.emplace_back(logicalDevice, vk::SemaphoreCreateInfo());
        }
        
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            presentCompleteSemaphores.emplace_back(logicalDevice, vk::SemaphoreCreateInfo());
            inFlightFences.emplace_back(logicalDevice, fenceInfo);
        }

        std::cout << GREEN << "All necesary semaphores and fences were properly set up!" << RESET << std::endl;
    }

    void transitionImageLayout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask)
    {
        //Before we can start rendering to an image, we need to transition its layout to one that is suitable for rendering
        vk::ImageMemoryBarrier2 barrier;
        barrier.srcStageMask = srcStageMask;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstStageMask = dstStageMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = swapChainImages[imageIndex];
        
        vk::ImageSubresourceRange subresourceRange;
        subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;

        barrier.subresourceRange = subresourceRange;

        vk::DependencyInfo dependencyInfo;
        dependencyInfo.dependencyFlags = {};
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;

        commandBuffers[frameIndex].pipelineBarrier2(dependencyInfo);
    }

    //Writes the functions we want to execute into an actual command buffer
    void recordCommandBuffer(uint32_t imageIndex)
    {
        auto &commandBuffer = commandBuffers[frameIndex];

        commandBuffer.begin({}); //None of the values we could want are relevant to us, so we just don't set any of them

        // Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
        transitionImageLayout(imageIndex, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, {}, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        
        //Now, we can set up the color attachment
        vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 1.0f, 0.0f, 1.0f);
        
        vk::RenderingAttachmentInfo attachmentInfo;
        attachmentInfo.imageView = swapChainImageViews[imageIndex];
        attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
        attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
        attachmentInfo.clearValue = clearColor;

        //Next, we have to set up the rendering info
        vk::RenderingInfo renderingInfo;
        renderingInfo.renderArea = vk::Rect2D({0, 0}, swapChainExtent);
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &attachmentInfo;


        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
        commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
        commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);

        //Since we set the viewport and the scissor to be dynamic, here's where we set those values
        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
        commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        commandBuffer.endRendering();

        //After rendering, we need to transition the image layout back to vk::ImageLayout::ePresentSrcKHR so it can be presented to the screen
        transitionImageLayout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eColorAttachmentWrite, {}, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);

        //And now, we have finished recording the command buffer
        commandBuffer.end();
    }

    void createCommandBuffers()
    {
        //Firstly, we need to tell the constructor function how allocation is going to work
        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = commandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

        commandBuffers = vk::raii::CommandBuffers(logicalDevice, allocInfo);
    
        std::cout << GREEN << "Command buffer was successfully created!" << RESET << std::endl;
    }

    void createCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo;
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        poolInfo.queueFamilyIndex = queueIndex;

        commandPool = vk::raii::CommandPool(logicalDevice, poolInfo);

        std::cout << GREEN << "Command pool was successfully created!" << RESET << std::endl;
    }

    void createGraphicsPipeline()
    {
        //Firstly, we create the shader module, which holds the shader's code
        vk::raii::ShaderModule shaderModule = createShaderModule(readFile("/home/luis/Documents/Projects/Vulkan 3D Renderer/build/shader.spv"));

        //Now, we can use the shader modules that holds both vertex and fragment shader functions to create both of these stages at once
        vk::PipelineShaderStageCreateInfo vertShaderStageCreateInto{};

        vertShaderStageCreateInto.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageCreateInto.module = shaderModule;
        vertShaderStageCreateInto.pName = "vertMain";

        vk::PipelineShaderStageCreateInfo fragShaderStageCreateInto{};

        fragShaderStageCreateInto.stage = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageCreateInto.module = shaderModule;
        fragShaderStageCreateInto.pName = "fragMain";

        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageCreateInto, fragShaderStageCreateInto};

        //Now we define the states we want to be dynamic so that we can change them without recreating the entire pipeline
        std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        //This struct defines the format of the vertex data to be passed onto the vertex shader
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescription();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        //We also need to specify how this input is to be interpreted to generate triangles
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
        
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList; //This essentailly makes it so that a triangle is made from every 3 vertices we send

        //We will also define the viewport, the region of the framebuffer we will render
        //vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f};
        vk::Viewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor; //This scissor doesn't do anything because it just covers the entire screen but I'm just testing things
        scissor.offset = vk::Offset2D{0, 0};
        scissor.extent = swapChainExtent;

        vk::PipelineViewportStateCreateInfo viewportState;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        //The next step is to define some things about the rasterizer, which converts our triangles into actual pixels (actially they're called fragments but oh well)
        vk::PipelineRasterizationStateCreateInfo rasterizer;
        rasterizer.depthClampEnable = vk::False; //If set to true, it discaards fragments that are beyond the near and far planes. Otherwise, it just clamps them
        rasterizer.rasterizerDiscardEnable = vk::False;
        rasterizer.polygonMode = vk::PolygonMode::eFill;
        rasterizer.cullMode = vk::CullModeFlagBits::eBack; //Wheat fases to cull out
        rasterizer.frontFace = vk::FrontFace::eClockwise; //Define the vertex order for the faces to be considered front-facing
        rasterizer.depthBiasEnable = vk::False;
        rasterizer.lineWidth = 1.0f;

        //We can also control how multisampling will work (for now, we'll just disable it for simplicity)
        vk::PipelineMultisampleStateCreateInfo multisampling;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
        multisampling.sampleShadingEnable = vk::False;

        //After this, we will manage color blending, which determines how the color returned from a framebuffer is combined with the one already there
        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.blendEnable = vk::False; //If this is false, the new color from the fragment shader is passed through unmodified
        colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        
        vk::PipelineColorBlendStateCreateInfo colorBlendingInfo;
        colorBlendingInfo.logicOpEnable = vk::False;
        colorBlendingInfo.logicOp = vk::LogicOp::eCopy;
        colorBlendingInfo.attachmentCount = 1;
        colorBlendingInfo.pAttachments = &colorBlendAttachment;

        //Finally, we define the pipeline layout
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &*descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        pipelineLayout = vk::raii::PipelineLayout(logicalDevice, pipelineLayoutInfo);

        //Now that all of the pipeline stages are created, we need to put them all together in another struct
        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlendingInfo;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = nullptr; //This is null because we are using dynamic rendering, which bypasses the need for a render pass

        vk::PipelineRenderingCreateInfo pipelineRenderingInfo;
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &swapChainSurfaceFormat.format;

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {pipelineInfo, pipelineRenderingInfo};

        //And now we can FINALLY create the actual graphics pipeline object
        graphicsPipeline = vk::raii::Pipeline(logicalDevice, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());

        std::cout << GREEN << "Graphics pipeline was successfully created!" << RESET << std::endl;
    }

    void createImageViews()
    {
        if(!swapChainImageViews.empty())
        {
            swapChainImageViews.clear();
        }

        vk::ImageViewCreateInfo imageViewCreateInfo {};
        imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
        imageViewCreateInfo.format = swapChainSurfaceFormat.format;
        imageViewCreateInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

        for(auto& image : swapChainImages)
        {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(logicalDevice, imageViewCreateInfo);
        }

        //std::cout << GREEN << "The image views were successfully set up!" << RESET << std::endl;
    }

    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
    {
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        if ((surfaceCapabilities.maxImageCount) > 0 && (surfaceCapabilities.maxImageCount < minImageCount))
        {
            minImageCount = surfaceCapabilities.maxImageCount;
        }
        return minImageCount;
    }

    void createSwapchain()
    {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        vk::Extent2D extent = chooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

        std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(availableFormats);
        auto availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);

        vk::SwapchainCreateInfoKHR swapChainCreateInfo {};

        swapChainCreateInfo.surface = *surface;
        swapChainCreateInfo.minImageCount = minImageCount;
        swapChainCreateInfo.imageFormat = surfaceFormat.format;
        swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapChainCreateInfo.imageExtent = extent;
        swapChainCreateInfo.imageArrayLayers = 1;
        swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
        swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
        swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
        swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapChainCreateInfo.presentMode = chooseSwapPresentMode(availablePresentModes);
        swapChainCreateInfo.clipped = true;
        swapChainCreateInfo.oldSwapchain = nullptr;

        swapChain = vk::raii::SwapchainKHR(logicalDevice, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
        swapChainSurfaceFormat = surfaceFormat;
        swapChainExtent = extent;

        //std::cout << GREEN << "Swap chain was successfully set up!" << RESET << std::endl;
    }
    

    void createSurface()
    {
        VkSurfaceKHR windowSurface;

        if(glfwCreateWindowSurface(*instance, window, nullptr, &windowSurface) != 0)
        {
            throw std::runtime_error("Failed to create window surface!");
        }

        surface = vk::raii::SurfaceKHR(instance, windowSurface);
    }

    void createLogicalDevice()
    {
        //Firstly, we specify the queues that need to be created
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) {return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);});
        auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
        float queuePriority = 0.5f;

        //It would also be pretty nice if the device could present images to our window
        for(uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
            {
                queueIndex = qfpIndex;
                break;
            }
        }
        if(queueIndex == ~0)
        {
            throw std::runtime_error("Could not find a queue for graphics and presentation");
        }

        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{};

        deviceQueueCreateInfo.queueFamilyIndex = graphicsIndex;
        deviceQueueCreateInfo.queueCount = 1;
        deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

        //Then we need to specify the used features of the device
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain;

        vk::PhysicalDeviceVulkan11Features vk11Features;
        vk11Features.shaderDrawParameters = true;
        vk::PhysicalDeviceVulkan13Features vk13Features;
        vk13Features.dynamicRendering = true;
        vk13Features.synchronization2 = true;
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedStateFeatures;
        extendedStateFeatures.extendedDynamicState = true;

        featureChain = { {}, vk11Features, vk13Features, extendedStateFeatures};

        //Set up and prepare swapchain support
        std::vector<const char*> requiredDeviceExtensions = {vk::KHRSwapchainExtensionName, vk::KHRSynchronization2ExtensionName};
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
        std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);

        //Finally, with all the information we need all gathered, we can actually create the logical device
        vk::DeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(); //Because we are using a feature chain, Vulkan automatically detects all the other features in the chain
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();


        logicalDevice = vk::raii::Device(physicalDevice, deviceCreateInfo);
        graphicsQueue = vk::raii::Queue(logicalDevice, graphicsIndex, 0);

        std::cout << GREEN << "Logical device was successfully set up!" << RESET << std::endl;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
    {
        //Each vk::SurfaceFormatKHR has a format (for color channels and types) and a colorSpace member
        // We will pick by default the sRGB color space when possible and the B8G8R8A8Srgb format (because it's standard)
        assert(!availableFormats.empty());

        const auto formatIt = std::ranges::find_if(availableFormats, [](const auto& format){return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;});

        if(formatIt != availableFormats.end())
        {
            return *formatIt;
        }
        else
        {
            return availableFormats[0];
        }

    }

    vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes)
    {
        //There are 4 possible ways to store and send images to the screen in Vulkan, but only vk::PresentModeKHR::eFifo (aka pretty much just vsync), so that's a good enough starting point
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities)
    {
        if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent; 
            //If the width of the extent is its maximum value, it means (by convention) that we can not match the resolution of the window by setting the width and height in the currentExtent member
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        return 
        {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width), 
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

    void pickPhysicalDevice()
    {
        //To pick a physical device to use, we first need to check for all the available ones
        std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
        if(physicalDevices.empty())
        {
            throw std::runtime_error("Failed to find any GPU with Vulkan support");
        }

        //A map associating each device with a score
        std::multimap<int, vk::raii::PhysicalDevice> candidates;
        
        for(vk::raii::PhysicalDevice device : physicalDevices)
        {
            int score = 0;
            
            if(!isDeviceSuitable(device))
            {
                score = INT32_MIN;
                continue;
            }

            auto deviceProperties = device.getProperties();
            auto deviceFeatures = device.getFeatures();

            //Since dGPUs often run faster, we'll favour those
            if(deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            {
                score += 100;
            }

            candidates.insert(std::make_pair(score, device));
        }

        if(!candidates.empty() && candidates.rbegin()->first > 0)
        {
            physicalDevice = candidates.rbegin()->second;
            std::cout << GREEN << "The selected device was " << UNDERLINE << physicalDevice.getProperties().deviceName << RESET << std::endl;
        }
    }

    //This function determines wether a device is capable of doing the task at hand
    bool isDeviceSuitable(vk::raii::PhysicalDevice const& physicalDevice)
    {
        auto deviceProperties = physicalDevice.getProperties();
        auto deviceFeatures = physicalDevice.getFeatures();

        //First off, we need for the GPU to support at least the 1.3 API version
        if(deviceProperties.apiVersion < vk::ApiVersion13) return false;

        //We alse need the GPU to support a queue family that supports graphics operations
        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        if(!std::ranges::any_of(queueFamilies, [](auto const &qfp) {return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);})) return false;

        //We will need out chosen GPU to support the required extensions
        std::vector<const char*> requiredDeviceExtensions = {vk::KHRSwapchainExtensionName, vk::KHRSynchronization2ExtensionName}; //This is the only extension we need as of right now, but that may change later on
        
        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        if(!std::ranges::all_of(requiredDeviceExtensions, 
        [&availableDeviceExtensions](auto const& requiredDeviceExtension)
        {
            return std::ranges::any_of( availableDeviceExtensions,
                [requiredDeviceExtension]( auto const & availableDeviceExtension )
                { return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0; } ); 
        }))
        {
            return false;
        }

        return true;
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        if(severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
        {
            std::cout << BRIGHT_WHITE << "Validation layers: type " << to_string(type) << " Message: " << pCallbackData->pMessage << RESET << std::endl;
        }
        else if(severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        {
            std::cerr << YELLOW << "Validation layers: type" << to_string(type) << " Message: " << pCallbackData->pMessage << RESET << std::endl;
        }
        else if(severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        {
            std::cerr << RED << "Validation layers: type" << to_string(type) << " Message: " << pCallbackData->pMessage << RESET << std::endl;
        }
        

        return VK_FALSE;
    }

    void setupDebugMessenger()
    {
        if(!enableValidationLayers) return; // Only do something if validation layers are enabled
        
        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
        debugMessengerCreateInfo.messageSeverity = severityFlags;
        debugMessengerCreateInfo.messageType = messageTypeFlags;
        debugMessengerCreateInfo.pfnUserCallback = &debugCallback;

        debugMessenger = instance.createDebugUtilsMessengerEXT(debugMessengerCreateInfo);
    }

    void createInstance()
    {
        //NOTE: Information may be often passed via structs instead of function arguments
        
        //This is just some basic information about out application
        vk::ApplicationInfo appInfo{};
        
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName = "No Engine",
        appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0),
        appInfo.apiVersion = vk::ApiVersion14; // The API version being 1.4 will help us with using Slang for shaders

        //Now we need to check if the requested validation layers (used for debugging) are available
        std::vector<char const*> requiredLayers;
        
        if(enableValidationLayers)
        {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        //Check if the required layers are supported by the current Vulkan implementation
        auto layerProperties = context.enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, 
            [&layerProperties](auto const &requiredLayer)
            {
                return std::ranges::none_of(layerProperties, [requiredLayer](auto const &layerProperty){return strcmp(layerProperty.layerName, requiredLayer) == 0; });
            });
        
        if(unsupportedLayerIt != requiredLayers.end())
        {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
        }
        else
        {
            std::cout << GREEN << "All required validation layers are supported!" << RESET << std::endl;
        }

        //Check if all the required extensions are available
        auto requiredExtenisons = getRequiredInstanceExtensions();

        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        auto unsupportedPropertyIt = std::ranges::find_if(requiredExtenisons,
            [&extensionProperties](auto const &requiredExtension) 
            {
                return std::ranges::none_of(extensionProperties, [requiredExtension](auto const &extensionProperty) {return strcmp(extensionProperty.extensionName, requiredExtension) == 0;});
            });
        if(unsupportedPropertyIt != requiredExtenisons.end())
        {
            throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedLayerIt));
        }
        else
        {
            std::cout << GREEN << "All required extensions are supported!" << RESET << std::endl;
        }

        vk::InstanceCreateInfo createInfo{};

        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
        createInfo.ppEnabledLayerNames = requiredLayers.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtenisons.size());
        createInfo.ppEnabledExtensionNames = requiredExtenisons.data();

        //Now that we've specified all the necessary information, we can actially create the instance
        instance = vk::raii::Instance(context, createInfo);
    }

    std::vector<const char*> getRequiredInstanceExtensions()
    {
        //Get the required GLFW extensions so that Vulkan can actually interact with the window
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if(enableValidationLayers)
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }
    
    void mainLoop()
    {
        while(!glfwWindowShouldClose(window))
        {
            // Do cool stuff here please and thank you :)
            glfwPollEvents();
            drawFrame();
        }

        logicalDevice.waitIdle(); //This function just waits until everything that needs to be done actually occurs
    }

    void updateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        CameraUBO UBO;
        UBO.modelMatrix = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        UBO.viewMatrix = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        UBO.projectionMatrix = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 10.0f);

        memcpy(uniformBuffersMapped[currentImage], &UBO, sizeof(UBO));
    }

    void drawFrame()
    {
        //To synchronise queue operations we will use semaphores and fences to sync up execution on the CPU (AKA knowing when the GPU finished something)
        

        //First, we have to wait for the previous frame to finish rendering
        vk::Result fenceResult = logicalDevice.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
        if(fenceResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to wait for fence");
        }

        graphicsQueue.waitIdle();

        //After that, we fetch the next image from the swap chain
        auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);
        
        if(result == vk::Result::eErrorOutOfDateKHR) //This error means that the swap chain has become incompatible with the surface and therefore needs to be remade
        {
            recreateSwapchain();
            return;
        }

        if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        {
            assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
            throw std::runtime_error("Failed to aquire swap chain image");
        }
        updateUniformBuffer(frameIndex);

        logicalDevice.resetFences(*inFlightFences[frameIndex]);

        //Then, we record a command buffer which draws the scene onto that image
        recordCommandBuffer(imageIndex);


        //Now we have to submit the recorded command buffer
        vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        vk::SubmitInfo submitInfo;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &*presentCompleteSemaphores[frameIndex];
        submitInfo.pWaitDstStageMask = &waitDestinationStageMask; //These 3 first parameters specify which semaphores to wait on before execution begins and in which stage(s) of the pipeline to wait
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &*commandBuffers[frameIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &*renderFinishedSemaphores[frameIndex];

        graphicsQueue.submit(submitInfo, *inFlightFences[frameIndex]);
        
        //And finally, we present the swap chain image to the screen
        vk::PresentInfoKHR presentInfo;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &*renderFinishedSemaphores[frameIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &*swapChain;
        presentInfo.pImageIndices = &imageIndex;

        result = graphicsQueue.presentKHR(presentInfo);

        if(result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR || framebufferResized)
        {
            framebufferResized = false;
            recreateSwapchain();
        }
        else
        {
            assert(result == vk::Result::eSuccess);
        }
    }
    
    void cleanUp()
    {
        std::cout << "Cleaning up..." << std::endl;

        cleanUpSwapChain();

        glfwDestroyWindow(window);
        glfwTerminate();
    }
};