#ifndef TOGR_COMPONENTEDITORS_HPP
#define TOGR_COMPONENTEDITORS_HPP
#pragma once

namespace togr {

class Component;

namespace ComponentEditors {
// Borrows the component only for this call, within an active ImGui frame/window.
// Resource commits require the main thread's current GL context. Balances its own
// per-component ImGui ID scope and retains no component or editor state.
void draw(Component& component);
}

} // namespace togr

#endif // !TOGR_COMPONENTEDITORS_HPP
