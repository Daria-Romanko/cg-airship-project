#include "game.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <random>
#include <vector>

#include "shader_utils.h"

static float WrapDeg(float deg) {
    deg = glm::mod(deg, 360.0f);
    if (deg < 0.0f) deg += 360.0f;
    return deg;
}

Game::Game(sf::RenderWindow& window)
    : m_window(window) {
    std::random_device rd;
    m_rng = std::mt19937(rd());
}

Game::~Game() {
    if (m_program) glDeleteProgram(m_program);

    auto DestroyIf = [](Model& m) {
        if (m.vao || m.vbo || m.ebo) DestroyModelGL(m);
        };

    DestroyIf(m_airshipModel);
    DestroyIf(m_treeModel);
    DestroyIf(m_houseModel);
    DestroyIf(m_decor1Model);
    DestroyIf(m_decor2Model);
    DestroyIf(m_cloudModel);
    DestroyIf(m_balloonModel);
    DestroyIf(m_fieldModel);
    DestroyIf(m_packageModel);

    if (m_whiteTex) glDeleteTextures(1, &m_whiteTex);
    if (m_defaultNormalTex) glDeleteTextures(1, &m_defaultNormalTex);
    if (m_airshipNormalTex) glDeleteTextures(1, &m_airshipNormalTex);
}

unsigned int Game::Create1x1TextureRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    unsigned int tex = 0;
    unsigned char px[4] = { r, g, b, a };
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return tex;
}

void Game::EnsureTextures(Model& model, unsigned int fallbackTex) {
    for (auto& sm : model.subMeshes) {
        if (sm.texture == 0) sm.texture = fallbackTex;
    }
}

bool Game::Initialize() {
    glEnable(GL_DEPTH_TEST);

    m_whiteTex = Create1x1TextureRGBA(255, 255, 255, 255);
    m_defaultNormalTex = Create1x1TextureRGBA(128, 128, 255, 255);

    m_program = CreateShaderProgramFromFiles("game.vert", "game.frag");
    if (!m_program) {
        std::cerr << "Failed to create shader program (game.vert/game.frag)\n";
        return false;
    }

    glUseProgram(m_program);

    m_uModel = glGetUniformLocation(m_program, "u_model");
    m_uView = glGetUniformLocation(m_program, "u_view");
    m_uProj = glGetUniformLocation(m_program, "u_projection");
    m_uNormalMatrix = glGetUniformLocation(m_program, "u_normalMatrix");
    m_uViewPos = glGetUniformLocation(m_program, "u_viewPos");
    m_uTime = glGetUniformLocation(m_program, "u_time");

    m_uDirDir = glGetUniformLocation(m_program, "u_dirLight.direction");
    m_uDirAmbient = glGetUniformLocation(m_program, "u_dirLight.ambient");
    m_uDirDiffuse = glGetUniformLocation(m_program, "u_dirLight.diffuse");
    m_uDirSpecular = glGetUniformLocation(m_program, "u_dirLight.specular");
    m_uDirIntensity = glGetUniformLocation(m_program, "u_dirLight.intensity");

    m_uDiffuseSampler = glGetUniformLocation(m_program, "u_diffuse");
    m_uNormalSampler = glGetUniformLocation(m_program, "u_normalMap");
    m_uUseNormalMap = glGetUniformLocation(m_program, "u_useNormalMap");
    m_uSwayStrength = glGetUniformLocation(m_program, "u_swayStrength");
    m_uEmissionStrength = glGetUniformLocation(m_program, "u_emissionStrength");
    m_uTint = glGetUniformLocation(m_program, "u_tint");

    glUniform1i(m_uDiffuseSampler, 0);
    glUniform1i(m_uNormalSampler, 1);

    glUniform3fv(m_uDirDir, 1, glm::value_ptr(m_dirLight.direction));
    glUniform3fv(m_uDirAmbient, 1, glm::value_ptr(m_dirLight.ambient));
    glUniform3fv(m_uDirDiffuse, 1, glm::value_ptr(m_dirLight.diffuse));
    glUniform3fv(m_uDirSpecular, 1, glm::value_ptr(m_dirLight.specular));
    glUniform1f(m_uDirIntensity, m_dirLight.intensity);

    LoadAll();
    CreateProceduralMeshes();
    GenerateScene();

    return true;
}

