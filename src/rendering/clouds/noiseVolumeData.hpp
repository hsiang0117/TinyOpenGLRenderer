#pragma once

#include <vector>

namespace togr {

// CPU-only noise data. A depth of 1 represents a 2D map.
// Generation runs on workers; texture upload runs on the GL-context thread.
struct NoiseVolumeData {
    int width = 0, height = 0, depth = 0;
    std::vector<unsigned char> data;
};

} // namespace togr
