#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>

#ifndef VERTEX_H
#define VERTEX_H

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 col;
    glm::vec2 tex;

    static vk::VertexInputBindingDescription getBindingDescription();

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescription();
};

#endif