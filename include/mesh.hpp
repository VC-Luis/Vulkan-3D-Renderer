#ifndef MESH_H
#define MESH_H

#include <vertex.hpp>

class Mesh
{
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    Mesh(std::vector<Vertex> meshVertices, std::vector<uint32_t> meshIndices);
    Mesh(std::string meshFile);

    void getData(std::string meshFile);
    void drawMesh();
};

#endif