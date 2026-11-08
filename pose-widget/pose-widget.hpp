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
#include <QVector3D>

namespace pose_widget_impl {

using namespace euler;

// Trackhat: shows the pose as a shaded, featureless 3D head (software-rendered with QPainter,
// so it still works as a transparent overlay on top of the camera preview).
struct OTR_POSE_WIDGET_EXPORT pose_widget final : QWidget
{
public:
    explicit pose_widget(QWidget *parent = nullptr);
    void present(double xAngle, double yAngle, double zAngle, double x, double y, double z);
    QCheckBox mirror{"Mirror", this};
    void set_grid_background(bool dark_theme);
private:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent*) override;
    void build_head();

    Pose_ R, T;
    QImage background;

    std::vector<QVector3D> verts, normals;
    std::vector<std::array<int, 3>> tris;
    // per-frame scratch
    std::vector<QVector3D> xverts, xnormals;
    std::vector<QPointF> sverts;
    std::vector<float> zbuf;
};

}

using pose_widget = pose_widget_impl::pose_widget;
