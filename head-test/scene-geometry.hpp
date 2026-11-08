#pragma once

// CPU-side scene description for the head-tracking test window.
// Geometry is baked into world space when a scene is built; the renderer uploads it as-is.

#include <vector>
#include <cstdint>

#include <QString>
#include <QImage>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>

namespace head_test {

struct Material
{
    QString tex_key;           // base colour: file path, or "gen:<name>" for a generated image; empty = untextured
    QImage tex_image;          // set for generated textures
    QVector4D color { 1, 1, 1, 1 }; // sRGB, multiplied with the texture
    QString normal_key;        // tangent-space normal map (OpenGL convention)
    float normal_scale = 1;
    QString mr_key;            // glTF metallic-roughness: G = roughness, B = metallic
    float roughness = .75f, metallic = 0;
    QString ao_key;            // occlusion in R
    bool ao_uv1 = false;       // occlusion uses the second UV set
    QString emissive_key;
    QVector3D emissive { 0, 0, 0 }; // linear, multiplied with the emissive texture
    QVector4D base_uv { 1, 1, 0, 0 }; // base/emissive texture transform: scale.xy, offset.zw
    bool unlit = false;        // instruments, emissive bits
    float alpha_cutoff = -1;   // >= 0: alpha-tested
    bool blend = false;        // alpha-blended (glass)
    bool cast_shadow = true;
    bool shadow_catcher = false; // invisible except for the sun shadows falling on it (ground under vehicles)
    float uv_scale = 1;        // texture tiling
    float fade_start = 0, fade_end = 0; // horizontal distance fade-out (ground patches blending into the sky)
    bool mipmaps = true;

    bool same_as(const Material& o) const;
};

struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float u1, v1;   // second UV set (lightmap-style occlusion)
};

struct Prim
{
    std::vector<Vertex> verts;
    std::vector<std::uint32_t> idx;
    Material mat;
};

enum class Layer { far_, near_ };

struct SceneDesc
{
    QString name;
    QString sky_file;
    float sky_yaw_deg = 0;      // rotates the panorama around the vertical axis
    QVector3D sun_dir { 0.3f, 0.8f, -0.4f };
    QVector3D sun_color { 1.0f, 0.95f, 0.85f };   // sRGB
    float sun_strength = 3.0f;    // multiplier on sun_color (linear)
    float ambient = 1.0f;         // multiplier on sky-based ambient light and reflections
    float exposure = 1.0f;
    float shadow_radius = 0;      // half-size of the sun shadow map around the eye (m); 0 = no shadows
    bool sun_from_sky = false;    // find the sun in the panorama (brightest spot) instead of using sun_dir
    QVector3D sky_ambient { 0.45f, 0.5f, 0.6f };  // only used when the sky image is missing
    QVector3D ground_ambient { 0.25f, 0.22f, 0.2f };
    QVector3D fog_color { 0.7f, 0.75f, 0.8f };
    float fog_density = 0;      // far layer only (1/m)
    bool fog_from_sky = false;  // use the panorama's horizon colour as fog colour
    bool fill_below_horizon = false; // paint the panorama's lower half with the fog colour (aircraft at altitude)
    float far_near = 0.5f, far_far = 2000.f; // clip planes for the far layer
    float default_hfov = 90;
    std::vector<Prim> far_prims, near_prims;
};

// Appends simple shapes. Primitives with identical materials are merged.
class MeshBuilder
{
public:
    explicit MeshBuilder(std::vector<Prim>& out) : out(out) {}

    Prim& prim(const Material& m);

    // axis-aligned unit box scaled to half-extents, then transformed
    void box(const Material& m, const QMatrix4x4& xf, QVector3D half);
    void box_at(const Material& m, QVector3D center, QVector3D half, QVector3D euler_deg = {});
    // cylinder/cone between two points
    void cylinder(const Material& m, QVector3D a, QVector3D b, float r0, float r1, int seg = 20, bool caps = true);
    // tube following a polyline
    void tube(const Material& m, const std::vector<QVector3D>& pts, float r, int seg = 12);
    // torus in the local XY plane, facing +Z
    void torus(const Material& m, const QMatrix4x4& xf, float R, float r, int seg = 48, int side = 12, float a0_deg = 0, float a1_deg = 360);
    void ellipsoid(const Material& m, const QMatrix4x4& xf, QVector3D radii, int seg = 18, int rings = 10);
    // textured disc in the local XY plane facing +Z, the texture's top at +Y
    void disc(const Material& m, const QMatrix4x4& xf, float radius, int seg = 48);
    // quad from a corner and two edge vectors, normal = ex x ey; uv repeats `rep` times
    void quad(const Material& m, QVector3D corner, QVector3D ex, QVector3D ey, QVector2D rep = { 1, 1 });

private:
    std::vector<Prim>& out;
};

// Loads a glTF 2.0 file (base colour only) and appends its primitives transformed by `placement`.
// Returns false (and appends nothing) on error.
bool load_gltf(const QString& path, const QMatrix4x4& placement, std::vector<Prim>& out, QString* error = nullptr);

// Axis-aligned bounds of primitives appended after index `first`.
void bounds(const std::vector<Prim>& prims, std::size_t first, QVector3D& mn, QVector3D& mx);

QString asset_dir();

} // namespace head_test
