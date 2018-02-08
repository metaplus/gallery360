#pragma once
//TODO
namespace ipc
{
    namespace info
    {

    }
#pragma warning(push)
#pragma warning(disable:4251)
    class DLLAPI message
    {
    public:
        template<typename U, typename ...Types>
        struct basic_serializable
        {
            std::conditional_t<(sizeof...(Types) > 0), std::tuple<U, Types... >, U> data;
            using value_type = decltype(data);
            basic_serializable() = default;
            explicit basic_serializable(U&& a0, Types&& ...args)
                : data{ std::forward<U>(a0), std::forward<Types>(args)... } {}
            template<typename Archive>
            void serialize(Archive& archive)
            {
                archive(data);
            }
        };
    private:
        std::variant<
            vr::Compositor_FrameTiming,
            vr::Compositor_CumulativeStats
        > data_;
        std::chrono::high_resolution_clock::duration duration_;
        using size_trait = core::max_size<decltype(data_)>;
        using value_type = decltype(data_);
    public:
        template<typename Alternate>
        struct is_alternative : core::is_within <Alternate, value_type> {};
        constexpr static size_t size() noexcept; 
        constexpr static size_t aligned_size(size_t align = 128) noexcept;
        message() = default;
        message(message&&) noexcept = default;
        message& operator=(message&&) noexcept = default;
        template<typename Alternate, typename = std::enable_if_t<is_alternative<Alternate>::value>>
        explicit message(Alternate data, std::chrono::high_resolution_clock::duration duration = 0ns);
        size_t valid_size() const noexcept;
        constexpr size_t index() const noexcept;
        template<typename Visitor>
        decltype(auto) visit(Visitor&& visitor);
        template<typename Alternate, typename Callable>
        std::invoke_result_t<Callable, std::add_lvalue_reference_t<Alternate>> visit_as(Callable&& callable);
    private:
        friend cereal::access;
        template<typename Archive>
        void serialize(Archive& archive);
    };
    template <typename Alternate, typename>
    message::message(Alternate data, std::chrono::high_resolution_clock::duration duration)
        : data_(std::move(data)), duration_(std::move(duration)) {}
    template <typename Visitor>
    decltype(auto) message::visit(Visitor&& visitor) 
    { 
        return std::visit(std::forward<Visitor>(visitor), data_);
    }
    template <typename Alternate, typename Callable>
    std::invoke_result_t<Callable, std::add_lvalue_reference_t<Alternate>> 
        message::visit_as(Callable&& callable)
    {
        static_assert(!std::is_reference_v<Alternate>);
        static_assert(is_alternative<Alternate>::value);
        auto& alternate = std::get<Alternate>(data_);               //exception if invalid
        return std::invoke(std::forward<Callable>(callable), alternate);
    }
    template <typename Archive>
    void message::serialize(Archive& archive) 
    {
        archive(duration_, data_);
    }

    class DLLAPI channel : protected std::enable_shared_from_this<channel>
    {
        std::atomic<bool> running_;
        struct endpoint
        {   //std::packaged_task & std::function are inferior in claiming closure CopyConstructible
            //tbb::concurrent_queue<std::packaged_task<void()>> task_queue; 
            tbb::concurrent_queue<std::future<void>> task_queue;
            std::thread task_worker;                                //task concurrent model or std::async may be more slight
            core::scope_guard shmem_remover;                        //RAII guarder for shared memory management 
            std::optional<interprocess::message_queue> messages;    //overcome NonDefaultConstructible limit
            endpoint() = default;
        };           
        endpoint send_context_;
        endpoint recv_context_;
    public:
        explicit channel(bool open_only = true);
        std::pair<std::future<ipc::message>, size_t> async_receive();
        template<typename Alternate>
        std::enable_if_t<ipc::message::is_alternative<Alternate>::value>
            async_send(Alternate message, std::chrono::high_resolution_clock::duration duration);
        bool valid() const noexcept;
        ~channel();
    private:
        static_assert(std::is_same_v<size_t, interprocess::message_queue::size_type>);
        static_assert(std::chrono::high_resolution_clock::is_steady);
        constexpr static size_t buffer_size() noexcept;
    };
    template <typename Alternate>
    std::enable_if_t<ipc::message::is_alternative<Alternate>::value>
        channel::async_send(Alternate message, std::chrono::high_resolution_clock::duration duration) 
    {
        send_context_.task_queue.emplace(std::async(std::launch::deferred,
            [this, message = ipc::message{ std::move(message), std::move(duration) }]() mutable
        {
            const auto priority = static_cast<unsigned int>(message.index());
            static thread_local std::stringstream stream;
            stream.clear();
            stream.str(""s);
            {
                cereal::BinaryOutputArchive oarchive{ stream };
                oarchive << message;
            }
            auto buffer = stream.str();
            core::verify(buffer.size() < buffer_size());  //assume buffer_size never achieved
            while (running_.load(std::memory_order_acquire))
            {
                if (send_context_.messages->try_send(buffer.data(), buffer.size(), priority))
                    return;
                std::this_thread::sleep_for(1ms);
            }
            throw core::force_exit_exception{};
        }));
    }
#pragma warning(pop)
}