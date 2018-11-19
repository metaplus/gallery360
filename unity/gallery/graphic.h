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

        ID3D11Device* device_ = nullptr;
        std::array<ID3D11Texture2D*, 3> alphas_{};
        std::array<ID3D11Texture2D*, 3> alphas_temp_{};
        mutable std::vector<std::function<void()>> cleanup_;
        mutable std::uint64_t update_index_ = 0;

    public:
        struct resource
        {
            ID3D11ShaderResourceView* shader = nullptr;
            ID3D11Texture2D* texture = nullptr;

            void release() const {
                shader->Release();
                texture->Release();
            }
        };

        using texture_array = std::array<ID3D11Texture2D*, 3>;
        using resource_array = std::array<resource, 3>;

        texture_array make_texture_array(media::frame& frame) const;

        resource_array make_resource_array(media::frame& frame) const;

        void process_event(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

        void store_textures(HANDLE texY, HANDLE texU, HANDLE texV);

        void store_temp_textures(HANDLE texY, HANDLE texU, HANDLE texV);

        resource make_shader_resource(int width,
                                      int height,
                                      void* data = nullptr) const;

        void update_textures(media::frame& frame,
                             texture_array& texture_array,
                             unsigned width_offset,
                             unsigned height_offset) const;

        void copy_temp_textures(resource_array resource_array,
                                unsigned width_offset,
                                unsigned height_offset);

        void overwrite_main_texture();

        void clean_up() const;

        graphic() = default;

    private:
        void clear();

        std::unique_ptr<ID3D11DeviceContext, deleter> context() const;

        ID3D11Texture2D* make_dynamic_texture(int width,
                                              int height) const;

        ID3D11Texture2D* make_default_texture(int width,
                                              int height,
                                              void* data) const;

        static void map_texture_data(ID3D11DeviceContext* context,
                                     ID3D11Texture2D* texture,
                                     void* data,
                                     size_t size);
    };
}
