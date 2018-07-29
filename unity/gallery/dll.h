#pragma once

namespace dll
{
    folly::CPUThreadPoolExecutor& cpu_executor();
    boost::asio::io_context::executor_type asio_executor();

    std::shared_ptr<folly::NamedThreadFactory> create_thread_factory(std::string_view name_prefix);

    int16_t register_module();
    int16_t deregister_module();

    namespace net_module
    {
        using protocal_type = net::protocal::http;
        using response_body = boost::beast::http::dynamic_body;
        using client_session = net::client::session<protocal_type, net::policy<response_body>>;

        boost::asio::io_context& initialize(boost::thread_group& thread_group);
        void release();
        std::unique_ptr<client_session> establish_http_session(std::string_view host, std::string_view service);
        boost::fibers::fiber make_continuous_receiver(client_session& client,
                                                      boost::fibers::unbuffered_channel<response_body>& response);
    }

    struct media_module
    {
        static std::optional<media::frame> try_grab_decoded_frame();
    };

    class media_context : public boost::noncopyable
    {
        static constexpr auto frame_capacity = 90 + 20;
        template<typename Element>
        using container = folly::small_vector<Element, 1, uint16_t>;

        std::string url_;
        std::pair<int16_t, int16_t> ordinal_;
        std::optional<media::io_context> media_io_;
        media::format_context media_format_;
        media::codec_context video_codec_;
        mutable folly::Baton<> parse_complete_;
        mutable folly::Baton<> decode_complete_;
        mutable folly::Baton<> read_complete_;
        mutable folly::Baton<> read_token_;
        mutable std::atomic<bool> stop_request_ = false;
        folly::ProducerConsumerQueue<folly::Future<container<media::frame>>> frames_{ frame_capacity };
        folly::Executor::KeepAlive<folly::SerialExecutor> decode_executor_;
        mutable int64_t read_step_count_ = 0;
        mutable int64_t decode_step_count_ = 0;

    public:
        explicit media_context(std::string url);
        bool operator<(media_context const& that) const;
        std::optional<media::frame> pop_decode_frame();
        void wait_parse_complete() const;
        std::pair<int64_t, int64_t> stop_and_wait();
        void wait_decode_complete() const;
        bool is_decode_complete() const;
        std::pair<int, int> resolution() const;

    private:
        folly::Function<void(std::string_view)> on_parse_read_loop();
        folly::Function<container<media::frame>()> on_decode_frame_container(media::packet&& packet);
        void pop_frame_and_resume_read();
        folly::Future<container<media::frame>> wait_queue_vacancy(media::packet&& packet);
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
