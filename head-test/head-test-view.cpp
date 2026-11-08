#include "head-test-view.hpp"

#include <algorithm>
#include <cmath>

#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QPainter>
#include <QDebug>

namespace head_test {

static constexpr float pi = 3.14159265358979f;

// ------------------------------------------------------------------ shaders

static const char* mesh_vs = R"(#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec2 a_uv1;
uniform mat4 u_vp;
out vec3 v_pos;
out vec3 v_nrm;
out vec2 v_uv;
out vec2 v_uv1;
void main()
{
    v_pos = a_pos;
    v_nrm = a_nrm;
    v_uv = a_uv;
    v_uv1 = a_uv1;
    gl_Position = u_vp * vec4(a_pos, 1.0);
}
)";

static const char* mesh_fs = R"(#version 330 core
in vec3 v_pos;
in vec3 v_nrm;
in vec2 v_uv;
in vec2 v_uv1;

uniform sampler2D u_tex, u_nrm_tex, u_mr_tex, u_ao_tex, u_em_tex, u_env;
uniform sampler2DShadow u_shadow;
uniform bool u_has_tex, u_has_nrm, u_has_mr, u_has_ao, u_has_em;
uniform vec4 u_color;          // sRGB
uniform vec3 u_emissive;       // linear
uniform float u_rough, u_metal, u_nrm_scale;
uniform vec4 u_base_uv;        // scale.xy, offset.zw
uniform bool u_unlit, u_blend, u_ao_uv1, u_catcher;
uniform float u_alpha_cutoff, u_uv_scale;

uniform vec3 u_sun_dir, u_sun_col;   // sun_col: linear radiance
uniform float u_ambient, u_exposure, u_env_mips, u_sky_yaw;
uniform bool u_has_env;
uniform vec3 u_sky_amb, u_gnd_amb;   // fallback when there's no sky image

uniform bool u_shadows;
uniform mat4 u_shadow_vp;
uniform float u_shadow_texel, u_shadow_world_texel;

uniform vec3 u_eye, u_fog_col;
uniform float u_fog_density, u_fade_start, u_fade_end;
out vec4 frag;

vec3 lin(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }

vec2 env_uv(vec3 d)
{
    float az = atan(d.x, -d.z) + u_sky_yaw;
    return vec2(fract(az / 6.28318530718 + 0.5), acos(clamp(d.y, -1.0, 1.0)) / 3.14159265359);
}

vec3 env(vec3 d, float lod)
{
    if (!u_has_env)
        return lin(mix(u_gnd_amb, u_sky_amb, d.y * 0.5 + 0.5));
    return lin(textureLod(u_env, env_uv(d), lod).rgb);
}

mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv)
{
    vec3 dp1 = dFdx(p), dp2 = dFdy(p);
    vec2 du1 = dFdx(uv), du2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N), dp1perp = cross(N, dp1);
    vec3 T = dp2perp * du1.x + dp1perp * du2.x;
    vec3 B = dp2perp * du1.y + dp1perp * du2.y;
    float m = max(dot(T, T), dot(B, B));
    if (m < 1e-20)
        return mat3(vec3(1, 0, 0), vec3(0, 1, 0), N);
    float inv = inversesqrt(m);
    return mat3(T * inv, B * inv, N);
}

float shadow(vec3 p, vec3 n)
{
    if (!u_shadows)
        return 1.0;
    vec4 s = u_shadow_vp * vec4(p + n * u_shadow_world_texel * 1.5, 1.0);
    vec3 c = s.xyz / s.w * 0.5 + 0.5;
    if (c.x < 0.0 || c.y < 0.0 || c.x > 1.0 || c.y > 1.0 || c.z > 1.0)
        return 1.0;
    float sum = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            sum += texture(u_shadow, vec3(c.xy + vec2(x, y) * u_shadow_texel, c.z - 0.0004));
    return sum / 9.0;
}

