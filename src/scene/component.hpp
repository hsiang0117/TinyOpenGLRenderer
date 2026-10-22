#ifndef TOGR_COMPONENT_HPP
#define TOGR_COMPONENT_HPP
#pragma once

#include "animation/animation.hpp"
#include "rendering/material.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace togr {

class Mesh;
class Model;
class Skybox;
class Shader;
class Texture2D;

// Model-local bounds derived when a mesh/model is assigned to a component.
struct AxisAlignedBounds {
    glm::vec3 min;
    glm::vec3 max;
};

class Component {
public:
    Component(std::string name) : name(name) {}
    virtual ~Component() {}
    std::string getName() const;
private:
    std::string name;
};

class Transform : public Component {
public:
    Transform() : Component("Transform") {
        translate = glm::vec3(0.0f);
        scale = glm::vec3(1.0f);
        rotate = glm::vec3(0.0f);
    }

    glm::vec3 translate;
    glm::vec3 scale;
    glm::vec3 rotate;
};

class ModelRenderComponent : public Component {
public:
    ModelRenderComponent() : Component("ModelRenderComponent") {}

    // Requires a non-null model. Shares the resource and rebuilds bind-pose bounds.
    // Resource replacement must pass through this method to keep bounds in sync.
    // Keep GL current if replacement releases the last owner of an uploaded model.
    void setModel(std::shared_ptr<Model> model);

    // Borrowed component state. Bounds are valid after setModel(); copy the
    // shared owner if the resource must survive a later component replacement.
    const std::shared_ptr<Model>& getModel() const { return model_; }
    const AxisAlignedBounds& getBounds() const { return bounds_; }

private:
    std::shared_ptr<Model> model_;
    AxisAlignedBounds bounds_;
};

class PointLightComponent : public Component {
public:
    PointLightComponent() : Component("PointLightComponent") {
        color = glm::vec3(1.0f);
        brightness = 1.0f;
        constant = 1.0f;
        linear = 0.009f;
        quadratic = 0.032f;
    }

    glm::vec3 color;
    float brightness;
    float constant;
    float linear;
    float quadratic;

    static constexpr int glslSize = 48;
};

class DirectionalLightComponent : public Component {
public:
    DirectionalLightComponent() : Component("DirectionalLightComponent") {
        color = glm::vec3(1.0f);
        brightness = 1.0f;
    }

    glm::vec3 color;
    float brightness;

    static constexpr int glslSize = 32;
};

class SpotLightComponent : public Component {
public:
    SpotLightComponent() : Component("SpotLightComponent") {
        color = glm::vec3(1.0f);
        cutOff = 12.5f;
        outerCutOff = 17.5f;
        brightness = 1.0f;
    }

    glm::vec3 color;
    float brightness;
    float cutOff;
    float outerCutOff;

    static constexpr int glslSize = 64;
};

class CloudVolumeComponent : public Component {
public:
    enum class Mode { SkyLayer, LocalVolume };
    enum class Preset { ScatteredCumulus, BrokenStratocumulus, Overcast };
    Mode mode = Mode::SkyLayer;
    float layerBottom = 100.0f;         // world-space altitude, independent of Transform
    float layerThickness = 200.0f;
    float maxTraceDistance = 10000.0f;  // view/light ray budget in world units
    float distanceFadeStart = 8000.0f;  // view-only fade to the sky at the tracing limit
    float weatherScale;                // individual cloud coverage frequency
    // Broad sky weather; independent of the cloud body and local weather texture.
    float regionScale;                 // inverse world units; smaller values make broader regions
    float regionCoverage;              // large-scale cloud/clear-sky distribution, in [0,1]
    float densityVariation;            // regional density variation, in [0,1]
    float heightVariation;             // maximum fractional reduction of local layer thickness, in [0,0.8]

    CloudVolumeComponent();
    // Changes appearance only; preserves altitude, bounds, animation and quality.
    void applyPreset(Preset preset);

    // Shape
    float cloudType;              // 0=stratus, 0.5=stratocumulus, 1=cumulus
    // Coverage (world weather for sky layers; weather texture for local volumes)
    float coverageLow;
    float coverageHigh;
    // Noise sampling scales (UV multipliers into the pre-generated noise textures)
    float baseNoiseScale;
    float warpScale;
    float warpStrength;           // maximum X/Z displacement in world units; no Y warp
    float detailScale;
    float detailErosionStrength;
    // Medium and lighting: one extinction field for both view and shadow rays.
    float densityMultiplier;      // scales the medium density, not its brightness
    float extinctionCoefficient;  // inverse world units at density 1
    float scatteringAlbedo;       // sigma_s / sigma_t, in [0,1]
    float ambientStrength;        // diffuse sky illumination
    float sunStrength;            // cloud sun illumination relative to the directional light
    float phaseForwardG;          // HG forward lobe, in [0,0.85]
    float multipleScattering;     // strength of the approximate higher scattering orders
    // Animation
    glm::vec3 windVelocity = glm::vec3(1.0f, 0.0f, 1.0f); // world units/sec; all fields move together
    // Quality / boundary
    float edgeFadeDistance = 10.0f; // local-volume boundary only; sky layers have no XZ edge
    float viewStepWorldSize = 2.0f; // target world spacing between view march steps
    int viewMaxSteps = 96;
    int lightSteps = 8;
    int emptyStepMultiplier = 4; // skip only blocks proven to contain no density
    bool temporalAccumulation = true;
};

