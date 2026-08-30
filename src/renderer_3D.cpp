#include "../include/Renderer3D.hpp"

#include <string>
#include <map>
#include <fstream>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE //Forces GLM's depth values to range from Vulkan's 0 - 1 instead of OpenGL's -1 - 1 
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

std::vector<const char*> getRequiredInstanceExtensions(bool enableValidationLayers)
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

bool isDeviceSuitable(vk::raii::PhysicalDevice const& physicalDevice)
{
    auto deviceProperties = physicalDevice.getProperties();

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

    auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

    bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
		                            features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
		                            features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
		                            features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

    if(!supportsRequiredFeatures) return false;

    return true;
}

int myScoringFunction(vk::raii::PhysicalDevice GPU)
{
    int score = 0;

    auto deviceProperties = GPU.getProperties();
    auto deviceFeatures = GPU.getFeatures();

    //Since dGPUs often run faster, we'll favour those
    if(deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
    {
        score += 100;
    }

    return score;
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
{
    //Each vk::SurfaceFormatKHR has a format (for color channels and types) and a colorSpace member
    // We will pick by default the sRGB color space when possible and the B8G8R8A8Srgb format (because it's standard)
    assert(!availableFormats.empty());

    const auto formatIt = std::ranges::find_if(availableFormats, [](const auto& format){return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;});

    //If one if the available formats is sRBG, pick that one. Otherwise, just pick the first one
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
    //There are 4 possible ways to store and send images to the screen in Vulkan, but only vk::PresentModeKHR::eFifo (aka pretty much just vsync) is always available, so that's a good enough starting point
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Renderer3D::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities, Window& showWindow)
{
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent; 
        //If the width of the extent is its maximum value, it means (by convention) that we can not match the resolution of the window by setting the width and height in the currentExtent member
    }
    int width, height;
    glfwGetFramebufferSize(showWindow.GLWindow, &width, &height);

    return 
    {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width), 
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
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

vk::Format Renderer3D::findDepthFormat()
{
    return findSupportedFormat( {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint}, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::Format Renderer3D::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
    for(const auto format : candidates)
    {
        vk::FormatProperties properties = physicalDevice.getFormatProperties(format);

        if( (tiling == vk::ImageTiling::eLinear && (properties.linearTilingFeatures & features) ==  features) || (tiling == vk::ImageTiling::eOptimal && (properties.optimalTilingFeatures & features) == features ) )
        {
            return format;
        }
    }
    throw std::runtime_error("Failed to find a supported depth format!");
}

vk::raii::CommandBuffer Renderer3D::beginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(logicalDevice, allocInfo).front());
    
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    
    commandBuffer.begin(beginInfo);

    return std::move(commandBuffer);
}

void Renderer3D::endSingleTimeCommands(vk::raii::CommandBuffer&& commandBuffer)
{
    commandBuffer.end();
    
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*commandBuffer;

    graphicsQueue.submit(submitInfo, nullptr);
    graphicsQueue.waitIdle();    
}

[[nodiscard]] vk::raii::ShaderModule Renderer3D::createShaderModule(const std::vector<char>& code) const
{
    vk::ShaderModuleCreateInfo shaderCreateInfo{};

    shaderCreateInfo.codeSize = code.size() * sizeof(char);
    shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    vk::raii::ShaderModule shaderModule(logicalDevice, shaderCreateInfo);
    return shaderModule;
}

uint32_t Renderer3D::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
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

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Renderer3D::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
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

void Renderer3D::copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size)
{
    vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();

    vk::BufferCopy bufferCopy;
    bufferCopy.size = size;

    commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, bufferCopy);
    endSingleTimeCommands(std::move(commandCopyBuffer));
}

void copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height)
{

    vk::ImageSubresourceLayers imageSubresourceLayers;
    imageSubresourceLayers.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageSubresourceLayers.mipLevel = 0;
    imageSubresourceLayers.baseArrayLayer = 0;
    imageSubresourceLayers.layerCount = 1;

    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = imageSubresourceLayers;
    region.imageOffset = vk::Offset3D(0, 0, 0);
    region.imageExtent = vk::Extent3D(width, height, 1);

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
}

void transitionLoadedImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    vk::ImageMemoryBarrier barrier;

    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;

    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image = image;
    barrier.subresourceRange = subresourceRange;

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if(oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if(oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else
    {
        throw std::invalid_argument("Unsuported layyout transition");
    }
    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
}

vk::raii::ImageView Renderer3D::createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags)
{
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = aspectFlags;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    vk::ImageViewCreateInfo imageViewInfo;
    imageViewInfo.image = image;
    imageViewInfo.viewType = vk::ImageViewType::e2D;
    imageViewInfo.format = format;
    imageViewInfo.subresourceRange = subresourceRange;

    return vk::raii::ImageView(logicalDevice, imageViewInfo);
}

void Renderer3D::createTextureImageView()
{
    textureImageView = createImageView(*textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

void Renderer3D::createTextureSampler()
{
    vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();

    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.compareEnable = vk::False;
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = vk::False;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    textureSampler = vk::raii::Sampler(logicalDevice, samplerInfo);
}

//Firstly, we need to create the Vulkan instance, the connection between this application and the Vulkan library
//To do this, we just need to give it some informations
void Renderer3D::createInstance(std::string engineName, Window& showWindow, bool enableValidationLayers, EngineVersion version)
{
    vk::ApplicationInfo appInfo;
    appInfo.pApplicationName = showWindow.windowName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(version.major, version.minor, version.patch);
    appInfo.pEngineName = engineName.c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(version.major, version.minor, version.patch);
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
    auto requiredExtenisons = getRequiredInstanceExtensions(enableValidationLayers);

    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    auto unsupportedPropertyIt = std::ranges::find_if(requiredExtenisons,
        [&extensionProperties](auto const &requiredExtension) 
        {
            return std::ranges::none_of(extensionProperties, [requiredExtension](auto const &extensionProperty) {return strcmp(extensionProperty.extensionName, requiredExtension) == 0;});
        });
    if(unsupportedPropertyIt != requiredExtenisons.end())
    {
        throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
    }
    else
    {
        std::cout << GREEN << "All required extensions are supported!" << RESET << std::endl;
    }

    vk::InstanceCreateInfo createInfo;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtenisons.size());
    createInfo.ppEnabledExtensionNames = requiredExtenisons.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
    createInfo.ppEnabledLayerNames = requiredLayers.data();

    instance = vk::raii::Instance(context, createInfo);

    int kk;
    kk=0;
}

//Now, we will set up debugging messages (in case they are enabled)
void Renderer3D::setupDebugMessenger(bool enableValidationLayers)
{
    if(!enableValidationLayers) return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo;
    debugMessengerCreateInfo.messageSeverity = severityFlags;
    debugMessengerCreateInfo.messageType = messageTypeFlags;
    debugMessengerCreateInfo.pfnUserCallback = &debugCallback;
}

//Because Vulkan is platform-agnostic, we have to make a connection between the window you see and Vulkan.
//This is called a surface
void Renderer3D::createSurface(Window& window)
{
    VkSurfaceKHR windowSurface;

    if(glfwCreateWindowSurface(*instance, window.GLWindow, nullptr, &windowSurface) != VkResult::VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create window surface!");
    }

    surface = vk::raii::SurfaceKHR(instance, windowSurface);

}

//We now have to choose a GPU to do our computations
void Renderer3D::pickPhysicalDevice(int (*scoringFunction) (vk::raii::PhysicalDevice GPU))
{
    //To pick a physical device to use, we first need to check for all the available ones
    std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
    if(physicalDevices.empty())
    {
        throw std::runtime_error("Failed to find any GPU with Vulkan support");
    }

    //A map associating each device with a score
    std::multimap<int, vk::raii::PhysicalDevice> candidates;
    
    //First check if there is only one physical device to avoid unnecessary calculations
    if(physicalDevices.size() == 1)
    {
        physicalDevice = physicalDevices[0];
        return;
    }

    for(vk::raii::PhysicalDevice device : physicalDevices)
    {
        int score = 0;
        
        if(!isDeviceSuitable(device))
        {
            score = INT32_MIN;
            continue;
        }

        score = scoringFunction(device);

        candidates.insert(std::make_pair(score, device));
    }

    if(!candidates.empty() && candidates.rbegin()->first > 0)
    {
        physicalDevice = candidates.rbegin()->second;
    }
    else
    {
        physicalDevice = physicalDevices[0];
    }
    std::cout << GREEN << "The selected device was " << UNDERLINE << physicalDevice.getProperties().deviceName << RESET << std::endl;
}

//After choosing a GPU for our computations, we need to create a logical device for us to interface with it
void Renderer3D::createLogicalDevice()
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

    if(queueIndex == UINT32_MAX)
    {
        throw std::runtime_error("Could not find a queue for graphics and presentation");
    }

    vk::DeviceQueueCreateInfo deviceQueueCreateInfo;
    deviceQueueCreateInfo.queueFamilyIndex = graphicsIndex;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

    //Then we need to specify the used features of the device
    vk::StructureChain<vk::PhysicalDeviceFeatures2,
    vk::PhysicalDeviceVulkan11Features,
    vk::PhysicalDeviceVulkan13Features,
    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain;

    vk::PhysicalDeviceFeatures2 physicalDeviceFeatures;
    vk::PhysicalDeviceFeatures pdFeatures;
    pdFeatures.samplerAnisotropy = true;
    physicalDeviceFeatures.features = pdFeatures;

    vk::PhysicalDeviceVulkan11Features vk11Features;
    vk11Features.shaderDrawParameters = true;

    vk::PhysicalDeviceVulkan13Features vk13Features;
    vk13Features.dynamicRendering = true;
    vk13Features.synchronization2 = true;

    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedStateFeatures;
    extendedStateFeatures.extendedDynamicState = true;

    featureChain = { physicalDeviceFeatures, vk11Features, vk13Features, extendedStateFeatures};

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

//Vulkan requires an infrastructure that will own the buffers we will render to before we visualize them on the screen.
//This is known as a swap chain
void Renderer3D::createSwapchain(Window& showWindow)
{
    vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    vk::Extent2D extent = chooseSwapExtent(surfaceCapabilities, showWindow);
    uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

    std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(availableFormats);
    std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);

    vk::SwapchainCreateInfoKHR swapChainCreateInfo;
    swapChainCreateInfo.surface = *surface;
    swapChainCreateInfo.minImageCount = minImageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.imageArrayLayers = 1; //The number of layers each image consists of
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
}

