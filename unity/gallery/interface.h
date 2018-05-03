#pragma once

namespace unity
{   // Exported c-linkage interface invoked as c# function for Unity extension. Familiar C# CamelCase naming style.
    EXTERN_C void DLLAPI _nativeMediaCreate();
    EXTERN_C void DLLAPI _nativeMediaRelease();
    EXTERN_C UINT64 DLLAPI _nativeMediaSessionCreate(LPCSTR url);
    EXTERN_C void DLLAPI _nativeMediaSessionPause(UINT64 hashID);
    EXTERN_C void DLLAPI _nativeMediaSessionRelease(UINT64 hashID);
    EXTERN_C void DLLAPI _nativeMediaSessionGetResolution(UINT64 hashID, INT& width, INT& height);
    EXTERN_C BOOL DLLAPI _nativeMediaSessionHasNextFrame(UINT64 hashID);

    namespace debug
    {
        EXTERN_C UINT64 DLLAPI _nativeMediaSessionGetFrameCount(UINT64 hashID);
        EXTERN_C BOOL DLLAPI _nativeMediaSessionDropFrame(UINT64 hashID, UINT64 count = 1);
    }

    EXTERN_C void DLLAPI _nativeGraphicSetTextures(HANDLE textureY, HANDLE textureU, HANDLE textureV);
    EXTERN_C void DLLAPI _nativeGraphicRelease();
    EXTERN_C UnityRenderingEvent DLLAPI __stdcall _nativeGraphicGetRenderEventFunc();

#ifdef GALLERY_USE_LEGACY
    EXTERN_C BOOL DLLAPI global_create();
    EXTERN_C void DLLAPI global_release();
    EXTERN_C BOOL DLLAPI store_media_url(LPCSTR url);
    EXTERN_C void DLLAPI store_time(FLOAT t);
    EXTERN_C void DLLAPI store_alpha_texture(HANDLE texY, HANDLE texU, HANDLE texV);
    EXTERN_C UINT32 DLLAPI store_vr_frame_timing(HANDLE vr_timing);
    EXTERN_C UINT32 DLLAPI store_vr_cumulative_status(HANDLE vr_status);
    EXTERN_C void DLLAPI load_video_params(INT& width, INT& height);
    EXTERN_C BOOL DLLAPI is_video_available();
#endif  // GALLERY_USE_LEGACY

}

namespace dll
{   // Inherent c++ interface for transitting structures across translation units.
    struct media_module
    {
        struct getter
        {
            static std::optional<av::frame> decoded_frame();
        };
        struct waiter
        {

        };
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

    class media_context : public context_interface, public std::enable_shared_from_this<media_context>
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
        std::optional<av::frame> pop_frame();
        media_context() = delete;
        explicit media_context(const std::string& url);
        media_context(const media_context&) = delete;
        media_context& operator=(const media_context&) = delete;
        ~media_context() override;
    private:
        void push_frames(std::vector<av::frame>&& fvec);
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
        std::deque<av::frame> frame_deque_;
        const av::format_context media_format_;
        const av::codec_context video_codec_;
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