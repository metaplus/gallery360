#pragma once
#include "dll.h"

namespace dll
{
    folly::CPUThreadPoolExecutor& cpu_executor();
    boost::asio::io_context::executor_type asio_executor();

    std::shared_ptr<folly::NamedThreadFactory> create_thread_factory(std::string_view name_prefix);

    int16_t register_module();
    int16_t deregister_module();

    struct media_module
    {
        static std::optional<media::frame> decoded_frame();
    };

    class media_context : public boost::noncopyable
    {
        static constexpr auto frame_capacity = 90 + 20;
        using frame_container = folly::small_vector<media::frame, 1, uint16_t>;

        std::string url_;
        std::pair<int16_t, int16_t> ordinal_;
        media::format_context media_format_;
        media::codec_context video_codec_;
        folly::Baton<> mutable parse_complete_;
        folly::Baton<> mutable decode_complete_;
        folly::Baton<> mutable read_complete_;
        folly::Baton<> mutable read_token_;
        std::atomic<bool> mutable stop_request_ = false;
        folly::ProducerConsumerQueue<folly::Future<frame_container>> frames_{ frame_capacity };
        folly::Executor::KeepAlive<folly::SerialExecutor> decode_executor_;
        int64_t mutable read_step_count_ = 0;
        int64_t mutable decode_step_count_ = 0;

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
        folly::Function<frame_container()> on_decode_frame_container(media::packet&& packet);
        void pop_frame_and_resume_read();
        folly::Future<frame_container> wait_queue_vacancy(media::packet&& packet);
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