class SkyboxComponent : public Component {
public:
    SkyboxComponent() : Component("SkyboxComponent") {}

    // UTF-8 editor input; may differ from the applied resource after a failed edit.
    std::string draftFolderPath;

    // Main thread/current GL context. Publishes only a ready candidate; failure
    // preserves the applied skybox and the current draft, and records lastError().
    // Success also synchronizes the draft. Passing draftFolderPath itself is safe.
    bool setSkybox(std::string folderPath);
    // Borrowed slot, initially empty; copy the shared owner to retain a resource.
    const std::shared_ptr<Skybox>& getSkybox() const { return skybox_; }
    const std::string& lastError() const { return lastError_; }

private:
    std::shared_ptr<Skybox> skybox_;
    std::string lastError_;
};

class ShadowCaster2D : public Component {
public:
    ShadowCaster2D() : Component("ShadowCaster2D") {
        enabled = true;
    }

    bool enabled;
};

class ShadowCasterCube : public Component {
public:
    ShadowCasterCube() : Component("ShadowCasterCube") {
        enabled = true;
        farPlane = 25.0f;
    }

    bool enabled;
    float farPlane;
};

class StaticMeshComponent : public Component {
public:
    StaticMeshComponent() : Component("StaticMeshComponent") {}

    // Requires a non-null mesh. Shares the resource and rebuilds local bounds.
    // Keep GL current if replacement releases the last owner of an uploaded mesh.
    void setMesh(std::shared_ptr<Mesh> mesh);

    // Borrowed component state; bounds are valid after setMesh().
    const std::shared_ptr<Mesh>& getMesh() const { return mesh_; }
    const AxisAlignedBounds& getBounds() const { return bounds_; }

private:
    std::shared_ptr<Mesh> mesh_;
    AxisAlignedBounds bounds_;
};

class DynamicMaterialComponent : public Component {
public:
    struct TextureDraft {
        std::string albedoPath;
        std::string specularPath;
        std::string normalPath;
    };

    DynamicMaterialComponent() : Component("DynamicMaterialComponent") {}

    // UTF-8 editor paths. Editing the draft does not modify the applied material.
    TextureDraft draft;

    // Main thread/current GL context. Loads a candidate from the draft, retaining
    // the applied material's ambient/shininess paths. Failure keeps that material
    // usable and leaves the draft available for correction/retry.
    bool applyMaterial();
    // Requires a valid shader and current GL context; default material is usable
    // even before the first successful applyMaterial().
    void bindMaterial(std::shared_ptr<Shader> shader);
    const Material& getMaterial() const { return material_; }
    const std::string& lastError() const { return lastError_; }

private:
    Material material_;
    std::string lastError_;
};

class AnimatorComponent : public Component {
public:
    // Requires a non-null model and retains it for the borrowed hierarchy/clips.
    // Owns instance playback, pose and palette texture; no Viewer is required.
    // Construction/update/destruction require the owning thread's GL context.
    explicit AnimatorComponent(std::shared_ptr<const Model> model);
    std::vector<std::string> getAnimationsNames() const;
    void update(float dt) { animator.updateAnimation(dt); }
    // Resets the selected clip's clock; pose changes on the next update(). An
    // unknown name preserves the selected clip. The returned pose is read-only.
    void playAnimation(std::string name);
    std::string getCurrentAnimation() const;
    const SkeletonPose& getPose() const { return animator.getPose(); }
    Texture2D& getBoneMatrixTexture() { return animator.getBoneMatrixTexture(); }

    // A rendered model must match this binding. Rebuild the component when
    // replacing its model; render helpers skip mismatched model/pose pairs.
    bool isBoundTo(const Model& model) const { return model_.get() == &model; }
    // RenderSystem skips update() when paused, retaining the last sampled pose.
    bool playing;

private:
    // Declared first so the borrowed nodes/clips outlive Animator destruction.
    std::shared_ptr<const Model> model_;
    Animator animator;
};

class SkeletonViewerComponent : public Component {
public:
    // Owns only visibility and debug GL buffers. Construction, drawing, moving
    // over an existing viewer and destruction require a current GL context.
    SkeletonViewerComponent();
    ~SkeletonViewerComponent();
    SkeletonViewerComponent(const SkeletonViewerComponent&) = delete;
    SkeletonViewerComponent& operator=(const SkeletonViewerComponent&) = delete;
    SkeletonViewerComponent(SkeletonViewerComponent&& other) noexcept;
    SkeletonViewerComponent& operator=(SkeletonViewerComponent&& other) noexcept;
    // Borrows data for this call only; never samples animation or stores pose.
    // A null or differently sized pose displays all nodes at bind positions.
    void drawSkeleton(const std::vector<Node>& nodes, const SkeletonPose* pose = nullptr);

    bool show;
private:
    GLuint VAO = 0, VBO = 0, lineVAO = 0, lineVBO = 0;
};

} // namespace togr

#endif
