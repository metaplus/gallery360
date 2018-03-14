#include "stdafx.h"
#include "graphic.h"
#include "Gallery/interface.h"

void graphic::process_event(const UnityGfxDeviceEventType event_type, IUnityInterfaces* interfaces)
{
    switch (event_type)
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
        //deleter{}(device_);           //conflict against intangible vector destructor in Unity*.dll, thus irreponsible 
        clear();
        break;
    }
    default:;
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
        auto texture = alphas_[index];
        const auto data = frame->data[index];
        texture->GetDesc(&desc);
        context->UpdateSubresource(texture, 0, nullptr, data, desc.Width, 0);
    }
    static size_t frame_update_index = 0;
    if (static std::optional<ipc::message> msg_first_updata; !msg_first_updata.has_value())
    {
        msg_first_updata.emplace(ipc::message{}.emplace(ipc::message::first_frame_updated{}));
        dll::interprocess_async_send(msg_first_updata.value());
    }
    dll::interprocess_async_send(ipc::message{}.emplace(ipc::message::update_index{ frame_update_index++ }));
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
    device_ = nullptr;
    alphas_.fill(nullptr);
}

