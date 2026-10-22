#ifndef TOGR_SKELETONPOSE_HPP
#define TOGR_SKELETONPOSE_HPP
#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace togr {

// Per-instance animation output, owned by Animator. Imported nodes and clips
// remain unchanged; viewers borrow this data only while drawing.
struct SkeletonPose {
    // Model-space node origins before the object's world transform; node-vector order.
    std::vector<glm::vec3> nodePositions;
    // TOGR_MAX_BONES matrices, indexed by Node::id, for baked model-space mesh vertices.
    std::vector<glm::mat4> boneMatrices;
};

} // namespace togr

#endif // !TOGR_SKELETONPOSE_HPP
