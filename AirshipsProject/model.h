#pragma once
#include <SFML/Graphics.hpp>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct SubMesh {
    unsigned int indexOffset;
    unsigned int indexCount;
    GLuint texture = 0;
};

struct Model {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> bitangents;
    std::vector<unsigned int> indices;

    std::vector<SubMesh> subMeshes;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;

    int indexCount = 0;
    std::string name;

    float minY = 0.0f;
    float maxY = 0.0f;
};

bool LoadOBJModel(const std::string& filename, Model& model);
GLuint LoadTextureFromFile(const std::string& filename);
bool InitializeModelGL(Model& model, const std::string& textureFile = "");
void DestroyModelGL(Model& model);
void DrawModel(const Model& model);
void ComputeTangents(Model& model);
