#pragma once

#include <cstddef>

// Shared by scene insertion, the editor, and GPU allocation/submission.
namespace RenderLimits {
    inline constexpr std::size_t PointLights = 10;
    inline constexpr std::size_t DirectionalLights = 1;
    inline constexpr std::size_t SpotLights = 50;
}
