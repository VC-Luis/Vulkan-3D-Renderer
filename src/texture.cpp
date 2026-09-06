#include "texture.hpp"
#include <stb_image.h>


Texture::Texture(std::string textureFile)
{
    stbi_uc* pixels = stbi_load(textureFile.c_str(), &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);

    imageData = pixels;
    imageSize = textureWidth * textureHeight * 4;

    if(!pixels) throw std::runtime_error("Failed to load texture properly");
}