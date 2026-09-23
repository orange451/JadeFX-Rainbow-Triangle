#pragma once

// Draws one triangle whose corners are red, green, and blue.
// The rasterizer interpolates those colors across the face, which is the rainbow.
class Renderer {
public:
    bool initialize();
    void draw(int framebufferWidth, int framebufferHeight);
    void shutdown();

private:
    unsigned program_ = 0;
    unsigned vao_ = 0;
    unsigned vbo_ = 0;
    bool ready_ = false;
};
