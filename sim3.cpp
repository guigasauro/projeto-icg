#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>

const int NUM_BODIES = 9;

// Shader sources
const char* vertexShaderSource = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;  // Adicionado: normais para iluminação

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  // Transforma normais para espaço de mundo
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 330 core
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec4 objectColor;
uniform bool isSun;          // Identifica se é o Sol
uniform vec3 lightPos;       // Posição da luz (Sol)
uniform vec3 lightColor;     // Cor da luz (branca)
uniform vec3 viewPos;        // Posição da câmera

void main() {
    if (isSun) {
        // Efeito de emissão para o Sol (brilho próprio)
        FragColor = vec4(objectColor.rgb * 2.0, objectColor.a);  // Intensidade aumentada
        return;
    }

    // Iluminação Phong para planetas
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);

    // Componentes da iluminação
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    float specularStrength = 0.5;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    // Combina as componentes
    vec3 result = (ambient + diffuse + specular) * objectColor.rgb;
    FragColor = vec4(result, objectColor.a);
}
)glsl";

// Physics constants
const double G = 6.67430e-11;
const double timeStep = 43200.0;     // 12 hours in seconds (0.5 Earth day)
const double positionScale = 5e10;
const double radiusScale = 150;
const int STACKS = 30;
const int SECTORS = 30;

struct CelestialBody {
    GLuint VAO, VBO;
    glm::dvec3 position;
    glm::dvec3 velocity;
    double mass;
    double radius;
    glm::vec4 color;
    size_t vertexCount;
    bool isSun;

    CelestialBody(const glm::dvec3& pos, const glm::dvec3& vel,
                 double m, double realRadius, const glm::vec4& col, bool sun = false)
        : position(pos), velocity(vel), mass(m), color(col), isSun(sun) {
        radius = std::cbrt(realRadius) / radiusScale;
        auto vertices = createSphere(radius);
        vertexCount = vertices.size() / 3;
        
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
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
                
                glm::vec3 v1(radius * sin(theta1) * cos(phi1), radius * cos(theta1), radius * sin(theta1) * sin(phi1));
                glm::vec3 v2(radius * sin(theta1) * cos(phi2), radius * cos(theta1), radius * sin(theta1) * sin(phi2));
                glm::vec3 v3(radius * sin(theta2) * cos(phi1), radius * cos(theta2), radius * sin(theta2) * sin(phi1));
                glm::vec3 v4(radius * sin(theta2) * cos(phi2), radius * cos(theta2), radius * sin(theta2) * sin(phi2));
                
                auto addVertex = [&](const glm::vec3& v) {

                    glm::vec3 n = glm::normalize(v);
                    vertices.push_back(v.x);
                    vertices.push_back(v.y);
                    vertices.push_back(v.z);

                    vertices.push_back(n.x);
                    vertices.push_back(n.y);
                    vertices.push_back(n.z);
                };
                
                addVertex(v1); addVertex(v2); addVertex(v3);
                addVertex(v2); addVertex(v4); addVertex(v3);
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

    GLuint shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);
    
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint colorLoc = glGetUniformLocation(shaderProgram, "objectColor");

    struct BodyData {
        double mass;
        double orbitRadius;
        double radius;
        glm::vec4 color;
        double inclination;
    };
    
    std::vector<BodyData> solarSystemData = {
        {1.98847e30,    0.0,        7.9634e7, {1.0f, 0.8f, 0.0f, 1.0f}, 0.0}, // Sun
        {3.3011e23,  5.4e11,    2.4397e5, {0.8f, 0.5f, 0.2f, 1.0f}, 7.0},  // Mercury
        {4.8675e24, 7e11,    6.0518e5, {0.9f, 0.7f, 0.2f, 1.0f}, 3.4},  // Venus
        {5.9724e24, 11e11,    6.3710e5, {0.0f, 0.5f, 1.0f, 1.0f}, 0.0},  // Earth
        {6.4171e23, 15e11,    3.3895e5, {1.0f, 0.2f, 0.1f, 1.0f}, 1.9},  // Mars
        {1.8982e27, 23e11,   1e7, {0.9f, 0.6f, 0.3f, 1.0f}, 1.3},  // Jupiter
        {5.6834e26, 28e11,   4.8232e6, {0.9f, 0.8f, 0.5f, 1.0f}, 2.5},  // Saturn
        {8.6810e25, 37e11,   2.5362e6, {0.5f, 0.8f, 0.9f, 1.0f}, 0.8},  // Uranus
        {1.02413e26, 4.503e12,  2.4622e6, {0.3f, 0.4f, 0.9f, 1.0f}, 1.8}   // Neptune
    };

    std::vector<CelestialBody> bodies;
    bodies.reserve(NUM_BODIES);
    
    // Create Sun at center
    bodies.emplace_back(
        glm::dvec3(0.0),
        glm::dvec3(0.0),
        solarSystemData[0].mass,
        solarSystemData[0].radius,
        solarSystemData[0].color,
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
            solarSystemData[i].color
        );
    }

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
    int cameraTargetIndex = 0;  // -1 = free camera, 0-8 = follow body
    float baseCameraDistance = cameraDistance;
    float cameraFollowDistance = 5.0f;

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

        glm::vec3 cameraPosition(
            cameraDistance * sin(cameraAngle),
            cameraHeight,
            cameraDistance * cos(cameraAngle)
        );

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
            
            glm::mat4 viewMatrix = glm::lookAt(cameraPosition, cameraTarget, cameraUp);
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
            
            glm::mat4 viewMatrix = glm::lookAt(cameraPosition, cameraTarget, cameraUp);
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
        }

        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(glm::vec3(0.0f)));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(glm::vec3(1.0f)));  // Luz branca
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPosition));  // Posição da câmera

        // Render all bodies
        for (size_t i = 0; i < bodies.size(); ++i) {
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            
            if (!bodies[i].isSun) {
                glm::vec3 scaledPosition = glm::vec3(bodies[i].position / positionScale);
                modelMatrix = glm::translate(modelMatrix, scaledPosition);
            }
            
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
            glUniform4fv(colorLoc, 1, glm::value_ptr(bodies[i].color));

            glUniform1i(glGetUniformLocation(shaderProgram, "isSun"), bodies[i].isSun);

            glBindVertexArray(bodies[i].VAO);
            glDrawArrays(GL_TRIANGLES, 0, bodies[i].vertexCount);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    for (auto& body : bodies) {
        glDeleteVertexArrays(1, &body.VAO);
        glDeleteBuffers(1, &body.VBO);
    }
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    
    return 0;
}