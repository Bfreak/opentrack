#pragma once

// Trackhat: the pose is sent to every enabled output protocol at once, instead of one chosen protocol.

#include "api/plugin-support.hpp"
#include "export.hpp"

#include <memory>
#include <vector>

#include <QString>

class OTR_LOGIC_EXPORT multi_protocol final : public IProtocol
{
public:
    using dylib_ptr = std::shared_ptr<dylib>;

    struct entry
    {
        dylib_ptr lib;
        std::shared_ptr<IProtocol> instance; // null when it failed to start
        QString error;                       // why it failed, empty when running
    };

    // Creates and initializes each protocol. One that fails is skipped (its error is kept);
    // it never prevents tracking from starting.
    explicit multi_protocol(const std::vector<dylib_ptr>& libs);
    ~multi_protocol() override;

    module_status initialize() override;
    void pose(const double* pose, const double* raw) override;
    QString game_name() override;

    const std::vector<entry>& entries() const { return entries_; }
    // running instance of a protocol by module name ("freetrack", "udp", ...), or null
    IProtocol* find(const QString& module_name) const;

    // --- which protocols are enabled (stored per profile in the "modules" section) ---
    // Everything is on by default except mouse emulation.
    static bool default_enabled(const QString& module_name);
    static QString settings_key(const QString& module_name);
    static bool is_mouse(const QString& module_name);

private:
    std::vector<entry> entries_;
};