//An image view is quite literally a view into an image.
//It describes how to access the image and which part of the image to access
void Renderer3D::createImageViews()
{
    if(!swapChainImageViews.empty())
    {
        swapChainImageViews.clear();
    }

    swapChainImageViews.reserve(swapChainImages.size());

    for(auto& image : swapChainImages)
    {
        swapChainImageViews.emplace_back(createImageView(image, swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor));
    }
}

//We need to provide details about every descriptor binding used in the shaders for pipeline creation
void Renderer3D::createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding UBOLayoutBinding;
    UBOLayoutBinding.binding = 0;
    UBOLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    UBOLayoutBinding.descriptorCount = 1;
    UBOLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    vk::DescriptorSetLayoutBinding samplerLayoutBinding;
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {UBOLayoutBinding, samplerLayoutBinding};

    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    descriptorSetLayout = vk::raii::DescriptorSetLayout(logicalDevice, layoutInfo);

    if(descriptorSetLayout == nullptr)
    {
        throw std::runtime_error("The descriptor set layout could not be made");
    }
}

//template<typename VertexStruct> void Renderer3D::createGraphicsPipeline(const std::string& vertShaderPath, const char* vertStartpoint, const std::string& fragShaderPath, const char* fragStartpoint)

//Command pools manage the memory that is used to store the buffers and command buffers are allocated from them
void Renderer3D::createCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = queueIndex;

    commandPool = vk::raii::CommandPool(logicalDevice, poolInfo);
    
    std::cout << GREEN << "Command pool was successfully created!" << std::endl;
}

void Renderer3D::createVertexBuffer(std::vector<Vertex> vertices)
{
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    
    auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    
    void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(dataStaging, vertices.data(), bufferSize);
    stagingBufferMemory.unmapMemory();
    
    std::tie(vertexBuffer, vertexBufferMemory) = createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
    
    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
}

