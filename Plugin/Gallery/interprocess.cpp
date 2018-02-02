#include "stdafx.h"
#include "interprocess.h"
namespace
{
    namespace config
    {
        constexpr auto identity_monitor = "___MessageQueue$MonitorExe_"sv;
        constexpr auto identity_plugin = "___MessageQueue$PluginDll_"sv;
    }
}
ipc::channel::channel(size_t threads, bool open_only)
try : context_(threads)
//, sender_(std::in_place, interprocess::open_only, "")
{
    if(open_only)
    {
        sender_.emplace(interprocess::open_only, config::identity_plugin.data());
        receiver_.emplace(interprocess::open_only, config::identity_monitor.data());
        return;
    }
    sender_.emplace(interprocess::create_only, config::identity_monitor.data(),100,100);
    receiver_.emplace(interprocess::create_only, config::identity_plugin.data(),100,100);
}
catch (...)
{
    sender_.reset();
    receiver_.reset();
    context_.join();
    throw;
}
bool ipc::channel::valid() const noexcept
{
    return sender_.has_value() && receiver_.has_value();
}
void ipc::channel::clear()
{
    terminate_.set_value();
}
ipc::channel::~channel()
{
    context_.stop();
    sender_.reset();
    receiver_.reset();
    context_.join();
}
