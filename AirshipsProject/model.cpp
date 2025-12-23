#include "model.h"
#include <fstream>
#include <cmath>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

void ComputeTangents(Model& model)
{
    model.tangents.assign(model.vertices.size(), glm::vec3(0.0f));
    model.bitangents.assign(model.vertices.size(), glm::vec3(0.0f));

    if (model.indices.size() < 3 || model.vertices.empty())
        return;

    for (size_t i = 0; i + 2 < model.indices.size(); i += 3)
    {
        unsigned int i0 = model.indices[i + 0];
        unsigned int i1 = model.indices[i + 1];
        unsigned int i2 = model.indices[i + 2];

        if (i0 >= model.vertices.size() || i1 >= model.vertices.size() || i2 >= model.vertices.size())
            continue;

        const glm::vec3& p0 = model.vertices[i0];
        const glm::vec3& p1 = model.vertices[i1];
        const glm::vec3& p2 = model.vertices[i2];

        const glm::vec2& uv0 = (i0 < model.texCoords.size()) ? model.texCoords[i0] : glm::vec2(0.0f);
        const glm::vec2& uv1 = (i1 < model.texCoords.size()) ? model.texCoords[i1] : glm::vec2(0.0f);
        const glm::vec2& uv2 = (i2 < model.texCoords.size()) ? model.texCoords[i2] : glm::vec2(0.0f);

        glm::vec3 e1 = p1 - p0;
        glm::vec3 e2 = p2 - p0;
        glm::vec2 dUV1 = uv1 - uv0;
        glm::vec2 dUV2 = uv2 - uv0;

        float det = dUV1.x * dUV2.y - dUV2.x * dUV1.y;
        if (std::abs(det) < 1e-8f)
            continue;

        float f = 1.0f / det;
        glm::vec3 tangent = f * (dUV2.y * e1 - dUV1.y * e2);
        glm::vec3 bitangent = f * (-dUV2.x * e1 + dUV1.x * e2);

        model.tangents[i0] += tangent;
        model.tangents[i1] += tangent;
        model.tangents[i2] += tangent;

        model.bitangents[i0] += bitangent;
        model.bitangents[i1] += bitangent;
        model.bitangents[i2] += bitangent;
    }

    for (size_t v = 0; v < model.vertices.size(); ++v)
    {
        glm::vec3 n = (v < model.normals.size()) ? model.normals[v] : glm::vec3(0, 0, 1);
        glm::vec3 t = model.tangents[v];

        if (glm::length(t) < 1e-6f) {
            model.tangents[v] = glm::vec3(1, 0, 0);
            model.bitangents[v] = glm::vec3(0, 1, 0);
            continue;
        }

        t = glm::normalize(t - n * glm::dot(n, t));
        glm::vec3 b = model.bitangents[v];
        if (glm::length(b) < 1e-6f) {
            b = glm::normalize(glm::cross(n, t));
        }
        else {
            b = glm::normalize(b);
        }

        model.tangents[v] = t;
        model.bitangents[v] = b;
    }
}

static void ComputeBoundsY(Model& model)
{
    if (model.vertices.empty()) {
        model.minY = model.maxY = 0.0f;
        return;
    }

    float minY = model.vertices[0].y;
    float maxY = model.vertices[0].y;

    for (const auto& v : model.vertices) {
        minY = std::min(minY, v.y);
        maxY = std::max(maxY, v.y);
    }

    model.minY = minY;
    model.maxY = maxY;
}

static std::string GetDirectoryFromPath(const std::string& path)
{
    size_t slashPos = path.find_last_of("/\\");
    if (slashPos == std::string::npos) return ".";
    return path.substr(0, slashPos);
}

static std::string ExtractFileName(const std::string& path)
{
    size_t slash1 = path.find_last_of("/\\");
    if (slash1 != std::string::npos)
        return path.substr(slash1 + 1);
    return path;
}

static GLuint LoadMaterialTexture(aiMaterial* material, const std::string& directory, const std::string& objBaseName)
{
    aiString texPathAI;

    if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0 &&
        material->GetTexture(aiTextureType_DIFFUSE, 0, &texPathAI) == AI_SUCCESS)
    {
        std::string rawPath = texPathAI.C_Str();
        std::string fileName = ExtractFileName(rawPath);
        std::string fullPath = directory + "/" + fileName;

        GLuint tex = LoadTextureFromFile(fullPath);
        if (tex != 0)
            return tex;

    }

    static const char* exts[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };

    for (const char* ext : exts)
    {
        std::string fallback = directory + "/" + objBaseName + ext;
        GLuint tex = LoadTextureFromFile(fallback);
        if (tex != 0) {
            std::cout << "Loaded fallback texture: " << fallback << "\n";
            return tex;
        }
    }

    return 0;
}


