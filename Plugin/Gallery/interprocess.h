#pragma once
//TODO
namespace ipc
{

    class message
    {
        using value_type = std::variant<
            //std::monostate,
            vr::Compositor_FrameTiming,
            vr::Compositor_CumulativeStats,
            int
        >;
        using size_trait = core::max_size<value_type>;
        value_type data_;
    public:
        using container = std::array<std::byte, size_trait::value>;
        message(container& raw_bytes, size_t which);
        constexpr static size_t size() noexcept     //message body size
        {
            return size_trait::value;
        }
        template<typename U>
        constexpr bool has_type() const noexcept
        {
            return core::is_within_v<U, value_type>;
        }
        void get_if();
        template<typename U>
        struct handler;
        template<typename ...Types>
        struct handler<std::variant<Types...>>
        {
            //static_assert((std::is_default_constructible_v<handler<Types>>&&...));
            using type = std::variant<handler<Types>...>;
        };
        //using handler_type = handler<value_type>;
        using handler_type = handler<std::variant<size_t>>;
    private:
        constexpr static size_t aligned_size(size_t align = 128) noexcept
        {
            return size() + (align - size() % align) % align;
        } 
        //static_assert(std::is_nothrow_move_constructible_v<ipc::message>);
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
}