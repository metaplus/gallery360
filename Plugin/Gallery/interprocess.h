#pragma once

namespace ipc
{
    //TODO: UNFULFILLED
    namespace config
    {
        constexpr auto identity_servant = "___MessageQueue$Servant_"sv;
        constexpr auto identity_master = "___MessageQueue$Master_"sv;
    }
    class channel : std::enable_shared_from_this<channel>
    {
        std::promise<void> terminate_;
        fmt::MemoryWriter data_;
        struct port
        {
            std::unique_ptr<boost::interprocess::message_queue> queue;

        };
    public:
        enum class role { servant, master };
        explicit channel(role r = role::servant);
        void clear();
        ~channel();
    };
    using role = channel::role;
}