void Renderer3D::createIndexBuffer(std::vector<uint32_t> indices)
{
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    
    auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    
    void *data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, indices.data(), bufferSize);
    stagingBufferMemory.unmapMemory();
    
    std::tie(indexBuffer, indexBufferMemory) = createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
    
    copyBuffer(stagingBuffer, indexBuffer, bufferSize);
}

void Renderer3D::createUniformBuffers(size_t UBOSize)
{
    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DeviceSize bufferSize = UBOSize;

        auto [buffer, bufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        
        uniformBuffers.emplace_back(std::move(buffer));
        uniformBuffersMemory.emplace_back(std::move(bufferMemory));
        uniformBuffersMapped.emplace_back(uniformBuffersMemory.back().mapMemory(0, bufferSize));
    }
}

void Renderer3D::createCommandBuffers()
{
    //Firstly, we need to tell the constructor function how allocation is going to work
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    
    commandBuffers = vk::raii::CommandBuffers(logicalDevice, allocInfo);
    
    std::cout << GREEN << "Command buffer was successfully created!" << RESET << std::endl;
}

void Renderer3D::createDescriptorPool()
{
    vk::DescriptorPoolSize UBOPoolSize;
    UBOPoolSize.type = vk::DescriptorType::eUniformBuffer;
    UBOPoolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    vk::DescriptorPoolSize samplerPoolSize;
    samplerPoolSize.type = vk::DescriptorType::eCombinedImageSampler;
    samplerPoolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    std::array<vk::DescriptorPoolSize, 2> poolSize = {UBOPoolSize, samplerPoolSize};
    
    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSize.size());
    poolInfo.pPoolSizes = poolSize.data();
    
    descriptorPool = vk::raii::DescriptorPool(logicalDevice, poolInfo);
}

void Renderer3D::createDescriptorSets(size_t UBOSize)
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
        bufferInfo.range = UBOSize;

        vk::DescriptorImageInfo imageInfo;
        imageInfo.sampler = textureSampler;
        imageInfo.imageView = textureImageView;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        vk::WriteDescriptorSet descriptorWriteBuffer;
        descriptorWriteBuffer.dstSet = descriptorSets[i];
        descriptorWriteBuffer.dstBinding = 0;
        descriptorWriteBuffer.dstArrayElement = 0;
        descriptorWriteBuffer.descriptorCount = 1;
        descriptorWriteBuffer.descriptorType = vk::DescriptorType::eUniformBuffer;
        descriptorWriteBuffer.pBufferInfo = &bufferInfo;

        vk::WriteDescriptorSet descriptorWriteImage;
        descriptorWriteImage.dstSet = descriptorSets[i];
        descriptorWriteImage.dstBinding = 1;
        descriptorWriteImage.dstArrayElement = 0;
        descriptorWriteImage.descriptorCount = 1;
        descriptorWriteImage.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWriteImage.pImageInfo = &imageInfo;

        std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {descriptorWriteBuffer, descriptorWriteImage};
        
        logicalDevice.updateDescriptorSets(descriptorWrites, {});
    }
}

void Renderer3D::createSyncObjects()
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

void Renderer3D::recreateSwapchain(Window& showWindow)
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(showWindow.GLWindow, &width, &height);

    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(showWindow.GLWindow, &width, &height);
        glfwWaitEvents();
        std::cout << YELLOW << "Window was either minimised or has no size, so we'll stop until it is visible again" << RESET << std::endl;
    }

    logicalDevice.waitIdle();

    swapChainImageViews.clear();
    swapChain = nullptr;

    createSwapchain(showWindow);
    createImageViews();
}

void Renderer3D::updateUniformBuffer(uint32_t currentImage, Camera cam)
{
    static auto startTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    float time = std::chrono::duration<float>(currentTime - startTime).count();

    UniformBufferObject UBO;
    UBO.modelMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    UBO.viewMatrix = glm::lookAt(cam.position, cam.position + cam.direction, glm::vec3(0.0f, 0.0f, 1.0f));
    UBO.projectionMatrix = glm::perspective(glm::radians(cam.fov), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), cam.nearPlane, cam.farPlane);
    UBO.projectionMatrix[1][1] *= -1.0f;

    memcpy(uniformBuffersMapped[currentImage], &UBO, sizeof(UBO));
}

