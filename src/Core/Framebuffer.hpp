#pragma once
#include <glad/gl.h>

class Framebuffer
{
public:
    GLuint fbo = 0;
    GLuint textureColor = 0;
    GLuint rbo = 0;
    int width = 0;
    int height = 0;

    Framebuffer() = default;
    ~Framebuffer();

    bool init(int w, int h);
    void resize(int newWidth, int newHeight);
    void bind() const;
    void unbind() const;
    void cleanup();
};

