#pragma once

#include "export.hpp"

#include "gui/ui_options-dialog.h"
#include "input/shortcuts.h"
#include "logic/main-settings.hpp"

#include "api/plugin-support.hpp"

#include <functional>
#include <memory>
#include <vector>

#include <QObject>
#include <QDialog>
#include <QWidget>

class OTR_GUI_EXPORT options_dialog final : public QDialog
{
    Q_OBJECT
signals:
    void closing();
public:
    // Trackhat: the Protocols tab (which output protocols are on, their settings and status)
    struct protocol_status
    {
        enum { unknown, active, failed } state = unknown;
        QString error;
    };
    struct protocol_hooks
    {
        std::vector<std::shared_ptr<dylib>> protocols;
        std::function<void(const QString& module_name)> show_settings;
        std::function<protocol_status(const QString& module_name)> status;
    };

    options_dialog(std::unique_ptr<ITrackerDialog>& tracker_dialog_,
                   std::unique_ptr<IProtocolDialog>& proto_dialog_,
                   std::unique_ptr<IFilterDialog>& filter_dialog_,
                   std::function<void(bool)> pause_keybindings,
                   protocol_hooks protocols = {});
    void switch_to_protocols_tab();
    void refresh_protocol_status();
    ~options_dialog() override;
    inline bool embeddable() noexcept { return false; }
    void switch_to_tracker_tab();
    void switch_to_proto_tab();
    void switch_to_filter_tab();
    void tracker_module_changed();
    void proto_module_changed();
    void filter_module_changed();
    void register_tracker(ITracker* t);
    void unregister_tracker();
    void register_protocol(IProtocol* p);
    void unregister_protocol();
    void register_filter(IFilter* f);
    void unregister_filter();
    void save();
    void reload();
private:
    void closeEvent(QCloseEvent*) override;
    static QString kopts_to_string(const key_opts& opts);

    void init_protocols_tab();
    void sync_protocol_checkboxes();
    bool confirm_mouse_emulation();

    struct protocol_row
    {
        QString module_name;
        class QCheckBox* enabled = nullptr;
        QLabel* status = nullptr;
    };
    protocol_hooks proto_hooks;
    std::vector<protocol_row> proto_rows;
    QWidget* protocols_tab = nullptr;
    main_settings_impl::module_settings mods;

    main_settings main;
    std::function<void(bool)> pause_keybindings;
    Ui::options_dialog ui;

    ITrackerDialog*  tracker_dialog = nullptr;
    IProtocolDialog* proto_dialog   = nullptr;
    IFilterDialog*   filter_dialog  = nullptr;

private slots:
    void doOK();
    void doCancel();
    void doAccept();
    void doReject();

    void bind_key(key_opts &kopts, QLabel* label);
    void set_disable_translation_state(bool value);
};
