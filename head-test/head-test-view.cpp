#include "head-test-view.hpp"

#include <cmath>

#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QPainter>
#include <QFile>
#include <QDebug>

namespace head_test {

static const char* mesh_vs = R"(#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_vp;
out vec3 v_pos;
out vec3 v_nrm;
out vec2 v_uv;
void main()
{
    v_pos = a_pos;
    v_nrm = a_nrm;
    v_uv = a_uv;
    gl_Position = u_vp * vec4(a_pos, 1.0);
}
)";

static const char* mesh_fs = R"(#version 330 core
in vec3 v_pos;
in vec3 v_nrm;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform bool u_has_tex;
uniform vec4 u_color;       // sRGB
uniform bool u_unlit;
uniform float u_alpha_cutoff;
uniform float u_uv_scale;
uniform vec3 u_sun_dir, u_sun_col, u_sky_amb, u_gnd_amb;
uniform vec3 u_eye;
uniform vec3 u_fog_col;
uniform float u_fog_density;
uniform float u_fade_start, u_fade_end;
out vec4 frag;

vec3 lin(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }

void main()
{
    vec4 base = vec4(lin(u_color.rgb), u_color.a);
    if (u_has_tex)
    {
        vec4 t = texture(u_tex, v_uv * u_uv_scale);
        base *= vec4(lin(t.rgb), t.a);
    }
    if (u_alpha_cutoff >= 0.0 && base.a < u_alpha_cutoff)
        discard;
    vec3 c;
    if (u_unlit)
        c = base.rgb;
    else
    {
        vec3 n = normalize(v_nrm);
        if (!gl_FrontFacing) n = -n;
        float d = max(dot(n, u_sun_dir), 0.0);
        vec3 amb = mix(u_gnd_amb, u_sky_amb, n.y * 0.5 + 0.5);
        c = base.rgb * (lin(amb) + lin(u_sun_col) * d);
    }
    if (u_fog_density > 0.0)
    {
        float f = 1.0 - exp(-pow(length(v_pos - u_eye) * u_fog_density, 2.0));
        c = mix(c, lin(u_fog_col), clamp(f, 0.0, 1.0));
    }
    float a = 1.0;
    if (u_fade_end > 0.0)
        a = 1.0 - smoothstep(u_fade_start, u_fade_end, length(v_pos.xz - u_eye.xz));
    frag = vec4(pow(c, vec3(1.0 / 2.2)), a);
}
)";

static const char* sky_vs = R"(#version 330 core
out vec2 v_ndc;
void main()
{
    vec2 p = vec2(float((gl_VertexID & 1) << 2) - 1.0, float((gl_VertexID & 2) << 1) - 1.0);
    v_ndc = p;
    gl_Position = vec4(p, 1.0, 1.0);
}
)";

static const char* sky_fs = R"(#version 330 core
in vec2 v_ndc;
uniform mat4 u_inv_vp;
uniform sampler2D u_sky;
uniform float u_yaw;
uniform bool u_has_sky;
uniform bool u_fill_below;
uniform vec3 u_below_col;
out vec4 frag;
void main()
{
    vec4 w = u_inv_vp * vec4(v_ndc, 1.0, 1.0);
    vec3 d = normalize(w.xyz / w.w);
    if (!u_has_sky)
    {
        frag = vec4(mix(vec3(0.35, 0.33, 0.3), vec3(0.45, 0.6, 0.85), smoothstep(-0.05, 0.05, d.y)), 1.0);
        return;
    }
    float az = atan(d.x, -d.z) + u_yaw;
    float u = az / 6.28318530718 + 0.5;
    float v = acos(clamp(d.y, -1.0, 1.0)) / 3.14159265359;
    vec3 c = textureLod(u_sky, vec2(fract(u), v), 0.0).rgb;
    if (u_fill_below)
        c = mix(u_below_col, c, smoothstep(-0.03, 0.01, d.y));
    frag = vec4(c, 1.0);
}
)";

