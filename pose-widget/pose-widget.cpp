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
        // jaw: narrower and shallower towards the chin
        const float t = (-dy - .05f) / .95f;
        x *= 1 - .30f * t * t;
        z *= dz > 0 ? 1 - .10f * t * t : 1 - .38f * t;
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
    // mannequin features (no eyes or mouth): nose, brow, eye hollows, cheekbones, chin
    z += .15f * bump(0, -.10f, .085f, .20f) * front;            // nose
    z += .05f * bump(0, .20f, .42f, .06f) * front;              // brow
    z -= .05f * (bump(-.27f, .09f, .11f, .07f) + bump(.27f, .09f, .11f, .07f)) * front; // eye hollows
    z += .03f * (bump(-.40f, -.12f, .12f, .10f) + bump(.40f, -.12f, .12f, .10f)) * front; // cheekbones
    z += .09f * bump(0, -.84f, .20f, .10f) * front;             // chin
    return { x, y + .08f, z };
}

void pose_widget::build_head()
{
    verts.clear();
    tris.clear();

    // head: latitude/longitude grid
    constexpr int rings = 44, segs = 60;
    for (int i = 0; i <= rings; i++)
        for (int j = 0; j < segs; j++)
            verts.push_back(head_point(pi * float(i) / rings, 2 * pi * float(j) / segs));
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
    if (dark_theme)
    {
        // a darker, slightly violet version of the grid
        background = QImage(":/images/grid-dark.png").convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < background.height(); y++)
        {
            QRgb* row = reinterpret_cast<QRgb*>(background.scanLine(y));
            for (int x = 0; x < background.width(); x++)
            {
                const QRgb c = row[x];
                row[x] = qRgba(int(qRed(c) * .38f) + 4, int(qGreen(c) * .36f) + 3, int(qBlue(c) * .42f) + 9, qAlpha(c));
            }
        }
    }
    else
        background = QImage(":/images/grid-light.png");
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
    p.fillRect(rect(), palette().brush(backgroundRole()));
    if (!background.isNull())
        p.drawImage(rect(), background);
    {
        p.save();
        p.setPen(QPen(QColor(70, 66, 84), 1, Qt::SolidLine));
        const int w = width(), h = height();
        p.drawLine(w / 2, 0, w / 2, h);
        p.drawLine(0, h / 2, w, h / 2);
        p.restore();
    }

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
