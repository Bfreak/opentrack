/* Copyright (c) 2015, Stanislaw Halik <sthalik@misaki.pl>

 * Permission to use, copy, modify, and/or distribute this
 * software for any purpose with or without fee is hereby granted,
 * provided that the above copyright notice and this permission
 * notice appear in all copies.
 */

#include "options-dialog.hpp"
#include "listener.h"

#include <utility>

#include "logic/multi-protocol.hpp"

#include <QPushButton>
#include <QLayout>
#include <QDialog>
#include <QFileDialog>
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QVBoxLayout>

using namespace options;
using namespace options::globals;

QString options_dialog::kopts_to_string(const key_opts& kopts)
{
    using namespace Qt::Literals::StringLiterals;
    if (!kopts.guid->isEmpty())
    {
        const int btn = kopts.button & ~Qt::KeyboardModifierMask;
        const int mods = kopts.button & Qt::KeyboardModifierMask;
        QString mm;
        if (mods & Qt::ControlModifier) mm += "Control+";
        if (mods & Qt::AltModifier) mm += "Alt+";
        if (mods & Qt::ShiftModifier) mm += "Shift+";
        const auto& str = kopts.guid == "mouse"_L1
                          ? tr("Mouse %1")
                          : kopts.guid->startsWith("GI!"_L1)
                          ? tr("Gamepad button %1")
                          : tr("Joy button %1");
        return mm + str.arg(QString::number(btn));
    }
    if (kopts.keycode->isEmpty())
        return tr("None");
    return kopts.keycode;
}

void options_dialog::set_disable_translation_state(bool value)
{
    with_global_settings_object([&](QSettings& s)
    {
        s.setValue("disable-translation", value);
        mark_global_ini_modified();
    });
}