view::view(QWidget* parent) : QOpenGLWidget(parent)
{
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    fmt.setSwapInterval(1);
    setFormat(fmt);
    setMinimumSize(320, 200);
    // redraw continuously, paced by vsync
    connect(this, &QOpenGLWidget::frameSwapped, this, qOverload<>(&QWidget::update));
}

view::~view()
{
    makeCurrent();
    free_gpu();
    textures.clear();
    sky_tex.reset();
    if (sky_vao.isCreated())
        sky_vao.destroy();
    doneCurrent();
}

void view::set_scene(SceneDesc s)
{
    scene = std::move(s);
    scene_dirty = true;
    update();
}

void view::initializeGL()
{
    initializeOpenGLFunctions();
    const QSurfaceFormat f = context()->format();
    if (f.majorVersion() * 10 + f.minorVersion() < 33)
    {
        gl_error_ = tr("This test needs OpenGL 3.3, but the graphics driver only provides %1.%2. "
                       "Updating the graphics driver usually fixes this.")
                        .arg(f.majorVersion()).arg(f.minorVersion());
        return;
    }
    if (!mesh_prog.addShaderFromSourceCode(QOpenGLShader::Vertex, mesh_vs) ||
        !mesh_prog.addShaderFromSourceCode(QOpenGLShader::Fragment, mesh_fs) || !mesh_prog.link() ||
        !sky_prog.addShaderFromSourceCode(QOpenGLShader::Vertex, sky_vs) ||
        !sky_prog.addShaderFromSourceCode(QOpenGLShader::Fragment, sky_fs) || !sky_prog.link())
    {
        gl_error_ = tr("Couldn't set up the 3D view: %1").arg(mesh_prog.log() + sky_prog.log());
        qDebug() << "head-test:" << gl_error_;
        return;
    }
    sky_vao.create();
    gl_ok = true;
    qDebug() << "head-test: OpenGL" << reinterpret_cast<const char*>(glGetString(GL_VERSION))
             << reinterpret_cast<const char*>(glGetString(GL_RENDERER));
}

void view::resizeGL(int, int) {}

QOpenGLTexture* view::texture(const Material& m)
{
    if (m.tex_key.isEmpty())
        return nullptr;
    auto it = textures.find(m.tex_key);
    if (it != textures.end())
        return it->second.get();

    QImage img = m.tex_image.isNull() ? QImage(m.tex_key) : m.tex_image;
    if (img.isNull())
    {
        qDebug() << "head-test: can't load texture" << m.tex_key;
        textures[m.tex_key] = nullptr;
        return nullptr;
    }
    img = img.convertToFormat(QImage::Format_RGBA8888);
    auto tex = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    tex->setFormat(QOpenGLTexture::RGBA8_UNorm);
    tex->setSize(img.width(), img.height());
    tex->setMipLevels(m.mipmaps ? tex->maximumMipLevels() : 1);
    tex->allocateStorage();
    // row 0 of the image = texture coordinate t = 0 (no vertical flip; glTF convention)
    tex->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, img.constBits());
    if (m.mipmaps)
    {
        tex->generateMipMaps();
        tex->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        tex->setMaximumAnisotropy(8);
    }
    else
        tex->setMinificationFilter(QOpenGLTexture::Linear);
    tex->setMagnificationFilter(QOpenGLTexture::Linear);
    tex->setWrapMode(QOpenGLTexture::Repeat);
    QOpenGLTexture* ret = tex.get();
    textures[m.tex_key] = std::move(tex);
    return ret;
}

void view::upload(const std::vector<Prim>& in, std::vector<gpu_prim>& out)
{
    for (const Prim& p : in)
    {
        if (p.idx.empty())
            continue;
        gpu_prim g;
        g.vao = std::make_unique<QOpenGLVertexArrayObject>();
        g.vao->create();
        QOpenGLVertexArrayObject::Binder bind(g.vao.get());
        g.vbo.create();
        g.vbo.bind();
        g.vbo.allocate(p.verts.data(), int(p.verts.size() * sizeof(Vertex)));
        g.ibo.create();
        g.ibo.bind();
        g.ibo.allocate(p.idx.data(), int(p.idx.size() * sizeof(std::uint32_t)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, px)));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, nx)));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
        g.count = int(p.idx.size());
        g.mat = p.mat;
        g.tex = texture(p.mat);
        out.push_back(std::move(g));
    }
}

