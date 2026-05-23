#include "simple_cube_renderer.h"
#include <glad/glad.h>
#include "log/log_macros.h"
#include <cstring>

namespace Entelechy {

static const char* s_vertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* s_fragmentShader = R"(#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

// Cube vertices (8 corners, centered at origin, edge length 1)
static const f32 s_cubeVertices[8 * 3] = {
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
};

// Indices for 12 triangles (6 faces * 2)
static const unsigned int s_cubeIndices[36] = {
    // front
    4, 5, 6, 4, 6, 7,
    // back
    1, 0, 3, 1, 3, 2,
    // left
    0, 4, 7, 0, 7, 3,
    // right
    5, 1, 2, 5, 2, 6,
    // top
    3, 2, 6, 3, 6, 7,
    // bottom
    0, 1, 5, 0, 5, 4,
};

SimpleCubeRenderer::SimpleCubeRenderer() = default;

SimpleCubeRenderer::~SimpleCubeRenderer() {
    if (m_initialized) shutdown();
}

bool SimpleCubeRenderer::compileShader(const char* vertexSrc, const char* fragmentSrc) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexSrc, nullptr);
    glCompileShader(vs);

    GLint success = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vs, sizeof(log), nullptr, log);
        LOG_ERROR(LogCategories::kEngine, "Vertex shader compile error: %s", log);
        glDeleteShader(vs);
        return false;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentSrc, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(fs, sizeof(log), nullptr, log);
        LOG_ERROR(LogCategories::kEngine, "Fragment shader compile error: %s", log);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vs);
    glAttachShader(m_shaderProgram, fs);
    glLinkProgram(m_shaderProgram);

    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(m_shaderProgram, sizeof(log), nullptr, log);
        LOG_ERROR(LogCategories::kEngine, "Shader link error: %s", log);
        glDeleteProgram(m_shaderProgram);
        glDeleteShader(vs);
        glDeleteShader(fs);
        m_shaderProgram = 0;
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    m_mvpLoc = glGetUniformLocation(m_shaderProgram, "uMVP");
    m_colorLoc = glGetUniformLocation(m_shaderProgram, "uColor");
    return true;
}

bool SimpleCubeRenderer::init() {
    if (m_initialized) return true;

    if (!compileShader(s_vertexShader, s_fragmentShader)) {
        return false;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_cubeVertices), s_cubeVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(s_cubeIndices), s_cubeIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    m_initialized = true;
    LOG_INFO(LogCategories::kEngine, "SimpleCubeRenderer initialized");
    return true;
}

void SimpleCubeRenderer::shutdown() {
    if (!m_initialized) return;
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(1, &m_ebo);
    glDeleteProgram(m_shaderProgram);
    m_vao = m_vbo = m_ebo = m_shaderProgram = 0;
    m_initialized = false;
}

void SimpleCubeRenderer::drawCube(const Mat4& mvp, const Vec3& color) {
    if (!m_initialized) return;

    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_mvpLoc, 1, GL_FALSE, mvp.m);
    glUniform3f(m_colorLoc, color.x, color.y, color.z);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void SimpleCubeRenderer::drawCube(const Mat4& world, const Mat4& view, const Mat4& proj, const Vec3& color) {
    drawCube(proj * view * world, color);
}

} // namespace Entelechy
