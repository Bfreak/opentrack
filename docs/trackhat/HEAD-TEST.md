# Head-tracking test window

The **Test head tracking** button in the main window opens a 3D view driven by the *mapped* output pose
(after filter and Mapping curves, i.e. what games receive). The user can switch between three scenes and tune
curves in the Mapping window alongside it.

| Scene | Built from |
|---|---|
| Cockpit | Procedural single-seat cockpit (canopy frame, instrument panel with drawn gauges, wings, tail) above CC0 terrain; sky `kloofendal_48d_partly_cloudy_puresky` |
| Race car | Procedural GT car interior (wheel, dash, roll cage, mirrors, pillars); sky `zwartkops_start_afternoon` (Zwartkops Raceway start line) |
| Soldier | Sandbag position, CC0 props (crates, jerrycans, barriers, tyres, tree trunk), body-fixed bolt-action rifle; sky `mud_road` with fading mud ground |

Not animated or interactive by design.

## Code
- `head-test/` is its own library (`opentrack-head-test`), linked into the executable. It is listed in `cmake/opentrack-variant.cmake`.
- `head-test-window.*`: the window (scene buttons, FOV slider, settings, screenshot mode).
- `head-test-view.*`: QOpenGLWidget renderer (GL 3.3 core). The sky is an equirectangular panorama drawn per pixel. Far geometry
  (terrain/props) and near geometry (interior) use separate depth ranges.
- `scenes.cpp`: the three scenes. Units are metres, eye at the origin, -Z forward.
- `scene-geometry.*`: shape builders and glTF loading (cgltf, MIT, `head-test/3rdparty/cgltf.h`).
- `gauges.cpp`: instrument faces drawn with QPainter.
- Main window: `btnHeadTest` in `main-window.ui`, `main_window::show_head_test_window()`.

Pose convention, from opentrack's X-Plane plugin: +yaw looks right, +pitch looks up, +roll tilts clockwise,
+X right, +Y up, +Z backwards, translation in cm applied 1:1. Vehicles and the rifle are fixed to the body,
so only the view moves (freelook).

Window: about 62% of the screen width (capped near 16:9 on ultra-wide screens) × 78% of the height, placed on the right.
It is never full screen. Geometry, scene and per-scene FOV are saved under the `trackhat-head-test/` settings group.

## Assets
`head-test/assets/` (11 MB, committed), installed to `modules/head-test/`. All assets are Poly Haven CC0 and listed in `assets/CREDITS.txt`.
To regenerate or change them, use `head-test/tools/fetch_assets.py` (needs Pillow). It keeps base-colour textures only and
resizes the skies to 4096×2048.

## Screenshot mode (verification)
Set `OPENTRACK_HEAD_TEST_SNAPSHOT=<dir>` and start `opentrack.exe`. The app opens the window, saves `window.png`,
`main-window.png` and `stopped.png`, then `<scene>-<pose>.png` for six poses per scene, and quits.
This is how the scenes were checked during development (RTX 3090, Windows 11).

## Not verified yet
- Live tracking driving the view with a real sensor. The pose path is the same call the main window's pose display uses,
  but it hasn't been exercised with tracking running.
- Machines without OpenGL 3.3 (the window then shows an explanatory message instead of the scene).
- Linux build of this module.