void Renderer3D::waitForFrame()
{
    vk::Result fenceResult = logicalDevice.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
    if(fenceResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to wait for fence");
    }

    graphicsQueue.waitIdle();
}

void Renderer3D::transitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask, vk::ImageAspectFlags imageAspectFlags)
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
    barrier.image = image;
    
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = imageAspectFlags;
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

void Renderer3D::recordCommandBuffer(uint32_t imageIndex, uint32_t numIndices)
{

    commandBuffers[frameIndex].begin({}); //None of the values we could want are relevant to us, so we just don't set any of them

    // Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
    transitionImageLayout(
        swapChainImages[imageIndex],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor
    );
    
    //We should also transition the depth immage to the depth attachment optimal layout
    transitionImageLayout(
        *depthImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth
    );

    //Now, we can set up the color attachment
    vk::ClearValue clearColor = vk::ClearColorValue(0.22734375f, 0.5046875f, 0.61796875f, 1.0f);

    vk::RenderingAttachmentInfo colorAttachmentInfo;
    colorAttachmentInfo.imageView = swapChainImageViews[imageIndex];
    colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachmentInfo.clearValue = clearColor;

    //Now we can also set up attachment stuff for the depth map as well
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
    
    vk::RenderingAttachmentInfo depthAttachmentInfo;
    depthAttachmentInfo.imageView = depthImageView;
    depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depthAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachmentInfo.storeOp = vk::AttachmentStoreOp::eDontCare;
    depthAttachmentInfo.clearValue = clearDepth;

    //Next, we have to set up the rendering info
    vk::RenderingInfo renderingInfo;
    renderingInfo.renderArea = vk::Rect2D({0, 0}, swapChainExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;


    commandBuffers[frameIndex].beginRendering(renderingInfo);
    commandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
    commandBuffers[frameIndex].bindVertexBuffers(0, *vertexBuffer, {0});
    commandBuffers[frameIndex].bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);
    commandBuffers[frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);

    //Since we set the viewport and the scissor to be dynamic, here's where we set those values
    commandBuffers[frameIndex].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
    commandBuffers[frameIndex].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
    commandBuffers[frameIndex].drawIndexed(numIndices, 1, 0, 0, 0);
    commandBuffers[frameIndex].endRendering();

    //After rendering, we need to transition the image layout back to vk::ImageLayout::ePresentSrcKHR so it can be presented to the screen
    transitionImageLayout(swapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eColorAttachmentWrite, {}, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe, vk::ImageAspectFlagBits::eColor);

    //And now, we have finished recording the command buffer
    commandBuffers[frameIndex].end();
}


void Renderer3D::fetchNewImage(Window& showWindow, Camera cam)
{
    auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);
        
    if(result == vk::Result::eErrorOutOfDateKHR) //This error means that the swap chain has become incompatible with the surface and therefore needs to be remade
    {
        recreateSwapchain(showWindow);
        return;
    }

    if(result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
        throw std::runtime_error("Failed to aquire swap chain image");
    }
    updateUniformBuffer(frameIndex, cam);

    logicalDevice.resetFences(*inFlightFences[frameIndex]);

    recordCommandBuffer(imageIndex, static_cast<uint32_t>(indices.size()));
    
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
        recreateSwapchain(showWindow);
    }
    else
    {
        assert(result == vk::Result::eSuccess);
    }
}

void Renderer3D::engineSetup(std::string engineName, Window& showWindow, bool enableValidationLayers, EngineVersion version)
{
    createInstance(engineName, showWindow, enableValidationLayers, version);
    setupDebugMessenger(enableValidationLayers);
    createSurface(showWindow);
}

void Renderer3D::setupGPU(int (*GPUScoringFunction) (vk::raii::PhysicalDevice GPU))
{
    pickPhysicalDevice(GPUScoringFunction);
    createLogicalDevice();
}

