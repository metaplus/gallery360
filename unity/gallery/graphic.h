#pragma once

inline namespace plugin
{
    class graphic final
    {
    public:
        struct resource final
        {
            ID3D11ShaderResourceView* shader = nullptr;
            ID3D11Texture2D* texture = nullptr;
        };

        struct deleter final
        {
            template<typename T>
            std::enable_if_t<std::is_base_of_v<IUnknown, T>> operator()(T* object) {
                if (object != nullptr) {
                    object->Release();
                }
            }

            void operator()(resource* resource) {
                operator()(resource->shader);
                operator()(resource->texture);
            }
        };

        using texture_array = std::array<ID3D11Texture2D*, 3>;
        using resource_array = std::array<resource, 3>;
        using resource_array_iterator = boost::circular_buffer<resource_array>::iterator;

    private:
        ID3D11Device* device_ = nullptr;
        texture_array alphas_{};
        texture_array alphas_temp_{};
        boost::circular_buffer<resource_array> textures_ring_{ 3 };
        std::optional<decltype(textures_ring_)::iterator> render_texture_iterator_;
        mutable std::uint64_t update_index_ = 0;

    public:

        texture_array make_texture_array(media::frame& frame) const;

        resource_array make_resource_array(media::frame& frame) const;

        void process_event(UnityGfxDeviceEventType type,
                           IUnityInterfaces* interfaces);

        void store_textures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v);

        void store_temp_textures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v);

        resource make_shader_resource(int width, int height, void* data) const;

        resource make_shader_resource(int width, int height, char value) const;

        void update_frame_texture(ID3D11DeviceContext& context,
                                  texture_array& texture_array,
                                  media::frame& frame) const;

        void copy_backward_texture_region(ID3D11DeviceContext& context,
                                          texture_array& texture_array,
                                          unsigned width_offset,
                                          unsigned height_offset);

        void copy_temp_texture_region(ID3D11DeviceContext& context,
                                      texture_array& texture_array,
                                      unsigned width_offset,
                                      unsigned height_offset);

        size_t available_texture_slot() const;

        void overwrite_main_texture();

        std::optional<resource_array> rotate_ring_front_if_full();

        std::unique_ptr<ID3D11DeviceContext, deleter> update_context() const;

        resource_array& emplace_ring_back_if_vacant(int width, int height);

        bool has_rendered_texture() const;

    private:
        void clear();

        ID3D11Texture2D* make_dynamic_texture(int width, int height) const;

        ID3D11Texture2D* make_default_texture(int width, int height, void* data) const;

        ID3D11ShaderResourceView* make_shader_resource(ID3D11Texture2D* texture) const;

        static void map_texture_data(ID3D11DeviceContext* context,
                                     ID3D11Texture2D* texture,
                                     void* data, size_t size);
    };
}
