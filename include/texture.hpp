#ifndef TEXTURE_H
#define TEXTURE_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>

class Texture
{
public:
    void* imageData;
    uint64_t imageSize;

    int textureWidth; 
    int textureHeight; 
    int textureChannels;
    
    Texture(std::string textureFile);

    vk::raii::Image textureImage = nullptr;
    vk::raii::DeviceMemory textureImageMemory = nullptr;
    vk::raii::ImageView textureImageView = nullptr;
    vk::raii::Sampler textureSampler = nullptr;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&&) noexcept = default;
    Texture& operator=(Texture&&) noexcept = default;
};

#endif