void view::free_gpu()
{
    for (auto* v : { &far_gpu, &near_gpu })
    {
        for (gpu_prim& g : *v)
        {
            g.vbo.destroy();
            g.ibo.destroy();
            g.vao->destroy();
        }
        v->clear();
    }
}

void view::upload_scene()
{
    free_gpu();
    // generated textures depend on the scene; file textures are shared across scenes
    for (auto it = textures.begin(); it != textures.end();)
        it = it->first.startsWith("gen:") ? textures.erase(it) : std::next(it);

    upload(scene.far_prims, far_gpu);
    upload(scene.near_prims, near_gpu);

    sky_tex.reset();
    QImage sky(scene.sky_file);
    if (!sky.isNull() && scene.fog_from_sky)
    {
        // average colour of the band just above the horizon
        const QImage band = sky.copy(0, int(sky.height() * .47), sky.width(), std::max(1, int(sky.height() * .025)))
                                .scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const QColor c = band.pixelColor(0, 0);
        scene.fog_color = QVector3D(float(c.redF()), float(c.greenF()), float(c.blueF()));
    }
    if (!sky.isNull())
    {
        Material m;
        m.tex_key = "sky";
        m.tex_image = sky;
        m.mipmaps = false;
        textures.erase("sky");
        texture(m);
        sky_tex = std::move(textures["sky"]);
        textures.erase("sky");
        sky_tex->setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
        sky_tex->setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
    }
    else
        qDebug() << "head-test: can't load sky" << scene.sky_file;

    // the CPU copy isn't needed any more
    scene.far_prims.clear();
    scene.near_prims.clear();
    scene_dirty = false;
}

void view::draw_layer(std::vector<gpu_prim>& prims, const QMatrix4x4& vp, const QVector3D& eye, bool fog)
{
    mesh_prog.bind();
    mesh_prog.setUniformValue("u_vp", vp);
    mesh_prog.setUniformValue("u_eye", eye);
    mesh_prog.setUniformValue("u_sun_dir", scene.sun_dir.normalized());
    mesh_prog.setUniformValue("u_sun_col", scene.sun_color);
    mesh_prog.setUniformValue("u_sky_amb", scene.sky_ambient);
    mesh_prog.setUniformValue("u_gnd_amb", scene.ground_ambient);
    mesh_prog.setUniformValue("u_fog_col", scene.fog_color);
    mesh_prog.setUniformValue("u_fog_density", fog ? scene.fog_density : 0.f);
    mesh_prog.setUniformValue("u_tex", 0);

    for (gpu_prim& g : prims)
    {
        const Material& m = g.mat;
        mesh_prog.setUniformValue("u_color", m.color);
        mesh_prog.setUniformValue("u_unlit", m.unlit);
        mesh_prog.setUniformValue("u_alpha_cutoff", m.alpha_cutoff);
        mesh_prog.setUniformValue("u_uv_scale", m.uv_scale);
        mesh_prog.setUniformValue("u_fade_start", m.fade_start);
        mesh_prog.setUniformValue("u_fade_end", m.fade_end);
        mesh_prog.setUniformValue("u_has_tex", g.tex != nullptr);
        if (g.tex)
            g.tex->bind(0);
        QOpenGLVertexArrayObject::Binder bind(g.vao.get());
        glDrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_INT, nullptr);
    }
    mesh_prog.release();
}

