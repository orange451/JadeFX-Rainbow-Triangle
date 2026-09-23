#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Renderer.hpp"
#include "gl.hpp"
#include "jadefx/jadefx.hpp"

#include <cmath>
#include <cstdio>

namespace {

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 640;
constexpr int kMinWidth = 320;
constexpr int kMinHeight = 240;
constexpr const char* kTitle = "JadeFX-Rainbow-Triangle";
constexpr double kSpinDegreesPerSecond = 90.0;

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

struct FrameState;

FrameState* StateOf(GLFWwindow* window) {
    return static_cast<FrameState*>(glfwGetWindowUserPointer(window));
}

void OnKey(GLFWwindow* window, int key, int /*scancode*/, int action, int mods);

// Cocoa and Win32 do not return from glfwPollEvents while the pointer is dragging
// the window border. The frame has to be drawn from the callbacks that nested loop
// already invokes, or the window stops updating until the drag ends.
struct FrameState {
    GLFWwindow* window = nullptr;
    Renderer* renderer = nullptr;
    jadefx::Stage* stage = nullptr;
    jadefx::Label* fpsLabel = nullptr;
    jadefx::CheckBox* spinBox = nullptr;
    jadefx::Slider* angleSlider = nullptr;
    bool writingSlider = false;
    double angleDeg = 0.0;
    double spinClock = 0.0;
    bool drawing = false;
    int frameCount = 0;
    double reportAt = 0.0;
};

void OnAngleEdited(FrameState& state) {
    if (state.writingSlider || state.angleSlider == nullptr) {
        return;
    }
    state.angleDeg = state.angleSlider->getValue();
}

void AdvanceAngle(FrameState& state) {
    const double now = glfwGetTime();
    double dt = now - state.spinClock;
    state.spinClock = now;
    if (dt < 0.0) {
        dt = 0.0;
    }
    if (dt > 0.1) {
        dt = 0.1;
    }

    const bool dragging = state.angleSlider != nullptr && state.angleSlider->isValueChanging();
    const bool spin = state.spinBox != nullptr && state.spinBox->isSelected();
    if (!spin || dragging) {
        return;
    }
    state.angleDeg = std::fmod(state.angleDeg + dt * kSpinDegreesPerSecond, 360.0);
    if (state.angleDeg < 0.0) {
        state.angleDeg += 360.0;
    }
    if (state.angleSlider != nullptr) {
        state.writingSlider = true;
        state.angleSlider->setValue(state.angleDeg);
        state.writingSlider = false;
    }
}

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
        AdvanceAngle(state);
        state.renderer->draw(framebufferWidth, framebufferHeight, static_cast<float>(state.angleDeg));
        if (state.stage != nullptr && pointWidth > 0 && pointHeight > 0) {
            state.stage->frame(pointWidth, pointHeight, framebufferWidth, framebufferHeight);
        }
        glfwSwapBuffers(state.window);
        NotePresentedFrame(state);
    }
    state.drawing = false;
}

void OnKey(GLFWwindow* window, int key, int /*scancode*/, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (FrameState* state = StateOf(window); state != nullptr && state->stage != nullptr) {
        state->stage->pushKey(key, action != GLFW_RELEASE, mods, action == GLFW_REPEAT);
    }
}

void OnMouseMove(GLFWwindow* window, double x, double y) {
    if (FrameState* state = StateOf(window); state != nullptr && state->stage != nullptr) {
        state->stage->pushMove(x, y);
    }
}

void OnMouseButton(GLFWwindow* window, int button, int action, int /*mods*/) {
    FrameState* state = StateOf(window);
    if (state == nullptr || state->stage == nullptr) {
        return;
    }
    double x = 0;
    double y = 0;
    glfwGetCursorPos(window, &x, &y);
    state->stage->pushButton(button, action == GLFW_PRESS, x, y);
}

void OnScroll(GLFWwindow* window, double dx, double dy) {
    FrameState* state = StateOf(window);
    if (state == nullptr || state->stage == nullptr) {
        return;
    }
    double x = 0;
    double y = 0;
    glfwGetCursorPos(window, &x, &y);
    state->stage->pushScroll(x, y, dx, dy);
}

