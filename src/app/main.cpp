#include "engine.hpp"
#include <memory>
#include <exception>

int main() {
    try {
        auto engine = std::make_unique<togr::Engine>();
        engine->init();
        engine->run();
        return 0;
    } catch (const std::exception& error) {
        togr::WindowSystem::showError(error.what());
        return 1;
    }
}
