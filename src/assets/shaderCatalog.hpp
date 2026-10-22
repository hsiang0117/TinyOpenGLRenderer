#ifndef TOGR_SHADER_CATALOG_HPP
#define TOGR_SHADER_CATALOG_HPP
#pragma once

#include <array>

namespace togr {

// Paths remain relative to the application working directory.
namespace ShaderCatalog {
inline constexpr const char* directory = "data/shader";
inline constexpr const char* settingsPath = "data/shader/settings.glsl";
inline constexpr const char* defaultsPath = "data/shader/settings.defaults.glsl";
inline constexpr const char* fullscreenVertexPath = "data/shader/fullscreen.vert";

struct Program {
    const char* key;
    const char* vertexPath;
    const char* fragmentPath;
    const char* geometryPath = nullptr;
};

inline constexpr Program forward{
    "forward", "data/shader/forward.vert", "data/shader/forward.frag"
};
inline constexpr Program skybox{
    "skybox", "data/shader/skybox.vert", "data/shader/skybox.frag"
};
inline constexpr Program directionalShadow{
    "directionalShadow", "data/shader/directionalShadow.vert", "data/shader/directionalShadow.frag"
};
inline constexpr Program pointShadow{
    "pointShadow", "data/shader/pointShadow.vert", "data/shader/pointShadow.frag", "data/shader/pointShadow.geom"
};
inline constexpr Program composite{
    "composite", fullscreenVertexPath, "data/shader/composite.frag"
};
inline constexpr Program lightMarker{
    "lightMarker", "data/shader/lightMarker.vert", "data/shader/lightMarker.frag"
};
inline constexpr Program gaussianBlur{
    "gaussianBlur", "data/shader/gaussianBlur.vert", "data/shader/gaussianBlur.frag"
};
inline constexpr Program skeleton{
    "skeleton", "data/shader/skeleton.vert", "data/shader/skeleton.frag"
};
inline constexpr Program cloudRaymarch{
    "cloudRaymarch", "data/shader/cloudRaymarch.vert", "data/shader/cloudRaymarch.frag"
};
inline constexpr Program cloudUpsample{
    "cloudUpsample", fullscreenVertexPath, "data/shader/cloudUpsample.frag"
};
inline constexpr Program cloudTemporal{
    "cloudTemporal", fullscreenVertexPath, "data/shader/cloudTemporal.frag"
};
inline constexpr Program gizmo{
    "gizmo", "data/shader/gizmo.vert", "data/shader/gizmo.frag"
};

// Registration order is explicit; geometry stages are optional.
inline constexpr auto programs = std::array{
    forward, skybox, directionalShadow, pointShadow, composite, lightMarker,
    gaussianBlur, skeleton, cloudRaymarch, cloudUpsample, cloudTemporal, gizmo
};
}

} // namespace togr

#endif // !TOGR_SHADER_CATALOG_HPP
