#include "mesh.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <iostream>

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

Mesh::Mesh(std::vector<Vertex> meshVertices, std::vector<uint32_t> meshIndices)
{
    vertices = meshVertices;
    indices = meshIndices;
}

Mesh::Mesh(std::string meshFile)
{
    getData(meshFile);
}

void Mesh::getData(std::string meshFile)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, meshFile.c_str()))
    {
        throw std::runtime_error(warn + err);
    }

    for(const auto& shape : shapes)
    {
        for(const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};


            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if(2 * index.texcoord_index + 0 <= 0 || 2 * index.texcoord_index + 1 <= 0)
            {
                std::cout << YELLOW << "TEXTURE COORD WITH FUNKY INDEX ALERT" << RESET << std::endl;
                vertex.tex = {0.0f, 0.0f};

            }
            else
            {
                vertex.tex = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }


            vertex.col = {1.0f, 1.0f, 1.0f};

            vertices.push_back(vertex);
            indices.push_back(indices.size());
        }
    }
}