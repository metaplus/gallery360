#include "stdafx.h"
#include "multimedia/pch.h"
#include "graphic.h"

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
                           return make_shader_resource(stretch(frame->width),
                                                       stretch(frame->height),
                                                       frame_data);
                       });
        return resource_array;
    }

    void graphic::process_event(const UnityGfxDeviceEventType event_type,
                                IUnityInterfaces* interfaces) {
        switch (event_type) {
            case kUnityGfxDeviceEventInitialize: {
                IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
                clear();
                device_ = d3d->GetDevice();
                //assert(device_->GetCreationFlags() != D3D11_CREATE_DEVICE_SINGLETHREADED);
                break;
            }
            case kUnityGfxDeviceEventShutdown: {
                clear();
                break;
            }
            default: ;
                //assert(!false) 
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

    ID3D11Texture2D* graphic::make_dynamic_texture(int width,
                                                   int height) const {
        assert(false);
        HRESULT result;
        ID3D11Texture2D* texture = nullptr;
        {
            auto description = alpha_texture_description(
                [=](D3D11_TEXTURE2D_DESC& desc) {
                    desc.Width = width;
                    desc.Height = height;
                    desc.Usage = D3D11_USAGE_DYNAMIC;
                    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                });
            result = device_->CreateTexture2D(&description, nullptr, &texture);
        }
        assert(SUCCEEDED(result));
        return texture;
    }

    ID3D11Texture2D* graphic::make_default_texture(int width,
                                                   int height,
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
            D3D11_SUBRESOURCE_DATA texture_data;
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
        //desc.ViewDimension = multiSampling ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
        description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        description.Texture2D.MipLevels = 1;
        const auto result = device_->CreateShaderResourceView(texture, &description, &shader);
        assert(SUCCEEDED(result));
        return shader;
    }

    void graphic::map_texture_data(ID3D11DeviceContext* context,
                                   ID3D11Texture2D* texture,
                                   void* data,
                                   size_t size) {
        D3D11_MAPPED_SUBRESOURCE mapped_resource{};
        context->Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);     //D3D11_MAP_WRITE_NO_OVERWRITE 
        std::copy_n(static_cast<const char*>(data), size,
                    static_cast<char*>(mapped_resource.pData));
        context->Unmap(texture, 0);
    }

    graphic::resource_array& graphic::emplace_ring_back_if_vacant(int width, int height) {
        if (!textures_ring_.full()) {
            resource_array resource_array{};
            std::generate(
                resource_array.begin(), resource_array.end(),
                [=, stretch = planar_automatic_stretch()]() mutable {
                    return make_shader_resource(stretch(width),
                                                stretch(height),
                                                char{ -128 });
                });
            textures_ring_.push_back(resource_array);
        }
        return textures_ring_.back();
    }

    bool graphic::has_rendered_texture() const {
        return render_texture_iterator_.has_value();
    }

    graphic::resource graphic::make_shader_resource(int width, int height, void* data) const {
        auto* texture = make_default_texture(width, height, data);
        auto* shader = make_shader_resource(texture);
        return resource{ shader, texture };
    }

    graphic::resource graphic::make_shader_resource(int width, int height, char value) const {
        static thread_local std::vector<char> texture_data;
        texture_data.assign(width * height, value);
        return make_shader_resource(width, height, texture_data.data());
    }

    void graphic::update_frame_texture(ID3D11DeviceContext& context,
                                       texture_array& texture_array,
                                       media::frame& frame) const {
        folly::for_each(
            texture_array,
            [this, &context, &frame](ID3D11Texture2D* texture, int index) {
                assert(texture);
                D3D11_TEXTURE2D_DESC desc{};
                auto* frame_planar_data = frame->data[index];
                texture->GetDesc(&desc);
                context.UpdateSubresource(texture, 0, nullptr, frame_planar_data, desc.Width, 0);
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
            context->CopyResource(alphas_[0], alphas_temp_[0]);
            context->CopyResource(alphas_[1], alphas_temp_[1]);
            context->CopyResource(alphas_[2], alphas_temp_[2]);
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