void view::paintGL()
{
    if (!gl_ok)
    {
        draw_overlay();
        return;
    }
    if (scene_dirty)
        upload_scene();

    const double* p = pose_.data();
    const int w = width(), h = height();
    const float aspect = float(w) / float(std::max(1, h));
    const float vfov = 2 * std::atan(std::tan(hfov * 3.14159265f / 360) / aspect) * 180 / 3.14159265f;

    QMatrix4x4 rot;
    rot.rotate(float(-p[3]), 0, 1, 0); // yaw: positive looks right
    rot.rotate(float(p[4]), 1, 0, 0);  // pitch: positive looks up
    rot.rotate(float(-p[5]), 0, 0, 1); // roll: positive tilts clockwise
    const QVector3D eye(float(p[0]) / 100, float(p[1]) / 100, float(p[2]) / 100);
    QMatrix4x4 cam;
    cam.translate(eye);
    cam *= rot;
    const QMatrix4x4 view_m = cam.inverted(), view_rot = rot.inverted();

    QMatrix4x4 proj_far, proj_near;
    proj_far.perspective(vfov, aspect, scene.far_near, scene.far_far);
    proj_near.perspective(vfov, aspect, .02f, 60.f);

    glClearColor(.5f, .5f, .5f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // sky
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    sky_prog.bind();
    sky_prog.setUniformValue("u_inv_vp", (proj_far * view_rot).inverted());
    sky_prog.setUniformValue("u_yaw", scene.sky_yaw_deg * 3.14159265f / 180);
    sky_prog.setUniformValue("u_has_sky", sky_tex != nullptr);
    sky_prog.setUniformValue("u_fill_below", scene.fill_below_horizon);
    sky_prog.setUniformValue("u_below_col", scene.fog_color);
    sky_prog.setUniformValue("u_sky", 0);
    if (sky_tex)
        sky_tex->bind(0);
    {
        QOpenGLVertexArrayObject::Binder bind(&sky_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    sky_prog.release();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    draw_layer(far_gpu, proj_far * view_m, eye, true);
    glClear(GL_DEPTH_BUFFER_BIT);
    draw_layer(near_gpu, proj_near * view_m, eye, false);

    draw_overlay();
}

void view::draw_overlay()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    if (!gl_ok)
    {
        painter.fillRect(rect(), QColor(40, 40, 44));
        painter.setPen(Qt::white);
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 1.4);
        painter.setFont(f);
        painter.drawText(rect().adjusted(40, 40, -40, -40), Qt::AlignCenter | Qt::TextWordWrap,
                         gl_error_.isEmpty() ? tr("Starting 3D view...") : gl_error_);
        return;
    }

    const double* p = pose_.data();
    const QString pose_text = QStringLiteral("Yaw %1°   Pitch %2°   Roll %3°\nX %4 cm   Y %5 cm   Z %6 cm")
        .arg(p[3], 0, 'f', 1).arg(p[4], 0, 'f', 1).arg(p[5], 0, 'f', 1)
        .arg(p[0], 0, 'f', 1).arg(p[1], 0, 'f', 1).arg(p[2], 0, 'f', 1);

    QFont f = font();
    f.setPointSizeF(std::max(9.0, f.pointSizeF()));
    painter.setFont(f);
    const QFontMetrics fm(f);
    const int pad = 8;
    QRect r = fm.boundingRect(QRect(0, 0, 1000, 1000), Qt::AlignLeft, pose_text);
    r.moveTopLeft({ 12 + pad, 12 + pad });
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 140));
    painter.drawRoundedRect(r.adjusted(-pad, -pad, pad, pad), 6, 6);
    painter.setPen(QColor(235, 235, 235));
    painter.drawText(r, Qt::AlignLeft, pose_text);

    if (!overlay.isEmpty())
    {
        QFont big = f;
        big.setPointSizeF(f.pointSizeF() * 1.25);
        big.setBold(true);
        painter.setFont(big);
        const QFontMetrics bm(big);
        QRect br = bm.boundingRect(QRect(0, 0, width() - 80, 1000), Qt::AlignCenter | Qt::TextWordWrap, overlay);
        br.moveCenter({ width() / 2, height() - br.height() / 2 - 40 });
        painter.setPen(Qt::NoPen);
        painter.setBrush(overlay_warn ? QColor(150, 60, 0, 200) : QColor(0, 0, 0, 150));
        painter.drawRoundedRect(br.adjusted(-14, -10, 14, 10), 8, 8);
        painter.setPen(Qt::white);
        painter.drawText(br, Qt::AlignCenter | Qt::TextWordWrap, overlay);
    }
}

} // namespace head_test
