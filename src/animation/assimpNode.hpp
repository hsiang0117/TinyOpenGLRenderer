#ifndef TOGR_ASSIMPNODE_HPP
#define TOGR_ASSIMPNODE_HPP
#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace togr {

// Imported hierarchy and bind data, shared read-only by model instances.
// Current animation output belongs to Animator's SkeletonPose, not this node.
struct Node {
    std::string name;
    int id; // Node vector index; also the palette slot for a skinned node.
    int parentIndex; // Node vector index, or -1 when there is no parent.
    glm::vec3 bindPosition; // Bind node origin in model space, before object-to-world.
    glm::mat4 localBindTransform; // Imported bind transform relative to the parent.
    // Fixed O * inverse(meshToModelBind): baked model space -> bone bind space.
    glm::mat4 modelToBoneBind;
    bool isBoneNode = false;
    std::vector<int> childrenIndices; // Indices into the same node vector.
};

} // namespace togr

#endif
