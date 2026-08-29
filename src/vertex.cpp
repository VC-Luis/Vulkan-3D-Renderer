#include "../include/vertex.hpp"

vk::VertexInputBindingDescription Vertex::getBindingDescription()
{
    vk::VertexInputBindingDescription description;
    description.binding = 0; //The number of bytes between data entries
    description.stride = sizeof(Vertex); //The size of each entry
    description.inputRate = vk::VertexInputRate::eVertex;
    return description;
}

std::array<vk::VertexInputAttributeDescription, 3> Vertex::getAttributeDescription()
{
    vk::VertexInputAttributeDescription posDescription;
    posDescription.location = 0;
    posDescription.binding = 0;
    posDescription.format = vk::Format::eR32G32B32Sfloat;
    posDescription.offset = offsetof(Vertex, pos);

    vk::VertexInputAttributeDescription colorDescription;
    colorDescription.location = 1;
    colorDescription.binding = 0;
    colorDescription.format = vk::Format::eR32G32B32Sfloat;
    colorDescription.offset = offsetof(Vertex, col);

    vk::VertexInputAttributeDescription textureDescription;
    textureDescription.location = 2;
    textureDescription.binding = 0;
    textureDescription.format = vk::Format::eR32G32Sfloat;
    textureDescription.offset = offsetof(Vertex, tex);

    return {posDescription, colorDescription, textureDescription};
}