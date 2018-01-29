#include "stdafx.h"
#include "graphic.h"
void graphic::process_event(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
    switch (type)
    {
        case kUnityGfxDeviceEventInitialize:
        {
            IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
            clear();
            //device_.reset(d3d->GetDevice(),deleter{});
            device_ = d3d->GetDevice();
            break;
        }
        case kUnityGfxDeviceEventShutdown:
        {
            //deleter{}(device_);   //!
            clear();
            break;
        }
    }
}
void graphic::store_textures(HANDLE texY, HANDLE texU, HANDLE texV)
{
    core::verify(alphas_.size() == 3);
    //alphas_[0].reset(static_cast<ID3D11Texture2D*>(texY),deleter{});
    //alphas_[1].reset(static_cast<ID3D11Texture2D*>(texU),deleter{});
    //alphas_[2].reset(static_cast<ID3D11Texture2D*>(texV),deleter{});
    alphas_[0] = static_cast<ID3D11Texture2D*>(texY);
    alphas_[1] = static_cast<ID3D11Texture2D*>(texU);
    alphas_[2] = static_cast<ID3D11Texture2D*>(texV);
}

void graphic::update_textures(av::frame& frame)
{
    auto context = this->context();
    core::verify(context != nullptr);
    for (auto index : core::range<0,2>())    
    {
        D3D11_TEXTURE2D_DESC desc;
        auto tex = alphas_[index];
        const auto data = frame->data[index];
        tex->GetDesc(&desc);
        context->UpdateSubresource(tex, 0, nullptr, data, desc.Width, 0);
    }
}

std::unique_ptr<ID3D11DeviceContext, graphic::deleter> graphic::context() const
{
    ID3D11DeviceContext* ctx = nullptr;
    device_->GetImmediateContext(&ctx);
    core::verify(ctx);
    return std::unique_ptr<ID3D11DeviceContext, deleter>{ctx, deleter{}};
}

void graphic::clear()
{
    //deleter{}(device_);
    device_ = nullptr;
    alphas_.fill(nullptr);
}