void Game::LoadAll() {
    auto Load = [&](const char* path, Model& m) {
        if (!LoadOBJModel(path, m)) {
            std::cerr << "Model load failed: " << path << "\n";
            return false;
        }
        EnsureTextures(m, m_whiteTex);
        if (!InitializeModelGL(m)) {
            std::cerr << "Model GL init failed: " << path << "\n";
            return false;
        }
        return true;
        };

    Load("models/airship.obj", m_airshipModel);
    Load("models/tree.obj", m_treeModel);
    Load("models/house.obj", m_houseModel);
    Load("models/decor1.obj", m_decor1Model);
    Load("models/decor2.obj", m_decor2Model);
    Load("models/cloud.obj", m_cloudModel);
    Load("models/balloon.obj", m_balloonModel);

    m_airshipNormalTex = LoadTextureFromFile("models/airship_normal.jpg");
    if (m_airshipNormalTex == 0) {
        m_airshipNormalTex = m_defaultNormalTex;
        std::cerr << "Warning: airship normal map not found, using default normal.\n";
    }

    m_airship.model = &m_airshipModel;
    m_airship.position = m_airshipPos;
    m_airship.scale = { 1.0f, 1.0f, 1.0f };
    m_airship.useNormalMap = true;

    float headingDeg = WrapDeg(m_airshipYawDeg + m_airshipYawModelOffsetDeg);
    m_airship.rotationDeg = { 0.0f, headingDeg, 0.0f };
    m_cameraYawDeg = headingDeg;

    m_tree.model = &m_treeModel;
    m_tree.position = { 0.0f, 0.0f, 0.0f };
    m_tree.scale = { 2.0f, 2.0f, 2.0f };
    m_tree.swayStrength = 0.06f;
}

void Game::CreateProceduralMeshes() {
    m_fieldModel.vertices = {
        {-m_fieldHalfSize, 0.0f, -m_fieldHalfSize},
        { m_fieldHalfSize, 0.0f, -m_fieldHalfSize},
        { m_fieldHalfSize, 0.0f,  m_fieldHalfSize},
        {-m_fieldHalfSize, 0.0f,  m_fieldHalfSize},
    };
    m_fieldModel.texCoords = {
        {0.0f, 0.0f},
        {50.0f, 0.0f},
        {50.0f, 50.0f},
        {0.0f, 50.0f},
    };
    m_fieldModel.normals = {
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
    };
    m_fieldModel.indices = { 0, 1, 2, 2, 3, 0 };
    ComputeTangents(m_fieldModel);

    SubMesh sm{};
    sm.indexOffset = 0;
    sm.indexCount = static_cast<unsigned int>(m_fieldModel.indices.size());
    sm.texture = LoadTextureFromFile("models/field.jpg");
    if (sm.texture == 0) sm.texture = m_whiteTex;
    m_fieldModel.subMeshes = { sm };

    if (!InitializeModelGL(m_fieldModel)) {
        std::cerr << "Failed to init field mesh\n";
    }

    m_field.model = &m_fieldModel;
    m_field.position = { 0.0f, 0.0f, 0.0f };
    m_field.scale = { 1.0f, 1.0f, 1.0f };

    const float s = 0.35f;
    struct V { glm::vec3 p; glm::vec2 uv; glm::vec3 n; };
    std::vector<V> v;
    std::vector<unsigned int> idx;
    v.reserve(24);
    idx.reserve(36);

    auto AddFace = [&](glm::vec3 n, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
        unsigned int base = static_cast<unsigned int>(v.size());
        v.push_back({ a, {0,0}, n });
        v.push_back({ b, {1,0}, n });
        v.push_back({ c, {1,1}, n });
        v.push_back({ d, {0,1}, n });
        idx.insert(idx.end(), { base + 0, base + 1, base + 2, base + 2, base + 3, base + 0 });
        };

    AddFace({ 0, 0, 1 }, { -s,-s, s }, { s,-s, s }, { s, s, s }, { -s, s, s });
    AddFace({ 0, 0,-1 }, { s,-s,-s }, { -s,-s,-s }, { -s, s,-s }, { s, s,-s });
    AddFace({ 1, 0, 0 }, { s,-s, s }, { s,-s,-s }, { s, s,-s }, { s, s, s });
    AddFace({ -1, 0, 0 }, { -s,-s,-s }, { -s,-s, s }, { -s, s, s }, { -s, s,-s });
    AddFace({ 0, 1, 0 }, { -s, s, s }, { s, s, s }, { s, s,-s }, { -s, s,-s });
    AddFace({ 0,-1, 0 }, { -s,-s,-s }, { s,-s,-s }, { s,-s, s }, { -s,-s, s });

    m_packageModel.vertices.clear();
    m_packageModel.texCoords.clear();
    m_packageModel.normals.clear();
    m_packageModel.indices = idx;

    for (const auto& vv : v) {
        m_packageModel.vertices.push_back(vv.p);
        m_packageModel.texCoords.push_back(vv.uv);
        m_packageModel.normals.push_back(vv.n);
    }

    ComputeTangents(m_packageModel);

    SubMesh psm{};
    psm.indexOffset = 0;
    psm.indexCount = static_cast<unsigned int>(m_packageModel.indices.size());
    psm.texture = LoadTextureFromFile("models/package.jpg");
    if (psm.texture == 0) psm.texture = m_whiteTex;
    m_packageModel.subMeshes = { psm };

    if (!InitializeModelGL(m_packageModel)) {
        std::cerr << "Failed to init package mesh\n";
    }
}

