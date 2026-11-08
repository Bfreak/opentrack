# Head-tracking test window

The **Test head tracking** button in the main window opens a 3D view driven by the *mapped* output pose
(after filter and Mapping curves, i.e. what games receive). The user can switch between three scenes and tune
curves in the Mapping window alongside it.

| Scene | Built from |
|---|---|
| Cockpit | FlightGear Cessna 172P cockpit (GPL-2.0+), converted by `tools/fetch_c172.py`, lined up on a runway; sky `derelict_airfield_01` |
| Race car | Khronos glTF sample "CarConcept" (CC-BY 4.0), with a central driving seat; sky `zwartkops_start_afternoon` (Zwartkops Raceway) |
| Soldier | Hessian sandbag position, about 20 Poly Haven CC0 props, body-fixed rifle; sky `mud_road` with a fading mud ground |

Not animated or interactive by design. Interiors were chosen over Speed Dreams cars (too low detail), Grab3D's
CC0 cockpit (toy-like) and Poly Haven trees (`fir_tree_01` alone is 460 MB).

## Code
- `head-test/` is its own library (`opentrack-head-test`), linked into the executable. It is listed in `cmake/opentrack-variant.cmake`.
- `head-test-window.*`: the window (scene buttons, FOV slider, settings, screenshot mode).
- `head-test-view.*`: QOpenGLWidget renderer (GL 3.3 core):
  - glTF PBR (normal, metal/rough, AO on UV0 or UV1, emissive) and blended glass;
  - 4096² sun shadow map, plus a shadow-catcher ground under the vehicles;
  - sky-based ambient light and reflections. The sun position is found in the panorama;
  - ACES tone mapping, and a seam-free mip-mapped 8K sky;
  - far geometry and near geometry (the interior) use separate depth ranges.
- `scenes.cpp`: the three scenes. Units are metres, eye at the origin, -Z forward. `OPENTRACK_HEAD_TEST_CAR_EYE=x,y,z` tunes the car eye.
- `scene-geometry.*`: shape builders and glTF loading (cgltf, MIT, `head-test/3rdparty/cgltf.h`).
- Main window: `btnHeadTest` in `main-window.ui`, `main_window::show_head_test_window()`.

Pose convention, from opentrack's X-Plane plugin: +yaw looks right, +pitch looks up, +roll tilts clockwise,
+X right, +Y up, +Z backwards, translation in cm applied 1:1. Vehicles and the rifle are fixed to the body,
so only the view moves (freelook).

Window: about 62% of the screen width (capped near 16:9 on ultra-wide screens) × 78% of the height, placed on the right.
It is never full screen. Geometry, scene and per-scene FOV are saved under the `trackhat-head-test/` settings group.

## Assets
`head-test/assets/` (about 77 MB, committed) is installed to `modules/head-test/`. Sources and licences are in `assets/CREDITS.txt`
and `3rdparty-notices/HEAD-TEST-ASSETS.txt`. Tools (they need Pillow):
- `fetch_assets.py` handles Poly Haven: 8K skies, and PBR textures and models (2k up close, 1k further away).
- `fetch_c172.py` handles the C172. It follows the FlightGear XML includes, drops optional, damage and effect parts and the sun visors,
  and writes `COPYING` and `AUTHORS` (including the source commit).
- `ac3d.py` is the AC3D reader (with ear clipping) and the glTF writer.

CarConcept is the Khronos glTF-JPG variant, copied unchanged with its `LICENSE.md`.

## Screenshot mode (verification)
Set `OPENTRACK_HEAD_TEST_SNAPSHOT=<dir>` and start `opentrack.exe`. The app opens the window, saves `window.png`,
`main-window.png` and `stopped.png`, then `<scene>-<pose>.png` for six poses per scene. It writes rough frame times to `log.txt`
(6–11 ms on the RTX 3090, including a read-back per frame) and quits.

## Not verified yet
- Live tracking driving the view with a real sensor. The pose path is the same call the main window's pose display uses,
  but it hasn't been exercised with tracking running.
- Performance on integrated GPUs.
- Machines without OpenGL 3.3 (the window then shows an explanatory message instead of the scene).
- Linux build of this module.