vec2 env_brdf(float NdV, float r)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r4 = r * c0 + c1;
    float a004 = min(r4.x * r4.x, exp2(-9.28 * NdV)) * r4.x + r4.y;
    return vec2(-1.04, 1.04) * a004 + r4.zw;
}

vec3 aces(vec3 x) { return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }

void main()
{
    vec2 uv = v_uv * u_uv_scale;
    vec2 buv = uv * u_base_uv.xy + u_base_uv.zw;
    vec4 base = vec4(lin(u_color.rgb), u_color.a);
    if (u_has_tex)
    {
        vec4 t = texture(u_tex, buv);
        base *= vec4(lin(t.rgb), t.a);
    }
    if (u_alpha_cutoff >= 0.0 && base.a < u_alpha_cutoff)
        discard;

    float fade = 1.0;
    if (u_fade_end > 0.0)
        fade = 1.0 - smoothstep(u_fade_start, u_fade_end, length(v_pos.xz - u_eye.xz));

    if (u_unlit)
    {
        frag = vec4(pow(base.rgb, vec3(1.0 / 2.2)), 1.0);
        return;
    }
    if (u_catcher)
    {
        float s = shadow(v_pos, normalize(v_nrm));
        frag = vec4(0.0, 0.0, 0.0, (1.0 - s) * 0.6 * fade);
        return;
    }

    vec3 N = normalize(v_nrm);
    if (!gl_FrontFacing)
        N = -N;
    if (u_has_nrm)
    {
        vec3 tn = texture(u_nrm_tex, uv).xyz * 2.0 - 1.0;
        tn.xy *= u_nrm_scale;
        N = normalize(cotangent_frame(N, v_pos, uv) * tn);
    }
    float rough = u_rough, metal = u_metal, ao = 1.0;
    if (u_has_mr)
    {
        vec4 m = texture(u_mr_tex, uv);
        rough *= m.g;
        metal *= m.b;
    }
    if (u_has_ao)
        ao = texture(u_ao_tex, u_ao_uv1 ? v_uv1 : uv).r;
    rough = clamp(rough, 0.05, 1.0);

    vec3 V = normalize(u_eye - v_pos);
    vec3 L = u_sun_dir;
    vec3 H = normalize(V + L);
    float NdL = max(dot(N, L), 0.0), NdV = max(dot(N, V), 1e-4);
    float NdH = max(dot(N, H), 0.0), VdH = max(dot(V, H), 0.0);
    vec3 F0 = mix(vec3(0.04), base.rgb, metal);
    vec3 diffc = base.rgb * (1.0 - metal);

    // sun: GGX specular + Lambert
    float a = rough * rough, a2 = a * a;
    float dd = NdH * NdH * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159265 * dd * dd);
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float G = (NdL / (NdL * (1.0 - k) + k)) * (NdV / (NdV * (1.0 - k) + k));
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdH, 5.0);
    vec3 spec = D * G * F / max(4.0 * NdL * NdV, 1e-4);
    float sh = shadow(v_pos, N);
    vec3 body = diffc * (1.0 - F) * u_sun_col * NdL * sh;
    vec3 shine = spec * u_sun_col * NdL * sh * 3.14159265;

    // sky: diffuse irradiance and prefiltered reflection from the panorama's mip chain
    vec2 ab = env_brdf(NdV, rough);
    vec3 irr = env(N, u_env_mips - 3.0);
    vec3 R = reflect(-V, N);
    vec3 pref = env(R, rough * (u_env_mips - 2.0));
    body += diffc * irr * u_ambient * ao;
    shine += pref * (F0 * ab.x + ab.y) * u_ambient * ao;

    float alpha = base.a * fade;
    vec3 c = u_blend ? body * alpha + shine * 0.6 : (body + shine) * fade;
    vec3 em = u_emissive;
    if (u_has_em)
        em *= lin(texture(u_em_tex, buv).rgb);
    c += em * fade;

    if (u_fog_density > 0.0)
    {
        float f = 1.0 - exp(-pow(length(v_pos - u_eye) * u_fog_density, 2.0));
        c = mix(c, lin(u_fog_col) * alpha, clamp(f, 0.0, 1.0));
    }
    // premultiplied alpha output
    frag = vec4(pow(aces(c * u_exposure), vec3(1.0 / 2.2)), u_blend ? alpha : fade);
}
)";

