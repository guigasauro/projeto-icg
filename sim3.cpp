#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const int NUM_BODIES = 9;

// Shader sources
const char* vertexShaderSource = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aTexCoord;
layout(location=2) in vec3 aNormal;


out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture1;
uniform bool isSun;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

void main() {
    if(isSun){
        FragColor = texture(texture1, TexCoord) * vec4(2.0, 2.0, 1.5, 1.0);
        return; 
    }

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    float specularStrength = 0.5;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 texColor = texture(texture1, TexCoord).rgb;
    vec3 result = (ambient + diffuse + specular) * texColor;
    FragColor = vec4(result, 1.0);
}
)glsl";


// Physics constants
const double G = 6.67430e-11;
const double timeStep = 43200.0;     // 12 hours in seconds (0.5 Earth day)
const double positionScale = 5e10;
const double radiusScale = 120;
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
    {1.98847e30,    0.0,        7.9634e7, {1.0f, 0.8f, 0.0f, 1.0f}, 0.0, "2k_sun.jpg"},
    {3.3011e23,  5.4e11,    2.4397e5, {0.8f, 0.5f, 0.2f, 1.0f}, 7.0, "2k_mercury.jpg"},
    {4.8675e24, 7e11,    6.0518e5, {0.9f, 0.7f, 0.2f, 1.0f}, 3.4, "2k_venus_surface.jpg"},
    {5.9724e24, 11e11,    6.3710e5, {0.0f, 0.5f, 1.0f, 1.0f}, 0.0, "2k_earth_daymap.jpg"},
    {6.4171e23, 15e11,    3.3895e5, {1.0f, 0.2f, 0.1f, 1.0f}, 1.9, "2k_mars.jpg"},
    {1.8982e27, 23e11,   1e7, {0.9f, 0.6f, 0.3f, 1.0f}, 1.3, "2k_jupiter.jpg"},
    {5.6834e26, 28e11,   4.8232e6, {0.9f, 0.8f, 0.5f, 1.0f}, 2.5, "2k_saturn.jpg"},
    {8.6810e25, 37e11,   2.5362e6, {0.5f, 0.8f, 0.9f, 1.0f}, 0.8, "2k_uranus.jpg"},
    {1.02413e26, 4.503e12,  2.4622e6, {0.3f, 0.4f, 0.9f, 1.0f}, 1.8, "2k_neptune.jpg"}
};

struct CelestialBody {
    GLuint VAO, VBO, textureID;
    glm::dvec3 position;
    glm::dvec3 velocity;
    double mass;
    double radius;
    size_t vertexCount;
    bool isSun;

    CelestialBody(const glm::dvec3& pos, const glm::dvec3& vel,
                double m, double realRadius, const glm::vec4& col,
                const char* textureFile, bool sun = false)
        : position(pos), velocity(vel), mass(m), isSun(sun) {
        
        radius = std::cbrt(realRadius) / radiusScale;
        auto vertices = createSphere(radius);
        vertexCount = vertices.size() / 8;  // 5 floats per vertex (position + texture)
        
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
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Texture coordinate attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);
        
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

                    glm::vec3 normal = glm::normalize(pos);
                    // Texture coordinates
                    float u = phi / (2 * PI);
                    float v = 1.0f - theta / PI;
                    
                    vertices.push_back(pos.x);
                    vertices.push_back(pos.y);
                    vertices.push_back(pos.z);
                    vertices.push_back(u);
                    vertices.push_back(v);
                    vertices.push_back(normal.x);
                    vertices.push_back(normal.y);
                    vertices.push_back(normal.z);
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

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error:\n" << infoLog << std::endl;
    }
    
    return shader;
}

