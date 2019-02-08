#include "stdafx.h"
#include "graphic.h"
#include "unity/IUnityGraphicsD3D11.h"
#include <folly/Conv.h>
#pragma warning(disable: 4267)
#include <folly/container/Foreach.h>
#include <range/v3/view/iota.hpp>

inline namespace plugin
{
    auto texture_cast = [](auto* tex) {
        return tex ? static_cast<ID3D11Texture2D*>(tex) : nullptr;
    };

    auto planar_stretch = [](int index) {
        return [index](int offset) {
            if (index > 0) {
                return offset / 2;
            }
            return offset;
        };
    };

    auto planar_automatic_stretch = [] {
        return [index = 0](int offset) mutable {
            if (index++ > 0) {
                return offset / 2;
            }
            return offset;
        };
    };

    graphic::texture_array graphic::make_texture_array(media::frame& frame) const {
        texture_array texture_array{};
        std::transform(std::begin(frame->data),
                       std::begin(frame->data) + std::size(texture_array),
                       std::begin(texture_array),
                       [this, stretch = planar_automatic_stretch(), &frame](uint8_t* frame_data) mutable {
                           return make_default_texture(stretch(frame->width),
                                                       stretch(frame->height),
                                                       frame_data);
                       });
        return texture_array;
    }

    graphic::resource_array graphic::make_resource_array(media::frame& frame) const {
        resource_array resource_array{};
        std::transform(std::begin(frame->data),
                       std::begin(frame->data) + std::size(resource_array),
                       std::begin(resource_array),
                       [this, stretch = planar_automatic_stretch(), &frame](uint8_t* frame_data) mutable {
                           return make_shader_resource(stretch(frame->width), stretch(frame->height),
                                                       frame_data, false);
                       });
        return resource_array;
    }

    void graphic::process_event(const UnityGfxDeviceEventType event_type,
                                IUnityInterfaces* interfaces) {
        switch (event_type) {
            case kUnityGfxDeviceEventInitialize: {
                auto* graphic_dx11 = interfaces->Get<IUnityGraphicsD3D11>();
                clear();
                device_ = graphic_dx11->GetDevice();
                //assert(device_->GetCreationFlags() != D3D11_CREATE_DEVICE_SINGLETHREADED);
                break;
            }
            case kUnityGfxDeviceEventShutdown: {
                clear();
                break;
            }
            default: ;
        }
    }

    auto assign_texture_array = [](graphic::texture_array& texture_array,
                                   std::initializer_list<void*> textures) {
        const auto iterator = std::transform(
            textures.begin(), textures.end(),
            texture_array.begin(),
            [](void* texture) {
                return texture_cast(texture);
            });
        assert(iterator == texture_array.end());
    };

    void graphic::store_textures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v) {
        assert(alphas_.size() == 3);
        assign_texture_array(alphas_, { tex_y, tex_u, tex_v });
    }

    void graphic::store_temp_textures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v) {
        assert(alphas_temp_.size() == 3);
        assign_texture_array(alphas_temp_, { tex_y, tex_u, tex_v });
    }

    auto alpha_texture_description = [](auto initial) {
        D3D11_TEXTURE2D_DESC description{};
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_A8_UNORM;
        description.SampleDesc.Count = 1;
        description.SampleDesc.Quality = 0;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        description.CPUAccessFlags = 0;
        description.MiscFlags = 0;
        initial(description);
        return description;
    };

    ID3D11Texture2D* graphic::make_dynamic_texture(int width, int height,
                                                   void* data) const {
        ID3D11Texture2D* texture = nullptr;
        {
            auto description = alpha_texture_description(
                [=](D3D11_TEXTURE2D_DESC& desc) {
                    desc.Width = width;
                    desc.Height = height;
                    desc.Usage = D3D11_USAGE_DYNAMIC;
                    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                });
            D3D11_SUBRESOURCE_DATA texture_data{};
            texture_data.pSysMem = data;
            texture_data.SysMemPitch = folly::to<UINT>(width);
            const auto result = device_->CreateTexture2D(&description, &texture_data, &texture);
            assert(SUCCEEDED(result));
        }
        return texture;
    }

    ID3D11Texture2D* graphic::make_default_texture(int width, int height,
                                                   void* data) const {
        ID3D11Texture2D* texture = nullptr;
        {
            auto description = alpha_texture_description(
                [=](D3D11_TEXTURE2D_DESC& desc) {
                    desc.Width = width;
                    desc.Height = height;
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.CPUAccessFlags = 0;
                });
            D3D11_SUBRESOURCE_DATA texture_data{};
            texture_data.pSysMem = data;
            texture_data.SysMemPitch = folly::to<UINT>(width);
            const auto result = device_->CreateTexture2D(&description, &texture_data, &texture);
            assert(SUCCEEDED(result));
        }
        return texture;
    }

    ID3D11ShaderResourceView* graphic::make_shader_resource(ID3D11Texture2D* texture) const {
        ID3D11ShaderResourceView* shader = nullptr;
        D3D11_SHADER_RESOURCE_VIEW_DESC description{};
        description.Format = DXGI_FORMAT_A8_UNORM;
        description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        description.Texture2D.MipLevels = 1;
        const auto result = device_->CreateShaderResourceView(texture, &description, &shader);
        assert(SUCCEEDED(result));
        return shader;
    }

    void graphic::map_texture_data(ID3D11DeviceContext& context,
                                   ID3D11Texture2D* texture,
                                   void* data, size_t size) {
        D3D11_MAPPED_SUBRESOURCE mapped_resource{};
        context.Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        std::copy_n(static_cast<uint8_t*>(data), size,
                    static_cast<uint8_t*>(mapped_resource.pData));
        context.Unmap(texture, 0);
    }

    graphic::resource_array& graphic::emplace_ring_back_if_vacant(int width, int height) {
        if (!textures_ring_.full()) {
            resource_array resource_array{};
            std::generate(
                resource_array.begin(), resource_array.end(),
                [=, stretch = planar_automatic_stretch()]() mutable {
                    return make_shader_resource(stretch(width), stretch(height),
                                                char{ -128 }, false);
                });
            textures_ring_.push_back(resource_array);
        }
        return textures_ring_.back();
    }

    bool graphic::has_rendered_texture() const {
        return render_texture_iterator_.has_value();
    }

    graphic::resource graphic::make_shader_resource(int width, int height,
                                                    void* data, bool dynamic) const {
        auto* texture = dynamic
                            ? make_dynamic_texture(width, height, data)
                            : make_default_texture(width, height, data);
        auto* shader = make_shader_resource(texture);
        return resource{ shader, texture };
    }

    graphic::resource graphic::make_shader_resource(int width, int height,
                                                    char value, bool dynamic) const {
        static thread_local std::vector<char> texture_data;
        texture_data.assign(width * height, value);
        return make_shader_resource(width, height, texture_data.data(), dynamic);
    }

    void graphic::update_frame_texture(ID3D11DeviceContext& context,
                                       texture_array& texture_array,
                                       media::frame& frame) const {
        folly::for_each(
            texture_array,
            [this, &context, &frame](ID3D11Texture2D* texture, int index) {
                assert(texture);
                D3D11_TEXTURE2D_DESC desc{};
                texture->GetDesc(&desc);
                const auto stretch = planar_stretch(index);
                auto* frame_planar_data = frame->data[index];
                assert(stretch(frame->width) == desc.Width);
                assert(stretch(frame->height) == desc.Height);
                assert(stretch(frame->width) == frame->linesize[index]);
                switch (desc.Usage) {
                    case D3D11_USAGE_DEFAULT:
                        context.UpdateSubresource(texture, 0, nullptr,
                                                  frame_planar_data, desc.Width, 0);
                        break;
                    case D3D11_USAGE_DYNAMIC: {
                        const auto planar_size = desc.Width * desc.Height;
                        map_texture_data(context, texture, frame_planar_data, planar_size);
                        break;
                    }
                    default:
                        assert(!"unexpected texture usage");
                }
            });
    }

    void graphic::copy_backward_texture_region(ID3D11DeviceContext& context,
                                               texture_array& texture_array,
                                               const unsigned width_offset,
                                               const unsigned height_offset) {
        folly::for_each(
            texture_array,
            [=, &context](ID3D11Texture2D* texture, int index) {
                assert(texture);
                const auto stretch = planar_stretch(index);
                context.CopySubresourceRegion(
                    textures_ring_.back()[index].texture, 0,
                    stretch(width_offset), stretch(height_offset), 0,
                    texture, 0, nullptr);
            });

    }

    void graphic::copy_temp_texture_region(ID3D11DeviceContext& context,
                                           texture_array& texture_array,
                                           const unsigned width_offset,
                                           const unsigned height_offset) {
        folly::for_each(
            texture_array,
            [=, &context](ID3D11Texture2D* texture, int index) {
                assert(texture);
                const auto stretch = planar_stretch(index);
                context.CopySubresourceRegion(
                    alphas_temp_[index], 0,
                    stretch(width_offset), stretch(height_offset), 0,
                    texture, 0, nullptr);
            });
    }

    size_t graphic::available_texture_slot() const {
        const auto ring_size = textures_ring_.size();
        const auto ring_capacity = textures_ring_.capacity();
        return ring_capacity - ring_size;
    }

    void graphic::overwrite_main_texture() {
        if (auto context = this->update_context(); context != nullptr) {
            for (auto index : ranges::view::ints(0, 3)) {
                context->CopyResource(alphas_[index], alphas_temp_[index]);
            }
        }
    }

    std::optional<graphic::resource_array> graphic::rotate_ring_front_if_full() {
        if (textures_ring_.full()) {
            if (render_texture_iterator_) {
                textures_ring_.rotate(std::next(*render_texture_iterator_));
            }
            render_texture_iterator_ = textures_ring_.begin();
            return **render_texture_iterator_;
        }
        return std::nullopt;
    }

    std::unique_ptr<ID3D11DeviceContext, graphic::deleter>
    graphic::update_context() const {
        if (device_) {
            ID3D11DeviceContext* context = nullptr;
            device_->GetImmediateContext(&context);
            assert(context != nullptr);
            return std::unique_ptr<ID3D11DeviceContext, deleter>{ context, deleter{} };
        }
        return nullptr;
    }

    void graphic::clear() {
        update_index_ = 0;
        device_ = nullptr;
        alphas_.fill(nullptr);
    }
}

