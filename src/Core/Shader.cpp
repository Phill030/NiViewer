#include "Shader.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

Shader::Shader(const char* vertexSource, const char* fragmentSource) {
    init(vertexSource, fragmentSource);
}

Shader::~Shader() {
    if (id != 0) {
        glDeleteProgram(id);
    }
}

bool Shader::init(const char* vertexSource, const char* fragmentSource) {
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertexSource, nullptr);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragmentSource, nullptr);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    id = glCreateProgram();
    glAttachShader(id, vertex);
    glAttachShader(id, fragment);
    glLinkProgram(id);
    checkCompileErrors(id, "PROGRAM");

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return id != 0;
}

void Shader::use() const {
    glUseProgram(id);
}

GLint Shader::getLoc(const char* name) const {
    auto it = locationCache.find(name);
    if (it != locationCache.end()) return it->second;
    GLint loc = glGetUniformLocation(id, name);
    locationCache[name] = loc;
    return loc;
}

void Shader::setMat4(const char* name, const glm::mat4& mat) const {
    glUniformMatrix4fv(getLoc(name), 1, GL_FALSE, glm::value_ptr(mat));
}
void Shader::setMat4(const std::string& name, const glm::mat4& mat) const {
    setMat4(name.c_str(), mat);
}

void Shader::setVec3(const char* name, const glm::vec3& value) const {
    glUniform3fv(getLoc(name), 1, glm::value_ptr(value));
}
void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    setVec3(name.c_str(), value);
}

void Shader::setVec4(const char* name, const glm::vec4& value) const {
    glUniform4fv(getLoc(name), 1, glm::value_ptr(value));
}
void Shader::setVec4(const std::string& name, const glm::vec4& value) const {
    setVec4(name.c_str(), value);
}

void Shader::setFloat(const char* name, float value) const {
    glUniform1f(getLoc(name), value);
}
void Shader::setFloat(const std::string& name, float value) const {
    setFloat(name.c_str(), value);
}

void Shader::setInt(const char* name, int value) const {
    glUniform1i(getLoc(name), value);
}
void Shader::setInt(const std::string& name, int value) const {
    setInt(name.c_str(), value);
}

void Shader::checkCompileErrors(GLuint shader, const std::string& type) {
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "SHADER_COMPILATION_ERROR (" << type << "):\n" << infoLog << "\n";
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "PROGRAM_LINKING_ERROR (" << type << "):\n" << infoLog << "\n";
        }
    }
}