static const char* depth_vs = R"(#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 2) in vec2 a_uv;
uniform mat4 u_vp;
out vec2 v_uv;
void main()
{
    v_uv = a_uv;
    gl_Position = u_vp * vec4(a_pos, 1.0);
}
)";

static const char* depth_fs = R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_tex;
uniform bool u_test;
uniform float u_cutoff;
uniform vec4 u_base_uv;
void main()
{
    if (u_test && texture(u_tex, v_uv * u_base_uv.xy + u_base_uv.zw).a < u_cutoff)
        discard;
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
uniform bool u_has_sky, u_fill_below;
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
    vec2 uv = vec2(fract(az / 6.28318530718 + 0.5), acos(clamp(d.y, -1.0, 1.0)) / 3.14159265359);
    // mip selection that ignores the wrap-around seam
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    dx.x -= round(dx.x);
    dy.x -= round(dy.x);
    vec3 c = textureGrad(u_sky, uv, dx, dy).rgb;
    if (u_fill_below)
        c = mix(u_below_col, c, smoothstep(-0.03, 0.01, d.y));
    frag = vec4(c, 1.0);
}
)";

// ------------------------------------------------------------------ setup

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
    env_tex.reset();
    if (sky_vao.isCreated())
        sky_vao.destroy();
    if (shadow_fbo)
        glDeleteFramebuffers(1, &shadow_fbo);
    if (shadow_depth)
        glDeleteTextures(1, &shadow_depth);
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
    auto build = [this](QOpenGLShaderProgram& p, const char* vs, const char* fs) {
        if (p.addShaderFromSourceCode(QOpenGLShader::Vertex, vs) &&
            p.addShaderFromSourceCode(QOpenGLShader::Fragment, fs) && p.link())
            return true;
        gl_error_ = tr("Couldn't set up the 3D view: %1").arg(p.log());
        qDebug() << "head-test:" << gl_error_;
        return false;
    };
    if (!build(mesh_prog, mesh_vs, mesh_fs) || !build(sky_prog, sky_vs, sky_fs) || !build(depth_prog, depth_vs, depth_fs))
        return;
    sky_vao.create();
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    gl_ok = true;
    qDebug() << "head-test: OpenGL" << reinterpret_cast<const char*>(glGetString(GL_VERSION))
             << reinterpret_cast<const char*>(glGetString(GL_RENDERER)) << "max texture" << max_texture_size;
}

void view::resizeGL(int, int) {}

