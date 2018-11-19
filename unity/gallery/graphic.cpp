#include "stdafx.h"
#include "multimedia/pch.h"
#include "graphic.h"

namespace dll
{
    const auto texture_cast = [](auto* tex) {
        return tex ? static_cast<ID3D11Texture2D*>(tex) : nullptr;
    };

    graphic::texture_array graphic::make_texture_array(media::frame& frame) const {
        texture_array texture_array{};
        assert(frame->width > 700);
        assert(frame->height > 400);
        texture_array[0] = make_default_texture(frame->width, frame->height, frame->data[0]);
        texture_array[1] = make_default_texture(frame->width / 2, frame->height / 2, frame->data[1]);
        texture_array[2] = make_default_texture(frame->width / 2, frame->height / 2, frame->data[2]);
        return texture_array;
    }

    graphic::resource_array graphic::make_resource_array(media::frame& frame) const {
        resource_array resource_array{};
        assert(frame->width > 700);
        assert(frame->height > 400);
        resource_array[0] = make_shader_resource(frame->width, frame->height, frame->data[0]);
        resource_array[1] = make_shader_resource(frame->width / 2, frame->height / 2, frame->data[1]);
        resource_array[2] = make_shader_resource(frame->width / 2, frame->height / 2, frame->data[2]);
        return resource_array;
    }

    void graphic::process_event(const UnityGfxDeviceEventType event_type,
                                IUnityInterfaces* interfaces) {
        switch (event_type) {
            case kUnityGfxDeviceEventInitialize: {
                IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
                clear();
                device_ = d3d->GetDevice();
                assert(device_->GetCreationFlags() != D3D11_CREATE_DEVICE_SINGLETHREADED);
                break;
            }
            case kUnityGfxDeviceEventShutdown: {
                clear();
                break;
            }
            default:
                //assert(!false) 
                ;
        }
    }

    void graphic::store_textures(HANDLE texY, HANDLE texU, HANDLE texV) {
        assert(alphas_.size() == 3);
        alphas_[0] = texture_cast(texY);
        alphas_[1] = texture_cast(texU);
        alphas_[2] = texture_cast(texV);
    }

    void graphic::store_temp_textures(HANDLE texY, HANDLE texU, HANDLE texV) {
        assert(alphas_temp_.size() == 3);
        alphas_temp_[0] = texture_cast(texY);
        alphas_temp_[1] = texture_cast(texU);
        alphas_temp_[2] = texture_cast(texV);
    }

    ID3D11Texture2D* graphic::make_dynamic_texture(int width,
                                                   int height) const {
        assert(false);
        HRESULT result;
        ID3D11Texture2D* texture = nullptr;
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;
            result = device_->CreateTexture2D(&desc, nullptr, &texture);
        }
        assert(SUCCEEDED(result));
        return texture;
    }

    ID3D11Texture2D* graphic::make_default_texture(int width,
                                                   int height,
                                                   void* data) const {
        HRESULT result;
        ID3D11Texture2D* texture = nullptr;
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = 0;
            //desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;
            D3D11_SUBRESOURCE_DATA init_data;
            init_data.pSysMem = data;
            init_data.SysMemPitch = folly::to<UINT>(width);
            result = device_->CreateTexture2D(&desc, &init_data, &texture);
        }
        assert(SUCCEEDED(result));
        return texture;
    }

    void graphic::map_texture_data(ID3D11DeviceContext* context,
                                   ID3D11Texture2D* texture,
                                   void* data,
                                   size_t size) {
        D3D11_MAPPED_SUBRESOURCE mapped_resource{};
        context->Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource);
        std::copy_n(static_cast<const char*>(data), size,
                    static_cast<char*>(mapped_resource.pData));
        context->Unmap(texture, 0);
    }

    graphic::resource graphic::make_shader_resource(int width,
                                                    int height,
                                                    void* data) const {
        HRESULT result;
        ID3D11Texture2D* texture = nullptr;
        if (data != nullptr) {
            texture = make_default_texture(width, height, data);
        } else {
            std::vector<char> texture_data(width * height, 127);
            texture = make_default_texture(width, height, texture_data.data());
        }
        ID3D11ShaderResourceView* shader = nullptr;
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_A8_UNORM;
            //desc.ViewDimension = multiSampling ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
            desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipLevels = 1;
            result = device_->CreateShaderResourceView(texture, &desc, &shader);
        }
        assert(SUCCEEDED(result));
        return resource{ shader, texture };
    }

    void graphic::update_textures(media::frame& frame,
                                  texture_array& texture_array,
                                  unsigned width_offset,
                                  unsigned height_offset) const {
        if (auto context = this->context(); context != nullptr) {
            folly::for_each(
                texture_array,
                [this, &context, &frame, width_offset, height_offset](ID3D11Texture2D* texture,
                                                                      int index) {
                    assert(texture);
                    D3D11_TEXTURE2D_DESC desc;
                    auto* frame_data = frame->data[index];
                    texture->GetDesc(&desc);
                    context->UpdateSubresource(texture, 0, nullptr, frame_data, desc.Width, 0);
                    const auto adjust = [index](int offset) {
                        if (index > 0) {
                            return offset / 2;
                        }
                        return offset;
                    };
                    context->CopySubresourceRegion(alphas_temp_[index], 0,
                                                   adjust(width_offset), adjust(height_offset), 0,
                                                   texture, 0, nullptr);
                });
        }
    }

    void graphic::copy_temp_textures(resource_array resource_array,
                                     unsigned width_offset,
                                     unsigned height_offset) {
        if (auto context = this->context(); context != nullptr) {
            assert(context != nullptr);
            folly::for_each(
                alphas_temp_,
                [&context, &resource_array, width_offset, height_offset](ID3D11Texture2D* texture_temp,
                                                                         int index) {
                    const auto adjust = [index](int offset) {
                        if (index > 0) {
                            return offset / 2;
                        }
                        return offset;
                    };
                    context->CopySubresourceRegion(texture_temp, 0,
                                                   adjust(width_offset), adjust(height_offset), 0,
                                                   resource_array[index].texture, 0, nullptr);
                });
        }
    }

    void graphic::overwrite_main_texture() {
        if (auto context = this->context(); context != nullptr) {
            context->CopyResource(alphas_[0], alphas_temp_[0]);
            context->CopyResource(alphas_[1], alphas_temp_[1]);
            context->CopyResource(alphas_[2], alphas_temp_[2]);
        }
    }

    void graphic::clean_up() const {
        update_index_ = 0;
        for (const auto& func : cleanup_) {
            func();
        }
        cleanup_.clear();
    }

    std::unique_ptr<ID3D11DeviceContext, graphic::deleter>
    graphic::context() const {
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