void OnCursorEnter(GLFWwindow* window, int entered) {
    FrameState* state = StateOf(window);
    if (state == nullptr || state->stage == nullptr) {
        return;
    }
    if (entered == GLFW_FALSE) {
        state->stage->pushPointerExit();
        return;
    }
    double x = 0;
    double y = 0;
    glfwGetCursorPos(window, &x, &y);
    state->stage->pushMove(x, y);
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
    fpsLabel->setAlignment(jadefx::Pos::CenterLeft);
    fpsLabel->setSubpixelRendering(false);

    auto hello = jadefx::make<jadefx::Label>("Hello World!");
    auto drawn = jadefx::make<jadefx::Label>("OpenGL drawn straight to window.");
    auto onTop = jadefx::make<jadefx::Label>("JadeFX ontop!");
    const jadefx::Font captionFont("Open Sans", 26.f);
    for (jadefx::Label* caption : {hello.get(), drawn.get(), onTop.get()}) {
        caption->setFont(captionFont);
        caption->setAlignment(jadefx::Pos::Center);
        caption->setSubpixelRendering(false);
    }
    hello->setTextFill(jadefx::Color::rgb8(126, 198, 255));
    drawn->setTextFill(jadefx::Color::white());
    onTop->setTextFill(jadefx::Color::rgb8(255, 72, 72));

    auto column = jadefx::make<jadefx::VBox>();
    column->setAlignment(jadefx::Pos::Center);
    column->setSpacing(4);
    column->setPadding(jadefx::Insets::uniform(18));
    column->setBackground(jadefx::Color::rgba(0.10f, 0.20f, 0.62f, 0.72f));
    column->getChildren().add(hello);
    column->getChildren().add(drawn);
    column->getChildren().add(onTop);

    auto center = jadefx::make<jadefx::StackPane>();
    center->setBackground(jadefx::Color::transparent());
    center->setAlignment(jadefx::Pos::Center);
    center->getChildren().add(column);

    auto spinBox = jadefx::make<jadefx::CheckBox>("Spin");
    spinBox->setSelected(true);
    spinBox->setFont(jadefx::Font("Open Sans", 18.f));
    spinBox->setTextFill(jadefx::Color::white());

    auto angleSlider = jadefx::make<jadefx::Slider>(0.0, 360.0, 0.0);
    angleSlider->setPrefWidth(280);

    auto controls = jadefx::make<jadefx::HBox>();
    controls->setAlignment(jadefx::Pos::Center);
    controls->setSpacing(12);
    controls->setBackground(jadefx::Color::transparent());
    controls->getChildren().add(spinBox);
    controls->getChildren().add(angleSlider);

    auto root = jadefx::make<jadefx::BorderPane>();
    root->setBackground(jadefx::Color::transparent());
    // The scene centers a root at its preferred size. Fill the window so the
    // labels stay in the middle and the spin controls sit on the bottom edge.
    root->setPrefWidthRatio(1);
    root->setPrefHeightRatio(1);
    root->setTop(fpsLabel);
    root->setCenter(center);
    root->setBottom(controls);

    jadefx::Stage stage;
    // The triangle already filled the framebuffer. Leave it there and draw the UI on top.
    stage.setClearsColor(false);
    if (!stage.initializeGraphics(proc_address)) {
        std::fprintf(stderr, "OpenGL setup failed.\n");
        stage.shutdownGraphics();
        renderer.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    stage.getScene().setRoot(root);
    stage.getScene().setPadding(jadefx::Insets::uniform(16));
    stage.getScene().setBackground(jadefx::Color::transparent());

    FrameState frame;
    frame.window = window;
    frame.renderer = &renderer;
    frame.stage = &stage;
    frame.fpsLabel = fpsLabel.get();
    frame.spinBox = spinBox.get();
    frame.angleSlider = angleSlider.get();
    frame.spinClock = glfwGetTime();
    frame.reportAt = frame.spinClock;
    angleSlider->setOnValueChanged([&frame] { OnAngleEdited(frame); });
    glfwSetWindowUserPointer(window, &frame);
    glfwSetCursorPosCallback(window, OnMouseMove);
    glfwSetMouseButtonCallback(window, OnMouseButton);
    glfwSetScrollCallback(window, OnScroll);
    glfwSetCursorEnterCallback(window, OnCursorEnter);
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

    angleSlider->setOnValueChanged(nullptr);
    stage.shutdownGraphics();
    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
