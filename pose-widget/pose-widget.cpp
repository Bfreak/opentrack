/* Copyright (c) 2013, 2015 Stanislaw Halik <sthalik@misaki.pl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

// Trackhat: the 2D "octopus" picture is replaced by a shaded, featureless 3D head.

#include "pose-widget.hpp"
#include "compat/check-visible.hpp"
#include "compat/math.hpp"
#include "opentrack/defs.hpp"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPolygonF>
#include <QtEvents>
#include <QMatrix4x4>

namespace pose_widget_impl {

static constexpr float pi = 3.14159265358979f;

pose_widget::pose_widget(QWidget* parent) : QWidget(parent)
{
    mirror.setFocusPolicy(Qt::NoFocus);
#ifdef UI_COMPACT_VIDEO_FEED
    mirror.setVisible(false);
#endif
    build_head();
}

// Mannequin-style head in model space: +X right, +Y up, +Z towards the viewer (face), ~2 units tall.
static QVector3D head_point(float theta, float phi)
{
    // unit direction, phi = 0 faces +Z
    const float dx = std::sin(theta) * std::sin(phi), dy = std::cos(theta), dz = std::sin(theta) * std::cos(phi);
    float x = dx * .74f, y = dy * .98f, z = dz * .86f;

    if (dy < -.05f)
    {
        // jaw: stays broad, then turns in at a defined angle towards the chin
        const float t = (-dy - .05f) / .95f;
        x *= 1 - .34f * t * t * t - .06f * t;
        z *= dz > 0 ? 1 - .06f * t * t : 1 - .38f * t;
    }
    if (dz < 0 && dy > -.2f)
        z *= 1.06f + .04f * dy;           // back of the skull
    const float front = std::max(dz, 0.f);
    if (dz > 0)
    {
        // flatter face plane and a defined jaw line
        z = std::min(z, .74f + .06f * front);
        z *= .97f;
    }
    auto bump = [&](float cx, float cy, float sx, float sy) {
        const float u = (dx - cx) / sx, v = (dy - cy) / sy;
        return std::exp(-(u * u + v * v));
    };
    auto smooth = [](float e0, float e1, float v) {
        const float t = std::clamp((v - e0) / (e1 - e0), 0.f, 1.f);
        return t * t * (3 - 2 * t);
    };
    // minimalist mannequin: only three bold features (no eyes or mouth)
    {
        // nose: a straight wedge from between the brows to a crisp tip, cut off underneath
        const float along = smooth(.20f, -.17f, dy) * (1 - smooth(-.19f, -.27f, dy));
        const float half_width = .045f + .055f * along;
        const float across = std::exp(-(dx / half_width) * (dx / half_width));
        z += .24f * along * across * front;
    }
    z += .08f * bump(0, .21f, .46f, .055f) * front;             // brow ridge
    z += .14f * bump(0, -.86f, .24f, .09f) * front;             // chin
    return { x, y + .08f, z };
}

void pose_widget::build_head()
{
    verts.clear();
    tris.clear();

    // head: latitude/longitude grid
    // longitudes are packed towards the face (phi = 0) so the narrow nose ridge is resolved
    constexpr int rings = 56, segs = 80;
    for (int i = 0; i <= rings; i++)
        for (int j = 0; j < segs; j++)
        {
            const float s = float(j) / segs - .5f;                     // -0.5 .. 0.5, 0 = front
            const float phi = pi * (s < 0 ? -1.f : 1.f) * std::pow(std::fabs(2 * s), 1.7f);
            verts.push_back(head_point(pi * float(i) / rings, phi));
        }
    auto H = [&](int i, int j) { return i * segs + (j % segs); };
    for (int i = 0; i < rings; i++)
        for (int j = 0; j < segs; j++)
        {
            tris.push_back({ H(i, j), H(i + 1, j), H(i + 1, j + 1) });
            tris.push_back({ H(i, j), H(i + 1, j + 1), H(i, j + 1) });
        }

    // neck: slightly flared tube
    const int base = int(verts.size());
    // starts well inside the skull so the joint is hidden from every side
    constexpr int nrings = 6, nsegs = 40;
    for (int i = 0; i <= nrings; i++)
    {
        const float t = float(i) / nrings;
        const float y = -.25f - t * 1.0f;
        const float flare = 1 + .40f * t * t * t;
        for (int j = 0; j < nsegs; j++)
        {
            const float a = 2 * pi * float(j) / nsegs;
            verts.push_back({ .30f * flare * std::sin(a), y, -.16f + .32f * flare * std::cos(a) });
        }
    }
    auto N = [&](int i, int j) { return base + i * nsegs + (j % nsegs); };
    for (int i = 0; i < nrings; i++)
        for (int j = 0; j < nsegs; j++)
        {
            tris.push_back({ N(i, j), N(i + 1, j), N(i + 1, j + 1) });
            tris.push_back({ N(i, j), N(i + 1, j + 1), N(i, j + 1) });
        }

    // smooth vertex normals, oriented outwards
    normals.assign(verts.size(), QVector3D());
    for (const auto& t : tris)
    {
        const QVector3D n = QVector3D::crossProduct(verts[t[1]] - verts[t[0]], verts[t[2]] - verts[t[0]]);
        for (int k : t)
            normals[k] += n;
    }
    for (std::size_t i = 0; i < verts.size(); i++)
    {
        QVector3D n = normals[i].normalized();
        QVector3D out = verts[i];
        if (int(i) >= base)
            out = QVector3D(verts[i].x(), 0, verts[i].z() + .16f);
        if (QVector3D::dotProduct(n, out) < 0)
            n = -n;
        normals[i] = n;
    }
}

void pose_widget::set_grid_background(bool dark_theme)
{
    dark_grid = dark_theme;
    update();
}

// Vector perspective "room" behind the head (replaces the old grid PNGs): stays sharp at any size/DPI.
void pose_widget::draw_grid(QPainter& p) const
{
    const float w = float(width()), h = float(height());
    const QPointF c(w * .5, h * .5);
    constexpr float depth_k = 1.75f; // the back wall is 1 / (1 + k) the size of the widget
    // X, Y in [-1, 1] across the room, Z from 0 (screen) to 1 (back wall)
    auto proj = [&](float X, float Y, float Z) {
        const float s = 1 / (1 + depth_k * Z);
        return QPointF(c.x() + X * s * w * .5f, c.y() - Y * s * h * .5f);
    };
    const QColor line = dark_grid ? QColor(150, 136, 205) : QColor(70, 70, 100);
    auto pen = [&](float alpha) {
        QColor col = line;
        col.setAlphaF(std::clamp(alpha, 0.f, 1.f));
        QPen pn(col, 1);
        pn.setCosmetic(true);
        return pn;
    };
    // lines running into the room fade with distance
    auto depth_line = [&](float X, float Y, float a_near, float a_far) {
        const QPointF a = proj(X, Y, 0), b = proj(X, Y, 1);
        QLinearGradient g(a, b);
        QColor cn = line, cf = line;
        cn.setAlphaF(a_near);
        cf.setAlphaF(a_far);
        g.setColorAt(0, cn);
        g.setColorAt(1, cf);
        QPen pn(QBrush(g), 1);
        pn.setCosmetic(true);
        p.setPen(pn);
        p.drawLine(a, b);
    };

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // soft vignette instead of a flat fill
    QRadialGradient bg(c, std::max(w, h) * .75f);
    bg.setColorAt(0, dark_grid ? QColor(26, 23, 38) : QColor(238, 238, 244));
    bg.setColorAt(1, dark_grid ? QColor(9, 9, 13) : QColor(206, 206, 216));
    p.fillRect(rect(), bg);

    // floor, ceiling and side walls
    constexpr int cols = 8, rows = 6;
    for (int k = 1; k < cols; k++)
        for (float Y : { -1.f, 1.f })
            depth_line(-1 + 2.f * k / cols, Y, .42f, .14f);
    for (int k = 1; k < rows; k++)
        for (float X : { -1.f, 1.f })
            depth_line(X, -1 + 2.f * k / rows, .42f, .14f);
    for (float X : { -1.f, 1.f })
        for (float Y : { -1.f, 1.f })
            depth_line(X, Y, .60f, .30f); // room edges

    // depth frames, fading towards the back
    constexpr int frames = 7;
    for (int i = 1; i < frames; i++)
    {
        const float Z = float(i) / frames;
        p.setPen(pen(.10f + .26f * (1 - Z)));
        p.drawPolygon(QPolygonF { proj(-1, -1, Z), proj(1, -1, Z), proj(1, 1, Z), proj(-1, 1, Z) });
    }

    // back wall
    p.setPen(pen(.16f));
    for (int k = 1; k < cols; k++)
    {
        const float X = -1 + 2.f * k / cols;
        p.drawLine(proj(X, -1, 1), proj(X, 1, 1));
    }
    for (int k = 1; k < rows; k++)
    {
        const float Y = -1 + 2.f * k / rows;
        p.drawLine(proj(-1, Y, 1), proj(1, Y, 1));
    }
    p.setPen(pen(.55f));
    p.drawPolygon(QPolygonF { proj(-1, -1, 1), proj(1, -1, 1), proj(1, 1, 1), proj(-1, 1, 1) });

    // centre cross-hair
    p.setPen(pen(.30f));
    p.drawLine(QPointF(c.x(), 0), QPointF(c.x(), h));
    p.drawLine(QPointF(0, c.y()), QPointF(w, c.y()));
    p.restore();
}

void pose_widget::present(double yaw, double pitch, double roll, double x, double y, double z)
{
    T = { x, y, z };
    R = { yaw, pitch, roll };

    repaint();
}

void pose_widget::resizeEvent(QResizeEvent *event)
{
    float w = event->size().width();
    float h = event->size().height();

    // move the mirror checkbox in the lower right corner of the widget
    mirror.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    mirror.move(w - mirror.width(), h - mirror.height());
}

void pose_widget::paintEvent(QPaintEvent*)
{
    constexpr float XYZmax = 50;    // cm for one head height of on-screen movement
    constexpr float Kz = .25f;      // depth scaling

    // the caller passes roll already negated (see main_window::show_pose_)
    auto [ yaw, pitch, roll ] = R;
    auto [ x, y, z ] = T;
    const bool mirrored = mirror.checkState() == Qt::Checked;

    QPainter p(this);
    draw_grid(p);

    // Camera view of the user (mirror off): turning right turns the face towards screen left.
    QMatrix4x4 rot;
    rot.rotate(float(mirrored ? yaw : -yaw), 0, 1, 0);
    rot.rotate(float(-pitch), 1, 0, 0);
    rot.rotate(float(mirrored ? roll : -roll), 0, 0, 1);

    const float unit = height() * .19f * std::exp(-float(z) * Kz / XYZmax);
    const float head_px = 2.2f * unit;
    const QPointF centre(width() * .5 + (mirrored ? x : -x) / XYZmax * head_px,
                         height() * .5 - y / XYZmax * head_px);
    constexpr float cam = 7;  // mild perspective

    const std::size_t nv = verts.size();
    xverts.resize(nv);
    xnormals.resize(nv);
    for (std::size_t i = 0; i < nv; i++)
    {
        xverts[i] = rot.map(verts[i]);
        xnormals[i] = rot.mapVector(normals[i]);
    }
    // software rasteriser: 2x supersampled, z-buffered, per-pixel lighting from interpolated normals
    constexpr int ss = 2;
    const qreal dpr = devicePixelRatioF();
    const int W = int(width() * dpr * ss), Hh = int(height() * dpr * ss);
    const float k = float(dpr * ss);
    QImage layer(W, Hh, QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);
    zbuf.assign(std::size_t(W) * std::size_t(Hh), -1e9f);

    auto project = [&](const QVector3D& v) {
        const float s = cam / (cam - v.z());
        return QPointF((centre.x() + v.x() * s * unit) * k, (centre.y() - v.y() * s * unit) * k);
    };
    sverts.resize(nv);
    for (std::size_t i = 0; i < nv; i++)
        sverts[i] = project(xverts[i]);

    const QVector3D L = QVector3D(-.45f, .65f, .62f).normalized();   // key light: top left front
    const QVector3D base(.80f, .80f, .86f), rim_col(.55f, .36f, .96f);

    for (const auto& tr : tris)
    {
        const QPointF a = sverts[tr[0]], b = sverts[tr[1]], c = sverts[tr[2]];
        const double area = (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
        if (area >= -1e-9)
            continue; // back-facing (y down => clockwise is front-facing for our winding)
        const int x0 = std::max(0, int(std::floor(std::min({ a.x(), b.x(), c.x() }))));
        const int x1 = std::min(W - 1, int(std::ceil(std::max({ a.x(), b.x(), c.x() }))));
        const int y0 = std::max(0, int(std::floor(std::min({ a.y(), b.y(), c.y() }))));
        const int y1 = std::min(Hh - 1, int(std::ceil(std::max({ a.y(), b.y(), c.y() }))));
        const QVector3D &na = xnormals[tr[0]], &nb = xnormals[tr[1]], &nc = xnormals[tr[2]];
        const float za = xverts[tr[0]].z(), zb = xverts[tr[1]].z(), zc = xverts[tr[2]].z();
        const double inv = 1 / area;
        for (int py = y0; py <= y1; py++)
        {
            QRgb* row = reinterpret_cast<QRgb*>(layer.scanLine(py));
            float* zrow = zbuf.data() + std::size_t(py) * std::size_t(W);
            const double fy = py + .5;
            for (int px = x0; px <= x1; px++)
            {
                const double fx = px + .5;
                const double w0 = ((b.x() - fx) * (c.y() - fy) - (b.y() - fy) * (c.x() - fx)) * inv;
                const double w1 = ((c.x() - fx) * (a.y() - fy) - (c.y() - fy) * (a.x() - fx)) * inv;
                const double w2 = 1 - w0 - w1;
                if (w0 < -1e-6 || w1 < -1e-6 || w2 < -1e-6)
                    continue;
                const float zz = float(w0 * za + w1 * zb + w2 * zc);
                if (zz <= zrow[px])
                    continue;
                zrow[px] = zz;
                const QVector3D n = (na * float(w0) + nb * float(w1) + nc * float(w2)).normalized();
                const float diff = std::max(QVector3D::dotProduct(n, L), 0.f);
                const float rim = std::pow(1 - std::max(n.z(), 0.f), 3.f);
                const QVector3D col = base * (.22f + .78f * diff) + rim_col * (rim * .85f);
                row[px] = qRgb(int(std::min(col.x(), 1.f) * 255), int(std::min(col.y(), 1.f) * 255),
                               int(std::min(col.z(), 1.f) * 255));
            }
        }
    }
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(rect(), layer);
}

} // ns pose_widget_impl