options_dialog::options_dialog(std::unique_ptr<ITrackerDialog>& tracker_dialog_,
                               std::unique_ptr<IProtocolDialog>& proto_dialog_,
                               std::unique_ptr<IFilterDialog>& filter_dialog_,
                               std::function<void(bool)> pause_keybindings,
                               protocol_hooks protocols) :
    pause_keybindings(std::move(pause_keybindings)),
    proto_hooks(std::move(protocols))
{
    ui.setupUi(this);
    init_protocols_tab();

    tie_setting(main.tray_enabled, ui.trayp);
    tie_setting(main.tray_start, ui.tray_start);

    tie_setting(main.center_at_startup, ui.center_at_startup);
    tie_setting(main.auto_center, ui.auto_center);
    tie_setting(main.auto_center_speed, ui.auto_center_speed);

    const centering_state centering_modes[] = {
        center_disabled,
        center_point,
        center_vr360,
        center_roll_compensated,
    };
    for (int k = 0; k < 4; k++)
        ui.cbox_centering->setItemData(k, centering_modes[k]);
    tie_setting(main.centering_mode, ui.cbox_centering);

    const reltrans_state reltrans_modes[] = {
        reltrans_disabled,
        reltrans_enabled,
        reltrans_non_center,
    };

    for (int k = 0; k < 3; k++)
        ui.reltrans_mode->setItemData(k, reltrans_modes[k]);

    tie_setting(main.apply_mapping_curves, ui.apply_mapping_curves);

    tie_setting(main.reltrans_mode, ui.reltrans_mode);

    tie_setting(main.reltrans_disable_tx, ui.tcomp_tx_disable);
    tie_setting(main.reltrans_disable_ty, ui.tcomp_ty_disable);
    tie_setting(main.reltrans_disable_tz, ui.tcomp_tz_disable);

    tie_setting(main.reltrans_disable_src_yaw, ui.tcomp_src_yaw_disable);
    tie_setting(main.reltrans_disable_src_pitch, ui.tcomp_src_pitch_disable);
    tie_setting(main.reltrans_disable_src_roll, ui.tcomp_src_roll_disable);

    tie_setting(main.neck_z, ui.neck_z);

    tie_setting(main.a_x.zero, ui.pos_tx);
    tie_setting(main.a_y.zero, ui.pos_ty);
    tie_setting(main.a_z.zero, ui.pos_tz);
    tie_setting(main.a_yaw.zero, ui.pos_rx);
    tie_setting(main.a_pitch.zero, ui.pos_ry);
    tie_setting(main.a_roll.zero, ui.pos_rz);

    tie_setting(main.a_yaw.invert_pre, ui.invert_yaw_pre);
    tie_setting(main.a_pitch.invert_pre, ui.invert_pitch_pre);
    tie_setting(main.a_roll.invert_pre, ui.invert_roll_pre);
    tie_setting(main.a_x.invert_pre, ui.invert_x_pre);
    tie_setting(main.a_y.invert_pre, ui.invert_y_pre);
    tie_setting(main.a_z.invert_pre, ui.invert_z_pre);

    tie_setting(main.a_yaw.invert_post, ui.invert_yaw_post);
    tie_setting(main.a_pitch.invert_post, ui.invert_pitch_post);
    tie_setting(main.a_roll.invert_post, ui.invert_roll_post);
    tie_setting(main.a_x.invert_post, ui.invert_x_post);
    tie_setting(main.a_y.invert_post, ui.invert_y_post);
    tie_setting(main.a_z.invert_post, ui.invert_z_post);

    tie_setting(main.a_yaw.src, ui.src_yaw);
    tie_setting(main.a_pitch.src, ui.src_pitch);
    tie_setting(main.a_roll.src, ui.src_roll);
    tie_setting(main.a_x.src, ui.src_x);
    tie_setting(main.a_y.src, ui.src_y);
    tie_setting(main.a_z.src, ui.src_z);

    tie_setting(main.enable_camera_offset, ui.enable_camera_offset);
    tie_setting(main.camera_offset_yaw,   ui.camera_offset_yaw);
    tie_setting(main.camera_offset_pitch, ui.camera_offset_pitch);
    tie_setting(main.camera_offset_roll,  ui.camera_offset_roll);
    tie_setting(main.camera_offset_x,  ui.camera_offset_x);
    tie_setting(main.camera_offset_y,  ui.camera_offset_y);
    tie_setting(main.camera_offset_z,  ui.camera_offset_z);

    //tie_setting(main.center_method, ui.center_method);

    tie_setting(main.tracklogging_enabled, ui.tracklogging_enabled);

    tie_setting(main.neck_enable, ui.neck_enable);

    const bool is_translation_disabled = with_global_settings_object([] (QSettings& s) {
        return s.value("disable-translation", false).toBool();
    });
    ui.disable_translation->setChecked(is_translation_disabled);

    struct tmp
    {
        key_opts& opt;
        QLabel* label;
        QPushButton* button;
    } tuples[] =
    {
        { main.key_center1, ui.center_text, ui.bind_center },
        { main.key_center2, ui.center_text_2, ui.bind_center_2 },

        { main.key_toggle1, ui.toggle_text, ui.bind_toggle },
        { main.key_toggle2, ui.toggle_text_2, ui.bind_toggle_2 },

        { main.key_toggle_press1, ui.toggle_held_text, ui.bind_toggle_held },
        { main.key_toggle_press2, ui.toggle_held_text_2, ui.bind_toggle_held_2 },

        { main.key_zero1, ui.zero_text, ui.bind_zero },
        { main.key_zero2, ui.zero_text_2, ui.bind_zero_2 },

        { main.key_zero_press1, ui.zero_held_text, ui.bind_zero_held },
        { main.key_zero_press2, ui.zero_held_text_2, ui.bind_zero_held_2 },

        { main.key_start_tracking1, ui.start_tracking_text, ui.bind_start },
        { main.key_start_tracking2, ui.start_tracking_text_2, ui.bind_start_2 },

        { main.key_stop_tracking1, ui.stop_tracking_text , ui.bind_stop },
        { main.key_stop_tracking2, ui.stop_tracking_text_2 , ui.bind_stop_2 },

        { main.key_toggle_tracking1, ui.toggle_tracking_text, ui.bind_toggle_tracking },
        { main.key_toggle_tracking2, ui.toggle_tracking_text_2, ui.bind_toggle_tracking_2 },

        { main.key_restart_tracking1, ui.restart_tracking_text, ui.bind_restart_tracking },
        { main.key_restart_tracking2, ui.restart_tracking_text_2, ui.bind_restart_tracking_2 },
    };

    for (const tmp& val_ : tuples)
    {
        tmp val = val_;
        val.label->setText(kopts_to_string(val.opt));
        connect(&val.opt.keycode,
                static_cast<void (value_::*)(const QString&) const>(&value_::valueChanged),
                val.label,
                [=](const QString&) { val.label->setText(kopts_to_string(val.opt)); });
        {
            connect(val.button, &QPushButton::clicked, this, [=, this] { bind_key(val.opt, val.label); });
        }
    }

    auto add_module_tab = [this] (auto& place, auto&& dlg, const QString& label) {
        if (dlg && dlg->embeddable())
        {
            using BaseDialog = plugin_api::detail::BaseDialog;

            dlg->set_buttons_visible(false);
            place = dlg.release();
            ui.tabWidget->addTab(place, label);
            QObject::connect(place, &BaseDialog::closing, this, &QDialog::close);
        }
    };

    add_module_tab(tracker_dialog, tracker_dialog_, tr("Tracker"));
    add_module_tab(proto_dialog, proto_dialog_, tr("Output"));
    add_module_tab(filter_dialog, filter_dialog_, tr("Filter"));

    connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &options_dialog::accept);
    connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &options_dialog::reject);
    connect(this, &options_dialog::accepted, this, &options_dialog::doAccept);
    connect(this, &options_dialog::rejected, this, &options_dialog::doReject);
}

