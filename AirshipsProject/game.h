#pragma once

#include <SFML/Graphics.hpp>
#include <glm/glm.hpp>

#include <random>
#include <string>
#include <vector>

#include "model.h"

struct DirectionalLight {
    glm::vec3 direction{ -0.25f, -1.0f, -0.35f };
    glm::vec3 ambient{ 0.20f, 0.20f, 0.20f };
    glm::vec3 diffuse{ 0.85f, 0.85f, 0.85f };
    glm::vec3 specular{ 1.0f, 1.0f, 1.0f };
    float intensity{ 1.0f };
};

struct RenderInstance {
    Model* model = nullptr;

    glm::vec3 position{ 0.0f };
    glm::vec3 rotationDeg{ 0.0f };
    glm::vec3 scale{ 1.0f };

    float swayStrength{ 0.0f };
    float emissionStrength{ 0.0f };

    bool useNormalMap{ false };
    glm::vec3 tint{ 1.0f, 1.0f, 1.0f };
};

struct TargetHouse {
    RenderInstance inst;
    float radius{ 2.5f };
    bool delivered{ false };
};

struct Cloud {
    RenderInstance inst;
    glm::vec3 basePosition{ 0.0f };
    float phase{ 0.0f };
    float speed{ 0.35f };
    float amplitude{ 5.0f };
};

struct Balloon {
    RenderInstance inst;
    glm::vec3 basePosition{ 0.0f };
    float phase{ 0.0f };
};

struct Package {
    RenderInstance inst;
    glm::vec3 velocity{ 0.0f };
    bool active{ false };
};

class Game {
public:
    explicit Game(sf::RenderWindow& window);
    ~Game();
    bool Initialize();
    void Run();
private:
    void LoadAll();
    void CreateProceduralMeshes();
    void GenerateScene();

    void HandleEvents();
    void Update(float dt);
    void Render();

    void SpawnPackage();
    void ResolvePackageCollisions();

    void UpdateCamera(glm::mat4& outView, glm::vec3& outViewPos);
    glm::mat4 MakeModelMatrix(const RenderInstance& inst) const;
    void DrawInstance(const RenderInstance& inst);

    unsigned int Create1x1TextureRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
    void EnsureTextures(Model& model, unsigned int fallbackTex);
    void SnapToGround(RenderInstance& inst);

private:
    sf::RenderWindow& m_window;
    std::mt19937 m_rng{ std::random_device{}() };

    unsigned int m_program{ 0 };

    int m_uModel{ -1 }, m_uView{ -1 }, m_uProj{ -1 }, m_uNormalMatrix{ -1 };
    int m_uViewPos{ -1 }, m_uTime{ -1 };

    int m_uDirDir{ -1 }, m_uDirAmbient{ -1 }, m_uDirDiffuse{ -1 }, m_uDirSpecular{ -1 }, m_uDirIntensity{ -1 };

    int m_uDiffuseSampler{ -1 }, m_uNormalSampler{ -1 }, m_uUseNormalMap{ -1 };
    int m_uSwayStrength{ -1 }, m_uEmissionStrength{ -1 }, m_uTint{ -1 };

    unsigned int m_whiteTex{ 0 };
    unsigned int m_defaultNormalTex{ 0 };
    unsigned int m_airshipNormalTex{ 0 };

    DirectionalLight m_dirLight{};

    Model m_airshipModel, m_treeModel, m_houseModel, m_decor1Model, m_decor2Model, m_cloudModel, m_balloonModel;
    Model m_fieldModel, m_packageModel;

    RenderInstance m_airship, m_tree, m_field;

    std::vector<TargetHouse> m_houses;
    std::vector<RenderInstance> m_decorations;
    std::vector<Cloud> m_clouds;
    std::vector<Balloon> m_balloons;
    std::vector<Package> m_packages;

    float m_time{ 0.0f };

    glm::vec3 m_airshipPos{ 0.0f, 18.0f, 25.0f };

    float m_cameraYawDeg{ 180.0f };
    float m_airshipYawDeg{ 180.0f };

    float m_airshipSpeed{ 16.0f };

    float m_fieldHalfSize{ 60.0f };

    float m_cameraDist{ 18.0f };
    float m_cameraHeight{ 9.0f };
    float m_fovDeg{ 60.0f };

    bool m_aimMode{ false };

    float m_airshipYawModelOffsetDeg{ 180.0f };
    float m_airshipRollDeg{ 0.0f };
};