QOpenGLTexture* view::texture(const QString& key, const QImage& generated, bool mipmaps)
{
    if (key.isEmpty())
        return nullptr;
    auto it = textures.find(key);
    if (it != textures.end())
        return it->second.get();

    QImage img = generated.isNull() ? QImage(key) : generated;
    if (img.isNull())
    {
        qDebug() << "head-test: can't load texture" << key;
        textures[key] = nullptr;
        return nullptr;
    }
    if (std::max(img.width(), img.height()) > max_texture_size)
        img = img.scaled(std::min(img.width(), max_texture_size), std::min(img.height(), max_texture_size),
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img = img.convertToFormat(QImage::Format_RGBA8888);
    auto tex = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    tex->setFormat(QOpenGLTexture::RGBA8_UNorm);
    tex->setSize(img.width(), img.height());
    tex->setMipLevels(mipmaps ? tex->maximumMipLevels() : 1);
    tex->allocateStorage();
    // row 0 of the image = texture coordinate t = 0 (no vertical flip; glTF convention)
    tex->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, img.constBits());
    if (mipmaps)
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
    textures[key] = std::move(tex);
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
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, px)));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, nx)));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u1)));
        g.count = int(p.idx.size());
        g.mat = p.mat;
        QVector3D sum;
        for (const Vertex& v : p.verts)
            sum += QVector3D(v.px, v.py, v.pz);
        g.center = sum / float(p.verts.size());
        g.tex = texture(p.mat.tex_key, p.mat.tex_image, p.mat.mipmaps);
        g.nrm = texture(p.mat.normal_key);
        g.mr = texture(p.mat.mr_key);
        g.ao = texture(p.mat.ao_key);
        g.em = texture(p.mat.emissive_key);
        g.mat.tex_image = {};
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
    // textures are per scene (they're big)
    textures.clear();

    upload(scene.far_prims, far_gpu);
    upload(scene.near_prims, near_gpu);

    sky_tex.reset();
    env_tex.reset();
    QImage sky(scene.sky_file);
    if (!sky.isNull())
    {
        if (scene.fog_from_sky)
        {
            // average colour of the band just above the horizon
            const QImage band = sky.copy(0, int(sky.height() * .47), sky.width(), std::max(1, int(sky.height() * .025)))
                                    .scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            const QColor c = band.pixelColor(0, 0);
            scene.fog_color = QVector3D(float(c.redF()), float(c.greenF()), float(c.blueF()));
        }
        if (scene.sun_from_sky)
        {
            // the sun is the brightest area of the upper hemisphere
            const QImage small = sky.scaled(512, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                     .convertToFormat(QImage::Format_RGB32);
            int best = -1;
            double sx = 0, sy = 0, sw = 0;
            for (int pass = 0; pass < 2; pass++)
                for (int y = 0; y < 128; y++)
                {
                    const QRgb* row = reinterpret_cast<const QRgb*>(small.constScanLine(y));
                    for (int x = 0; x < 512; x++)
                    {
                        const int l = qRed(row[x]) * 3 + qGreen(row[x]) * 6 + qBlue(row[x]);
                        if (pass == 0)
                            best = std::max(best, l);
                        else if (l >= best - 8)
                        {
                            sx += x + .5; sy += y + .5; sw += 1;
                        }
                    }
                }
            if (sw > 0)
            {
                const float u = float(sx / sw / 512), vv = float(sy / sw / 256);
                const float az = (u - .5f) * 2 * pi - scene.sky_yaw_deg * pi / 180, el = (.5f - vv) * pi;
                scene.sun_dir = QVector3D(std::sin(az) * std::cos(el), std::sin(el), -std::cos(az) * std::cos(el)).normalized();
                qDebug() << "head-test: sun from sky at" << u << vv << scene.sun_dir;
            }
        }
        texture("sky", sky, true);
        sky_tex = std::move(textures["sky"]);
        textures.erase("sky");
        sky_tex->setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
        sky_tex->setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
        sky_tex->setMaximumAnisotropy(1);

        // small copy for lighting (its mip chain approximates blurred irradiance/reflections)
        QImage env = sky.scaled(1024, 512, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (scene.fill_below_horizon)
        {
            QPainter p(&env);
            const QColor fog = QColor::fromRgbF(scene.fog_color.x(), scene.fog_color.y(), scene.fog_color.z());
            p.fillRect(0, 258, 1024, 254, fog.darker(130));
        }
        texture("env", env, true);
        env_tex = std::move(textures["env"]);
        textures.erase("env");
        env_tex->setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
        env_tex->setMaximumAnisotropy(1);
        env_mips = float(env_tex->mipLevels());
    }
    else
        qDebug() << "head-test: can't load sky" << scene.sky_file;

    // the CPU copy isn't needed any more
    scene.far_prims.clear();
    scene.near_prims.clear();
    scene_dirty = false;
}

// ------------------------------------------------------------------ shadows

void view::ensure_shadow_map()
{
    if (shadow_fbo)
        return;
    glGenTextures(1, &shadow_depth);
    glBindTexture(GL_TEXTURE_2D, shadow_depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadow_size, shadow_size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenFramebuffers(1, &shadow_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_depth, 0);
    const GLenum none = GL_NONE;
    glDrawBuffers(1, &none);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        qDebug() << "head-test: shadow framebuffer incomplete; shadows disabled";
        glDeleteFramebuffers(1, &shadow_fbo);
        shadow_fbo = 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
}

void view::render_shadows(const QVector3D& eye)
{
    shadows_on = false;
    if (scene.shadow_radius <= 0)
        return;
    ensure_shadow_map();
    if (!shadow_fbo)
        return;

    const float R = scene.shadow_radius;
    const QVector3D L = scene.sun_dir.normalized();
    const QVector3D up = std::fabs(L.y()) > .95f ? QVector3D(0, 0, -1) : QVector3D(0, 1, 0);
    QMatrix4x4 light_view;
    light_view.lookAt(L, QVector3D(0, 0, 0), up);
    // snap the box centre to the texel grid so shadows don't shimmer as the head moves
    QVector3D c = light_view.map(eye);
    const float texel = 2 * R / shadow_size;
    c.setX(std::floor(c.x() / texel) * texel);
    c.setY(std::floor(c.y() / texel) * texel);
    QMatrix4x4 proj;
    proj.ortho(c.x() - R, c.x() + R, c.y() - R, c.y() + R, -c.z() - 4 * R, -c.z() + 4 * R);
    shadow_vp = proj * light_view;

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glViewport(0, 0, shadow_size, shadow_size);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.f, 4.f);
    depth_prog.bind();
    depth_prog.setUniformValue("u_vp", shadow_vp);
    depth_prog.setUniformValue("u_tex", 0);
    for (auto* layer : { &far_gpu, &near_gpu })
        for (gpu_prim& g : *layer)
        {
            if (!g.mat.cast_shadow || g.mat.unlit || g.mat.shadow_catcher)
                continue;
            const bool test = g.mat.alpha_cutoff >= 0 && g.tex;
            depth_prog.setUniformValue("u_test", test);
            depth_prog.setUniformValue("u_cutoff", g.mat.alpha_cutoff);
            depth_prog.setUniformValue("u_base_uv", g.mat.base_uv);
            if (test)
                g.tex->bind(0);
            QOpenGLVertexArrayObject::Binder bind(g.vao.get());
            glDrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_INT, nullptr);
        }
    depth_prog.release();
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    const qreal dpr = devicePixelRatioF();
    glViewport(0, 0, int(width() * dpr), int(height() * dpr));
    shadows_on = true;
}