void Game::GenerateScene() {
    std::uniform_real_distribution<float> posDist(-m_fieldHalfSize * 0.85f, m_fieldHalfSize * 0.85f);

    auto FarFromCenter = [&](glm::vec3 p) {
        return glm::length(glm::vec2(p.x, p.z)) > 10.0f;
        };

    const int houseCount = 20;
    m_houses.clear();
    for (int i = 0; i < houseCount; ++i) {
        glm::vec3 p;
        for (int tries = 0; tries < 100; ++tries) {
            p = { posDist(m_rng), 0.0f, posDist(m_rng) };
            if (!FarFromCenter(p)) continue;
            bool ok = true;
            for (const auto& h : m_houses) {
                if (glm::distance(glm::vec2(p.x, p.z), glm::vec2(h.inst.position.x, h.inst.position.z)) < 8.0f) {
                    ok = false;
                    break;
                }
            }
            if (ok) break;
        }

        TargetHouse house;
        house.inst.model = &m_houseModel;
        house.inst.position = p;
        house.inst.scale = { 1.6f, 1.6f, 1.6f };
        SnapToGround(house.inst);
        house.radius = 2.5f;
        m_houses.push_back(house);
    }

    m_decorations.clear();
    const int decorCount = 30;
    for (int i = 0; i < decorCount; ++i) {
        RenderInstance d;
        d.model = (i % 2 == 0) ? &m_decor1Model : &m_decor2Model;
        d.position = { posDist(m_rng), 0.0f, posDist(m_rng) };
        d.scale = { 1.0f, 1.0f, 1.0f };
        d.rotationDeg = { 0.0f, posDist(m_rng) * 3.0f, 0.0f };
        d.swayStrength = (i % 3 == 0) ? 0.03f : 0.0f;
        SnapToGround(d);
        m_decorations.push_back(d);
    }

    std::uniform_real_distribution<float> cloudDist(-m_fieldHalfSize, m_fieldHalfSize);
    std::uniform_real_distribution<float> phaseDist(0.0f, 1000.0f);

    const int cloudCount = 15;
    m_clouds.clear();
    for (int i = 0; i < cloudCount; ++i) {
        Cloud c;
        c.basePosition = { cloudDist(m_rng), 20.0f + (i % 3) * 1.5f, cloudDist(m_rng) };
        c.phase = phaseDist(m_rng);
        c.speed = 0.25f + 0.15f * (i % 3);
        c.amplitude = 4.0f + 2.0f * (i % 2);

        c.inst.model = &m_cloudModel;
        c.inst.position = c.basePosition;
        c.inst.scale = { 2.5f, 2.5f, 2.5f };
        c.inst.tint = { 0.95f, 0.95f, 1.0f };
        m_clouds.push_back(c);
    }

    const int balloonCount = 10;
    m_balloons.clear();
    for (int i = 0; i < balloonCount; ++i) {
        Balloon b;
        b.basePosition = { cloudDist(m_rng), 13.0f + (i % 2) * 2.0f, cloudDist(m_rng) };
        b.phase = phaseDist(m_rng);
        b.inst.model = &m_balloonModel;
        b.inst.position = b.basePosition;
        b.inst.scale = { 1.2f, 1.2f, 1.2f };
        m_balloons.push_back(b);
    }

    m_packages.assign(40, Package{});
    for (auto& p : m_packages) {
        p.inst.model = &m_packageModel;
        p.inst.scale = { 1.0f, 1.0f, 1.0f };
        p.inst.tint = { 1.0f, 1.0f, 1.0f };
    }
}

