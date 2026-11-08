#include "head-test-window.hpp"
#include "head-test-view.hpp"
#include "scenes.hpp"

#include "opentrack-org.hxx"

#include <algorithm>

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QSettings>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>

using namespace head_test;

static constexpr const char* settings_group = "trackhat-head-test";

head_test_window::head_test_window(pose_source src, QWidget* parent)
    : QWidget(parent, Qt::Window), source(std::move(src))
{
    setWindowTitle(tr("Head tracking test"));
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* bar = new QWidget(this);
    auto* bar_layout = new QHBoxLayout(bar);
    bar_layout->setContentsMargins(10, 6, 10, 6);
    bar_layout->setSpacing(6);

    scene_buttons = new QButtonGroup(this);
    scene_buttons->setExclusive(true);
    const QString names[scene_count] = { tr("Cockpit"), tr("Race car"), tr("Soldier") };
    for (int i = 0; i < scene_count; i++)
    {
        auto* b = new QToolButton(bar);
        b->setText(names[i]);
        b->setCheckable(true);
        b->setMinimumWidth(96);
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        scene_buttons->addButton(b, i);
        bar_layout->addWidget(b);
    }
    bar_layout->addSpacing(16);
    auto* hint = new QLabel(tr("Use the Mapping window to tune the curves while you look around."), bar);
    hint->setStyleSheet("color: palette(window-text);");
    bar_layout->addWidget(hint, 1);

    fov_label = new QLabel(bar);
    fov_slider = new QSlider(Qt::Horizontal, bar);
    fov_slider->setRange(50, 130);
    fov_slider->setFixedWidth(140);
    fov_slider->setToolTip(tr("Horizontal field of view"));
    bar_layout->addWidget(new QLabel(tr("Field of view"), bar));
    bar_layout->addWidget(fov_slider);
    bar_layout->addWidget(fov_label);

    bar->setStyleSheet("QToolButton { padding: 4px 10px; }"
                       "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text); font-weight: bold; }");
    layout->addWidget(bar);
    gl = new view(this);
    layout->addWidget(gl, 1);

    QSettings s(OPENTRACK_ORG);
    s.beginGroup(settings_group);
    scene = std::clamp(s.value("scene", 0).toInt(), 0, scene_count - 1);
    const QByteArray geom = s.value("geometry").toByteArray();
    s.endGroup();
    if (!geom.isEmpty())
        restoreGeometry(geom);

    connect(scene_buttons, &QButtonGroup::idClicked, this, &head_test_window::set_scene);
    connect(fov_slider, &QSlider::valueChanged, this, [this](int v) {
        gl->set_hfov(float(v));
        fov_label->setText(QStringLiteral("%1°").arg(v));
        QSettings s(OPENTRACK_ORG);
        s.setValue(QStringLiteral("%1/hfov-%2").arg(settings_group).arg(scene), v);
    });

    set_scene(scene);

    connect(&timer, &QTimer::timeout, this, &head_test_window::tick);
    timer.setTimerType(Qt::PreciseTimer);
    timer.start(4);
    tick();

    const QString snap_dir = qEnvironmentVariable("OPENTRACK_HEAD_TEST_SNAPSHOT");
    if (!snap_dir.isEmpty())
    {
        snapshot_mode = true;
        QTimer::singleShot(1500, this, [this, snap_dir] { run_snapshots(snap_dir); });
    }
}

head_test_window::~head_test_window() = default;

void head_test_window::set_scene(int idx)
{
    scene = idx;
    if (auto* b = scene_buttons->button(idx))
        b->setChecked(true);
    QStringList warnings;
    SceneDesc desc = build_scene(scene_id(idx), warnings);
    for (const QString& w : warnings)
        qDebug() << "head-test:" << w;

    QSettings s(OPENTRACK_ORG);
    s.setValue(QStringLiteral("%1/scene").arg(settings_group), idx);
    const int fov = s.value(QStringLiteral("%1/hfov-%2").arg(settings_group).arg(idx), int(desc.default_hfov)).toInt();

    gl->set_scene(std::move(desc));
    fov_slider->setValue(fov);
    gl->set_hfov(float(fov));
    fov_label->setText(QStringLiteral("%1°").arg(fov));
}