// ---- Trackhat: Protocols tab ----

void options_dialog::init_protocols_tab()
{
    if (proto_hooks.protocols.empty())
        return;

    protocols_tab = new QWidget(this);
    auto* outer = new QVBoxLayout(protocols_tab);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(10);

    auto* intro = new QLabel(tr("Head tracking is sent to every protocol ticked below at the same time, "
                                "so most games and simulators work without choosing one."), protocols_tab);
    intro->setWordWrap(true);
    outer->addWidget(intro);

    auto* box = new QGroupBox(tr("Output protocols"), protocols_tab);
    auto* grid = new QGridLayout(box);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(1, 1);

    int row = 0;
    for (const auto& lib : proto_hooks.protocols)
    {
        if (!lib)
            continue;
        const QString name = lib->module_name;
        protocol_row r;
        r.module_name = name;
        r.enabled = new QCheckBox(lib->name, box);
        r.enabled->setIcon(lib->icon);
        r.status = new QLabel(box);
        r.status->setWordWrap(true);
        auto* settings = new QPushButton(tr("Settings"), box);
        settings->setEnabled(lib->Dialog != nullptr);
        settings->setToolTip(tr("Settings for %1").arg(lib->name));

        grid->addWidget(r.enabled, row, 0);
        grid->addWidget(r.status, row, 1);
        grid->addWidget(settings, row, 2);
        row++;

        QCheckBox* cb = r.enabled;
        connect(cb, &QCheckBox::toggled, this, [this, cb, name](bool on) {
            if (on && multi_protocol::is_mouse(name) && !confirm_mouse_emulation())
            {
                QSignalBlocker block(cb);
                cb->setChecked(false);
                return;
            }
            mods.b->store_kv(multi_protocol::settings_key(name), on);
            refresh_protocol_status();
        });
        connect(settings, &QPushButton::clicked, this, [this, name] {
            if (proto_hooks.show_settings)
                proto_hooks.show_settings(name);
        });
        proto_rows.push_back(r);
    }
    outer->addWidget(box);

    auto* note = new QLabel(tr("Changes take effect the next time tracking is started."), protocols_tab);
    note->setWordWrap(true);
    note->setEnabled(false);
    outer->addWidget(note);
    outer->addStretch(1);

    ui.tabWidget->insertTab(2, protocols_tab, tr("Protocols"));
    ui.tabWidget->setCurrentIndex(0);

    connect(&*mods.b, &options::detail::bundle::reloading, this, &options_dialog::sync_protocol_checkboxes);
    sync_protocol_checkboxes();
}

