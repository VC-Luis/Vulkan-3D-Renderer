#ifndef MODEL_H
#define MODEL_H

#include "mesh.hpp"
#include "texture.hpp"
#include <glm/glm.hpp>

class Model
{
public:
    Mesh& mesh;

    glm::vec3 position;
    float rotationAngle;
    glm::vec3 rotationVector;
    Texture& texture;

    Model(Mesh& modelMesh, glm::vec3 modelPosition, float modelRotationAngle, glm::vec3 modelRotationVector, Texture& modelTexture);

    glm::mat4 modelMatrix;
};

#endif