/* Copyright (c) 2013, 2015 Stanislaw Halik <sthalik@misaki.pl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

// Trackhat: the 2D "octopus" picture is replaced by a shaded mannequin head with a 3D crosshair
// and a reticle; see pose-widget.hpp.

#include "pose-widget.hpp"
#include "compat/check-visible.hpp"
#include "compat/math.hpp"
#include "opentrack/defs.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QMatrix4x4>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QtEvents>

namespace pose_widget_impl {

static constexpr float pi = 3.14159265358979f;

pose_widget::pose_widget(QWidget* parent) : QWidget(parent)
{
    mirror.setFocusPolicy(Qt::NoFocus);
#ifdef UI_COMPACT_VIDEO_FEED
    mirror.setVisible(false);
#endif
    build_head();
    clock.start();
}

// ---------------------------------------------------------------- head model
// Model space: +X right, +Y up, +Z towards the viewer (face). The head spans y = -1 (cut just
// under the jaw, no neck) to y = +1 (crown).

namespace {

float gauss(float v) { return std::exp(-v * v); }

float smoothstep(float e0, float e1, float v)
{
    const float t = std::clamp((v - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}

// Catmull-Rom through (y, value) control points sorted by descending y
float profile(std::initializer_list<std::pair<float, float>> pts, float y)
{
    const auto* p = pts.begin();
    const int n = int(pts.size());
    if (y >= p[0].first)
        return p[0].second;
    if (y <= p[n - 1].first)
        return p[n - 1].second;
    int i = 0;
    while (i + 1 < n && p[i + 1].first > y)
        i++;
    const float t = (p[i].first - y) / (p[i].first - p[i + 1].first);
    const float v0 = p[std::max(i - 1, 0)].second, v1 = p[i].second, v2 = p[i + 1].second,
                v3 = p[std::min(i + 2, n - 1)].second;
    return .5f * ((2 * v1) + (-v0 + v2) * t + (2 * v0 - 5 * v1 + 4 * v2 - v3) * t * t +
                  (-v0 + 3 * v1 - 3 * v2 + v3) * t * t * t);
}

constexpr float y_dome = .30f; // above this the skull is an ellipsoidal dome

float dome(float r, float y)
{
    if (y <= y_dome)
        return r;
    const float t = (y - y_dome) / (1 - y_dome);
    return r * std::sqrt(std::max(0.f, 1 - t * t));
}

float front_depth(float y)
{
    return dome(profile({ { .30f, .78f }, { 0, .76f }, { -.45f, .76f }, { -.80f, .72f }, { -.93f, .60f }, { -1, .46f } }, y), y);
}

float back_depth(float y)
{
    return dome(profile({ { .30f, .93f }, { 0, .91f }, { -.30f, .80f }, { -.55f, .60f }, { -.80f, .45f }, { -1, .40f } }, y), y);
}

float half_width(float y)
{
    return dome(profile({ { .30f, .70f }, { 0, .68f }, { -.25f, .665f }, { -.50f, .63f }, { -.70f, .56f }, { -.88f, .41f }, { -1, .30f } }, y), y);
}

// u: angle around the head, 0 = front (+Z), +90 deg = +X. scale < 1 is used for the flat base.
QVector3D head_point(float y, float u, float scale)
{
    const float s = std::sin(u), c = std::cos(u);
    const float W = half_width(y) * scale;
    float x, z;
    if (c >= 0)
    {
        // the face is a little flatter than an ellipse
        constexpr float e = .86f;
        x = W * (s < 0 ? -1.f : 1.f) * std::pow(std::fabs(s), e);
        z = front_depth(y) * scale * std::pow(c, e);
    }
    else
    {
        x = W * s;
        z = back_depth(y) * scale * c;
    }
    if (c <= 0 || scale < 1)
        return { x, y, z };

    const float facing = smoothstep(0, .40f, c);
    const float ax = std::fabs(x);
    float d = 0;

    // brow ridge and eye sockets
    d += .034f * gauss((y - .285f) / .080f) * smoothstep(.66f, .30f, ax);
    d -= .062f * gauss((ax - .30f) / .125f) * gauss((y - .15f) / .095f);
    // cheekbones
    d += .028f * gauss((ax - .43f) / .15f) * gauss((y + .03f) / .15f);
    // nose: a ridge that grows from between the brows to the tip, cut off underneath
    {
        constexpr float y0 = .25f, y1 = -.17f, tip = .25f;
        float h = 0, wd = .06f;
        if (y > y0)
            h = .03f * smoothstep(y0 + .08f, y0, y);
        else if (y >= y1)
        {
            const float t = (y0 - y) / (y0 - y1);
            h = .03f + (tip - .03f) * std::pow(t, 1.2f);
            wd = .055f + .085f * std::pow(t, 1.6f);
        }
        else
        {
            h = tip * (1 - smoothstep(y1, y1 - .085f, y));
            wd = .14f;
        }
        d += h * gauss(x / wd);
    }
    // mouth: a soft muzzle, two lips and the line between them
    d += .026f * gauss((y + .46f) / .15f) * gauss(x / .27f);
    d += .026f * gauss((y + .405f) / .042f) * gauss(x / .19f);
    d += .026f * gauss((y + .515f) / .048f) * gauss(x / .16f);
    d -= .018f * gauss((y + .458f) / .022f) * gauss(x / .20f);
    // chin
    d += .060f * gauss((y + .80f) / .10f) * gauss(x / .21f);

    return { x, y, z + d * facing };
}

} // namespace

void pose_widget::build_head()
{
    verts.clear();
    tris.clear();
    normals.clear();

    // Adds a closed lat-long surface; ring 0 is the top. Normals are area-weighted and made to
    // point away from `centre`.
    auto add_surface = [&](int rings, int segs, QVector3D centre, auto&& point) {
        const int base = int(verts.size());
        for (int i = 0; i <= rings; i++)
            for (int j = 0; j < segs; j++)
                verts.push_back(point(i, j));
        auto at = [&](int i, int j) { return base + i * segs + (j % segs); };
        const std::size_t first_tri = tris.size();
        for (int i = 0; i < rings; i++)
            for (int j = 0; j < segs; j++)
            {
                tris.push_back({ at(i, j), at(i + 1, j), at(i + 1, j + 1) });
                tris.push_back({ at(i, j), at(i + 1, j + 1), at(i, j + 1) });
            }
        normals.resize(verts.size());
        for (std::size_t t = first_tri; t < tris.size(); t++)
        {
            const auto& tr = tris[t];
            const QVector3D n = QVector3D::crossProduct(verts[tr[1]] - verts[tr[0]], verts[tr[2]] - verts[tr[0]]);
            for (int k : tr)
                normals[k] += n;
        }
        double outward = 0;
        for (std::size_t i = base; i < verts.size(); i++)
            outward += QVector3D::dotProduct(normals[i], verts[i] - centre);
        for (std::size_t i = base; i < verts.size(); i++)
            normals[i] = (outward < 0 ? -normals[i] : normals[i]).normalized();
    };

    // head: a dome, the face/skull, then a flat base
    constexpr int dome_rings = 20, body_rings = 84, cap_rings = 3, segs = 104;
    add_surface(dome_rings + body_rings + cap_rings, segs, { 0, 0, 0 }, [&](int i, int j) {
        // longitudes are packed towards the face so the nose and lips are resolved
        const float sj = float(j) / segs - .5f;
        const float u = pi * (sj < 0 ? -1.f : 1.f) * std::pow(std::fabs(2 * sj), 1.6f);
        if (i <= dome_rings)
            return head_point(y_dome + (1 - y_dome) * std::cos(pi * .5f * float(i) / dome_rings), u, 1);
        if (i <= dome_rings + body_rings)
            return head_point(y_dome - (1 + y_dome) * float(i - dome_rings) / body_rings, u, 1);
        static constexpr float cap[cap_rings] = { .62f, .28f, 0 };
        return head_point(-1, u, cap[i - dome_rings - body_rings - 1]);
    });

    // ears: simple lobes
    for (float side : { -1.f, 1.f })
    {
        const QVector3D centre(side * (half_width(.04f) + .005f), .04f, -.10f);
        QMatrix4x4 rot;
        rot.rotate(side * 24, 0, 1, 0);   // the back edge stands away from the skull
        rot.rotate(-9, 1, 0, 0);          // leaning back a little
        add_surface(14, 20, centre, [&](int i, int j) {
            const float th = pi * float(i) / 14, ph = 2 * pi * float(j) / 20;
            const float taper = 1 - .30f * smoothstep(0, -1, std::cos(th)); // narrower lobe at the bottom
            const QVector3D local(.055f * std::sin(th) * std::sin(ph),
                                  .225f * std::cos(th),
                                  .140f * taper * std::sin(th) * std::cos(ph));
            return centre + rot.map(local);
        });
    }
}

// ---------------------------------------------------------------- state

void pose_widget::set_grid_background(bool dark_theme)
{
    dark_grid = dark_theme;
    update();
}

void pose_widget::set_tracking_state(tracking_state s)
{
    if (state == s)
        return;
    if (state == state_stopped)
        reticle_valid = false;
    state = s;
    update();
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

// ---------------------------------------------------------------- drawing

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

static QString brand_font_family()
{
    static const QString family = [] {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/kinishinai-nbp.ttf"));
        const QStringList names = QFontDatabase::applicationFontFamilies(id);
        return names.isEmpty() ? QString() : names.front();
    }();
    return family;
}

void pose_widget::draw_stopped(QPainter& p) const
{
    const QString text = QStringLiteral("tracking stopped"); // lower case = the font's wide small caps
    QFont f(brand_font_family());
    f.setPixelSize(24);
    // fit to about three quarters of the width
    const qreal tw = QFontMetricsF(f).horizontalAdvance(text);
    if (tw > 0)
        f.setPixelSize(std::max(10, int(24 * width() * .76 / tw)));
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setFont(f);
    const QFontMetricsF fm(f);
    const QRectF r(0, (height() - fm.height()) * .5, width(), fm.height());
    // a dark band so the text reads clearly over the grid
    QLinearGradient band(0, 0, width(), 0);
    band.setColorAt(0, QColor(9, 9, 13, 0));
    band.setColorAt(.18, QColor(9, 9, 13, 215));
    band.setColorAt(.82, QColor(9, 9, 13, 215));
    band.setColorAt(1, QColor(9, 9, 13, 0));
    p.fillRect(r.adjusted(0, -fm.height() * .45, 0, fm.height() * .45), band);
    p.setPen(QColor(196, 181, 253));
    p.drawText(r, Qt::AlignCenter, text);
    p.restore();
}

// A ring around the head with two markers pointing in at it, from opposite sides.
void pose_widget::draw_reticle(QPainter& p, QPointF c, float radius) const
{
    const QColor ring(164, 78, 184);
    const QColor marker = state == state_paused ? QColor(236, 64, 56) : QColor(40, 190, 86);

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ring, std::max(2.f, radius * .045f)));
    p.drawEllipse(c, radius, radius);

    const float gap = radius * .10f, len = radius * .30f, half = radius * .17f;
    p.setPen(QPen(marker, std::max(1.5f, radius * .035f), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(marker);
    for (int k = 0; k < 2; k++)
    {
        p.save();
        p.translate(c);
        p.rotate(marker_angle + 180 * k);
        // at angle 0 the marker sits above the ring, pointing down at it
        QPolygonF tri;
        tri << QPointF(0, -(radius + gap)) << QPointF(-half, -(radius + gap + len)) << QPointF(half, -(radius + gap + len));
        p.drawPolygon(tri);
        p.restore();
    }
    p.restore();
}

void pose_widget::paintEvent(QPaintEvent*)
{
    constexpr float XYZmax = 50;    // cm for one head height of on-screen movement
    constexpr float Kz = .25f;      // depth scaling

    QPainter p(this);
    draw_grid(p);

    // animation clock
    const qint64 now = clock.nsecsElapsed();
    const float dt = std::clamp(float(now - last_ns) * 1e-9f, 0.f, .25f);
    last_ns = now;

    if (state == state_stopped)
    {
        draw_stopped(p);
        return;
    }
    if (state == state_running)
        marker_angle = std::fmod(marker_angle + dt * 360 / 8, 360.f); // one turn every 8 seconds

    // the caller passes roll already negated (see main_window::show_pose_)
    auto [ yaw, pitch, roll ] = R;
    auto [ x, y, z ] = T;
    const bool mirrored = mirror.checkState() == Qt::Checked;

    // Camera view of the user (mirror off): turning right turns the face towards screen left.
    QMatrix4x4 rot;
    rot.rotate(float(mirrored ? yaw : -yaw), 0, 1, 0);
    rot.rotate(float(-pitch), 1, 0, 0);
    rot.rotate(float(mirrored ? roll : -roll), 0, 0, 1);

    // move_unit sets how far the head travels on screen per cm; unit sets the size of the head,
    // crosshair and reticle (60% of the original size, the travel is unchanged).
    constexpr float head_scale = .6f;
    const float move_unit = height() * .21f * std::exp(-float(z) * Kz / XYZmax);
    const float unit = move_unit * head_scale;
    const float head_px = 2.2f * move_unit;
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
    const QVector3D L2 = QVector3D(.70f, -.15f, .55f).normalized();  // soft fill from the right
    const QVector3D base(.84f, .85f, .90f), rim_col(.55f, .36f, .96f);

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
                const float fill = std::max(QVector3D::dotProduct(n, L2), 0.f);
                const float rim = std::pow(1 - std::max(n.z(), 0.f), 4.f);
                const QVector3D col = base * (.18f + .72f * diff + .18f * fill) + rim_col * (rim * .42f);
                row[px] = qRgb(int(std::min(col.x(), 1.f) * 255), int(std::min(col.y(), 1.f) * 255),
                               int(std::min(col.z(), 1.f) * 255));
            }
        }
    }

    // 3D crosshair: three axes through the head that turn with it; hidden where the head is in front
    {
        auto line3d = [&](QVector3D a3, QVector3D b3, QRgb col, int radius) {
            const QPointF a = project(a3), b = project(b3);
            const int steps = std::max(2, int(std::max(std::fabs(b.x() - a.x()), std::fabs(b.y() - a.y())) * 1.5));
            for (int i = 0; i <= steps; i++)
            {
                const float t = float(i) / steps;
                const int cx = int(a.x() + (b.x() - a.x()) * t), cy = int(a.y() + (b.y() - a.y()) * t);
                const float zz = a3.z() + (b3.z() - a3.z()) * t;
                for (int oy = -radius; oy <= radius; oy++)
                    for (int ox = -radius; ox <= radius; ox++)
                    {
                        const int px = cx + ox, py = cy + oy;
                        if (px < 0 || py < 0 || px >= W || py >= Hh || ox * ox + oy * oy > radius * radius + 1)
                            continue;
                        if (zz < zbuf[std::size_t(py) * std::size_t(W) + std::size_t(px)])
                            continue;
                        reinterpret_cast<QRgb*>(layer.scanLine(py))[px] = col;
                    }
            }
        };
        const QRgb axis = qRgb(205, 190, 255), nose_axis = qRgb(236, 228, 255);
        const int r = std::max(1, int(k * .5f));
        const QVector3D ex = rot.map(QVector3D(1, 0, 0)), ey = rot.map(QVector3D(0, 1, 0)), ez = rot.map(QVector3D(0, 0, 1));
        line3d(ex * -1.32f, ex * 1.32f, axis, r);
        line3d(ey * -1.36f, ey * 1.36f, axis, r);
        line3d(ez * -1.45f, ez * 1.62f, nose_axis, r);
        // a dot marks the way the face is pointing
        line3d(ez * 1.62f, ez * 1.63f, nose_axis, r * 3);
        // short ticks at the ends of the side and vertical axes
        for (float s : { -1.f, 1.f })
        {
            line3d(ex * (1.32f * s) - ey * .09f, ex * (1.32f * s) + ey * .09f, axis, r);
            line3d(ey * (1.36f * s) - ex * .09f, ey * (1.36f * s) + ex * .09f, axis, r);
        }
    }

    // head, crosshair and reticle are drawn slightly see-through
    p.setOpacity(.8);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(rect(), layer);

    // reticle: floats after the head with a little lag
    if (!reticle_valid)
    {
        reticle_centre = centre;
        reticle_valid = true;
    }
    else
    {
        const float follow = 1 - std::exp(-dt / .12f);
        reticle_centre += (centre - reticle_centre) * follow;
    }
    draw_reticle(p, reticle_centre, unit * 1.50f);
    p.setOpacity(1);
}

} // ns pose_widget_impl
