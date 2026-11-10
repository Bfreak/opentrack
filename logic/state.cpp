#include "state.hpp"
#include "opentrack/defs.hpp"
#include <iterator>

using dylib_ptr = Modules::dylib_ptr;
using dylib_list = Modules::dylib_list;

std::tuple<dylib_ptr, int> State::module_by_name(const QString& name, dylib_list& list)
{
    auto it = std::find_if(list.cbegin(), list.cend(), [&name](const dylib_ptr& lib) {
        if (!lib)
            return name.isEmpty();
        else
            return name == lib->module_name;
    });

    if (it == list.cend())
        return { nullptr, -1 };
    else
        return { *it, int(std::distance(list.cbegin(), it)) };
}

State::State(const QString& library_path) :
    modules(library_path),
    pose(s.all_axis_opts),
    library_path{library_path}
{}

dylib_ptr State::current_tracker()
{
    const QString& module =
#ifdef UI_FORCED_TRACKER
        UI_FORCED_TRACKER;
#else
        m.tracker_dll;
#endif
    auto [ptr, idx] = module_by_name(module, modules.trackers());
    return ptr;
}

dylib_ptr State::current_protocol()
{
    auto [ptr, idx] = module_by_name(m.protocol_dll, modules.protocols());
    return ptr;
}

bool State::protocol_enabled(const QString& module_name)
{
    const QString key = multi_protocol::settings_key(module_name);
    if (m.b->contains(key))
        return m.b->get_variant(key).toBool();
    return multi_protocol::default_enabled(module_name);
}

dylib_list State::enabled_protocols()
{
    dylib_list ret;
    for (const dylib_ptr& lib : modules.protocols())
        if (lib && protocol_enabled(lib->module_name))
            ret.push_back(lib);
    return ret;
}

dylib_ptr State::current_filter()
{
    const QString& module =
#ifdef UI_FORCED_FILTER
        UI_FORCED_FILTER;
#else
        m.filter_dll;
#endif
    auto [ptr, idx] = module_by_name(module, modules.filters());
    return ptr;
}
