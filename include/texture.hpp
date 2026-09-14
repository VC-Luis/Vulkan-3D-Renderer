#ifndef TEXTURE_H
#define TEXTURE_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>

class Texture
{
private:
    void* imageData;
    uint64_t imageSize;

    int textureWidth; 
    int textureHeight; 
    int textureChannels;
    


protected:
    vk::raii::Image textureImage = nullptr;
    vk::raii::DeviceMemory textureImageMemory = nullptr;
    vk::raii::ImageView textureImageView = nullptr;
    vk::raii::Sampler textureSampler = nullptr;

    std::vector<vk::raii::DescriptorSet> textureDescriptorSets;

public:    
    Texture(std::string textureFile);
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&&) noexcept = default;
    Texture& operator=(Texture&&) noexcept = default;

friend class Renderer3D;
};

#endif