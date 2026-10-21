#include "rendering/clouds/cloudTemporal.hpp"
#include "rendering/opengl/shader.hpp"
#include "rendering/renderUtils.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
bool equal(const glm::mat4& a, const glm::mat4& b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
bool equal(const glm::vec3& a, const glm::vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool equal(const std::vector<CloudVolumeGpu>& a, const std::vector<CloudVolumeGpu>& b) {
    return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(CloudVolumeGpu)) == 0);
}
}

void CloudTemporal::init(int width, int height) {
    destroy();
    width_ = std::max(width, 1); height_ = std::max(height, 1);
    // Full precision: ray distance can be much larger than half-float range, and
    // the opaque-sky sentinel must survive allocation/readback without infinity.
    currentDepth_ = Texture2D(width_, height_, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RG32F, GL_RG, GL_FLOAT);
    for (int i = 0; i < 2; ++i) {
        color_[i] = Texture2D(width_, height_, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
        depth_[i] = Texture2D(width_, height_, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RG32F, GL_RG, GL_FLOAT);
        fbo_[i].init(); fbo_[i].bind();
        fbo_[i].attachTexture2D(color_[i], GL_COLOR_ATTACHMENT0);
        fbo_[i].attachTexture2D(depth_[i], GL_COLOR_ATTACHMENT1);
        fbo_[i].drawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});
        fbo_[i].unbind();
    }
}

void CloudTemporal::resize(int width, int height) {
    width = std::max(width, 1); height = std::max(height, 1);
    if (width == width_ && height == height_) return;
    if (!currentDepth_.ID) { init(width, height); return; }
    width_ = width; height_ = height;
    currentDepth_.resetSize(width, height);
    for (int i = 0; i < 2; ++i) { color_[i].resetSize(width, height); depth_[i].resetSize(width, height); }
    invalidate();
}

void CloudTemporal::destroy() {
    for (int i = 0; i < 2; ++i) { fbo_[i].destroy(); color_[i].destroy(); depth_[i].destroy(); }
    currentDepth_.destroy(); width_ = height_ = 0; read_ = 0;
    invalidate(); current_ = {}; previous_ = {};
}

void CloudTemporal::invalidate() { valid_ = active_ = usable_ = false; sequence_ = 0; }

void CloudTemporal::beginFrame(const Frame& frame, bool enabled) {
    current_ = frame;
    // A single reprojected position has one velocity. Different winds or local
    // edge masks cannot share that motion model, so they use the raw marcher.
    enabled = enabled && currentDepth_.ID && !frame.volumes.empty();
    if (enabled) for (const auto& v : frame.volumes) {
        if (!v.skyLayer || v.windVelocity.y != 0 || !equal(v.windVelocity, frame.volumes[0].windVelocity)) enabled = false;
    }
    if (!enabled) { invalidate(); return; }
    const float dt = frame.seconds - previous_.seconds;
    usable_ = valid_ && dt >= 0 && dt <= 0.5f && frame.scene == previous_.scene &&
        equal(frame.volumes, previous_.volumes) && equal(frame.projection, previous_.projection) &&
        equal(frame.lightDir, previous_.lightDir) && equal(frame.lightColor, previous_.lightColor) &&
        frame.raymarchProgram == previous_.raymarchProgram && frame.resolveProgram == previous_.resolveProgram &&
        frame.noiseTextures == previous_.noiseTextures && glm::distance(frame.cameraPos, previous_.cameraPos) <= 250.0f &&
        glm::dot(frame.cameraFront, previous_.cameraFront) >= 0.94f;
    if (!usable_) { valid_ = false; sequence_ = 0; }
    active_ = true;
}

float CloudTemporal::jitterOffset() const {
    return active_ ? std::fmod(float(sequence_ % 1024) * 0.61803398875f, 1.0f) : 0.0f;
}

Texture2D& CloudTemporal::resolve(Texture2D& rawColor, Shader& shader) {
    if (!active_) return rawColor;
    const int write = 1 - read_;
    fbo_[write].bind(); glViewport(0, 0, width_, height_);
    shader.use();
    shader.setInt("currentColor", 0); shader.setInt("currentDepth", 1);
    shader.setInt("historyColor", 2); shader.setInt("historyDepth", 3);
    shader.setInt("historyValid", usable_ ? 1 : 0);
    shader.setMat4("invVP", current_.invVP); shader.setMat4("currentVP", current_.vp); shader.setMat4("previousVP", previous_.vp);
    shader.setVec3("cameraPos", current_.cameraPos); shader.setVec3("cameraDisplacement", current_.cameraPos - previous_.cameraPos);
    shader.setVec3("windDisplacement", current_.volumes[0].windVelocity * (current_.seconds - previous_.seconds));
    rawColor.use(GL_TEXTURE0); currentDepth_.use(GL_TEXTURE1);
    color_[read_].use(GL_TEXTURE2); depth_[read_].use(GL_TEXTURE3);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
    RenderHelpers::drawFullscreenQuad();
    glEnable(GL_DEPTH_TEST); fbo_[write].unbind();
    read_ = write; previous_ = current_; valid_ = true; ++sequence_;
    return color_[read_];
}
