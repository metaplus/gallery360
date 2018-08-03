#pragma once

namespace dll
{
    std::unique_ptr<folly::NamedThreadFactory> create_thread_factory(std::string_view name_prefix);

    inline namespace config
    {
        using protocal_type = net::protocal::http;
        using request_body = boost::beast::http::empty_body;
        using response_body = boost::beast::http::dynamic_body;
        using response_container = net::protocal::http::protocal_base::response_type<response_body>;
        using net_session_ptr = net::client::session_ptr<protocal_type, net::policy<response_body>>;
        using ordinal = std::pair<int16_t, int16_t>;
    }

    namespace net_module
    {
        boost::asio::io_context& initialize();
        void release();
        boost::future<net_session_ptr> establish_http_session(folly::Uri const& uri);
    }

    namespace media_module
    {
        media::frame try_take_decoded_frame(int64_t id);
    }

    namespace graphic_module
    {
        void update_textures(media::frame& frame, std::array<ID3D11Texture2D*, 3> alphas);
    }

    class player_context
    {
        static constexpr auto frame_capacity = 60;
        template<typename Element>
        using queue_type = folly::LifoSemMPMCQueue<Element, folly::QueueBehaviorIfFull::BLOCK>;

        folly::Uri uri_;
        ordinal ordinal_;
        boost::future<net_session_ptr> future_net_client_;
        boost::future<media::format_context> future_parsed_;
        queue_type<media::frame> frames_{ frame_capacity };
        folly::Baton<> on_decode_complete_;
        bool on_last_frame_ = false;
        std::atomic<bool> active_ = true;
        int64_t frame_amount_ = 0;
        std::thread thread_;

    public:
        player_context(folly::Uri uri, ordinal ordinal);
        ~player_context();
        std::pair<int, int> resolution();
        media::frame take_decode_frame();
        uint64_t available_size();
        bool is_last_frame_taken() const;
        void deactive();
        void deactive_and_wait();

    private:
        folly::Function<void()> on_media_streaming(boost::promise<media::format_context>&& promise_parsed);
    };

    class graphic
    {
    public:
        struct deleter
        {
            template<typename T>
            std::enable_if_t<std::is_base_of_v<IUnknown, T>>
                operator()(T* p) { if (p != nullptr) p->Release(); }
        };
        void process_event(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);
        void store_textures(HANDLE texY, HANDLE texU, HANDLE texV);
        void update_textures(media::frame& frame) const;
        void update_textures(media::frame& frame, std::array<ID3D11Texture2D*, 3> alphas) const;
        void clean_up() const;
        graphic() = default;
    private:
        void clear();
        std::unique_ptr<ID3D11DeviceContext, deleter> context() const;
        //std::shared_ptr<ID3D11Device> device_;
        //std::vector<std::shared_ptr<ID3D11Texture2D>> alphas_;
    private:
        const ID3D11Device* device_ = nullptr;
        std::array<ID3D11Texture2D*, 3> alphas_;
        mutable std::vector<std::function<void()>> cleanup_;
        mutable std::uint64_t update_index_ = 0;
    };

#ifdef GALLERY_USE_LEGACY
    void timer_startup();
    std::chrono::high_resolution_clock::duration timer_elapsed();
    void media_prepare();
    void media_create();
    void media_release();
    std::optional<av::frame> media_retrieve_frame();
    std::pair<std::string, av::codec_context> media_retrieve_format();
    void interprocess_create();
    void interprocess_release();
    void interprocess_async_send(ipc::message message);
    void interprocess_send(ipc::message message);
    void graphics_release();
#endif  // GALLERY_USE_LEGACY
}
