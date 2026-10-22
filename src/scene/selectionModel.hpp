#ifndef TOGR_SELECTIONMODEL_HPP
#define TOGR_SELECTIONMODEL_HPP
#pragma once

#include "gameObject.hpp"
#include <memory>

namespace togr {

// Engine owns this shared selection state; GUI writes it and rendering reads it.
// Selection observes object lifetime and does not keep a removed object alive.
class SelectionModel {
public:
    void set(const GameObjectPtr& object) { selected_ = object; }
    void clear() { selected_.reset(); }

    // A snapshot that keeps the object alive while the caller uses it. Empty if
    // nothing is selected or the last owning reference has been released.
    GameObjectPtr get() const { return selected_.lock(); }

private:
    std::weak_ptr<GameObject> selected_;
};

} // namespace togr

#endif // !TOGR_SELECTIONMODEL_HPP
