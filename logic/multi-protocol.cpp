#include "multi-protocol.hpp"

#include <QDebug>

multi_protocol::multi_protocol(const std::vector<dylib_ptr>& libs)
{
    for (const dylib_ptr& lib : libs)
    {
        if (!lib)
            continue;
        entry e;
        e.lib = lib;
        e.instance = make_dylib_instance<IProtocol>(lib);
        if (!e.instance)
            e.error = QStringLiteral("can't load module");
        else if (module_status st = e.instance->initialize(); !st.is_ok())
        {
            e.error = st.error.trimmed();
            e.instance = nullptr;
        }
        if (!e.error.isEmpty())
            qDebug() << "protocol" << lib->module_name << "not started:" << e.error;
        entries_.push_back(std::move(e));
    }
}

multi_protocol::~multi_protocol() = default;

module_status multi_protocol::initialize()
{
    return status_ok();
}

void multi_protocol::pose(const double* pose, const double* raw)
{
    for (const entry& e : entries_)
        if (e.instance)
            e.instance->pose(pose, raw);
}

QString multi_protocol::game_name()
{
    // only freetrack/TrackIR reports the game that is actually connected
    if (IProtocol* ft = find(QStringLiteral("freetrack")))
        return ft->game_name();
    return {};
}

IProtocol* multi_protocol::find(const QString& module_name) const
{
    for (const entry& e : entries_)
        if (e.instance && e.lib->module_name == module_name)
            return e.instance.get();
    return nullptr;
}

bool multi_protocol::is_mouse(const QString& module_name)
{
    return module_name.contains(QLatin1String("mouse"), Qt::CaseInsensitive);
}

bool multi_protocol::default_enabled(const QString& module_name)
{
    return !is_mouse(module_name);
}

QString multi_protocol::settings_key(const QString& module_name)
{
    return QStringLiteral("protocol-enabled-") + module_name;
}