void Game::Run() {
    sf::Clock clock;
    while (m_window.isOpen()) {
        float dt = clock.restart().asSeconds();
        dt = glm::clamp(dt, 0.0f, 0.05f);

        HandleEvents();
        Update(dt);
        Render();
    }
}

void Game::HandleEvents() {
    while (const std::optional<sf::Event> ev = m_window.pollEvent()) {
        if (ev->is<sf::Event::Closed>()) {
            m_window.close();
            continue;
        }

        if (const auto* wheel = ev->getIf<sf::Event::MouseWheelScrolled>()) {
            m_fovDeg -= wheel->delta * 3.0f;
            m_fovDeg = glm::clamp(m_fovDeg, 25.0f, 150.0f);
        }

        if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
            const auto code = key->code;

            if (code == sf::Keyboard::Key::Escape)
                m_window.close();

            if (code == sf::Keyboard::Key::C)
                m_aimMode = !m_aimMode;

            if (code == sf::Keyboard::Key::Space)
                SpawnPackage();
        }
    }
}

void Game::Update(float dt) {
    m_time += dt;

    float turnInput = 0.0f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) turnInput -= 1.0f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) turnInput += 1.0f;

    const float yawSpeedDeg = 90.0f;
    m_airshipYawDeg = WrapDeg(m_airshipYawDeg - turnInput * yawSpeedDeg * dt);

    const float yawDeg = m_airshipYawDeg;
    m_cameraYawDeg = yawDeg;

    const float maxRollDeg = 18.0f;
    const float targetRollDeg = -turnInput * maxRollDeg;

    const float rollResponsiveness = 8.0f;
    const float a = 1.0f - std::exp(-rollResponsiveness * dt);
    m_airshipRollDeg = glm::mix(m_airshipRollDeg, targetRollDeg, a);

    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const float yawRad = glm::radians(yawDeg);

    const glm::mat4 R = glm::rotate(glm::mat4(1.0f), yawRad, up);

    const glm::vec3 forward = glm::normalize(glm::vec3(R * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    const glm::vec3 right = glm::normalize(glm::vec3(R * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));

    glm::vec3 vel(0.0f);
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) vel += forward;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) vel -= forward;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q)) vel -= right;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E)) vel += right;

    if (glm::length(vel) > 0.01f)
        vel = glm::normalize(vel) * m_airshipSpeed;

    m_airshipPos += vel * dt;
    m_airshipPos.x = glm::clamp(m_airshipPos.x, -m_fieldHalfSize * 0.9f, m_fieldHalfSize * 0.9f);
    m_airshipPos.z = glm::clamp(m_airshipPos.z, -m_fieldHalfSize * 0.9f, m_fieldHalfSize * 0.9f);

    m_airship.position = m_airshipPos;

    const float modelYawDeg = WrapDeg(yawDeg + m_airshipYawModelOffsetDeg);

    m_airship.rotationDeg = {
        0.0f,
        modelYawDeg,
        m_airshipRollDeg
    };

    for (auto& c : m_clouds) {
        float t = m_time * c.speed + c.phase;
        c.inst.position = c.basePosition + glm::vec3(
            std::sin(t) * c.amplitude,
            std::sin(t * 0.6f) * 0.8f,
            std::cos(t * 0.9f) * c.amplitude
        );

        float flash = std::sin(m_time * 6.5f + c.phase * 0.25f);
        c.inst.emissionStrength = (flash > 0.98f) ? 6.0f : 0.0f;
    }

    for (auto& b : m_balloons) {
        float t = m_time * 0.7f + b.phase;
        b.inst.position = b.basePosition + glm::vec3(
            std::sin(t) * 1.4f,
            std::sin(t * 1.2f) * 0.6f,
            std::cos(t * 0.8f) * 1.4f
        );
    }

    for (auto& p : m_packages) {
        if (!p.active) continue;
        p.velocity += glm::vec3(0.0f, -9.81f, 0.0f) * dt;
        p.inst.position += p.velocity * dt;

        if (p.inst.position.y <= 0.0f) {
            p.inst.position.y = 0.0f;
            p.active = false;
        }
    }

    ResolvePackageCollisions();

    int delivered = 0;
    for (const auto& h : m_houses) if (h.delivered) ++delivered;
}

void Game::SpawnPackage() {
    for (auto& p : m_packages) {
        if (p.active) continue;
        p.active = true;

        p.inst.position = m_airshipPos + glm::vec3(0.0f, -2.0f, 0.0f);
        p.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
        p.inst.rotationDeg = { 0.0f, 0.0f, 0.0f };
        p.inst.swayStrength = 0.0f;
        p.inst.emissionStrength = 0.0f;
        p.inst.useNormalMap = false;
        p.inst.tint = { 1.0f, 1.0f, 1.0f };
        return;
    }
}