namespace debug
{
    constexpr auto enable_null_texture = true;
}

inline namespace resource
{
    std::optional<graphic> graphic_entity;
    auto unity_time = 0.f;
    UnityGfxRenderer unity_device = kUnityGfxRendererNull;
    IUnityInterfaces* unity_interface = nullptr;
    IUnityGraphics* unity_graphics = nullptr;
    IUnityGraphicsD3D11* unity_graphics_dx11 = nullptr;
}

namespace
{
    void __stdcall on_graphics_device_event(UnityGfxDeviceEventType eventType) {
        if (eventType == kUnityGfxDeviceEventInitialize) {
            assert(unity_graphics->GetRenderer() == kUnityGfxRendererD3D11);
            unity_time = 0;
            graphic_entity.emplace();
            unity_device = kUnityGfxRendererD3D11;
        }
        if (graphic_entity) {
            graphic_entity->process_event(eventType, unity_interface);
        }
        if (eventType == kUnityGfxDeviceEventShutdown) {
            unity_device = kUnityGfxRendererNull;
            graphic_entity.reset();
        }
    }

    void UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
        unity_interface = unityInterfaces;
        unity_graphics = unity_interface->Get<IUnityGraphics>();
        unity_graphics_dx11 = unity_interface->Get<IUnityGraphicsD3D11>();
        assert(unity_graphics);
        assert(unity_graphics_dx11);
        unity_graphics->RegisterDeviceEventCallback(on_graphics_device_event);
        on_graphics_device_event(kUnityGfxDeviceEventInitialize);
    }

    void UnityPluginUnload() {
        unity_graphics->UnregisterDeviceEventCallback(on_graphics_device_event);
    }
}

namespace unity
{
    void _nativeGraphicSetTextures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v, BOOL temp) {
        if constexpr (!debug::enable_null_texture) {
            assert(tex_y != nullptr);
            assert(tex_u != nullptr);
            assert(tex_v != nullptr);
        }
        if (temp) {
            graphic_entity->store_temp_textures(tex_y, tex_u, tex_v);
        }
        else {
            graphic_entity->store_textures(tex_y, tex_u, tex_v);
        }
    }

    HANDLE _nativeGraphicCreateTextures(INT width, INT height, CHAR value) {
        return graphic_entity->make_shader_resource(width, height, value, true)
            .shader;
    }

    namespace test
    {
        void _nativeMockGraphic() {
            graphic_entity.emplace();
            assert(graphic_entity);
        }
    }
}