// ------------------------------------------------------------------ drawing

void view::draw_prim(gpu_prim& g)
{
    const Material& m = g.mat;
    mesh_prog.setUniformValue("u_color", m.color);
    mesh_prog.setUniformValue("u_emissive", m.emissive);
    mesh_prog.setUniformValue("u_rough", m.roughness);
    mesh_prog.setUniformValue("u_metal", m.metallic);
    mesh_prog.setUniformValue("u_nrm_scale", m.normal_scale);
    mesh_prog.setUniformValue("u_base_uv", m.base_uv);
    mesh_prog.setUniformValue("u_unlit", m.unlit);
    mesh_prog.setUniformValue("u_blend", m.blend);
    mesh_prog.setUniformValue("u_ao_uv1", m.ao_uv1);
    mesh_prog.setUniformValue("u_catcher", m.shadow_catcher);
    mesh_prog.setUniformValue("u_alpha_cutoff", m.alpha_cutoff);
    mesh_prog.setUniformValue("u_uv_scale", m.uv_scale);
    mesh_prog.setUniformValue("u_fade_start", m.fade_start);
    mesh_prog.setUniformValue("u_fade_end", m.fade_end);
    struct { QOpenGLTexture* t; const char* has; int unit; } maps[] = {
        { g.tex, "u_has_tex", 0 }, { g.nrm, "u_has_nrm", 1 }, { g.mr, "u_has_mr", 2 },
        { g.ao, "u_has_ao", 3 }, { g.em, "u_has_em", 4 },
    };
    for (const auto& mp : maps)
    {
        mesh_prog.setUniformValue(mp.has, mp.t != nullptr);
        if (mp.t)
            mp.t->bind(GLuint(mp.unit));
    }
    QOpenGLVertexArrayObject::Binder bind(g.vao.get());
    glDrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_INT, nullptr);
}

