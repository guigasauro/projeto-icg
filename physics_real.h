#include "libs.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const int NUM_BODIES = 9;


// Physics constants
const double G = 6.67430e-11;
const double timeStep = 43200.0;
const double positionScale = 5e10;
const double radiusScale = 1e7;
const int STACKS = 30;
const int SECTORS = 30;

struct BodyData {
    double mass;
    double orbitRadius;
    double radius;
    glm::vec4 color;
    double inclination;
    const char* textureFile;
};

std::vector<BodyData> solarSystemData = {
    {1.98847e30,    0.0,        7.9634e7, {1.0f, 0.8f, 0.0f, 1.0f}, 0.0, "assets/2k_sun.jpg"},
    {3.3011e23,  5.4e11,    2.4397e5, {0.8f, 0.5f, 0.2f, 1.0f}, 7.0, "assets/2k_mercury.jpg"},
    {4.8675e24, 7e11,    6.0518e5, {0.9f, 0.7f, 0.2f, 1.0f}, 3.4, "assets/2k_venus_surface.jpg"},
    {5.9724e24, 11e11,    6.3710e5, {0.0f, 0.5f, 1.0f, 1.0f}, 0.0, "assets/2k_earth_daymap.jpg"},
    {6.4171e23, 15e11,    3.3895e5, {1.0f, 0.2f, 0.1f, 1.0f}, 1.9, "assets/2k_mars.jpg"},
    {1.8982e27, 23e11,   1e7, {0.9f, 0.6f, 0.3f, 1.0f}, 1.3, "assets/2k_jupiter.jpg"},
    {5.6834e26, 28e11,   4.8232e6, {0.9f, 0.8f, 0.5f, 1.0f}, 2.5, "assets/2k_saturn.jpg"},
    {8.6810e25, 37e11,   2.5362e6, {0.5f, 0.8f, 0.9f, 1.0f}, 0.8, "assets/2k_uranus.jpg"},
    {1.02413e26, 4.503e12,  2.4622e6, {0.3f, 0.4f, 0.9f, 1.0f}, 1.8, "assets/2k_neptune.jpg"}
};

struct CelestialBody {
    GLuint VAO, VBO, textureID;
    glm::dvec3 position;
    glm::dvec3 velocity;
    double mass;
    double radius;
    size_t vertexCount;
    bool isSun;

    CelestialBody(const glm::dvec3& pos, const glm::dvec3& vel, double m, double realRadius,
        const glm::vec4& col,const char* textureFile, bool sun = false)
        : position(pos), velocity(vel), mass(m), isSun(sun) {
        
        radius = realRadius / radiusScale;
        auto vertices = createSphere(radius);
        vertexCount = vertices.size() / 5;  // 5 floats per vertex (position + texture)
        
        // Load texture
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(textureFile, &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        } else {
            std::cerr << "Failed to load texture: " << textureFile << std::endl;
        }
        stbi_image_free(data);

        // Set up vertex data and buffers
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Texture coordinate attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindVertexArray(0);
    }

    static std::vector<float> createSphere(double radius) {
        std::vector<float> vertices;
        const float PI = glm::pi<float>();
        
        for (int i = 0; i < STACKS; ++i) {
            float theta1 = i * PI / STACKS;
            float theta2 = (i + 1) * PI / STACKS;
            
            for (int j = 0; j < SECTORS; ++j) {
                float phi1 = j * 2 * PI / SECTORS;
                float phi2 = (j + 1) * 2 * PI / SECTORS;
                
                auto addVertex = [&](float theta, float phi) {
                    glm::vec3 pos(
                        radius * sin(theta) * cos(phi),
                        radius * cos(theta),
                        radius * sin(theta) * sin(phi)
                    );
                    // Texture coordinates
                    float u = phi / (2 * PI);
                    float v = 1.0f - theta / PI;
                    
                    vertices.push_back(pos.x);
                    vertices.push_back(pos.y);
                    vertices.push_back(pos.z);
                    vertices.push_back(u);
                    vertices.push_back(v);
                };
                
                // Triangle 1
                addVertex(theta1, phi1);
                addVertex(theta1, phi2);
                addVertex(theta2, phi1);
                
                // Triangle 2
                addVertex(theta1, phi2);
                addVertex(theta2, phi2);
                addVertex(theta2, phi1);
            }
        }
        return vertices;
    }
};

