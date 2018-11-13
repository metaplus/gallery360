#include "stdafx.h"
#include "multimedia/pch.h"
#include "graphic.h"

namespace dll
{
    void graphic::process_event(const UnityGfxDeviceEventType event_type, IUnityInterfaces* interfaces) {
        switch (event_type) {
            case kUnityGfxDeviceEventInitialize: {
                IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
                clear();
                //device_.reset(d3d->GetDevice(), deleter{});
                const_cast<ID3D11Device*&>(device_) = d3d->GetDevice();
                break;
            }
            case kUnityGfxDeviceEventShutdown: {
                clear();
                break;
            }
            default: ;
        }
    }

    void graphic::store_textures(HANDLE texY, HANDLE texU, HANDLE texV) {
        core::verify(alphas_.size() == 3);
        const auto texture_cast = [](auto* tex) {
            return tex ? static_cast<ID3D11Texture2D*>(tex) : nullptr;
        };
        alphas_[0] = texture_cast(texY);
        alphas_[1] = texture_cast(texU);
        alphas_[2] = texture_cast(texV);
    }

    void graphic::update_textures(std::array<uint8_t*, 3>& pixel_array) const {
        if (auto context = this->context(); context != nullptr) {
            for (auto index : std::array<int, 3>{ 0, 1, 2 }) {
                D3D11_TEXTURE2D_DESC desc;
                auto* texture = alphas_[index];
                auto* data = pixel_array[index];
                texture->GetDesc(&desc);
                context->UpdateSubresource(texture, 0, nullptr, data, desc.Width, 0);
            }
        }
    }

    void graphic::update_textures(media::frame& frame) const {
        update_textures(frame, alphas_);
    }

    void graphic::update_textures(media::frame& frame,
                                  std::array<ID3D11Texture2D*, 3> alphas) const {
        if (auto context = this->context(); context != nullptr) {
            for (auto index : std::array<int, 3>{ 0, 1, 2 }) {
                D3D11_TEXTURE2D_DESC desc;
                auto* texture = alphas[index];
                auto* pixels = frame->data[index];
                assert(pixels != nullptr);
                texture->GetDesc(&desc);
                //assert(desc.Width == index == 0 ? 1280 : 640); //!
                context->UpdateSubresource(texture, 0, nullptr, pixels, desc.Width, 0);
            }
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
            ID3D11DeviceContext* ctx = nullptr;
            const_cast<ID3D11Device*&>(device_)->GetImmediateContext(&ctx);
            assert(ctx != nullptr);
            return std::unique_ptr<ID3D11DeviceContext, deleter>{ ctx, deleter{} };
        }
        return nullptr;
    }

    void graphic::clear() {
        update_index_ = 0;
        device_ = nullptr;
        alphas_.fill(nullptr);
    }
}