void view::draw_layer(std::vector<gpu_prim>& prims, const QMatrix4x4& vp, const QVector3D& eye, bool fog)
{
    auto lin = [](QVector3D c) {
        return QVector3D(std::pow(c.x(), 2.2f), std::pow(c.y(), 2.2f), std::pow(c.z(), 2.2f));
    };
    mesh_prog.bind();
    mesh_prog.setUniformValue("u_vp", vp);
    mesh_prog.setUniformValue("u_eye", eye);
    mesh_prog.setUniformValue("u_sun_dir", scene.sun_dir.normalized());
    mesh_prog.setUniformValue("u_sun_col", lin(scene.sun_color) * scene.sun_strength);
    mesh_prog.setUniformValue("u_ambient", scene.ambient);
    mesh_prog.setUniformValue("u_exposure", scene.exposure);
    mesh_prog.setUniformValue("u_sky_amb", scene.sky_ambient);
    mesh_prog.setUniformValue("u_gnd_amb", scene.ground_ambient);
    mesh_prog.setUniformValue("u_fog_col", scene.fog_color);
    mesh_prog.setUniformValue("u_fog_density", fog ? scene.fog_density : 0.f);
    mesh_prog.setUniformValue("u_sky_yaw", scene.sky_yaw_deg * pi / 180);
    mesh_prog.setUniformValue("u_has_env", env_tex != nullptr);
    mesh_prog.setUniformValue("u_env_mips", env_mips);
    mesh_prog.setUniformValue("u_shadows", shadows_on);
    mesh_prog.setUniformValue("u_shadow_vp", shadow_vp);
    mesh_prog.setUniformValue("u_shadow_texel", 1.f / shadow_size);
    mesh_prog.setUniformValue("u_shadow_world_texel", 2 * scene.shadow_radius / shadow_size);
    mesh_prog.setUniformValue("u_tex", 0);
    mesh_prog.setUniformValue("u_nrm_tex", 1);
    mesh_prog.setUniformValue("u_mr_tex", 2);
    mesh_prog.setUniformValue("u_ao_tex", 3);
    mesh_prog.setUniformValue("u_em_tex", 4);
    mesh_prog.setUniformValue("u_env", 5);
    mesh_prog.setUniformValue("u_shadow", 6);
    if (env_tex)
        env_tex->bind(5);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, shadows_on ? shadow_depth : 0);
    glActiveTexture(GL_TEXTURE0);

    // opaque first, then glass back to front without depth writes
    std::vector<gpu_prim*> blended;
    for (gpu_prim& g : prims)
    {
        if (g.mat.blend || g.mat.shadow_catcher)
            blended.push_back(&g);
        else
            draw_prim(g);
    }
    std::sort(blended.begin(), blended.end(), [&](const gpu_prim* a, const gpu_prim* b) {
        return (a->center - eye).lengthSquared() > (b->center - eye).lengthSquared();
    });
    glDepthMask(GL_FALSE);
    for (gpu_prim* g : blended)
        draw_prim(*g);
    glDepthMask(GL_TRUE);
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
    const float vfov = 2 * std::atan(std::tan(hfov * pi / 360) / aspect) * 180 / pi;

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

    render_shadows(eye);

    glClearColor(.5f, .5f, .5f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // shaders output premultiplied alpha

    // sky
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    sky_prog.bind();
    sky_prog.setUniformValue("u_inv_vp", (proj_far * view_rot).inverted());
    sky_prog.setUniformValue("u_yaw", scene.sky_yaw_deg * pi / 180);
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

    glDisable(GL_BLEND);
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