void updatePhysics(std::vector<CelestialBody>& bodies) {
    std::vector<glm::dvec3> newPositions(bodies.size());
    std::vector<glm::dvec3> newVelocities(bodies.size());
    
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].isSun) {
            newPositions[i] = glm::dvec3(0.0);
            newVelocities[i] = glm::dvec3(0.0);
            continue;
        }
        
        glm::dvec3 netForce(0.0);
        
        for (size_t j = 0; j < bodies.size(); ++j) {
            if (i == j) continue;
            
            glm::dvec3 r = bodies[j].position - bodies[i].position;
            double distanceSquared = glm::dot(r, r);
            double distance = sqrt(distanceSquared);
            netForce += (r / distance) * (G * bodies[i].mass * bodies[j].mass / distanceSquared);
        }
        
        glm::dvec3 acceleration = netForce / bodies[i].mass;
        newVelocities[i] = bodies[i].velocity + acceleration * timeStep;
        newPositions[i] = bodies[i].position + newVelocities[i] * timeStep;
    }
    
    for (size_t i = 0; i < bodies.size(); ++i) {
        bodies[i].position = newPositions[i];
        bodies[i].velocity = newVelocities[i];
    }
}

static std::vector<float> createTorusRing(double mainRadius, double tubeRadius, int mainSegments = 50, int tubeSegments = 20) {
    std::vector<float> vertices;
    const float PI = glm::pi<float>();
    const float TAU = 2.0f * PI;

    for (int i = 0; i <= mainSegments; ++i) {
        float mainAngle = i * TAU / mainSegments;
        glm::vec3 center(mainRadius * cos(mainAngle), mainRadius * sin(mainAngle), 0.0f);

        for (int j = 0; j <= tubeSegments; ++j) {
            float tubeAngle = j * TAU / tubeSegments;
            
            // Calcular posição do vértice
            glm::vec3 pos(
                center.x + tubeRadius * cos(tubeAngle) * cos(mainAngle),
                center.y + tubeRadius * cos(tubeAngle) * sin(mainAngle),
                tubeRadius * sin(tubeAngle)
            );

            // Coordenadas de textura
            float u = (float)i / mainSegments;
            float v = (float)j / tubeSegments;

            // Adicionar vértice
            vertices.push_back(pos.x);
            vertices.push_back(pos.y);
            vertices.push_back(pos.z);
            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    // Criar índices para formar triângulos
    std::vector<float> fullVertices;
    for (int i = 0; i < mainSegments; ++i) {
        for (int j = 0; j < tubeSegments; ++j) {
            int current = i * (tubeSegments + 1) + j;
            int next = current + tubeSegments + 1;

            // Triângulo 1
            fullVertices.insert(fullVertices.end(), vertices.begin() + current * 5, vertices.begin() + (current + 1) * 5);
            fullVertices.insert(fullVertices.end(), vertices.begin() + next * 5, vertices.begin() + (next + 1) * 5);
            fullVertices.insert(fullVertices.end(), vertices.begin() + (current + 1) * 5, vertices.begin() + (current + 2) * 5);

            // Triângulo 2
            fullVertices.insert(fullVertices.end(), vertices.begin() + (next + 1) * 5, vertices.begin() + (next + 2) * 5);
            fullVertices.insert(fullVertices.end(), vertices.begin() + next * 5, vertices.begin() + (next + 1) * 5);
            fullVertices.insert(fullVertices.end(), vertices.begin() + (current + 1) * 5, vertices.begin() + (current + 2) * 5);
        }
    }

    return fullVertices;
}