void head_test_window::tick()
{
    std::array<double, 6> pose {};
    bool running = false;
    if (snapshot_mode)
    {
        pose = override_pose;
        running = true;
    }
    else if (source)
        running = source(pose.data());
    if (!running)
        pose = {};
    gl->set_pose(pose);
    gl->set_overlay(running ? QString() : tr("Tracking is stopped. Press Start in the main window to try your head tracking here."),
                    !running);
}

void head_test_window::show_beside(QWidget* anchor)
{
    if (isVisible())
    {
        if (isMinimized())
            showNormal();
        raise();
        activateWindow();
        return;
    }

    QSettings s(OPENTRACK_ORG);
    const bool have_geometry = !s.value(QStringLiteral("%1/geometry").arg(settings_group)).toByteArray().isEmpty();
    if (!have_geometry)
    {
        QScreen* screen = anchor && anchor->screen() ? anchor->screen() : QGuiApplication::primaryScreen();
        const QRect avail = screen->availableGeometry();
        // large but not full screen; leave room on the left for the main and Mapping windows
        const int h = std::max(600, int(avail.height() * .78));
        // ultra-wide monitors: keep roughly 16:9 so the vertical view isn't squashed
        const int w = std::min(std::max(960, int(avail.width() * .62)), int(h * 1.8));
        resize(std::min(w, avail.width()), std::min(h, avail.height()));
        move(avail.right() - width() - avail.width() / 40, avail.top() + (avail.height() - height()) / 2);
    }
    showNormal();
    raise();
    activateWindow();
}

void head_test_window::save_settings()
{
    QSettings s(OPENTRACK_ORG);
    s.setValue(QStringLiteral("%1/geometry").arg(settings_group), saveGeometry());
}

void head_test_window::closeEvent(QCloseEvent* e)
{
    save_settings();
    e->accept();
}

// Debug/verification aid: OPENTRACK_HEAD_TEST_SNAPSHOT=<dir> renders each scene from a few
// head poses into <dir>/<scene>-<pose>.png and then quits the application.
void head_test_window::run_snapshots(const QString& dir)
{
    QDir().mkpath(dir);
    // whole windows as the user sees them, tracking stopped
    snapshot_mode = false;
    tick();
    QApplication::processEvents();
    grab().save(QDir(dir).filePath("window.png"));
    gl->grabFramebuffer().save(QDir(dir).filePath("stopped.png"));
    for (QWidget* w : QApplication::topLevelWidgets())
        if (w != this && w->isVisible() && w->inherits("QMainWindow"))
            w->grab().save(QDir(dir).filePath("main-window.png"));
    snapshot_mode = true;
    struct shot { const char* name; std::array<double, 6> pose; };
    static const shot shots[] = {
        { "center",      { 0, 0, 0,  0, 0, 0 } },
        { "left60",      { 0, 0, 0,  -60, -5, 0 } },
        { "right100",    { 0, 0, 0,  100, 0, 0 } },
        { "check6",      { -8, 0, 0, -150, 10, 0 } },
        { "down",        { 0, 0, 0,  0, -45, 0 } },
        { "lean-roll",   { 15, -3, -10, 20, 5, 15 } },
    };
    static const char* scene_names[] = { "cockpit", "racecar", "soldier" };
    QStringList log;
    for (int sc = 0; sc < scene_count; sc++)
    {
        set_scene(sc);
        for (const shot& s : shots)
        {
            override_pose = s.pose;
            tick();
            const QImage img = gl->grabFramebuffer();
            const QString file = QDir(dir).filePath(QStringLiteral("%1-%2.png").arg(scene_names[sc], s.name));
            qDebug() << "head-test: snapshot" << file << img.save(file);
        }
        // rough frame time (includes a framebuffer read-back per frame)
        QElapsedTimer t;
        t.start();
        constexpr int frames = 30;
        for (int i = 0; i < frames; i++)
            (void)gl->grabFramebuffer();
        log << QStringLiteral("%1: %2 ms/frame").arg(scene_names[sc]).arg(double(t.nsecsElapsed()) / 1e6 / frames, 0, 'f', 2);
    }
    if (!gl->gl_error().isEmpty())
        log << "GL error: " + gl->gl_error();
    QFile f(QDir(dir).filePath("log.txt"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(log.join(QLatin1Char('\n')).toUtf8());
    QApplication::exit(0);
}
