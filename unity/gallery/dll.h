#pragma once

namespace dll
{

    struct media_module
    {
        static std::optional<media::frame> decoded_frame();
    };

    class context_interface
    {
    public:
        virtual void start() = 0;
        virtual void stop() = 0;
        virtual size_t hash_code() const = 0;
        virtual bool operator<(const context_interface&) const = 0;
        virtual ~context_interface() = default;
    };

    class media_context : public context_interface
    {
    public:
        void start() override;
        void stop() override;
        size_t hash_code() const override;
        bool operator<(const context_interface&) const override;
        bool empty() const;
        static constexpr size_t capacity();
        std::pair<int, int> resolution() const;
        uint64_t count_frame() const;
        std::optional<media::frame> pop_frame();
        media_context() = delete;
        explicit media_context(const std::string& url);
        media_context(const media_context&) = delete;
        media_context& operator=(const media_context&) = delete;
        ~media_context() override;
    private:
        void push_frames(std::vector<media::frame>&& fvec);
        struct status
        {
            std::atomic<bool> is_active = false;
            std::atomic<bool> has_read = false;
            std::atomic<bool> has_decode = false;
        };
        struct pending
        {
            std::future<size_t> decode_video;
            std::future<size_t> read_media;
        };
        mutable status status_;
        mutable std::recursive_mutex rmutex_;
        mutable std::condition_variable_any condvar_;
        pending pending_;
        std::deque<media::frame> frame_deque_;
        const media::format_context media_format_;
        const media::codec_context video_codec_;
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