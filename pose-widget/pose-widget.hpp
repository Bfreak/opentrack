/* Copyright (c) 2013, 2015 Stanislaw Halik <sthalik@misaki.pl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#pragma once

#include "api/plugin-api.hpp"
#include "compat/euler.hpp"

#include "export.hpp"

#include <array>
#include <vector>

#include <QWidget>
#include <QImage>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QPointF>
#include <QVector3D>

class QPainter;

namespace pose_widget_impl {

using namespace euler;

// Trackhat: shows the pose as a shaded mannequin head with a 3D crosshair through it and a
// reticle around it (software-rendered with QPainter, so it still works as an overlay).
// - running: the reticle's markers are green and circle the ring once every 8 seconds
// - paused:  the markers stop and turn red
// - stopped: no head, just "tracking stopped" in the brand font
struct OTR_POSE_WIDGET_EXPORT pose_widget final : QWidget
{
public:
    enum tracking_state { state_stopped, state_running, state_paused };

    explicit pose_widget(QWidget *parent = nullptr);
    void present(double xAngle, double yAngle, double zAngle, double x, double y, double z);
    void set_tracking_state(tracking_state s);
    QCheckBox mirror{"Mirror", this};
    void set_grid_background(bool dark_theme);
private:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent*) override;
    void build_head();
    void draw_grid(QPainter& p) const;
    void draw_stopped(QPainter& p) const;
    void draw_reticle(QPainter& p, QPointF centre, float radius) const;

    Pose_ R, T;
    bool dark_grid = true;
    tracking_state state = state_stopped;

    std::vector<QVector3D> verts, normals;
    std::vector<std::array<int, 3>> tris;
    // per-frame scratch
    std::vector<QVector3D> xverts, xnormals;
    std::vector<QPointF> sverts;
    std::vector<float> zbuf;

    // animation
    QElapsedTimer clock;
    qint64 last_ns = 0;
    float marker_angle = 0;     // degrees
    QPointF reticle_centre;
    bool reticle_valid = false;
};

}

using pose_widget = pose_widget_impl::pose_widget;
