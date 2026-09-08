#include "model.hpp"

Model::Model(Mesh& modelMesh, glm::vec3 modelPosition, float modelRotationAngle, glm::vec3 modelRotationVector, Texture& modelTexture) : 
mesh(modelMesh), position(modelPosition), rotationAngle(modelRotationAngle), rotationVector(modelRotationVector), texture(modelTexture)
{}