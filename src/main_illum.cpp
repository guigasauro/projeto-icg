#include "headers/libs.h"
#include "headers/physics.h"
#include "headers/shader_illum.h"

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
    
    // Habilita teste de profundidade (Z-buffer)
    glEnable(GL_DEPTH_TEST);

    // Define cor de fundo padrão (preto)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Compila e linka os shaders (vertex e fragment)
    GLuint shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);
    
    // Obtém localizações dos uniforms (model, view, projection, etc.)
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint textureLoc = glGetUniformLocation(shaderProgram, "texture1");

    std::vector<CelestialBody> bodies;
    bodies.reserve(NUM_BODIES);
    
    // Criação do Sol no centro
    bodies.emplace_back(
        glm::dvec3(0.0),
        glm::dvec3(0.0),
        solarSystemData[0].mass,
        solarSystemData[0].radius,
        solarSystemData[0].color,
        solarSystemData[0].textureFile,
        true
    );
    
    // Criação dos planetas e definição no vetor bodies
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

    // Definição da distância máxima para setup da câmera
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

    // Controle de câmera
    int cameraTargetIndex = 0;  // 0-9 = Segue um corpo
    float baseCameraDistance = cameraDistance;
    float cameraFollowDistance = 5.0f;

    glm::vec3 cameraPosition(
        cameraDistance * sin(cameraAngle),
        cameraHeight,
        cameraDistance * cos(cameraAngle)
    );

    // Uniforms "lightPos" e "lightColor": posição e cor da luz (Sol)
    // Uniform "viewPos": posição da câmera para cálculo de especular
    // "isSun": flag para aplicar efeito de emissão no fragment shader
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(glm::vec3(0.0f)));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(glm::vec3(1.0f)));  // Luz branca
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPosition));  // Posição da câmera

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Atualiza física (posições e velocidades dos corpos)
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

        // Renderiza planetas e Sol (com iluminação e texturas)
        glUniform1i(textureLoc, 0); 
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

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Libera buffers, texturas e shaders
    for (auto& body : bodies) {
        glDeleteVertexArrays(1, &body.VAO);
        glDeleteBuffers(1, &body.VBO);
    }
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    
    return 0;
}