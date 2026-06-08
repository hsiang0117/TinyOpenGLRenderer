#include "engine.hpp"
#include <memory>

int main() {
    auto engine = std::make_unique<Engine>();
    engine->init();
    engine->run();
    return 0;
}