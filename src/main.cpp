#include "engine.hpp"

int main() {
    Engine::getInstance().init();
    Engine::getInstance().run();
    return 0;
}