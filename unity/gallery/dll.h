#pragma once

namespace dll
{
    folly::CPUThreadPoolExecutor& cpu_executor();
    folly::ThreadedExecutor& thread_executor();
    boost::asio::io_context::executor_type asio_executor();

    int16_t register_module();
    int16_t deregister_module();

    struct media_module
    {
        static std::optional<media::frame> decoded_frame();
    };

    class media_context_optimal
    {
        static constexpr auto frame_capacity = 90 + 20;

        using frame_container = folly::small_vector<media::frame, 1, uint16_t>;

        std::pair<int16_t, int16_t> ordinal_;
        media::format_context media_format_;
        media::codec_context video_codec_;
        folly::Baton<> mutable parse_complete_;
        folly::Baton<> mutable decode_complete_;
        folly::Baton<> mutable read_complete_;
        folly::Promise<std::string> url_;
        folly::ProducerConsumerQueue<folly::Future<frame_container>> frames_{ frame_capacity };
        folly::Executor::KeepAlive<folly::SerialExecutor> read_executor_;
        folly::Executor::KeepAlive<folly::SerialExecutor> decode_executor_;
        int64_t mutable read_step_count_ = 0;
        int64_t mutable decode_step_count_ = 0;

    public:
        explicit media_context_optimal(std::string url);
        bool operator<(media_context_optimal const& that) const;
        std::optional<media::frame> pop_decode_frame();
        void wait_parser_complete() const;
        void wait_decode_complete() const;
        bool is_decode_complete() const;
        std::pair<int, int> resolution() const;

    private:
        folly::Function<void(std::string_view)> on_parse_format();
        folly::Function<media::packet()> on_read_packet();
        folly::Function<frame_container(media::packet)> on_decode_frame();
        folly::Future<frame_container> chain_media_process_stage();
        void pop_frame_and_resume_read();
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
