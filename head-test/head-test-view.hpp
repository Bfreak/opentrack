#pragma once

#include "scene-geometry.hpp"

#include <array>
#include <map>
#include <memory>
#include <vector>

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QElapsedTimer>

namespace head_test {

// Renders a SceneDesc from a head pose: { x, y, z (cm), yaw, pitch, roll (degrees) },
// using opentrack's output convention: +yaw looks right, +pitch looks up, +roll tilts clockwise,
// +x right, +y up, +z backwards.
class view final : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    explicit view(QWidget* parent = nullptr);
    ~view() override;

    void set_scene(SceneDesc scene);
    void set_pose(const std::array<double, 6>& pose) { pose_ = pose; }
    void set_hfov(float deg) { hfov = deg; }
    void set_overlay(const QString& text, bool warn) { overlay = text; overlay_warn = warn; }

    QString gl_error() const { return gl_error_; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    struct gpu_prim
    {
        std::unique_ptr<QOpenGLVertexArrayObject> vao;
        QOpenGLBuffer vbo { QOpenGLBuffer::VertexBuffer }, ibo { QOpenGLBuffer::IndexBuffer };
        int count = 0;
        Material mat;
        QOpenGLTexture* tex = nullptr;
    };

    void upload_scene();
    void upload(const std::vector<Prim>& in, std::vector<gpu_prim>& out);
    void free_gpu();
    QOpenGLTexture* texture(const Material& m);
    void draw_layer(std::vector<gpu_prim>& prims, const QMatrix4x4& vp, const QVector3D& eye, bool fog);
    void draw_overlay();

    SceneDesc scene;
    bool scene_dirty = false;
    std::vector<gpu_prim> far_gpu, near_gpu;
    std::map<QString, std::unique_ptr<QOpenGLTexture>> textures;
    std::unique_ptr<QOpenGLTexture> sky_tex;
    QOpenGLShaderProgram mesh_prog, sky_prog;
    QOpenGLVertexArrayObject sky_vao;
    bool gl_ok = false;
    QString gl_error_;

    std::array<double, 6> pose_ {};
    float hfov = 90;
    QString overlay;
    bool overlay_warn = false;
};

} // namespace head_test
