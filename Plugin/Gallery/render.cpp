#include "stdafx.h"
#include "render.h"
/*
render::render()
    :device_(nullptr)
    ,alphas_()//,alphas_(3)
{
}
*/

void render::process_event(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
    switch (type)
    {
        case kUnityGfxDeviceEventInitialize:
        {
            IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
            //device_.reset(d3d->GetDevice(),deleter{});
            clear();
            device_ = d3d->GetDevice();
            //std::fill_n(alphas_.begin(), alphas_.size(), nullptr);
            break;
        }
        case kUnityGfxDeviceEventShutdown:
        {
            clear();
            break;
        }
    }
}
void render::store_textures(HANDLE texY, HANDLE texU, HANDLE texV)
{
    core::verify(alphas_.size() == 3);
    //alphas_[0].reset(static_cast<ID3D11Texture2D*>(texY),deleter{});
    //alphas_[1].reset(static_cast<ID3D11Texture2D*>(texU),deleter{});
    //alphas_[2].reset(static_cast<ID3D11Texture2D*>(texV),deleter{});
    alphas_[0] = static_cast<ID3D11Texture2D*>(texY);
    alphas_[1] = static_cast<ID3D11Texture2D*>(texU);
    alphas_[2] = static_cast<ID3D11Texture2D*>(texV);
}

void render::update_textures(av::frame& frame)
{
    auto context = this->context();
    core::verify(context != nullptr);
    for (auto index : { 0,1,2 })
    {
        D3D11_TEXTURE2D_DESC desc;
        //auto tex=alphas_[index].get();
        auto tex = alphas_[index];
        const auto data = frame->data[index];
        tex->GetDesc(&desc);
        context->UpdateSubresource(tex, 0, nullptr, data, desc.Width, 0);
    }
}

std::unique_ptr<ID3D11DeviceContext, render::deleter> render::context() const
{
    ID3D11DeviceContext* ctx = nullptr;
    device_->GetImmediateContext(&ctx);
    core::verify(ctx);
    return std::unique_ptr<ID3D11DeviceContext, deleter>{ctx, deleter{}};
}

void render::clear()
{
    //device_.reset();
    //if (!alphas_.empty()) { alphas_.clear(); }
    std::fill_n(alphas_.begin(), alphas_.size(), nullptr);
    device_ = nullptr;
    //deleter{}(device_);
}
