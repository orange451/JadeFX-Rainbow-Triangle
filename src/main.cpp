#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Renderer.hpp"
#include "gl.hpp"
#include "jadefx/jadefx.hpp"

#include <cstdio>

namespace {

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 640;
constexpr int kMinWidth = 320;
constexpr int kMinHeight = 240;
constexpr const char* kTitle = "JadeFX-Rainbow-Triangle";

char g_glfw_error[512] = {};

void CaptureGlfwError(int code, const char* description) {
    std::snprintf(g_glfw_error, sizeof g_glfw_error, "%s", description != nullptr ? description : "");
}

void SetCoreProfileHints() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // macOS only creates an OpenGL 3.2+ core context when this hint is set.
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#endif
}

GLFWwindow* CreateWindow() {
    g_glfw_error[0] = '\0';
    SetCoreProfileHints();
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, kTitle, nullptr, nullptr);
    if (window != nullptr) {
        return window;
    }

    std::fprintf(stderr, "4x multisampling is unavailable (%s). Retrying without it.\n", g_glfw_error);
    g_glfw_error[0] = '\0';
    glfwWindowHint(GLFW_SAMPLES, 0);
    window = glfwCreateWindow(kWindowWidth, kWindowHeight, kTitle, nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "Could not create an OpenGL 4.1 core window (%s).\n", g_glfw_error);
    }
    return window;
}

void OnKey(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

// Cocoa and Win32 do not return from glfwPollEvents while the pointer is dragging
// the window border. The frame has to be drawn from the callbacks that nested loop
// already invokes, or the window stops updating until the drag ends.
struct FrameState {
    GLFWwindow* window = nullptr;
    Renderer* renderer = nullptr;
    jadefx::Stage* stage = nullptr;
    jadefx::Label* fpsLabel = nullptr;
    bool drawing = false;
    int frameCount = 0;
    double reportAt = 0.0;
};

void NotePresentedFrame(FrameState& state) {
    ++state.frameCount;
    if (state.fpsLabel == nullptr) {
        return;
    }
    const double now = glfwGetTime();
    const double elapsed = now - state.reportAt;
    if (elapsed < 1.0) {
        return;
    }
    char text[32];
    std::snprintf(text, sizeof text, "FPS: %.1f", static_cast<double>(state.frameCount) / elapsed);
    state.fpsLabel->setText(text);
    std::printf("%s\n", text);
    std::fflush(stdout);
    state.frameCount = 0;
    state.reportAt = now;
}

void DrawFrame(FrameState& state) {
    if (state.drawing || state.window == nullptr || state.renderer == nullptr) {
        return;
    }
    state.drawing = true;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(state.window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth > 0 && framebufferHeight > 0) {
        int pointWidth = 0;
        int pointHeight = 0;
        glfwGetWindowSize(state.window, &pointWidth, &pointHeight);
        state.renderer->draw(framebufferWidth, framebufferHeight);
        if (state.stage != nullptr && pointWidth > 0 && pointHeight > 0) {
            state.stage->frame(pointWidth, pointHeight, framebufferWidth, framebufferHeight);
        }
        glfwSwapBuffers(state.window);
        NotePresentedFrame(state);
    }
    state.drawing = false;
}

FrameState* StateOf(GLFWwindow* window) {
    return static_cast<FrameState*>(glfwGetWindowUserPointer(window));
}

void OnContentChange(GLFWwindow* window, int, int) {
    if (FrameState* state = StateOf(window)) {
        DrawFrame(*state);
    }
}

void OnRefresh(GLFWwindow* window) {
    if (FrameState* state = StateOf(window)) {
        DrawFrame(*state);
    }
}

}  // namespace

int main() {
    glfwSetErrorCallback(CaptureGlfwError);
    if (glfwInit() != GLFW_TRUE) {
        std::fprintf(stderr, "glfwInit failed (%s).\n", g_glfw_error);
        return 1;
    }

    GLFWwindow* window = CreateWindow();
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }

    glfwSetWindowSizeLimits(window, kMinWidth, kMinHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetKeyCallback(window, OnKey);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    const auto proc_address = [](const char* name) -> void* {
        return reinterpret_cast<void*>(glfwGetProcAddress(name));
    };
    if (!LoadGl(proc_address)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    Renderer renderer;
    if (!renderer.initialize()) {
        std::fprintf(stderr, "OpenGL setup failed.\n");
        renderer.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    auto fpsLabel = jadefx::make<jadefx::Label>("FPS: --");
    fpsLabel->setFont(jadefx::Font("Open Sans", 18.f));
    fpsLabel->setTextFill(jadefx::Color::white());
    fpsLabel->setSubpixelRendering(false);

    jadefx::Stage stage;
    // The triangle already filled the framebuffer. Leave it there and draw the label on top.
    stage.setClearsColor(false);
    if (!stage.initializeGraphics(proc_address)) {
        std::fprintf(stderr, "OpenGL setup failed.\n");
        stage.shutdownGraphics();
        renderer.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    stage.getScene().setRoot(fpsLabel);
    stage.getScene().setAlignment(jadefx::Pos::TopLeft);
    stage.getScene().setPadding(jadefx::Insets::uniform(12));
    stage.getScene().setBackground(jadefx::Color::transparent());

    FrameState frame;
    frame.window = window;
    frame.renderer = &renderer;
    frame.stage = &stage;
    frame.fpsLabel = fpsLabel.get();
    frame.reportAt = glfwGetTime();
    glfwSetWindowUserPointer(window, &frame);
    glfwSetWindowSizeCallback(window, OnContentChange);
    glfwSetFramebufferSizeCallback(window, OnContentChange);
    glfwSetWindowRefreshCallback(window, OnRefresh);

    while (glfwWindowShouldClose(window) != GLFW_TRUE) {
        glfwPollEvents();
        if (glfwWindowShouldClose(window) == GLFW_TRUE) {
            break;
        }
        DrawFrame(frame);
    }

    stage.shutdownGraphics();
    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
