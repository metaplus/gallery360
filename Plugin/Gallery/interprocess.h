#pragma once
//TODO
namespace ipc
{
#pragma warning(push)
#pragma warning(disable:4251)
    class DLLAPI message
    {
    public:     // TODO: make non constructible type contained by std::optional
        template<typename U, typename ...Types>
        struct basic_serializable
        {
            std::conditional_t<(sizeof...(Types) > 0), std::tuple<U, Types...>, U> data;
            using value_type = decltype(data);
            basic_serializable() = default;
            explicit basic_serializable(const U& a0, const Types& ...args)
                //: data( std::forward<U>(a0), std::forward<Types>(args)... )
                : data(a0, args...)
            {}
            template<typename Archive>
            void serialize(Archive& archive) { archive(data); }
            static_assert(std::conjunction_v<std::is_object<U>, std::is_object<Types>...>);
        };
        template<>
        struct basic_serializable<void>
        {
            using value_type = void;
            basic_serializable() = default;
            template<typename Archive>
            static void serialize(Archive& archive) { archive("null"s); }
        };
        struct info_launch : basic_serializable<void> { using basic_serializable::basic_serializable; };
        struct info_started : basic_serializable<void> { using basic_serializable::basic_serializable; };
        struct info_exit : basic_serializable<void> { using basic_serializable::basic_serializable; };
        struct info_url : basic_serializable<std::string> { using basic_serializable::basic_serializable; };
        struct update_index : basic_serializable<size_t> { using basic_serializable::basic_serializable; };
        struct tagged_pack : basic_serializable<std::string, std::string> { using basic_serializable::basic_serializable; };
        struct first_frame_available : basic_serializable<void> { using basic_serializable::basic_serializable; };
        struct first_frame_updated : basic_serializable<void> { using basic_serializable::basic_serializable; };
    private:
        std::variant<
            info_launch, info_started, info_exit, info_url,
            first_frame_available, first_frame_updated,
            vr::Compositor_FrameTiming, vr::Compositor_CumulativeStats,
            update_index, tagged_pack
        > data_;
        std::chrono::high_resolution_clock::duration duration_;
        std::string description_;
        using size_trait = meta::max_size<decltype(data_)>;
                using value_type = decltype(data_);
    public:
        template<typename Various>
        struct is_alternative : meta::is_within<meta::remove_cv_ref_t<Various>, value_type> {};
        constexpr static size_t size();
        message();
        template<typename Various, typename = std::enable_if_t<is_alternative<Various>::value>>
        explicit message(Various&& data, std::chrono::high_resolution_clock::duration duration = 0ns);
        message(const message&) = default;
        message(message&&) noexcept = default;
        message& operator=(const message&) = default;
        message& operator=(message&&) noexcept = default;
        template<typename Various, typename = std::enable_if_t<is_alternative<Various>::value>>
        message& emplace(Various&& data);
        constexpr size_t index() const;
        template<typename Various>
        constexpr static size_t index();
        constexpr static size_t index_size() { return std::variant_size_v<value_type>; }
        template<typename Various>
        bool is() const;
        const std::chrono::high_resolution_clock::duration& timing() const;
    private:
        friend cereal::access;
        template<typename Various>
        void serialize(Various& archive);
    };

    template <typename Various>
    constexpr size_t message::index() 
    {
        static_assert(std::is_object_v<Various> && !std::is_const_v<Various>);
        static_assert(meta::is_within_v<Various, value_type>);
        return meta::index<Various, value_type>::value;
    }

    template <typename Various, typename>
    message::message(Various&& data, std::chrono::high_resolution_clock::duration duration)
        : data_(std::forward<Various>(data))
        , duration_(std::move(duration))
        , description_(core::type_shortname<meta::remove_cv_ref_t<Various>>())
    {}

    template <typename Various, typename>
    message& message::emplace(Various&& data)
    {
        data_.emplace<meta::remove_cv_ref_t<Various>>(std::forward<Various>(data));
        description_.assign(core::type_shortname<meta::remove_cv_ref_t<Various>>());
        return *this;
    }

    template <typename Various>
    bool message::is() const 
    {
        return std::get_if<Various>(&data_) != nullptr;
    }

    template <typename Archive>
    void message::serialize(Archive& archive)
    {
        archive(
            cereal::make_nvp("TimingNs", duration_),
            cereal::make_nvp("Description", description_),
            cereal::make_nvp("Message", data_));
    }

    class DLLAPI channel 
    {
        std::atomic<bool> running_;
        struct endpoint
        {   
            std::optional<core::scope_guard> shmem_remover;                        // RAII guarder for shared memory management 
            std::optional<interprocess::message_queue> messages;    // overcome NonDefaultConstructible limit
			sync::chain pending;
            endpoint() = default;
        };           
        endpoint send_context_; 
        endpoint recv_context_;
        std::function<unsigned(const ipc::message&)> prioritizer_;
    public:
        explicit channel(bool open_only = true);
        channel(const channel&) = delete;
        channel& operator=(const channel&) = delete;
        void prioritize_by(std::function<unsigned(const ipc::message&)> prior);
        std::future<ipc::message> async_receive();
        void async_send(ipc::message message);
        void send(ipc::message message);
        ipc::message receive();
        bool valid() const;
        void wait();
        ~channel();
    private:
        static constexpr size_t buffer_size() ;
        static unsigned default_prioritize(const ipc::message& message);
        void do_send(const ipc::message& message);
        ipc::message do_receive();
        static_assert(std::is_same_v<size_t, interprocess::message_queue::size_type>);
        static_assert(std::chrono::high_resolution_clock::is_steady);
    };
#pragma warning(pop)
}