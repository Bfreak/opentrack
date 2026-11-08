#include "scene-geometry.hpp"
#include "compat/base-path.hpp"
#include "compat/library-path.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#ifdef _MSC_VER
#   pragma warning(push, 0)
#endif
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#ifdef _MSC_VER
#   pragma warning(pop)
#endif

namespace head_test {

static constexpr float pi = 3.14159265358979f;

bool Material::same_as(const Material& o) const
{
    return tex_key == o.tex_key && color == o.color && unlit == o.unlit &&
           normal_key == o.normal_key && normal_scale == o.normal_scale && mr_key == o.mr_key &&
           roughness == o.roughness && metallic == o.metallic && ao_key == o.ao_key &&
           emissive_key == o.emissive_key && emissive == o.emissive && base_uv == o.base_uv && ao_uv1 == o.ao_uv1 &&
           blend == o.blend && cast_shadow == o.cast_shadow && shadow_catcher == o.shadow_catcher &&
           alpha_cutoff == o.alpha_cutoff && uv_scale == o.uv_scale &&
           fade_start == o.fade_start && fade_end == o.fade_end && mipmaps == o.mipmaps;
}

QString asset_dir()
{
    // OPENTRACK_LIBRARY_PATH starts and ends with a slash
    return OPENTRACK_BASE_PATH + OPENTRACK_LIBRARY_PATH "head-test/";
}

Prim& MeshBuilder::prim(const Material& m)
{
    for (auto& p : out)
        if (p.mat.same_as(m))
            return p;
    out.push_back({});
    out.back().mat = m;
    return out.back();
}

static void push(Prim& p, QVector3D pos, QVector3D n, float u, float v)
{
    p.verts.push_back({ pos.x(), pos.y(), pos.z(), n.x(), n.y(), n.z(), u, v, u, v });
}

static void push_quad_idx(Prim& p, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d)
{
    p.idx.insert(p.idx.end(), { a, b, c, a, c, d });
}

void MeshBuilder::box(const Material& m, const QMatrix4x4& xf, QVector3D h)
{
    Prim& p = prim(m);
    const QMatrix3x3 nm = xf.normalMatrix();
    auto nmap = [&](QVector3D n) {
        return QVector3D(nm(0,0)*n.x() + nm(0,1)*n.y() + nm(0,2)*n.z(),
                         nm(1,0)*n.x() + nm(1,1)*n.y() + nm(1,2)*n.z(),
                         nm(2,0)*n.x() + nm(2,1)*n.y() + nm(2,2)*n.z()).normalized();
    };
    static const QVector3D N[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    for (const QVector3D& n : N)
    {
        // two tangent axes
        QVector3D t1 = std::fabs(n.y()) > 0.5f ? QVector3D(1, 0, 0) : QVector3D(0, 1, 0);
        QVector3D t2 = QVector3D::crossProduct(n, t1);
        const auto base = (std::uint32_t)p.verts.size();
        const QVector3D wn = nmap(n);
        const float s[4][2] = { {-1,-1}, {1,-1}, {1,1}, {-1,1} };
        for (const auto& st : s)
        {
            QVector3D local = (n + t1 * st[0] + t2 * st[1]) * h;
            push(p, xf.map(local), wn, (st[0] + 1) * .5f, (st[1] + 1) * .5f);
        }
        push_quad_idx(p, base, base + 1, base + 2, base + 3);
    }
}

void MeshBuilder::box_at(const Material& m, QVector3D center, QVector3D half, QVector3D euler)
{
    QMatrix4x4 xf;
    xf.translate(center);
    if (!euler.isNull())
        xf.rotate(QQuaternion::fromEulerAngles(euler));
    box(m, xf, half);
}

void MeshBuilder::cylinder(const Material& m, QVector3D a, QVector3D b, float r0, float r1, int seg, bool caps)
{
    Prim& p = prim(m);
    QVector3D axis = b - a;
    const float len = axis.length();
    if (len < 1e-6f)
        return;
    axis /= len;
    QVector3D t1 = std::fabs(axis.y()) < 0.9f ? QVector3D::crossProduct(axis, {0, 1, 0}).normalized()
                                               : QVector3D::crossProduct(axis, {1, 0, 0}).normalized();
    QVector3D t2 = QVector3D::crossProduct(axis, t1);
    const float slope = (r0 - r1) / len;
    const auto base = (std::uint32_t)p.verts.size();
    for (int i = 0; i <= seg; i++)
    {
        float ang = 2 * pi * float(i) / float(seg);
        QVector3D dir = t1 * std::cos(ang) + t2 * std::sin(ang);
        QVector3D n = (dir + axis * slope).normalized();
        push(p, a + dir * r0, n, float(i) / float(seg), 0);
        push(p, b + dir * r1, n, float(i) / float(seg), 1);
    }
    for (int i = 0; i < seg; i++)
    {
        std::uint32_t k = base + std::uint32_t(i) * 2;
        push_quad_idx(p, k, k + 2, k + 3, k + 1);
    }
    if (caps)
    {
        for (int end = 0; end < 2; end++)
        {
            const QVector3D c = end ? b : a;
            const float r = end ? r1 : r0;
            if (r <= 0)
                continue;
            const QVector3D n = end ? axis : -axis;
            const auto cb = (std::uint32_t)p.verts.size();
            push(p, c, n, .5f, .5f);
            for (int i = 0; i <= seg; i++)
            {
                float ang = 2 * pi * float(i) / float(seg);
                push(p, c + (t1 * std::cos(ang) + t2 * std::sin(ang)) * r, n, .5f + .5f * std::cos(ang), .5f + .5f * std::sin(ang));
            }
            for (int i = 0; i < seg; i++)
                p.idx.insert(p.idx.end(), { cb, cb + 1 + std::uint32_t(i), cb + 2 + std::uint32_t(i) });
        }
    }
}

void MeshBuilder::tube(const Material& m, const std::vector<QVector3D>& pts, float r, int seg)
{
    for (std::size_t i = 0; i + 1 < pts.size(); i++)
    {
        cylinder(m, pts[i], pts[i + 1], r, r, seg, false);
        // joint
        QMatrix4x4 xf; xf.translate(pts[i + 1]);
        ellipsoid(m, xf, { r, r, r }, seg, seg / 2 + 1);
    }
    if (!pts.empty())
    {
        QMatrix4x4 xf; xf.translate(pts.front());
        ellipsoid(m, xf, { r, r, r }, seg, seg / 2 + 1);
    }
}

void MeshBuilder::torus(const Material& m, const QMatrix4x4& xf, float R, float r, int seg, int side, float a0, float a1)
{
    Prim& p = prim(m);
    const QMatrix3x3 nm = xf.normalMatrix();
    const auto base = (std::uint32_t)p.verts.size();
    for (int i = 0; i <= seg; i++)
    {
        float u = (a0 + (a1 - a0) * float(i) / float(seg)) * pi / 180;
        QVector3D c(R * std::cos(u), R * std::sin(u), 0);
        QVector3D radial(std::cos(u), std::sin(u), 0);
        for (int j = 0; j <= side; j++)
        {
            float v = 2 * pi * float(j) / float(side);
            QVector3D n = radial * std::cos(v) + QVector3D(0, 0, 1) * std::sin(v);
            QVector3D wn(nm(0,0)*n.x() + nm(0,1)*n.y() + nm(0,2)*n.z(),
                         nm(1,0)*n.x() + nm(1,1)*n.y() + nm(1,2)*n.z(),
                         nm(2,0)*n.x() + nm(2,1)*n.y() + nm(2,2)*n.z());
            push(p, xf.map(c + n * r), wn.normalized(), float(i) / float(seg), float(j) / float(side));
        }
    }
    const auto stride = std::uint32_t(side + 1);
    for (int i = 0; i < seg; i++)
        for (int j = 0; j < side; j++)
        {
            std::uint32_t k = base + std::uint32_t(i) * stride + std::uint32_t(j);
            push_quad_idx(p, k, k + stride, k + stride + 1, k + 1);
        }
}

void MeshBuilder::ellipsoid(const Material& m, const QMatrix4x4& xf, QVector3D radii, int seg, int rings)
{
    Prim& p = prim(m);
    const QMatrix3x3 nm = xf.normalMatrix();
    const auto base = (std::uint32_t)p.verts.size();
    for (int i = 0; i <= rings; i++)
    {
        float th = pi * float(i) / float(rings);
        for (int j = 0; j <= seg; j++)
        {
            float ph = 2 * pi * float(j) / float(seg);
            QVector3D unit(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
            QVector3D n(unit.x() / radii.x(), unit.y() / radii.y(), unit.z() / radii.z());
            QVector3D wn(nm(0,0)*n.x() + nm(0,1)*n.y() + nm(0,2)*n.z(),
                         nm(1,0)*n.x() + nm(1,1)*n.y() + nm(1,2)*n.z(),
                         nm(2,0)*n.x() + nm(2,1)*n.y() + nm(2,2)*n.z());
            push(p, xf.map(unit * radii), wn.normalized(), float(j) / float(seg), float(i) / float(rings));
        }
    }
    const auto stride = std::uint32_t(seg + 1);
    for (int i = 0; i < rings; i++)
        for (int j = 0; j < seg; j++)
        {
            std::uint32_t k = base + std::uint32_t(i) * stride + std::uint32_t(j);
            push_quad_idx(p, k, k + 1, k + stride + 1, k + stride);
        }
}

void MeshBuilder::disc(const Material& m, const QMatrix4x4& xf, float radius, int seg)
{
    Prim& p = prim(m);
    QVector3D n = xf.mapVector({ 0, 0, 1 }).normalized();
    const auto base = (std::uint32_t)p.verts.size();
    push(p, xf.map({ 0, 0, 0 }), n, .5f, .5f);
    for (int i = 0; i <= seg; i++)
    {
        float a = 2 * pi * float(i) / float(seg);
        float c = std::cos(a), s = std::sin(a);
        // texture top (v = 0) is at +Y
        push(p, xf.map({ c * radius, s * radius, 0 }), n, .5f + .5f * c, .5f - .5f * s);
    }
    for (int i = 0; i < seg; i++)
        p.idx.insert(p.idx.end(), { base, base + 1 + std::uint32_t(i), base + 2 + std::uint32_t(i) });
}

void MeshBuilder::quad(const Material& m, QVector3D c, QVector3D ex, QVector3D ey, QVector2D rep)
{
    Prim& p = prim(m);
    QVector3D n = QVector3D::crossProduct(ex, ey).normalized();
    const auto base = (std::uint32_t)p.verts.size();
    push(p, c, n, 0, rep.y());
    push(p, c + ex, n, rep.x(), rep.y());
    push(p, c + ex + ey, n, rep.x(), 0);
    push(p, c + ey, n, 0, 0);
    push_quad_idx(p, base, base + 1, base + 2, base + 3);
}

void bounds(const std::vector<Prim>& prims, std::size_t first, QVector3D& mn, QVector3D& mx)
{
    mn = QVector3D(1e9f, 1e9f, 1e9f);
    mx = -mn;
    for (std::size_t i = first; i < prims.size(); i++)
        for (const Vertex& v : prims[i].verts)
        {
            mn = QVector3D(std::min(mn.x(), v.px), std::min(mn.y(), v.py), std::min(mn.z(), v.pz));
            mx = QVector3D(std::max(mx.x(), v.px), std::max(mx.y(), v.py), std::max(mx.z(), v.pz));
        }
}

// ---- glTF ----

// cgltf file callbacks through QFile so that non-ASCII install paths work on Windows
static cgltf_result read_file(const cgltf_memory_options*, const cgltf_file_options*, const char* path, cgltf_size* size, void** data)
{
    QFile f(QString::fromUtf8(path));
    if (!f.open(QIODevice::ReadOnly))
        return cgltf_result_file_not_found;
    const QByteArray bytes = f.readAll();
    void* buf = std::malloc(std::size_t(bytes.size()) + 1);
    if (!buf)
        return cgltf_result_out_of_memory;
    std::memcpy(buf, bytes.constData(), std::size_t(bytes.size()));
    *size = cgltf_size(bytes.size());
    *data = buf;
    return cgltf_result_success;
}

static void release_file(const cgltf_memory_options*, const cgltf_file_options*, void* data)
{
    std::free(data);
}

bool load_gltf(const QString& path, const QMatrix4x4& placement, std::vector<Prim>& out, QString* error)
{
    cgltf_options opts {};
    opts.file.read = read_file;
    opts.file.release = release_file;
    cgltf_data* data = nullptr;
    const QByteArray path8 = path.toUtf8();

    auto fail = [&](const char* what) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, QString::fromLatin1(what));
        if (data)
            cgltf_free(data);
        return false;
    };

    if (cgltf_parse_file(&opts, path8.constData(), &data) != cgltf_result_success)
        return fail("parse error");
    if (cgltf_load_buffers(&opts, data, path8.constData()) != cgltf_result_success)
        return fail("can't load buffers");

    const QDir dir = QFileInfo(path).dir();
    std::vector<Prim> prims;

    for (cgltf_size ni = 0; ni < data->nodes_count; ni++)
    {
        const cgltf_node* node = &data->nodes[ni];
        if (!node->mesh)
            continue;
        float m[16];
        cgltf_node_transform_world(node, m);
        QMatrix4x4 node_xf(m);           // QMatrix4x4(const float*) is row-major input...
        node_xf = node_xf.transposed();  // ...cgltf gives column-major
        const QMatrix4x4 xf = placement * node_xf;
        const QMatrix3x3 nm = xf.normalMatrix();

        for (cgltf_size pi_ = 0; pi_ < node->mesh->primitives_count; pi_++)
        {
            const cgltf_primitive& pr = node->mesh->primitives[pi_];
            if (pr.type != cgltf_primitive_type_triangles)
                continue;
            const cgltf_accessor *pos = nullptr, *nrm = nullptr, *uv = nullptr, *uv1 = nullptr;
            for (cgltf_size ai = 0; ai < pr.attributes_count; ai++)
            {
                const cgltf_attribute& a = pr.attributes[ai];
                if (a.type == cgltf_attribute_type_position) pos = a.data;
                else if (a.type == cgltf_attribute_type_normal) nrm = a.data;
                else if (a.type == cgltf_attribute_type_texcoord && a.index == 0) uv = a.data;
                else if (a.type == cgltf_attribute_type_texcoord && a.index == 1) uv1 = a.data;
            }
            if (!pos)
                continue;

            Prim p;
            if (const cgltf_material* mat = pr.material)
            {
                auto tex_path = [&](const cgltf_texture_view& tv) -> QString {
                    if (!tv.texture || !tv.texture->image || !tv.texture->image->uri)
                        return {};
                    QByteArray uri(tv.texture->image->uri);
                    cgltf_decode_uri(uri.data());
                    return dir.filePath(QString::fromUtf8(uri.constData()));
                };
                const auto& pbr = mat->pbr_metallic_roughness;
                p.mat.color = { pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2], pbr.base_color_factor[3] };
                // base_color_factor is linear; Material::color is sRGB
                for (int c = 0; c < 3; c++)
                    p.mat.color[c] = std::pow(std::max(0.f, p.mat.color[c]), 1 / 2.2f);
                p.mat.tex_key = tex_path(pbr.base_color_texture);
                if (pbr.base_color_texture.has_transform)
                {
                    const auto& t = pbr.base_color_texture.transform;
                    p.mat.base_uv = { t.scale[0], t.scale[1], t.offset[0], t.offset[1] };
                }
                p.mat.roughness = pbr.roughness_factor;
                p.mat.metallic = pbr.metallic_factor;
                p.mat.mr_key = tex_path(pbr.metallic_roughness_texture);
                p.mat.normal_key = tex_path(mat->normal_texture);
                p.mat.normal_scale = mat->normal_texture.texture ? mat->normal_texture.scale : 1.f;
                p.mat.ao_key = tex_path(mat->occlusion_texture);
                p.mat.ao_uv1 = mat->occlusion_texture.texcoord == 1;
                if (p.mat.ao_uv1 && !uv1)
                    p.mat.ao_key.clear();
                p.mat.emissive_key = tex_path(mat->emissive_texture);
                const float es = mat->has_emissive_strength ? mat->emissive_strength.emissive_strength : 1.f;
                p.mat.emissive = QVector3D(mat->emissive_factor[0], mat->emissive_factor[1], mat->emissive_factor[2]) * es;
                p.mat.unlit = mat->unlit;
                if (mat->alpha_mode == cgltf_alpha_mode_mask)
                    p.mat.alpha_cutoff = mat->alpha_cutoff;
                else if (mat->alpha_mode == cgltf_alpha_mode_blend)
                    p.mat.blend = true;
                if (mat->has_transmission && mat->transmission.transmission_factor > .1f)
                {
                    // see-through glass: tinted, mostly transparent, reflective
                    p.mat.blend = true;
                    p.mat.color[3] = std::clamp(1.f - mat->transmission.transmission_factor, .08f, 1.f);
                    p.mat.roughness = std::min(p.mat.roughness, .1f);
                    p.mat.cast_shadow = false;
                }
                if (p.mat.blend && p.mat.color[3] < .5f)
                    p.mat.cast_shadow = false;
            }

            const cgltf_size n = pos->count;
            p.verts.resize(n);
            for (cgltf_size i = 0; i < n; i++)
            {
                float v3[3] = {}, n3[3] = { 0, 1, 0 }, t2[2] = {}, t3[2] = {};
                cgltf_accessor_read_float(pos, i, v3, 3);
                if (nrm) cgltf_accessor_read_float(nrm, i, n3, 3);
                if (uv) cgltf_accessor_read_float(uv, i, t2, 2);
                if (uv1) cgltf_accessor_read_float(uv1, i, t3, 2);
                QVector3D wp = xf.map(QVector3D(v3[0], v3[1], v3[2]));
                QVector3D wn(nm(0,0)*n3[0] + nm(0,1)*n3[1] + nm(0,2)*n3[2],
                             nm(1,0)*n3[0] + nm(1,1)*n3[1] + nm(1,2)*n3[2],
                             nm(2,0)*n3[0] + nm(2,1)*n3[1] + nm(2,2)*n3[2]);
                wn.normalize();
                p.verts[i] = { wp.x(), wp.y(), wp.z(), wn.x(), wn.y(), wn.z(), t2[0], t2[1], t3[0], t3[1] };
            }
            if (pr.indices)
            {
                p.idx.resize(pr.indices->count);
                for (cgltf_size i = 0; i < pr.indices->count; i++)
                    p.idx[i] = std::uint32_t(cgltf_accessor_read_index(pr.indices, i));
            }
            else
            {
                p.idx.resize(n);
                for (cgltf_size i = 0; i < n; i++)
                    p.idx[i] = std::uint32_t(i);
            }
            prims.push_back(std::move(p));
        }
    }
    cgltf_free(data);

    // merge into out (same material -> same prim)
    for (Prim& p : prims)
    {
        Prim* dst = nullptr;
        for (Prim& o : out)
            if (o.mat.same_as(p.mat)) { dst = &o; break; }
        if (!dst)
        {
            out.push_back(std::move(p));
            continue;
        }
        const auto off = (std::uint32_t)dst->verts.size();
        dst->verts.insert(dst->verts.end(), p.verts.begin(), p.verts.end());
        for (std::uint32_t i : p.idx)
            dst->idx.push_back(i + off);
    }
    return true;
}

} // namespace head_test
