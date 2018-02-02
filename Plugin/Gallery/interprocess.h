#pragma once
//TODO
namespace ipc
{

    struct message
    {
        using value_type = std::variant<
            vr::Compositor_FrameTiming, vr::Compositor_CumulativeStats>;
        using size_type = core::max_size<value_type>;
        //constexpr size_t size = size_type::value;
        constexpr static size_t size() noexcept
        {
            return size_type::value;
        }
        constexpr static size_t aligned_size(size_t align=128) noexcept
        {
            return size() + (align - size() % align) % align;
        }
    };
#pragma warning(push)
#pragma warning(disable:4251)
    class DLLAPI channel : protected std::enable_shared_from_this<channel>
    {
        std::promise<void> terminate_;
        asio::thread_pool context_;
        std::optional<interprocess::message_queue> sender_;  //overcome NonDefaultConstructible limit
        std::optional<interprocess::message_queue> receiver_;
    public:
        explicit channel(size_t threads = 1, bool open_only = true);
        channel(const channel&) = delete;
        channel(channel&&) = delete;
        channel& operator=(const channel&) = delete;
        channel& operator=(channel&&) = delete;
        std::pair<std::future<void>, size_t> async_receive();
        void async_send();
        bool valid() const noexcept;
        void clear();
        ~channel();
    private:
    };
#pragma warning(pop)
    //using role = channel::role;
}