void Game::ResolvePackageCollisions() {
    for (auto& p : m_packages) {
        if (!p.active) continue;

        for (auto& h : m_houses) {
            if (h.delivered) continue;

            float dx = p.inst.position.x - h.inst.position.x;
            float dz = p.inst.position.z - h.inst.position.z;
            float dist2 = dx * dx + dz * dz;
            if (dist2 <= h.radius * h.radius && p.inst.position.y <= 1.5f) {
                h.delivered = true;
                h.inst.tint = { 0.7f, 1.0f, 0.7f };
                p.active = false;
                break;
            }
        }
    }
}

void Game::UpdateCamera(glm::mat4& outView, glm::vec3& outViewPos) {
    const float yawRad = glm::radians(m_cameraYawDeg);

    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::mat4 R = glm::rotate(glm::mat4(1.0f), yawRad, up);

    const glm::vec3 forward = glm::normalize(glm::vec3(R * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));

    glm::vec3 camPos;
    glm::vec3 camTarget;

    if (!m_aimMode) {
        camPos = m_airshipPos - forward * m_cameraDist + glm::vec3(0.0f, m_cameraHeight, 0.0f);
        camTarget = m_airshipPos + forward * 6.0f + glm::vec3(0.0f, -1.5f, 0.0f);
    }
    else {
        camPos = m_airshipPos + glm::vec3(0.0f, -2.0f, 0.0f);
        camTarget = m_airshipPos + forward * 10.0f + glm::vec3(0.0f, -15.0f, 0.0f);
    }

    outViewPos = camPos;
    outView = glm::lookAt(camPos, camTarget, up);
}


glm::mat4 Game::MakeModelMatrix(const RenderInstance& inst) const {
    glm::mat4 m(1.0f);
    m = glm::translate(m, inst.position);

    glm::vec3 r = glm::radians(inst.rotationDeg);
    m = glm::rotate(m, r.x, glm::vec3(1, 0, 0));
    m = glm::rotate(m, r.y, glm::vec3(0, 1, 0));
    m = glm::rotate(m, r.z, glm::vec3(0, 0, 1));

    m = glm::scale(m, inst.scale);
    return m;
}

void Game::DrawInstance(const RenderInstance& inst) {
    if (!inst.model) return;

    glm::mat4 modelM = MakeModelMatrix(inst);
    glm::mat3 normalM = glm::transpose(glm::inverse(glm::mat3(modelM)));

    glUniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(modelM));
    glUniformMatrix3fv(m_uNormalMatrix, 1, GL_FALSE, glm::value_ptr(normalM));

    glUniform1f(m_uSwayStrength, inst.swayStrength);
    glUniform1f(m_uEmissionStrength, inst.emissionStrength);
    glUniform3fv(m_uTint, 1, glm::value_ptr(inst.tint));

    glUniform1i(m_uUseNormalMap, inst.useNormalMap ? 1 : 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, inst.useNormalMap ? m_airshipNormalTex : m_defaultNormalTex);
    glActiveTexture(GL_TEXTURE0);

    DrawModel(*inst.model);
}

void Game::Render() {
    int w = (int)m_window.getSize().x;
    int h = (int)m_window.getSize().y;

    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_program);

    glm::mat4 view(1.0f);
    glm::vec3 viewPos(0.0f);
    UpdateCamera(view, viewPos);

    glm::mat4 proj = glm::perspective(glm::radians(m_fovDeg), (float)w / (float)h, 0.1f, 300.0f);

    glUniformMatrix4fv(m_uView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(m_uProj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(m_uViewPos, 1, glm::value_ptr(viewPos));
    glUniform1f(m_uTime, m_time);

    DrawInstance(m_field);

    for (auto& hInst : m_houses) DrawInstance(hInst.inst);
    for (auto& d : m_decorations) DrawInstance(d);
    DrawInstance(m_tree);

    for (auto& c : m_clouds) DrawInstance(c.inst);
    for (auto& b : m_balloons) DrawInstance(b.inst);

    for (auto& p : m_packages) if (p.active) DrawInstance(p.inst);

    DrawInstance(m_airship);

    m_window.display();
}

void Game::SnapToGround(RenderInstance& inst) {
    if (!inst.model) return;
    inst.position.y += (-inst.model->minY) * inst.scale.y + 0.01f;
}
