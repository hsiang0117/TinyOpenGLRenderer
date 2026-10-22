#pragma once
#include "rendering/clouds/cloudVolume.hpp"
#include "rendering/opengl/glBuffer.hpp"
#include "rendering/opengl/texture.hpp"
#include <array>
#include <vector>

namespace togr {

class Shader;

// Owns cloud history only; the raw raymarch color remains owned by RenderSystem.
// All allocation, resize, resolve and destruction require a current GL context.
class CloudTemporal {
public:
    struct Frame {
        glm::mat4 vp{1}, invVP{1}, projection{1};
        glm::vec3 cameraPos{0}, cameraFront{0, 0, -1}, lightDir{0, 1, 0}, lightColor{1};
        float seconds = 0;
        const void* scene = nullptr;
        GLuint raymarchProgram = 0, resolveProgram = 0;
        std::array<GLuint, 3> noiseTextures{};
        std::vector<CloudVolumeGpu> volumes;
    };

    void init(int width, int height);
    void resize(int width, int height);
    void destroy();
    void invalidate();
    void beginFrame(const Frame& frame, bool enabled);
    Texture2D& resolve(Texture2D& rawColor, Shader& shader);
    Texture2D& currentDepth() { return currentDepth_; }
    bool active() const { return active_; }
    bool historyUsable() const { return usable_; }
    float jitterOffset() const;

private:
    Texture2D currentDepth_, color_[2], depth_[2];
    FrameBuffer fbo_[2];
    int width_ = 0, height_ = 0, read_ = 0;
    unsigned sequence_ = 0;
    bool valid_ = false, active_ = false, usable_ = false;
    Frame current_, previous_;
};

} // namespace togr