void options_dialog::sync_protocol_checkboxes()
{
    for (protocol_row& r : proto_rows)
    {
        const QString key = multi_protocol::settings_key(r.module_name);
        const bool on = mods.b->contains(key) ? mods.b->get_variant(key).toBool()
                                              : multi_protocol::default_enabled(r.module_name);
        QSignalBlocker block(r.enabled);
        r.enabled->setChecked(on);
    }
    refresh_protocol_status();
}

void options_dialog::refresh_protocol_status()
{
    for (protocol_row& r : proto_rows)
    {
        QString text, style;
        if (!r.enabled->isChecked())
            text = tr("Off");
        else if (proto_hooks.status)
        {
            const protocol_status st = proto_hooks.status(r.module_name);
            switch (st.state)
            {
            case protocol_status::active:
                text = tr("Active");
                style = QStringLiteral("color: #4ADE80;");
                break;
            case protocol_status::failed:
                text = tr("Not available: %1").arg(st.error);
                style = QStringLiteral("color: #FBBF24;");
                break;
            case protocol_status::unknown:
                break;
            }
        }
        r.status->setText(text);
        r.status->setStyleSheet(style);
        r.status->setEnabled(r.enabled->isChecked());
    }
}

bool options_dialog::confirm_mouse_emulation()
{
    auto bound = [](const key_opts& k) { return !k.keycode->isEmpty() || !k.guid->isEmpty(); };
    const bool has_pause = bound(main.key_toggle1) || bound(main.key_toggle2) ||
                           bound(main.key_toggle_press1) || bound(main.key_toggle_press2);
    const bool has_stop = bound(main.key_stop_tracking1) || bound(main.key_stop_tracking2) ||
                          bound(main.key_toggle_tracking1) || bound(main.key_toggle_tracking2);

    QString text = tr("With mouse emulation on, your head movement controls this computer's mouse cursor.\n\n"
                      "As soon as tracking starts, the cursor follows your head. That can make it hard "
                      "to click anything, including the Stop button.\n\n"
                      "Before you continue, set keyboard shortcuts for pausing tracking (Toggle) and for "
                      "stopping tracking in the Shortcuts tab, so you can always get the cursor back.");
    QStringList missing;
    if (!has_pause)
        missing << tr("Toggle (pause)");
    if (!has_stop)
        missing << tr("Stop tracking");
    if (!missing.isEmpty())
        text += QStringLiteral("\n\n") + tr("You don't have a shortcut set yet for: %1.").arg(missing.join(QStringLiteral(", ")));

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Mouse emulation"));
    box.setText(tr("Your head will move the mouse cursor"));
    box.setInformativeText(text);
    QPushButton* yes = box.addButton(tr("Enable mouse emulation"), QMessageBox::AcceptRole);
    QPushButton* no = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(no);
    box.exec();
    return box.clickedButton() == yes;
}

void options_dialog::switch_to_protocols_tab()
{
    if (protocols_tab)
        ui.tabWidget->setCurrentWidget(protocols_tab);
}

void options_dialog::bind_key(key_opts& kopts, QLabel* label)
{
    kopts.button = -1;
    kopts.guid = {};
    kopts.keycode = {};

    auto* k = new keyboard_listener;
    k->deleteLater();
    k->setWindowModality(Qt::ApplicationModal);

    {
        QObject obj;
        connect(&*k, &keyboard_listener::key_pressed,
                &obj,
                [&](const QKeySequence& s)
                {
                    kopts.keycode = s.toString(QKeySequence::PortableText);
                    kopts.guid = {};
                    kopts.button = -1;
                    k->close();
                });
        connect(&*k, &keyboard_listener::joystick_button_pressed,
                &obj,
                [&](const QString& guid, int idx, bool held)
                {
                    if (!k)
                        std::abort();
                    if (!held)
                    {
                        kopts.guid = guid;
                        kopts.keycode = {};
                        kopts.button = idx;
                        k->close();
                    }
                });
        connect(&*main.b, &options::detail::bundle::reloading, &*k, &QDialog::close);
        pause_keybindings(false);
        k->exec();
        k = nullptr;
        pause_keybindings(true);
    }
    const bool is_crap = progn(
        for (const QChar& c : kopts.keycode())
            if (!c.isPrint())
                return true;
        return false;
    );
    if (is_crap)
    {
        kopts.keycode = {};
        kopts.guid = {};
        kopts.button = -1;
        label->setText(tr("None"));
    }
    else
        label->setText(kopts_to_string(kopts));
    //qDebug() << "bind_key done" << kopts.guid << kopts.button << kopts.keycode;
}