bool LoadOBJModel(const std::string& filename, Model& model)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filename,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs
    );

    if (!scene || !scene->mRootNode) {
        std::cerr << "ASSIMP: Failed to load " << filename << "\n"
            << importer.GetErrorString() << std::endl;
        return false;
    }

    model.vertices.clear();
    model.texCoords.clear();
    model.normals.clear();
    model.tangents.clear();
    model.bitangents.clear();
    model.indices.clear();
    model.subMeshes.clear();

    std::string directory = GetDirectoryFromPath(filename);
    std::string fileOnly = filename.substr(filename.find_last_of("/\\") + 1);
    std::string baseName = fileOnly.substr(0, fileOnly.find_last_of('.'));

    size_t vertexOffset = 0;

    for (unsigned int m = 0; m < scene->mNumMeshes; m++)
    {
        aiMesh* mesh = scene->mMeshes[m];

        SubMesh sub;
        sub.indexOffset = model.indices.size();

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            model.vertices.emplace_back(mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z);

            if (mesh->HasTextureCoords(0)) {
                aiVector3D uv = mesh->mTextureCoords[0][i];
                model.texCoords.emplace_back(uv.x, uv.y);
            }
            else {
                model.texCoords.emplace_back(0.f, 0.f);
            }

            if (mesh->HasNormals()) {
                aiVector3D n = mesh->mNormals[i];
                model.normals.emplace_back(n.x, n.y, n.z);
            }
            else {
                model.normals.emplace_back(0.f, 0.f, 1.f);
            }
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
            aiFace face = mesh->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                model.indices.push_back(face.mIndices[j] + vertexOffset);
        }

        vertexOffset += mesh->mNumVertices;

        sub.indexCount = model.indices.size() - sub.indexOffset;

        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        sub.texture = LoadMaterialTexture(material, directory, baseName);

        model.subMeshes.push_back(sub);
    }

    model.indexCount = model.indices.size();
    ComputeTangents(model);
    ComputeBoundsY(model);

    std::cout << "Loaded OBJ with " << model.subMeshes.size()
        << " materials, " << model.vertices.size()
        << " vertices, " << model.indices.size() << " indices.\n";

    return true;
}

bool FileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

GLuint LoadTextureFromFile(const std::string& filename)
{
    sf::Image img;
    if (!FileExists(filename) || !img.loadFromFile(filename)) {
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        img.getSize().x,
        img.getSize().y,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        img.getPixelsPtr()
    );

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    std::cout << "Loaded texture: " << filename << std::endl;

    return tex;
}

bool InitializeModelGL(Model& model, const std::string& texFile)
{
    glGenVertexArrays(1, &model.vao);
    glGenBuffers(1, &model.vbo);
    glGenBuffers(1, &model.ebo);

    glBindVertexArray(model.vao);

    std::vector<float> vert;
    vert.reserve(model.vertices.size() * 14);

    for (size_t i = 0; i < model.vertices.size(); i++)
    {
        vert.push_back(model.vertices[i].x);
        vert.push_back(model.vertices[i].y);
        vert.push_back(model.vertices[i].z);

        vert.push_back(model.texCoords[i].x);
        vert.push_back(model.texCoords[i].y);

        glm::vec3 n = (i < model.normals.size()) ? model.normals[i] : glm::vec3(0, 0, 1);
        glm::vec3 t = (i < model.tangents.size()) ? model.tangents[i] : glm::vec3(1, 0, 0);
        glm::vec3 b = (i < model.bitangents.size()) ? model.bitangents[i] : glm::vec3(0, 1, 0);

        vert.push_back(n.x);
        vert.push_back(n.y);
        vert.push_back(n.z);

        vert.push_back(t.x);
        vert.push_back(t.y);
        vert.push_back(t.z);

        vert.push_back(b.x);
        vert.push_back(b.y);
        vert.push_back(b.z);
    }

    glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
    glBufferData(GL_ARRAY_BUFFER, vert.size() * sizeof(float), vert.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indices.size() * sizeof(unsigned int), model.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(11 * sizeof(float)));
    glEnableVertexAttribArray(4);

    glBindVertexArray(0);
    return true;
}

void DestroyModelGL(Model& model)
{
    for (auto& sm : model.subMeshes) {
        if (sm.texture)
            glDeleteTextures(1, &sm.texture);
    }

    if (model.vbo) glDeleteBuffers(1, &model.vbo);
    if (model.ebo) glDeleteBuffers(1, &model.ebo);
    if (model.vao) glDeleteVertexArrays(1, &model.vao);

    model.vbo = model.ebo = model.vao = 0;
}

void DrawModel(const Model& model)
{
    glBindVertexArray(model.vao);

    for (const SubMesh& sm : model.subMeshes)
    {
        glBindTexture(GL_TEXTURE_2D, sm.texture);
        glDrawElements(
            GL_TRIANGLES,
            sm.indexCount,
            GL_UNSIGNED_INT,
            (void*)(sm.indexOffset * sizeof(unsigned int))
        );
    }

    glBindVertexArray(0);
}