GLuint createShaderProgram() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking error:\n" << infoLog << std::endl;
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
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

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Solar System Simulation", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }
    
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    GLuint backgroundTexture;
    glGenTextures(1, &backgroundTexture);
    glBindTexture(GL_TEXTURE_2D, backgroundTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int bgWidth, bgHeight, bgChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* bgData = stbi_load("2k_stars.jpg", &bgWidth, &bgHeight, &bgChannels, 0);
    if (bgData) {
        GLenum format = (bgChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, bgWidth, bgHeight, 0, format, GL_UNSIGNED_BYTE, bgData);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "Failed to load background texture" << std::endl;
    }
    stbi_image_free(bgData);    

    // Criar geometria do background (quad full-screen)
    float quadVertices[] = {
        // positions        // texCoords
        -1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 1.0f, 1.0f, 0.0f,
        
        -1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 1.0f, 1.0f, 0.0f,
        1.0f,  1.0f, 1.0f, 1.0f, 1.0f
    };

    GLuint quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);


    

    GLuint shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);
    
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint textureLoc = glGetUniformLocation(shaderProgram, "texture1");

    std::vector<CelestialBody> bodies;
    bodies.reserve(NUM_BODIES);
    
    // Create Sun at center
    bodies.emplace_back(
        glm::dvec3(0.0),
        glm::dvec3(0.0),
        solarSystemData[0].mass,
        solarSystemData[0].radius,
        solarSystemData[0].color,
        solarSystemData[0].textureFile,
        true
    );
    
    // Create planets
    for (int i = 1; i < NUM_BODIES; ++i) {
        double inclination = glm::radians(solarSystemData[i].inclination);
        double minDistance = (solarSystemData[0].radius + solarSystemData[i].radius) * 1.5;
        double effectiveOrbitRadius = std::max(solarSystemData[i].orbitRadius, minDistance);
        
        glm::dvec3 position(effectiveOrbitRadius, 0.0, 0.0);
        double orbitalVelocity = sqrt(G * solarSystemData[0].mass / effectiveOrbitRadius);
        glm::dvec3 velocity(0.0, 0.0, orbitalVelocity);
        
        glm::dmat4 rotation = glm::rotate(glm::dmat4(1.0), inclination, glm::dvec3(0.0, 0.0, 1.0));
        position = glm::dvec3(rotation * glm::dvec4(position, 1.0));
        velocity = glm::dvec3(rotation * glm::dvec4(velocity, 0.0));
        
        bodies.emplace_back(
            position,
            velocity,
            solarSystemData[i].mass,
            solarSystemData[i].radius,
            solarSystemData[i].color,
            solarSystemData[i].textureFile
        );
    }
    

    // Crie os anéis de Saturno
    double ringInner = 5e6 / radiusScale; 
    double ringOuter = 1.2e7 / radiusScale;
    std::vector<float> ringVertices = createTorusRing(
        (ringInner + ringOuter)/2.0,  // Raio principal
        (ringOuter - ringInner)/2.0,  // Raio do tubo
        100,  // Segmentos principais
        2    // Segmentos do tubo
    );
    int ringVertexCount = ringVertices.size() / 5;
    
    // Carregue a textura dos anéis
    GLuint ringTexture;
    glGenTextures(1, &ringTexture);
    glBindTexture(GL_TEXTURE_2D, ringTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int width, height, nrChannels;
    unsigned char* data = stbi_load("2k_saturn_ring_alpha.png", &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    stbi_image_free(data);
        
    GLuint ringVAO, ringVBO;
    glGenVertexArrays(1, &ringVAO);
    glGenBuffers(1, &ringVBO);

    glBindVertexArray(ringVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
    glBufferData(GL_ARRAY_BUFFER, ringVertices.size() * sizeof(float), ringVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    double maxOrbitDistance = solarSystemData.back().orbitRadius;
    
    // Camera setup
    float cameraDistance = 3.0f * static_cast<float>(maxOrbitDistance / positionScale);
    float cameraAngle = 0.0f;
    float cameraHeight = cameraDistance * 0.5f;
    glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
    
    float nearPlane = 1.0f;
    float farPlane = 2.0f * static_cast<float>(maxOrbitDistance / positionScale);
    glm::mat4 projectionMatrix = glm::perspective(
        glm::radians(45.0f),
        1280.0f / 720.0f,
        nearPlane,
        farPlane
    );
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    // Camera control variables
    int cameraTargetIndex = -1;  // -1 = free camera, 0-8 = follow body
    float baseCameraDistance = cameraDistance;
    float cameraFollowDistance = 5.0f;

    glm::vec3 cameraPosition(
        cameraDistance * sin(cameraAngle),
        cameraHeight,
        cameraDistance * cos(cameraAngle)
    );

    // Configura a luz (posição do Sol)
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(glm::vec3(0.0f)));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(glm::vec3(1.0f, 0.9f, 0.7f)));  // Luz amarelada
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPosition));

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        updatePhysics(bodies);

        // Handle camera selection
        for (int i = 0; i < NUM_BODIES; ++i) {
            if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) {
                cameraTargetIndex = i;
                cameraFollowDistance = 5.0f * static_cast<float>(bodies[i].radius);
            }
        }
        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
            cameraTargetIndex = -1;
        }

        glm::mat4 viewMatrix = glm::mat4(1.0f); 
        // Handle camera movement
        if (cameraTargetIndex != -1) {
            // Follow selected body
            glm::vec3 targetPos = glm::vec3(bodies[cameraTargetIndex].position / positionScale);
            
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) cameraFollowDistance -= 0.1f;
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) cameraFollowDistance += 0.1f;
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) cameraAngle -= 0.01f;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) cameraAngle += 0.01f;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraHeight += 0.1f;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraHeight -= 0.1f;

            cameraPosition = targetPos + glm::vec3(
                cameraFollowDistance * sin(cameraAngle),
                cameraHeight,
                cameraFollowDistance * cos(cameraAngle)
            );
            cameraTarget = targetPos;
            
            viewMatrix = glm::lookAt(cameraPosition, cameraTarget, cameraUp);
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
        } else {
            // Free camera mode
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) cameraDistance -= 0.1f;
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) cameraDistance += 0.1f;
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) cameraAngle -= 0.01f;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) cameraAngle += 0.01f;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraHeight += 0.1f;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraHeight -= 0.1f;

            cameraTarget = glm::vec3(0.0f);
            
            viewMatrix = glm::lookAt(cameraPosition, cameraTarget, cameraUp);
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
        }
        
        glDepthMask(GL_FALSE); // Desativa escrita no depth buffer
        glDepthFunc(GL_LEQUAL); // Permite profundidade igual
    
        // Usar matrizes de identidade para o background
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, backgroundTexture);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    
        glDepthMask(GL_TRUE); // Reativa escrita no depth buffer
        glDepthFunc(GL_LESS); // Restaura função padrão
    
        // Restaurar matrizes da câmera para os planetas
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

        // Render all bodies
        glUniform1i(textureLoc, 0);  // Use texture unit 0
        for (size_t i = 0; i < bodies.size(); ++i) {
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            
            if (!bodies[i].isSun) {
                glm::vec3 scaledPosition = glm::vec3(bodies[i].position / positionScale);
                modelMatrix = glm::translate(modelMatrix, scaledPosition);
            }
            
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
            glActiveTexture(GL_TEXTURE0);
            glUniform1i(glGetUniformLocation(shaderProgram, "isSun"), bodies[i].isSun);
            glBindTexture(GL_TEXTURE_2D, bodies[i].textureID);
            glBindVertexArray(bodies[i].VAO);
            glDrawArrays(GL_TRIANGLES, 0, bodies[i].vertexCount);
        }
        
        glm::mat4 ringModel = glm::mat4(1.0f);
        glm::vec3 satPos = glm::vec3(bodies[6].position / positionScale);
        ringModel = glm::translate(ringModel, satPos);
        ringModel = glm::rotate(ringModel, glm::radians(27.0f), glm::vec3(0,0,1)); // Inclinação de Saturno
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(ringModel));

        glBindVertexArray(ringVAO);
        glDrawArrays(GL_TRIANGLES, 0, ringVertexCount);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ringTexture);
        glBindVertexArray(ringVAO);
        glDrawArrays(GL_TRIANGLES, 0, ringVertexCount);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    for (auto& body : bodies) {
        glDeleteVertexArrays(1, &body.VAO);
        glDeleteBuffers(1, &body.VBO);
        glDeleteTextures(1, &body.textureID);
    }
    glDeleteVertexArrays(1, &ringVAO);
    glDeleteBuffers(1, &ringVBO);
    glDeleteTextures(1, &ringTexture);

    glDeleteTextures(1, &backgroundTexture);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);


    glDeleteProgram(shaderProgram);
    glfwTerminate();

    return 0;
}