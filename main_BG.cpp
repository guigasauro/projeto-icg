#include "libs.h"
#include "physics_real.h"
#include "shader_BG.h"


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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
    unsigned char* bgData = stbi_load("assets/2k_stars.jpg", &bgWidth, &bgHeight, &bgChannels, 0);
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
    double ringInner = 7e6 / radiusScale; 
    double ringOuter = 1.1e7 / radiusScale;
    std::vector<float> ringVertices = createTorusRing(
        (ringInner + ringOuter)/2.0,  // Raio principal
        (ringOuter - ringInner)/2.0,  // Raio do tubo
        10000,  // Segmentos principais
        2    // Segmentos do tubo
    );
    int ringVertexCount = ringVertices.size() / 5;
    
    // Carregue a textura dos anéis
    GLuint ringTexture;
    glGenTextures(1, &ringTexture);
    glBindTexture(GL_TEXTURE_2D, ringTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int width, height, nrChannels;
    unsigned char* data = stbi_load("assets/2k_saturn_ring_alpha.png", &width, &height, &nrChannels, 0);
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

            glm::vec3 cameraPosition = targetPos + glm::vec3(
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

            glm::vec3 cameraPosition(
                cameraDistance * sin(cameraAngle),
                cameraHeight,
                cameraDistance * cos(cameraAngle)
            );
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
            glBindTexture(GL_TEXTURE_2D, bodies[i].textureID);
            glBindVertexArray(bodies[i].VAO);
            glDrawArrays(GL_TRIANGLES, 0, bodies[i].vertexCount);
        }
        
        glm::mat4 ringModel = glm::mat4(1.0f);
        glm::vec3 satPos = glm::vec3(bodies[6].position / positionScale);
        ringModel = glm::translate(ringModel, satPos);
        ringModel = glm::rotate(ringModel, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // Inclinação de Saturno
        ringModel = glm::rotate(ringModel, glm::radians(-26.73f), glm::vec3(0.0f, 0.0f, 1.0f)); // Inclinação axial de Saturno (26.73°)
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(ringModel));
        
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