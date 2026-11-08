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

namespace head_test {

// Renders a SceneDesc from a head pose: { x, y, z (cm), yaw, pitch, roll (degrees) },
// using opentrack's output convention: +yaw looks right, +pitch looks up, +roll tilts clockwise,
// +x right, +y up, +z backwards.
//
// Pipeline: sun shadow map -> panorama sky -> far layer (terrain/props) -> near layer (interior);
// glTF-style PBR shading with sun + sky-based ambient/reflections, blended glass, filmic tone mapping.
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
        QVector3D center;
        QOpenGLTexture *tex = nullptr, *nrm = nullptr, *mr = nullptr, *ao = nullptr, *em = nullptr;
    };

    void upload_scene();
    void upload(const std::vector<Prim>& in, std::vector<gpu_prim>& out);
    void free_gpu();
    QOpenGLTexture* texture(const QString& key, const QImage& generated = {}, bool mipmaps = true);
    void ensure_shadow_map();
    void render_shadows(const QVector3D& eye);
    void draw_layer(std::vector<gpu_prim>& prims, const QMatrix4x4& vp, const QVector3D& eye, bool fog);
    void draw_prim(gpu_prim& g);
    void draw_overlay();

    SceneDesc scene;
    bool scene_dirty = false;
    std::vector<gpu_prim> far_gpu, near_gpu;
    std::map<QString, std::unique_ptr<QOpenGLTexture>> textures;
    std::unique_ptr<QOpenGLTexture> sky_tex, env_tex;
    float env_mips = 1;
    QOpenGLShaderProgram mesh_prog, sky_prog, depth_prog;
    QOpenGLVertexArrayObject sky_vao;

    // sun shadow map
    static constexpr int shadow_size = 4096;
    GLuint shadow_fbo = 0, shadow_depth = 0;
    QMatrix4x4 shadow_vp;
    bool shadows_on = false;

    bool gl_ok = false;
    QString gl_error_;
    int max_texture_size = 4096;

    std::array<double, 6> pose_ {};
    float hfov = 90;
    QString overlay;
    bool overlay_warn = false;
};

} // namespace head_test