void options_dialog::switch_to_tracker_tab()
{
    if (tracker_dialog)
        ui.tabWidget->setCurrentWidget(tracker_dialog);
    else
        eval_once(qDebug() << "options: asked for tracker tab widget with old-style widget dialog!");
}

void options_dialog::switch_to_proto_tab()
{
    if (proto_dialog)
        ui.tabWidget->setCurrentWidget(proto_dialog);
    else
        eval_once(qDebug() << "options: asked for proto tab widget with old-style widget dialog!");
}

void options_dialog::switch_to_filter_tab()
{
    if (filter_dialog)
        ui.tabWidget->setCurrentWidget(filter_dialog);
    else
        eval_once(qDebug() << "options: asked for filter tab widget with old-style widget dialog!");
}

void options_dialog::tracker_module_changed()
{
    if (tracker_dialog)
    {
        unregister_tracker();
        reload();
        delete tracker_dialog;
        tracker_dialog = nullptr;
    }
}

void options_dialog::proto_module_changed()
{
    if (proto_dialog)
    {
        unregister_protocol();
        reload();
        delete proto_dialog;
        proto_dialog = nullptr;
    }
}

void options_dialog::filter_module_changed()
{
    if (filter_dialog)
    {
        unregister_filter();
        reload();
        delete filter_dialog;
        filter_dialog = nullptr;
    }
}

void options_dialog::register_tracker(ITracker* t)
{
    if (tracker_dialog)
        tracker_dialog->register_tracker(t);
}

void options_dialog::unregister_tracker()
{
    if (tracker_dialog)
        tracker_dialog->unregister_tracker();
}

void options_dialog::register_protocol(IProtocol* p)
{
    if (proto_dialog)
        proto_dialog->register_protocol(p);
}

void options_dialog::unregister_protocol()
{
    if (proto_dialog)
        proto_dialog->unregister_protocol();
}

void options_dialog::register_filter(IFilter* f)
{
    if (filter_dialog)
        filter_dialog->register_filter(f);
}

void options_dialog::unregister_filter()
{
    if (filter_dialog)
        filter_dialog->unregister_filter();
}

using module_list = std::initializer_list<plugin_api::detail::BaseDialog*>;

void options_dialog::save()
{
    qDebug() << "options_dialog: save";
    main.b->save();
    mods.b->save();
    ui.game_detector->save();
    set_disable_translation_state(ui.disable_translation->isChecked());

    for (auto* dlg : module_list{ tracker_dialog, proto_dialog, filter_dialog })
        if (dlg)
            dlg->save();
}

void options_dialog::reload()
{
    qDebug() << "options_dialog: reload";
    ui.game_detector->revert();

    main.b->reload();
    mods.b->reload();
    for (auto* dlg : module_list{ tracker_dialog, proto_dialog, filter_dialog })
        if (dlg)
            dlg->reload();
}

void options_dialog::closeEvent(QCloseEvent *)
{
    qDebug() << "options_dialog: closeEvent";
    reject();
    emit closing();
}

void options_dialog::doOK()
{
    qDebug() << "options_dialog: doOK";
    accept();
}
void options_dialog::doCancel()
{
    qDebug() << "options_dialog: doCancel";
    reject();
}

void options_dialog::doAccept()
{
    qDebug() << "options_dialog: doAccept";
    save();
}

void options_dialog::doReject()
{
    qDebug() << "options_dialog: doReject";
    reload();
}

options_dialog::~options_dialog()
{
    if (tracker_dialog)
        tracker_dialog->unregister_tracker();
    if (proto_dialog)
        proto_dialog->unregister_protocol();
    if (filter_dialog)
        filter_dialog->unregister_filter();
}