void Renderer3D::generateImageManagement(Window& showWindow)
{
    createSwapchain(showWindow);
    createImageViews();
    createDescriptorSetLayout();
}

void Renderer3D::generateCommandInfrastructure()
{
    createCommandPool();
    createCommandBuffers();
}

void Renderer3D::createTextureImage(std::string textureFile)
{
    //First, we load the image
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(textureFile.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if(!pixels)
    {
        throw std::runtime_error("Failed to load texture image!");
    }

    //After that, we need to send the image over to the GPU with a staging buffer
    auto [stagingBuffer, stagingBufferMemory] = createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    //Now we can create the Vulkan image objects
    std::tie(textureImage, textureImageMemory) = createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
    transitionLoadedImageLayout(commandBuffer, textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(commandBuffer, stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    transitionLoadedImageLayout(commandBuffer, textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    endSingleTimeCommands(std::move(commandBuffer));
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory> Renderer3D::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties)
{
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = format;
    imageInfo.extent = vk::Extent3D(width, height, 1);
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    vk::raii::Image image = vk::raii::Image(logicalDevice, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(logicalDevice, allocInfo);
    image.bindMemory(imageMemory, 0);

    return {std::move(image), std::move(imageMemory)};
}

void Renderer3D::loadModel(Mesh mesh)
{
    vertices = mesh.vertices;
    indices = mesh.indices;
}

void Renderer3D::createDepthResources()
{
    vk::Format depthFormat = findDepthFormat();

    std::tie(depthImage, depthImageMemory) = createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
    depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);

}

void Renderer3D::createGraphicsPipeline(const std::string& vertShaderPath, const char* vertStartpoint, const std::string& fragShaderPath, const char* fragStartpoint)
{
    //Firstly, we create the shader module, which holds the shader's code
    vk::raii::ShaderModule vertShaderModule = createShaderModule(readFile(vertShaderPath));
    vk::raii::ShaderModule fragShaderModule = createShaderModule(readFile(fragShaderPath));

    //Now, we can use the shader modules that holds both vertex and fragment shader functions to create both of these stages at once
    vk::PipelineShaderStageCreateInfo vertShaderStageCreateInto{};

    vertShaderStageCreateInto.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageCreateInto.module = vertShaderModule;
    vertShaderStageCreateInto.pName = vertStartpoint;

    vk::PipelineShaderStageCreateInfo fragShaderStageCreateInto{};

    fragShaderStageCreateInto.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageCreateInto.module = fragShaderModule;
    fragShaderStageCreateInto.pName = fragStartpoint;

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
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise; //Define the vertex order for the faces to be considered front-facing
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    //We can also control how multisampling will work (for now, we'll just disable it for simplicity)
    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.depthTestEnable = vk::True;
    depthStencil.depthWriteEnable = vk::True;
    depthStencil.depthCompareOp = vk::CompareOp::eLess;
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.stencilTestEnable = vk::False;

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
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = nullptr; //This is null because we are using dynamic rendering, which bypasses the need for a render pass

    vk::Format depthFormat = findDepthFormat();

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &swapChainSurfaceFormat.format;
    pipelineRenderingInfo.depthAttachmentFormat = depthFormat;

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {pipelineInfo, pipelineRenderingInfo};

    //And now we can FINALLY create the actual graphics pipeline object
    graphicsPipeline = vk::raii::Pipeline(logicalDevice, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());

    std::cout << GREEN << "Graphics pipeline was successfully created!" << RESET << std::endl;
}

void Renderer3D::createBuffers()
{
    createVertexBuffer(vertices);
    createIndexBuffer(indices);
    createUniformBuffers(sizeof(UniformBufferObject));
}

void Renderer3D::createDescriptors(size_t UBOSize)
{
    createDescriptorPool();
    createDescriptorSets(UBOSize);
}

void Renderer3D::cleanUpSwapchain()
{
    logicalDevice.waitIdle();

    swapChainImageViews.clear();
    swapChain = nullptr;
}

void Renderer3D::loadTexture(std::string texturePath)
{
    createTextureImage(texturePath);
    createTextureImageView();
    createTextureSampler();
}