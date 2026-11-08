#pragma once

#include "export.hpp"

#include <array>
#include <functional>

#include <QWidget>
#include <QTimer>

class QButtonGroup;
class QLabel;
class QSlider;

namespace head_test { class view; }

// Window with a 3D scene (cockpit / race car / soldier) driven by the mapped head pose,
// so the user can try out their tracking and mapping curves.
class OTR_HEAD_TEST_EXPORT head_test_window final : public QWidget
{
    Q_OBJECT
public:
    // Fills pose[6] = { x, y, z (cm), yaw, pitch, roll (deg) } with the mapped output pose.
    // Returns false when tracking isn't running.
    using pose_source = std::function<bool(double* pose)>;

    explicit head_test_window(pose_source src, QWidget* parent = nullptr);
    ~head_test_window() override;

    // Shows the window large (not full screen), next to `anchor` if there's room.
    void show_beside(QWidget* anchor);

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    void set_scene(int idx);
    void tick();
    void save_settings();
    void run_snapshots(const QString& dir);

    pose_source source;
    head_test::view* gl = nullptr;
    QButtonGroup* scene_buttons = nullptr;
    QSlider* fov_slider = nullptr;
    QLabel* fov_label = nullptr;
    QTimer timer;
    int scene = 0;
    bool snapshot_mode = false;
    std::array<double, 6> override_pose {};
};
