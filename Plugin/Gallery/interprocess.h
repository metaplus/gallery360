#pragma once
//TODO
namespace ipc
{
#pragma warning(push)
#pragma warning(disable:4251)
    class DLLAPI message
    {
        std::variant<
            vr::Compositor_FrameTiming,
            vr::Compositor_CumulativeStats,
            double
        > data_;
        std::chrono::high_resolution_clock::duration duration_;
    public:
        using value_type = decltype(data_);
        using size_trait = core::max_size<value_type>;
        message() = default;
        template<typename Alternate, typename = std::enable_if_t<core::is_within_v<Alternate, value_type>>>
        explicit message(Alternate data, std::chrono::high_resolution_clock::duration duration = {});
        constexpr static size_t size() noexcept; //message body size
        constexpr size_t index() const noexcept;
        template<typename Visitor>
        decltype(auto) visit(Visitor&& visitor);
        template<typename Alternate, typename Callable>
        auto visit_as(Callable&& callable)->std::invoke_result_t<Callable, std::add_lvalue_reference_t<Alternate>>;
    private:
        constexpr static size_t aligned_size(size_t align = 128) noexcept;
        friend cereal::access;
        template<typename Archive>
        void serialize(Archive& archive);
    };
    template <typename Alternate, typename>
    message::message(Alternate data, std::chrono::high_resolution_clock::duration duration)
        : data_(std::move(data)), duration_(std::move(duration)) {
    }
    template <typename Visitor>
    decltype(auto) message::visit(Visitor&& visitor) {
        return std::visit(std::forward<Visitor>(visitor), data_);
    }
    template <typename Alternate, typename Callable>
    auto message::visit_as(Callable&& callable)
        -> std::invoke_result_t<Callable, std::add_lvalue_reference_t<Alternate>> {
        static_assert(!std::is_reference_v<Alternate>);
        static_assert(core::is_within_v<Alternate, value_type>);
        auto& alternate = std::get<Alternate>(data_);               //exception if invalid
        return std::invoke(std::forward<Callable>(callable), alternate);
    }
    template <typename Archive>
    void message::serialize(Archive& archive) {
        archive(duration_, data_);
    }
    class DLLAPI channel : protected std::enable_shared_from_this<channel>
    {
        std::atomic<bool> running_;
        struct endpoint
        {
            tbb::concurrent_queue<std::packaged_task<void()>> task_queue;
            std::thread task_worker;
            core::scope_guard shmem_remover;         //RAII guarder for shmem management 
            //d::optional<core::scope_guard> shmem_remover;         //RAII guarder for shmem management 
            std::optional<interprocess::message_queue> messages;    //overcome NonDefaultConstructible limit
            endpoint() = default;
        };           
        endpoint send_context_;
        endpoint recv_context_;
    public:
        explicit channel(std::chrono::steady_clock::duration timing, bool open_only = true);
        std::pair<std::future<void>, size_t> async_receive();
        template<typename Message>
        auto async_send(Message msg)->std::enable_if_t<core::is_within_v<Message>, message>;
        bool valid() const noexcept;
        ~channel();
    private:
    };
#pragma warning(pop)
}