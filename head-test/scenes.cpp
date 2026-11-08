// The three test scenes. Units are metres; the eye is at the origin looking down -Z.
// Assets: see assets/CREDITS.txt (Poly Haven CC0, FlightGear C172P GPL-2.0+, Khronos CarConcept CC-BY 4.0).

#include "scenes.hpp"

#include <cmath>

#include <QFileInfo>

namespace head_test {

namespace {

constexpr float pi = 3.14159265358979f;

QString sky(const char* name)
{
    return asset_dir() + "sky/" + name + ".jpg";
}

// "name" -> models/name/name.gltf, "dir/file" -> models/dir/file.gltf
bool add_model(std::vector<Prim>& out, const QString& name, const QMatrix4x4& xf, QStringList& warnings)
{
    const QString path = asset_dir() + "models/" + (name.contains('/') ? name : name + "/" + name) + ".gltf";
    QString err;
    if (!QFileInfo::exists(path))
    {
        warnings << QStringLiteral("missing %1").arg(path);
        return false;
    }
    if (!load_gltf(path, xf, out, &err))
    {
        warnings << err;
        return false;
    }
    return true;
}

QMatrix4x4 place(QVector3D pos, float yaw_deg = 0, float pitch_deg = 0, float roll_deg = 0, float scale = 1)
{
    QMatrix4x4 m;
    m.translate(pos);
    m.rotate(yaw_deg, 0, 1, 0);
    m.rotate(pitch_deg, 1, 0, 0);
    m.rotate(roll_deg, 0, 0, 1);
    if (scale != 1)
        m.scale(scale);
    return m;
}

Material pbr_texture(const QString& id, float uv_scale)
{
    Material m;
    const QString base = asset_dir() + "ground/" + id;
    m.tex_key = base + "_diff.jpg";
    if (QFileInfo::exists(base + "_nor.jpg"))
        m.normal_key = base + "_nor.jpg";
    if (QFileInfo::exists(base + "_arm.jpg"))
        m.mr_key = m.ao_key = base + "_arm.jpg";
    m.roughness = 1;
    m.metallic = 1; // scaled by the arm texture's B channel
    m.uv_scale = uv_scale;
    return m;
}

// invisible ground that only shows the vehicle's shadow
void shadow_catcher(std::vector<Prim>& out, float y, float half)
{
    Material m;
    m.shadow_catcher = true;
    m.cast_shadow = false;
    m.fade_start = half * .6f;
    m.fade_end = half;
    MeshBuilder(out).quad(m, { -half, y, half }, { 2 * half, 0, 0 }, { 0, 0, -2 * half });
}

// ---------------------------------------------------------------- cockpit

SceneDesc cockpit(QStringList& warnings)
{
    SceneDesc s;
    s.name = "Cockpit";
    s.sky_file = sky("derelict_airfield_01");
    s.sun_from_sky = true;
    s.sun_color = { 1.0f, .96f, .9f };
    s.sun_strength = 3.2f;
    s.ambient = 1.0f;
    s.exposure = 1.0f;
    s.shadow_radius = 7;
    s.far_near = .3f;
    s.far_far = 3000;
    s.default_hfov = 90;

    // FlightGear Cessna 172P (GPL-2.0-or-later), pre-assembled with the pilot's eye at the origin
    if (add_model(s.near_prims, "c172p", QMatrix4x4(), warnings))
    {
        QVector3D mn, mx;
        bounds(s.near_prims, 0, mn, mx);
        shadow_catcher(s.far_prims, mn.y(), 14);
    }
    return s;
}

// ---------------------------------------------------------------- race car

SceneDesc race_car(QStringList& warnings)
{
    SceneDesc s;
    s.name = "Race car";
    s.sky_file = sky("zwartkops_start_afternoon");
    s.sun_from_sky = true;
    s.sun_color = { 1.0f, .97f, .9f };
    s.sun_strength = 3.2f;
    s.ambient = 1.0f;
    s.shadow_radius = 5;
    s.far_near = .3f;
    s.far_far = 2000;
    s.default_hfov = 95;
    s.exposure = 1.15f;

    // Khronos "CarConcept" sample asset (CC-BY 4.0): Y up, +Z forward, ground at y = 0, central driving seat
    QVector3D eye(0, 1.0f, .35f);
    if (const QStringList e = qEnvironmentVariable("OPENTRACK_HEAD_TEST_CAR_EYE").split(','); e.size() == 3)
        eye = { e[0].toFloat(), e[1].toFloat(), e[2].toFloat() };
    QMatrix4x4 xf;
    xf.rotate(180, 0, 1, 0); // car forward (+Z) -> -Z
    xf.translate(-eye);
    if (add_model(s.near_prims, "carconcept/CarConcept", xf, warnings))
        shadow_catcher(s.far_prims, -eye.y(), 9);
    return s;
}

// ---------------------------------------------------------------- soldier

SceneDesc soldier(QStringList& warnings)
{
    SceneDesc s;
    s.name = "Soldier";
    s.sky_file = sky("mud_road");
    s.sun_dir = QVector3D(.25f, .9f, -.35f).normalized();
    s.sun_color = { .9f, .9f, .92f };
    s.sun_strength = .9f;
    s.ambient = 1.25f;
    s.exposure = 1.05f;
    s.fog_color = { .76f, .77f, .79f };
    s.fog_density = .004f;
    s.far_near = .3f;
    s.far_far = 3000;
    s.shadow_radius = 18;
    s.default_hfov = 90;

    const float ground = -1.65f;

    // muddy ground patch that fades into the panorama
    {
        Material mud = pbr_texture("brown_mud_leaves_01", 1);
        mud.fade_start = 12;
        mud.fade_end = 30;
        mud.cast_shadow = false;
        const float half = 32;
        MeshBuilder(s.far_prims).quad(mud, { -half, ground, half }, { 2 * half, 0, 0 }, { 0, 0, -2 * half }, { 26, 26 });
    }

    auto far_ = [&](const char* m, QVector3D p, float yaw = 0, float pitch = 0, float roll = 0, float sc = 1) {
        add_model(s.far_prims, m, place(p, yaw, pitch, roll, sc), warnings);
    };
    auto near_ = [&](const char* m, QVector3D p, float yaw = 0, float pitch = 0, float roll = 0, float sc = 1) {
        add_model(s.near_prims, m, place(p, yaw, pitch, roll, sc), warnings);
    };

    // around the position
    far_("wooden_military_crate", { -1.35f, ground, .25f }, 75);
    far_("old_military_crate", { -1.30f, ground + .465f, .20f }, 82);
    far_("vintage_radio_transceiver", { -1.25f, ground + .465f + .30f, .55f }, 100);
    far_("metal_jerrycan_green", { 1.05f, ground, .55f }, -30);
    far_("metal_jerrycan_green", { 1.30f, ground, .75f }, -10);
    far_("barrel_03", { 1.6f, ground, -.1f }, 40);

    // behind (for checking six)
    far_("wooden_military_crate", { -1.8f, ground, 3.2f }, 10);
    far_("wooden_military_crate", { -1.75f, ground + .465f, 3.15f }, 4);
    far_("wooden_barrels_01", { 1.4f, ground, 3.6f }, -20);
    far_("old_tyre", { 2.6f, ground + .083f, 2.4f }, 0, 90);
    far_("concrete_road_barrier", { 3.5f, ground, 8.f }, 70);
    far_("dead_tree_trunk", { 6.f, ground, 14.f }, -40);

    // in front
    far_("dead_tree_trunk", { -4.5f, ground, -9.f }, 25);
    far_("dead_tree_trunk_02", { 3.8f, ground, -18.f }, -60);
    far_("tree_stump_01", { -2.2f, ground, -6.5f }, 15);
    far_("concrete_road_barrier", { 5.2f, ground, -12.f }, -10);
    far_("concrete_road_barrier_02", { 6.75f, ground, -12.4f }, -18);
    far_("old_tyre", { 2.6f, ground + .083f, -6.2f }, 0, 90);
    far_("old_tyre", { 2.75f, ground + .25f, -6.1f }, 20, 90);
    far_("rusted_wheel_rim_01", { 1.2f, ground + .05f, -4.0f }, 30, 80);
    far_("boulder_01", { -8.5f, ground, -14.f }, 50);
    far_("wooden_military_crate", { -7.f, ground, -16.f }, 30);
    far_("barrel_03", { 9.f, ground, -7.f }, 0, 90);

    // sandbag wall in an arc in front of the viewer
    {
        MeshBuilder b(s.near_prims);
        Material bag[3] = { pbr_texture("hessian_230", 2), pbr_texture("hessian_230", 2), pbr_texture("hessian_230", 2) };
        bag[0].color = { .92f, .86f, .74f, 1 };
        bag[1].color = { .80f, .74f, .62f, 1 };
        bag[2].color = { .70f, .64f, .54f, 1 };
        for (auto& m : bag)
            m.metallic = 0;
        const float R = 1.05f, bag_h = .16f;
        const int rows = 8;
        for (int row = 0; row < rows; row++)
        {
            const float y = ground + bag_h * (float(row) + .5f);
            const float r = R + (rows - 1 - row) * .03f; // slight batter
            const float off = (row % 2) ? .5f : 0.f;
            for (int k = -7; k <= 7; k++)
            {
                const float a = (float(k) + off) * 9.5f;
                if (std::fabs(a) > 68)
                    continue;
                const float ar = a * pi / 180;
                QMatrix4x4 xf;
                xf.translate(r * std::sin(ar), y, -r * std::cos(ar));
                xf.rotate(-a, 0, 1, 0);
                xf.rotate(float((k * 7 + row * 3) % 5) - 2.f, 0, 0, 1);
                b.ellipsoid(bag[(k + row * 2 + 9) % 3], xf, { .19f, .09f, .15f }, 16, 10);
            }
        }
        const float top = ground + bag_h * rows;
        near_("ammo_box", { .55f, top - .02f, -.98f }, 25);
        near_("signal_flashlight", { .30f, top + .005f, -.92f }, -40, 0, 90);
        near_("stick_grenade", { -.42f, top + .02f, -1.0f }, 70, 0, 90);
        near_("stick_grenade", { -.30f, top + .02f, -1.02f }, 55, 0, 90);
    }

    // rifle held at the low ready, fixed to the body (it doesn't follow head rotation)
    {
        QMatrix4x4 xf;
        xf.translate(.21f, -.27f, -.50f);
        xf.rotate(-7, 1, 0, 0);   // muzzle down a little
        xf.rotate(8, 0, 1, 0);    // muzzle slightly inward
        xf.rotate(-12, 0, 0, 1);  // canted
        xf.rotate(90, 0, 1, 0);   // model +X (muzzle) -> -Z
        add_model(s.near_prims, "bolt_action_rifle_7_62", xf, warnings);
    }
    return s;
}

} // namespace

SceneDesc build_scene(scene_id id, QStringList& warnings)
{
    switch (id)
    {
    case scene_id::cockpit: return cockpit(warnings);
    case scene_id::race_car: return race_car(warnings);
    case scene_id::soldier: break;
    }
    return soldier(warnings);
}

} // namespace head_test
