#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

class Shader
{
public:
    GLuint id = 0;
    mutable std::unordered_map<std::string, GLint> locationCache;

    Shader() = default;
    Shader(const char* vertexSource, const char* fragmentSource);
    ~Shader();

    bool init(const char* vertexSource, const char* fragmentSource);
    void use() const;

    GLint getLoc(const char* name) const;

    void setMat4(const char* name, const glm::mat4& mat) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;

    void setVec3(const char* name, const glm::vec3& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;

    void setVec4(const char* name, const glm::vec4& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;

    void setFloat(const char* name, float value) const;
    void setFloat(const std::string& name, float value) const;

    void setInt(const char* name, int value) const;
    void setInt(const std::string& name, int value) const;

private:
    void checkCompileErrors(GLuint shader, const std::string& type);
};

