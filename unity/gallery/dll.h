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
        using client_session = net::client::session<protocal_type, net::policy<response_body>>;
        using ordinal = std::pair<int16_t, int16_t>;
    }

    namespace net_module
    {
        boost::asio::io_context& initialize();
        void release();
        std::unique_ptr<client_session> establish_http_session(std::string_view host, std::string_view service);
        std::pair<std::string_view, std::string_view> split_url_components(std::string_view url);
    }

    namespace media_module
    {
        media::frame try_take_decoded_frame(int64_t id);
    }

    class player_context
    {
        static constexpr auto frame_capacity = 60;
        template<typename Element>
        using queue_type = folly::LifoSemMPMCQueue<Element, folly::QueueBehaviorIfFull::BLOCK>;

        std::string_view host_;
        std::string_view target_;
        ordinal ordinal_;
        std::unique_ptr<client_session> net_client_;
        queue_type<media::frame> frames_{ frame_capacity };
        folly::Baton<> on_decode_complete_;
        bool on_last_frame_ = false;
        std::atomic<bool> active_ = true;
        std::future<media::format_context> future_parsed_;
        std::atomic<int64_t> step_count_decode_ = 0;
        int64_t frame_amount_ = 0;
        std::thread thread_;

    public:
        explicit player_context(std::string_view host, std::string_view target, ordinal ordinal);
        ~player_context();
        std::pair<int, int> resolution();
        media::frame take_decode_frame();
        uint64_t available_size();
        bool is_codec_complete() const;
        bool is_last_frame_taken() const;
        void deactive();
        void deactive_and_wait();

    private:
        folly::Function<void()> on_media_streaming(std::promise<media::format_context>&& promise_parsed);
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
