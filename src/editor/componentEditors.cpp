#include "componentEditors.hpp"
#include "scene/component.hpp"
#include "imgui/imgui.h"
#include <glm/gtc/type_ptr.hpp>
#include <cstddef>
#include <string>
#include <typeindex>
#include <typeinfo>

namespace togr {

namespace {
int resizePathInput(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto& text = *static_cast<std::string*>(data->UserData);
        text.resize(static_cast<std::size_t>(data->BufTextLen));
        data->Buf = text.data();
    }
    return 0;
}

void inputPath(const char* label, std::string& text) {
    ImGui::InputText(label, text.data(), text.capacity() + 1,
        ImGuiInputTextFlags_CallbackResize, resizePathInput, &text);
}

void drawTransform(Transform& transform) {
    ImGui::Text(u8"Transform");
    ImGui::Separator();
    ImGui::DragFloat3(u8"Translate", glm::value_ptr(transform.translate));
    ImGui::DragFloat3(u8"Scale", glm::value_ptr(transform.scale));
    ImGui::DragFloat3(u8"Rotate", glm::value_ptr(transform.rotate));
    ImGui::Separator();
}

// Storage-only components intentionally produce no Inspector controls.
void drawModelRender(ModelRenderComponent&) {}

void drawPointLight(PointLightComponent& light) {
    ImGui::Text(u8"PointLight");
    ImGui::Separator();
    ImGui::ColorEdit3(u8"Color", glm::value_ptr(light.color));
    ImGui::DragFloat(u8"Brightness", &light.brightness, 0.1f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(u8"Constant", &light.constant, 0.01f, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(u8"Linear", &light.linear, 0.001f, 0.0f, 1.0f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(u8"Quadratic", &light.quadratic, 0.0005f, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Separator();
}

void drawDirectionalLight(DirectionalLightComponent& light) {
    ImGui::Text(u8"DirectionalLight");
    ImGui::Separator();
    ImGui::ColorEdit3(u8"Color", glm::value_ptr(light.color));
    ImGui::DragFloat(u8"Brightness", &light.brightness, 0.1f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Separator();
}

void drawSpotLight(SpotLightComponent& light) {
    ImGui::Text(u8"SpotLight");
    ImGui::Separator();
    ImGui::ColorEdit3(u8"Color", glm::value_ptr(light.color));
    ImGui::DragFloat(u8"Brightness", &light.brightness, 0.1f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(u8"CutOff", &light.cutOff, 0.5f, 0.0f, 89.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(u8"OuterCutOff", &light.outerCutOff, 0.5f, 0.0f, 90.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Separator();
}

void drawCloudModeAndPlacement(CloudVolumeComponent& cloud) {
    const char* modes[] = { "Sky Layer", "Local Volume" };
    int mode = cloud.mode == CloudVolumeComponent::Mode::SkyLayer ? 0 : 1;
    if (ImGui::Combo("Mode", &mode, modes, 2)) {
        cloud.mode = mode == 0 ? CloudVolumeComponent::Mode::SkyLayer : CloudVolumeComponent::Mode::LocalVolume;
    }
    if (ImGui::BeginCombo("Apply preset", "Choose appearance...")) {
        const char* presets[] = { "Scattered Cumulus", "Broken Stratocumulus", "Overcast" };
        for (int i = 0; i < 3; ++i) {
            if (ImGui::Selectable(presets[i])) cloud.applyPreset(static_cast<CloudVolumeComponent::Preset>(i));
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply shape and lighting settings. Keeps layer bounds, wind and quality settings.");
    if (cloud.mode == CloudVolumeComponent::Mode::SkyLayer) {
        ImGui::DragFloat("LayerBottom", &cloud.layerBottom, 1.0f, -10000.0f, 10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("LayerThickness", &cloud.layerThickness, 1.0f, 0.0f, 10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::DragFloat("MaxTraceDistance", &cloud.maxTraceDistance, 10.0f, 1.0f, 100000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
            cloud.distanceFadeStart = glm::clamp(cloud.distanceFadeStart, 0.0f, cloud.maxTraceDistance);
        }
        ImGui::DragFloat("DistanceFadeStart", &cloud.distanceFadeStart, 10.0f, 0.0f, cloud.maxTraceDistance, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    }
    ImGui::Separator();
}

void drawCloudShapeAndCoverage(CloudVolumeComponent& cloud) {
    ImGui::Text(u8"Shape");
    ImGui::SliderFloat(u8"CloudType", &cloud.cloudType, 0.0f, 1.0f, "%.2f");
    ImGui::Separator();

    ImGui::Text(u8"Coverage");
    if (cloud.mode == CloudVolumeComponent::Mode::SkyLayer) {
        ImGui::DragFloat("RegionScale", &cloud.regionScale, 0.00001f, 0.00001f, 0.01f, "%.5f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Broad cloud banks and clear sky. Region width is approximately 1 / scale in world units.");
        ImGui::SliderFloat("RegionCoverage", &cloud.regionCoverage, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lower values open broad clear-sky regions. 1 fills all regions; local weather still shapes individual clouds.");
        ImGui::SliderFloat("DensityVariation", &cloud.densityVariation, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Varies cloud density between regions. 0 uses the same density multiplier everywhere.");
        ImGui::SliderFloat("HeightVariation", &cloud.heightVariation, 0.0f, 0.8f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Varies cloud thickness in XZ. Keeps the layer bottom and maximum top altitude fixed.");
        ImGui::DragFloat("WeatherScale", &cloud.weatherScale, 0.00001f, 0.00001f, 0.05f, "%.5f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Coverage within a weather region. Smaller values make larger groups of individual clouds.");
    }
    ImGui::DragFloat(u8"CoverageLow", &cloud.coverageLow, 0.005f, 0.0f, cloud.coverageHigh, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(u8"CoverageHigh", &cloud.coverageHigh, 0.005f, cloud.coverageLow, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Weather maps this interval into cloud coverage. Lower values create clear sky gaps.");
    ImGui::Separator();
}

void drawCloudNoise(CloudVolumeComponent& cloud) {
    ImGui::Text(u8"Noise Scales");
    ImGui::DragFloat(u8"BaseNoiseScale", &cloud.baseNoiseScale, 0.0005f, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Smaller values make larger cloud structures.");
    ImGui::DragFloat(u8"WarpScale", &cloud.warpScale, 0.0002f, 0.0001f, 0.05f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(u8"WarpStrength", &cloud.warpStrength, 0.5f, 0.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Maximum displacement per horizontal axis, in world units.");
    ImGui::DragFloat(u8"DetailScale", &cloud.detailScale, 0.0005f, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat(u8"DetailErosion", &cloud.detailErosionStrength, 0.005f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Separator();
}

void drawCloudLighting(CloudVolumeComponent& cloud) {
    ImGui::Text(u8"Density and Lighting");
    ImGui::DragFloat("DensityMultiplier", &cloud.densityMultiplier, 0.01f, 0.0f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scales opacity and self-shadowing together. Zero removes the cloud medium.");
    ImGui::DragFloat("ExtinctionCoefficient", &cloud.extinctionCoefficient, 0.001f, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Inverse world units at density 1. Shared by view and shadow rays; typical sky values: 0.02-0.1.");
    ImGui::SliderFloat("ScatteringAlbedo", &cloud.scatteringAlbedo, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("AmbientStrength", &cloud.ambientStrength, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("SunStrength", &cloud.sunStrength, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("PhaseForwardG", &cloud.phaseForwardG, 0.0f, 0.85f, "%.2f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward scattering. Higher values brighten the cloud when looking toward the sun.");
    ImGui::SliderFloat("MultipleScattering", &cloud.multipleScattering, 0.0f, 1.0f, "%.2f");
    ImGui::Separator();
}

void drawCloudAnimationAndQuality(CloudVolumeComponent& cloud) {
    ImGui::Text(u8"Animation");
    ImGui::DragFloat3(u8"WindVelocity", glm::value_ptr(cloud.windVelocity), 0.05f, -10.0f, 10.0f, "%.2f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("World units/sec. All cloud patterns move together in this direction.");
    ImGui::Separator();

    ImGui::Text(u8"Quality");
    if (cloud.mode == CloudVolumeComponent::Mode::LocalVolume) {
        ImGui::DragFloat(u8"EdgeFadeDistance", &cloud.edgeFadeDistance, 0.5f, 0.0f, 200.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    }
    ImGui::DragFloat(u8"ViewStepWorldSize", &cloud.viewStepWorldSize, 0.05f, 0.25f, 16.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt("ViewMaxSteps", &cloud.viewMaxSteps, 32, 256);
    ImGui::SliderInt("LightSteps", &cloud.lightSteps, 4, 24);
    ImGui::SliderInt("EmptyStepMultiplier", &cloud.emptyStepMultiplier, 1, 8);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Skip certified empty blocks without changing the fine sampling grid. 1 disables block skipping.");
    ImGui::Checkbox("TemporalAccumulation", &cloud.temporalAccumulation);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reproject cloud history with wind compensation. Sky layers must share horizontal wind. Edits reset history.");
    ImGui::Separator();
}

void drawCloud(CloudVolumeComponent& cloud) {
    ImGui::Text(u8"Clouds");
    drawCloudModeAndPlacement(cloud);
    drawCloudShapeAndCoverage(cloud);
    drawCloudNoise(cloud);
    drawCloudLighting(cloud);
    drawCloudAnimationAndQuality(cloud);
}

void drawSkybox(SkyboxComponent& skybox) {
    ImGui::Text(u8"Skybox");
    ImGui::Separator();
    inputPath("Path", skybox.draftFolderPath);
    ImGui::SameLine();
    if (ImGui::Button("OK")) {
        skybox.setSkybox(skybox.draftFolderPath);
    }
    if (!skybox.lastError().empty())
        ImGui::TextWrapped("%s", skybox.lastError().c_str());
}

void drawStaticMesh(StaticMeshComponent&) {}

void drawMaterial(DynamicMaterialComponent& material) {
    ImGui::Text(u8"Material");
    ImGui::Separator();
    inputPath(u8"albedo", material.draft.albedoPath);
    inputPath(u8"specular", material.draft.specularPath);
    inputPath(u8"normal", material.draft.normalPath);
    if (ImGui::Button("OK")) {
        material.applyMaterial();
    }
    if (!material.lastError().empty())
        ImGui::TextWrapped("%s", material.lastError().c_str());
}

void drawShadowCaster2D(ShadowCaster2D& shadow) {
    ImGui::Text(u8"ShadowCaster");
    ImGui::Separator();
    ImGui::Checkbox(u8"Enabled", &shadow.enabled);
}

void drawShadowCasterCube(ShadowCasterCube& shadow) {
    ImGui::Text(u8"ShadowCaster");
    ImGui::Separator();
    ImGui::Checkbox(u8"Enabled", &shadow.enabled);
    ImGui::DragFloat(u8"FarPlane", &shadow.farPlane, 0.5f, 0.1f, 10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
}

void drawAnimator(AnimatorComponent& animator) {
    ImGui::Text(u8"Animator");
    ImGui::Separator();
    if (ImGui::BeginCombo(u8"##AnimatorCombo", animator.getCurrentAnimation().c_str())) {
        for (const auto& name : animator.getAnimationsNames()) {
            bool isSelected = (animator.getCurrentAnimation() == name);
            if (ImGui::Selectable(name.c_str(), isSelected)) {
                animator.playAnimation(name);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (animator.playing) {
        if (ImGui::Button("Pause")) {
            animator.playing = false;
        }
    } else {
        if (ImGui::Button("Play")) {
            animator.playing = true;
        }
    }
}

void drawSkeletonViewer(SkeletonViewerComponent& viewer) {
    ImGui::Text(u8"SkeletonViewer");
    ImGui::Separator();
    ImGui::Checkbox(u8"Show", &viewer.show);
    ImGui::Separator();
}

using EditorFunction = void (*)(Component&);

struct EditorEntry {
    std::type_index type;
    EditorFunction draw;
};

template<typename T, void (*DrawEditor)(T&)>
void drawTyped(Component& component) {
    DrawEditor(static_cast<T&>(component));
}

// Exact dynamic-type matching keeps storage-only and unknown component behavior
// explicit. The table owns no component, callback capture or mutable registration.
const EditorEntry kEditors[] = {
    {std::type_index(typeid(Transform)), &drawTyped<Transform, drawTransform>},
    {std::type_index(typeid(ModelRenderComponent)), &drawTyped<ModelRenderComponent, drawModelRender>},
    {std::type_index(typeid(PointLightComponent)), &drawTyped<PointLightComponent, drawPointLight>},
    {std::type_index(typeid(DirectionalLightComponent)), &drawTyped<DirectionalLightComponent, drawDirectionalLight>},
    {std::type_index(typeid(SpotLightComponent)), &drawTyped<SpotLightComponent, drawSpotLight>},
    {std::type_index(typeid(CloudVolumeComponent)), &drawTyped<CloudVolumeComponent, drawCloud>},
    {std::type_index(typeid(SkyboxComponent)), &drawTyped<SkyboxComponent, drawSkybox>},
    {std::type_index(typeid(StaticMeshComponent)), &drawTyped<StaticMeshComponent, drawStaticMesh>},
    {std::type_index(typeid(DynamicMaterialComponent)), &drawTyped<DynamicMaterialComponent, drawMaterial>},
    {std::type_index(typeid(ShadowCaster2D)), &drawTyped<ShadowCaster2D, drawShadowCaster2D>},
    {std::type_index(typeid(ShadowCasterCube)), &drawTyped<ShadowCasterCube, drawShadowCasterCube>},
    {std::type_index(typeid(AnimatorComponent)), &drawTyped<AnimatorComponent, drawAnimator>},
    {std::type_index(typeid(SkeletonViewerComponent)), &drawTyped<SkeletonViewerComponent, drawSkeletonViewer>},
};
}

namespace ComponentEditors {
void draw(Component& component) {
    ImGui::PushID(&component);
    const std::type_index type(typeid(component));
    const EditorEntry* editor = nullptr;
    for (const auto& entry : kEditors) {
        if (entry.type == type) {
            editor = &entry;
            break;
        }
    }
    if (editor) {
        editor->draw(component);
    } else {
        ImGui::Text("No editor registered for %s", component.getName().c_str());
    }
    ImGui::PopID();
}
}
} // namespace togr
