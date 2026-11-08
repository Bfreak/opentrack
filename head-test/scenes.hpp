#pragma once

#include "scene-geometry.hpp"

#include <QStringList>

namespace head_test {

enum class scene_id { cockpit = 0, race_car = 1, soldier = 2 };
constexpr int scene_count = 3;

// Builds a scene in world space with the viewer's eye at the origin, looking down -Z, +Y up.
// Missing asset files are reported in `warnings` and skipped.
SceneDesc build_scene(scene_id id, QStringList& warnings);

} // namespace head_test
