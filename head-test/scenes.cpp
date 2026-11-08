// The three test scenes. Units are metres; the eye is at the origin looking down -Z.
// Vehicle interiors are built from simple shapes; props, skies and ground textures are
// CC0 assets from Poly Haven (see assets/CREDITS.txt).

#include "scenes.hpp"
#include "gauges.hpp"

#include <cmath>

#include <QFileInfo>

namespace head_test {

namespace {

constexpr float pi = 3.14159265358979f;

Material solid(float r, float g, float b)
{
    Material m;
    m.color = { r, g, b, 1 };
    return m;
}

Material unlit(float r, float g, float b)
{
    Material m = solid(r, g, b);
    m.unlit = true;
    return m;
}

Material generated(const QString& key, const QImage& img)
{
    Material m;
    m.tex_key = "gen:" + key;
    m.tex_image = img;
    m.unlit = true;
    m.color = { .92f, .92f, .92f, 1 };
    return m;
}

QVector3D sun_from_panorama(float u, float v)
{
    // u, v: position of the sun in the equirectangular image (0..1, v = 0 at the top)
    const float az = (u - .5f) * 2 * pi, el = (.5f - v) * pi;
    return QVector3D(std::sin(az) * std::cos(el), std::sin(el), -std::cos(az) * std::cos(el)).normalized();
}

QString sky(const char* name)
{
    return asset_dir() + "sky/" + name + ".jpg";
}

void add_model(std::vector<Prim>& out, const QString& name, const QMatrix4x4& xf, QStringList& warnings)
{
    const QString path = asset_dir() + "models/" + name + "/" + name + ".gltf";
    QString err;
    if (!QFileInfo::exists(path))
        warnings << QStringLiteral("missing %1").arg(path);
    else if (!load_gltf(path, xf, out, &err))
        warnings << err;
}

QMatrix4x4 place(QVector3D pos, float yaw_deg = 0, float pitch_deg = 0, float roll_deg = 0)
{
    QMatrix4x4 m;
    m.translate(pos);
    m.rotate(yaw_deg, 0, 1, 0);
    m.rotate(pitch_deg, 1, 0, 0);
    m.rotate(roll_deg, 0, 0, 1);
    return m;
}

// arch in the XY plane at depth z: x = -w cos t, y = y0 + h sin t, t in [t0, t1]
std::vector<QVector3D> arch(float w, float y0, float h, float z, float t0 = 0, float t1 = pi, int n = 20)
{
    std::vector<QVector3D> pts;
    for (int i = 0; i <= n; i++)
    {
        const float t = t0 + (t1 - t0) * float(i) / float(n);
        pts.emplace_back(-w * std::cos(t), y0 + h * std::sin(t), z);
    }
    return pts;
}

void gauge(MeshBuilder& b, const QString& kind, QVector3D pos, float radius, float tilt_deg = 0)
{
    QMatrix4x4 xf;
    xf.translate(pos);
    xf.rotate(tilt_deg, 1, 0, 0);
    b.disc(generated(kind, gauge_image(kind)), xf, radius);
    // bezel ring
    b.torus(solid(.08f, .08f, .08f), xf, radius, radius * .09f, 40, 8);
}

void panel(MeshBuilder& b, const QString& kind, QVector3D center, float w, float h, float tilt_deg = 0)
{
    QMatrix4x4 xf;
    xf.translate(center);
    xf.rotate(tilt_deg, 1, 0, 0);
    const QVector3D c = xf.map(QVector3D(-w / 2, -h / 2, 0));
    const QVector3D ex = xf.mapVector(QVector3D(w, 0, 0)), ey = xf.mapVector(QVector3D(0, h, 0));
    b.quad(generated(kind, panel_image(kind)), c, ex, ey);
}

// ---------------------------------------------------------------- cockpit

SceneDesc cockpit(QStringList& warnings)
{
    SceneDesc s;
    s.name = "Cockpit";
    s.sky_file = sky("kloofendal_48d_partly_cloudy_puresky");
    s.sun_dir = sun_from_panorama(.6f, .22f);
    s.sun_color = { 1.0f, .96f, .88f };
    s.sky_ambient = { .50f, .56f, .66f };
    s.ground_ambient = { .22f, .22f, .24f };
    s.fog_color = { .74f, .80f, .90f };
    s.fog_density = 7e-5f;
    s.fog_from_sky = true;
    s.fill_below_horizon = true;
    s.far_near = 20;
    s.far_far = 120000;
    s.default_hfov = 90;

    // terrain far below
    {
        Material ground;
        ground.tex_key = asset_dir() + "ground/aerial_grass_rock.jpg";
        ground.uv_scale = 1;
        const float half = 150000, y = -1500;
        MeshBuilder(s.far_prims).quad(ground, { -half, y, half }, { 2 * half, 0, 0 }, { 0, 0, -2 * half },
                                      { 2 * half / 600, 2 * half / 600 });
    }

    MeshBuilder b(s.near_prims);
    const Material paint = solid(.70f, .72f, .74f), dark = solid(.16f, .16f, .17f), frame = solid(.24f, .25f, .26f),
                   black = solid(.05f, .05f, .05f), seat = solid(.20f, .19f, .17f), red = unlit(1, .1f, .1f),
                   green = unlit(.1f, 1, .2f), stripe = solid(.85f, .45f, .05f);

    // instrument panel + glareshield
    b.box_at(dark, { 0, -.30f, -.80f }, { .37f, .17f, .02f });
    b.box_at(dark, { 0, -.115f, -.83f }, { .40f, .02f, .10f });
    b.box_at(black, { 0, -.20f, -.79f }, { .02f, .06f, .01f }); // centre post
    const float gz = -.778f, gr = .058f;
    gauge(b, "asi", { -.22f, -.20f, gz }, gr);
    gauge(b, "ai",  { -.07f, -.20f, gz }, gr);
    gauge(b, "alt", {  .08f, -.20f, gz }, gr);
    gauge(b, "tc",  { -.22f, -.355f, gz }, gr);
    gauge(b, "hdg", { -.07f, -.355f, gz }, gr);
    gauge(b, "vsi", {  .08f, -.355f, gz }, gr);
    panel(b, "mfd", { .255f, -.28f, gz }, .17f, .085f);
    b.box_at(black, { .255f, -.28f, gz - .006f }, { .095f, .05f, .004f });

    // cockpit tub: sills, side walls, consoles
    for (float sx : { -1.f, 1.f })
    {
        b.box_at(frame, { sx * .40f, -.42f, -.25f }, { .03f, .22f, .78f });   // sill / inner wall
        b.box_at(paint, { sx * .47f, -.45f, -.25f }, { .05f, .26f, 1.15f });  // outer skin
        b.box_at(dark, { sx * .31f, -.50f, -.10f }, { .07f, .04f, .32f });   // console
        for (int k = 0; k < 4; k++)
            b.cylinder(black, { sx * .31f, -.46f, -.32f + k * .12f }, { sx * .31f, -.44f, -.32f + k * .12f }, .012f, .012f, 10);
    }
    // throttle
    b.cylinder(black, { -.31f, -.46f, -.05f }, { -.30f, -.36f, -.10f }, .008f, .008f, 8);
    b.ellipsoid(black, place({ -.30f, -.35f, -.10f }), { .025f, .02f, .03f });
    // stick
    b.cylinder(black, { 0, -.85f, -.36f }, { 0, -.50f, -.33f }, .014f, .014f, 12);
    b.cylinder(black, { 0, -.50f, -.33f }, { 0, -.40f, -.31f }, .022f, .02f, 12);
    // seat + headrest (visible over the shoulder)
    b.box_at(seat, { 0, -.10f, .27f }, { .12f, .12f, .05f });
    b.box_at(seat, { 0, -.50f, .30f }, { .22f, .24f, .06f });
    // floor and firewall
    b.box_at(dark, { 0, -.95f, -.25f }, { .40f, .02f, .75f });
    b.box_at(dark, { 0, -.70f, -.95f }, { .40f, .25f, .02f });

    // canopy frame
    const float fr = .017f;
    b.tube(frame, arch(.40f, -.18f, .50f, -.56f), fr);                // front bow
    b.tube(frame, arch(.40f, -.18f, .52f, .55f), fr);                 // rear bow
    for (float sx : { -1.f, 1.f })
    {
        b.tube(frame, { { sx * .38f, -.11f, -.90f }, { sx * .30f, .15f, -.57f } }, fr); // windscreen post
        b.tube(frame, { { sx * .40f, -.18f, -.56f }, { sx * .40f, -.18f, .55f } }, fr);  // canopy rail
    }
    b.box_at(black, { 0, .305f, -.53f }, { .07f, .02f, .008f }); // mirror

    // exterior: nose, wings, spine, tail
    b.cylinder(paint, { 0, -.78f, -.90f }, { 0, -.80f, -4.4f }, .62f, .10f, 32, false);
    b.cylinder(black, { 0, -.80f, -4.4f }, { 0, -.80f, -4.8f }, .10f, .0f, 16);
    b.box_at(stripe, { 0, -.168f, -1.4f }, { .05f, .006f, .45f }, { -8, 0, 0 });
    for (float sx : { -1.f, 1.f })
    {
        b.box_at(paint, { sx * 3.1f, -1.05f, .25f }, { 2.6f, .045f, .95f }, { 0, -sx * 6, sx * 3 });
        b.ellipsoid(sx < 0 ? red : green, place({ sx * 5.75f, -0.92f, .55f }), { .05f, .05f, .08f });
        b.box_at(paint, { sx * 1.15f, -.25f, 4.3f }, { .95f, .03f, .38f }, { 0, -sx * 8, 0 }); // stabiliser
    }
    b.box_at(paint, { 0, -.30f, 2.4f }, { .42f, .26f, 1.9f });  // spine
    b.box_at(paint, { 0, .45f, 4.35f }, { .045f, .70f, .85f }, { 25, 0, 0 }); // fin
    b.box_at(stripe, { 0, .05f, 3.6f }, { .43f, .02f, .5f });

    (void)warnings;
    return s;
}

// ---------------------------------------------------------------- race car

SceneDesc race_car(QStringList& warnings)
{
    SceneDesc s;
    s.name = "Race car";
    s.sky_file = sky("zwartkops_start_afternoon");
    s.sun_dir = sun_from_panorama(.60f, .19f);
    s.sun_color = { 1.0f, .97f, .9f };
    s.sky_ambient = { .48f, .55f, .66f };
    s.ground_ambient = { .25f, .25f, .25f };
    s.far_near = 1;
    s.far_far = 2000;
    s.default_hfov = 95;

    MeshBuilder b(s.near_prims);
    const Material paint = solid(.78f, .06f, .05f), dark = solid(.10f, .10f, .11f), trim = solid(.17f, .17f, .18f),
                   cage = solid(.80f, .80f, .78f), black = solid(.03f, .03f, .03f), alcantara = solid(.12f, .12f, .13f),
                   glass = solid(.12f, .16f, .22f), belt = solid(.15f, .25f, .75f), yellow = solid(.95f, .75f, .05f);
    const float cx = .40f; // car centre line (driver sits left of it)

    // steering wheel
    {
        QMatrix4x4 xf;
        xf.translate(0, -.27f, -.52f);
        xf.rotate(-22, 1, 0, 0);
        b.torus(alcantara, xf, .165f, .017f, 56, 12);
        b.box(yellow, [&] { QMatrix4x4 m = xf; m.translate(0, .165f, 0); return m; }(), { .012f, .02f, .02f }); // centre mark
        for (float a : { 0.f, 180.f, 270.f })
        {
            const float r = a * pi / 180;
            b.cylinder(trim, xf.map({ 0, 0, 0 }), xf.map({ .16f * std::cos(r), .16f * std::sin(r), 0 }), .016f, .012f, 10);
        }
        b.cylinder(black, xf.map({ 0, 0, -.02f }), xf.map({ 0, 0, .025f }), .045f, .04f, 20);
        b.cylinder(dark, xf.map({ 0, 0, -.02f }), xf.map({ 0, 0, -.40f }), .035f, .045f, 16); // column
        // paddles
        for (float sx : { -1.f, 1.f })
            b.box(trim, [&] { QMatrix4x4 m = xf; m.translate(sx * .09f, .05f, -.035f); return m; }(), { .035f, .012f, .004f });
    }

    // dashboard, binnacle, gauges
    b.box_at(dark, { cx, -.37f, -.86f }, { .95f, .09f, .20f });
    b.box_at(dark, { cx, -.29f, -1.03f }, { .95f, .03f, .17f }, { -6, 0, 0 });
    b.box_at(trim, { 0, -.205f, -.75f }, { .17f, .035f, .08f });          // binnacle hood
    b.box_at(dark, { 0, -.245f, -.78f }, { .17f, .035f, .06f });
    gauge(b, "rpm", { -.075f, -.255f, -.715f }, .052f, -10);
    gauge(b, "kmh", {  .075f, -.255f, -.715f }, .052f, -10);
    panel(b, "lap", { cx, -.34f, -.656f }, .16f, .08f, -8);
    gauge(b, "temp", { cx + .17f, -.35f, -.656f }, .032f, -8);
    gauge(b, "fuel", { cx + .26f, -.35f, -.656f }, .032f, -8);
    // floor + footwell
    b.box_at(dark, { cx, -1.0f, -.20f }, { .95f, .02f, 1.0f });
    b.box_at(dark, { cx, -.80f, -1.02f }, { .95f, .22f, .02f });
    // centre console + gear lever
    b.box_at(dark, { cx + .02f, -.60f, -.42f }, { .12f, .16f, .36f });
    b.cylinder(cage, { cx + .02f, -.44f, -.30f }, { cx + .02f, -.27f, -.33f }, .009f, .009f, 10);
    b.ellipsoid(black, place({ cx + .02f, -.26f, -.33f }), { .028f, .028f, .028f });

    // body shell: doors, sills, pillars, roof
    const float xl = -.55f, xr = 1.35f;
    for (float x : { xl, xr })
    {
        b.box_at(trim, { x, -.48f, -.15f }, { .03f, .32f, .70f });             // door panel
        b.box_at(paint, { x + (x < cx ? -.03f : .03f), -.16f, -.15f }, { .02f, .02f, .70f }); // belt line
        b.box_at(trim, { x + (x < cx ? .03f : -.03f), .08f, .52f }, { .04f, .26f, .07f });  // B-pillar
    }
    b.tube(trim, { { xl + .03f, -.20f, -.96f }, { -.42f, .33f, -.30f } }, .035f);  // A-pillars
    b.tube(trim, { { xr - .03f, -.20f, -.96f }, { 1.22f, .33f, -.30f } }, .035f);
    b.tube(trim, { { -.42f, .33f, -.30f }, { 1.22f, .33f, -.30f } }, .03f);       // header
    b.box_at(alcantara, { cx, .375f, .48f }, { .86f, .02f, .80f });                // roof lining
    b.box_at(black, { cx + .05f, .285f, -.38f }, { .10f, .028f, .01f });            // rear-view mirror
    b.cylinder(black, { cx + .05f, .30f, -.37f }, { cx + .05f, .37f, -.33f }, .007f, .007f, 8);
    // windscreen sun strip
    b.quad(solid(.05f, .05f, .08f), { -.42f, .27f, -.36f }, { 1.64f, 0, 0 }, { 0, .055f, .055f });

    // side mirrors (outside)
    for (float sx : { -1.f, 1.f })
    {
        const float x = sx < 0 ? xl - .17f : xr + .17f;
        b.ellipsoid(paint, place({ x, -.11f, -.74f }), { .085f, .055f, .045f });
        b.box_at(glass, { x, -.11f, -.70f }, { .07f, .042f, .006f });
        b.cylinder(paint, { x - sx * .02f, -.13f, -.74f }, { sx < 0 ? xl : xr, -.15f, -.76f }, .012f, .012f, 8);
    }

    // seats (shoulder wings in peripheral view) and belts
    for (float sx : { -1.f, 1.f })
        b.box_at(alcantara, { sx * .26f, -.42f, .18f }, { .035f, .26f, .15f }, { 0, sx * 12, 0 });
    b.box_at(alcantara, { cx + .78f, -.40f, .12f }, { .24f, .30f, .06f }); // passenger seat back
    for (float sx : { -1.f, 1.f })
        b.box_at(belt, { sx * .11f, -.32f, .08f }, { .022f, .20f, .004f }, { -15, 0, 0 });

    // roll cage
    const float cr = .021f;
    b.tube(cage, { { xl + .07f, -.95f, .58f }, { xl + .09f, .30f, .58f }, { xr - .09f, .30f, .58f }, { xr - .07f, -.95f, .58f } }, cr);
    b.tube(cage, { { xl + .09f, .30f, .58f }, { xr - .07f, -.95f, .62f } }, cr);           // diagonal
    b.tube(cage, { { -.40f, .31f, -.30f }, { xl + .08f, -.62f, -.90f } }, cr);             // A-pillar bars
    b.tube(cage, { { 1.20f, .31f, -.30f }, { xr - .08f, -.62f, -.90f } }, cr);
    b.tube(cage, { { -.40f, .31f, -.30f }, { xl + .09f, .30f, .58f } }, cr);              // roof bars
    b.tube(cage, { { 1.20f, .31f, -.30f }, { xr - .09f, .30f, .58f } }, cr);
    b.tube(cage, { { xl + .08f, -.70f, -.80f }, { xl + .08f, -.25f, .55f } }, cr);         // door bars
    b.tube(cage, { { xl + .08f, -.25f, -.80f }, { xl + .08f, -.70f, .55f } }, cr);

    // bonnet, wings, rear
    b.box_at(paint, { cx, -.39f, -1.70f }, { .93f, .07f, .72f }, { -4, 0, 0 });
    for (float x : { xl + .05f, xr - .05f })
        b.ellipsoid(paint, place({ x, -.40f, -1.75f }), { .14f, .12f, .55f });
    b.box_at(black, { cx, -.33f, -1.1f }, { .80f, .012f, .04f }); // wiper line
    b.tube(trim, { { -.38f, .30f, 1.02f }, { -.48f, -.10f, 1.65f } }, .05f); // C-pillars
    b.tube(trim, { { 1.18f, .30f, 1.02f }, { 1.28f, -.10f, 1.65f } }, .05f);
    b.box_at(trim, { cx, -.16f, 1.70f }, { .90f, .04f, .22f });
    b.box_at(black, { cx, .10f, 2.15f }, { .85f, .012f, .14f }, { -6, 0, 0 }); // rear wing
    for (float x : { cx - .5f, cx + .5f })
        b.box_at(black, { x, -.03f, 2.1f }, { .01f, .13f, .06f });

    (void)warnings;
    return s;
}

// ---------------------------------------------------------------- soldier

SceneDesc soldier(QStringList& warnings)
{
    SceneDesc s;
    s.name = "Soldier";
    s.sky_file = sky("mud_road");
    s.sun_dir = QVector3D(.25f, .9f, -.35f).normalized();
    s.sun_color = { .42f, .42f, .44f };
    s.sky_ambient = { .78f, .80f, .84f };
    s.ground_ambient = { .40f, .37f, .34f };
    s.fog_color = { .76f, .77f, .79f };
    s.fog_density = .004f;
    s.far_near = .3f;
    s.far_far = 3000;
    s.default_hfov = 90;

    const float ground = -1.65f;

    // muddy ground patch that fades into the panorama
    {
        Material mud;
        mud.tex_key = asset_dir() + "ground/brown_mud_leaves_01.jpg";
        mud.fade_start = 10;
        mud.fade_end = 26;
        const float half = 30;
        MeshBuilder(s.far_prims).quad(mud, { -half, ground, half }, { 2 * half, 0, 0 }, { 0, 0, -2 * half }, { 24, 24 });
    }

    // props around the position
    add_model(s.far_prims, "wooden_military_crate", place({ -1.35f, ground, .25f }, 75), warnings);
    add_model(s.far_prims, "metal_jerrycan_green", place({ 1.05f, ground, .55f }, -30), warnings);
    add_model(s.far_prims, "dead_tree_trunk", place({ -4.5f, ground, -9.f }, 25), warnings);
    add_model(s.far_prims, "concrete_road_barrier", place({ 5.2f, ground, -12.f }, -10), warnings);
    add_model(s.far_prims, "concrete_road_barrier", place({ 6.75f, ground, -12.4f }, -18), warnings);
    add_model(s.far_prims, "old_tyre", place({ 2.6f, ground + .083f, -6.2f }, 0, 90), warnings);
    add_model(s.far_prims, "old_tyre", place({ 2.75f, ground + .25f, -6.1f }, 20, 90), warnings);
    add_model(s.far_prims, "wooden_military_crate", place({ -7.f, ground, -16.f }, 30), warnings);
    // behind the position (for checking six)
    add_model(s.far_prims, "wooden_military_crate", place({ -1.8f, ground, 3.2f }, 10), warnings);
    add_model(s.far_prims, "wooden_military_crate", place({ -1.75f, ground + .465f, 3.15f }, 4), warnings);
    add_model(s.far_prims, "metal_jerrycan_green", place({ -.9f, ground, 3.4f }, 60), warnings);
    add_model(s.far_prims, "metal_jerrycan_green", place({ -.55f, ground, 3.3f }, 85), warnings);
    add_model(s.far_prims, "concrete_road_barrier", place({ 3.5f, ground, 8.f }, 70), warnings);
    add_model(s.far_prims, "dead_tree_trunk", place({ 6.f, ground, 14.f }, -40), warnings);

    // sandbag wall in an arc in front of the viewer
    {
        MeshBuilder b(s.near_prims);
        const Material bag[3] = { solid(.60f, .53f, .38f), solid(.55f, .48f, .34f), solid(.64f, .57f, .42f) };
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
                b.ellipsoid(bag[(k + row * 2 + 9) % 3], xf, { .19f, .09f, .15f }, 14, 8);
            }
        }
        const float top = ground + bag_h * rows;
        add_model(s.near_prims, "ammo_box", place({ .55f, top - .02f, -.98f }, 25), warnings);
        add_model(s.near_prims, "stick_grenade", place({ -.42f, top + .02f, -1.0f }, 70, 0, 90), warnings);
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
