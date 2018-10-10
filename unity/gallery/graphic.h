#pragma once

namespace media
{
    class frame;
}

namespace dll
{
    class graphic
    {
        struct deleter
        {
            template<typename T>
            std::enable_if_t<std::is_base_of_v<IUnknown, T>> operator()(T* p) {
                if (p != nullptr) {
                    p->Release();
                }
            }
        };

        const ID3D11Device* device_ = nullptr;
        std::array<ID3D11Texture2D*, 3> alphas_;
        mutable std::vector<std::function<void()>> cleanup_;
        mutable std::uint64_t update_index_ = 0;

    public:
        void process_event(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);
        void store_textures(HANDLE texY, HANDLE texU, HANDLE texV);
        void update_textures(std::array<uint8_t*, 3>& pixel_array) const;

        void update_textures(media::frame& frame) const;
        void update_textures(media::frame& frame, std::array<ID3D11Texture2D*, 3> alphas) const;
        void clean_up() const;
        graphic() = default;

    private:
        void clear();
        std::unique_ptr<ID3D11DeviceContext, deleter> context() const;
    };
}
