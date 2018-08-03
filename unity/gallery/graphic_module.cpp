#include "stdafx.h"

namespace dll
{
    void graphic::process_event(const UnityGfxDeviceEventType event_type, IUnityInterfaces* interfaces)
    {
        switch (event_type)
        {
            case kUnityGfxDeviceEventInitialize:
            {
                IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
                clear();
                //device_.reset(d3d->GetDevice(), deleter{});
                {
                    const_cast<ID3D11Device*&>(device_) = d3d->GetDevice();
                }
                break;
            }
            case kUnityGfxDeviceEventShutdown:
            {
                //deleter{}(device_); //conflict against intangible vector destructor in Unity*.dll, thus irreponsible 
                clear();
                break;
            }
            default:;
        }
    }

    void graphic::store_textures(HANDLE texY, HANDLE texU, HANDLE texV)
    {
        core::verify(alphas_.size() == 3);
        //alphas_[0].reset(static_cast<ID3D11Texture2D*>(texY), deleter{});
        //alphas_[1].reset(static_cast<ID3D11Texture2D*>(texU), deleter{});
        //alphas_[2].reset(static_cast<ID3D11Texture2D*>(texV), deleter{});
        alphas_[0] = static_cast<ID3D11Texture2D*>(texY);
        alphas_[1] = static_cast<ID3D11Texture2D*>(texU);
        alphas_[2] = static_cast<ID3D11Texture2D*>(texV);
    }

    void graphic::update_textures(media::frame& frame) const
    {
        auto context = this->context();
        core::verify(context != nullptr);
        for (auto index : std::array<int, 3>{ 0, 1, 2 })
        {
            D3D11_TEXTURE2D_DESC desc;
            auto texture = alphas_[index];
            const auto data = frame->data[index];
            texture->GetDesc(&desc);
            context->UpdateSubresource(texture, 0, nullptr, data, desc.Width, 0);
        }
        //    if (static std::optional<ipc::message> first_update; !first_update.has_value())
        //    {
        //        first_update.emplace(ipc::message{}.emplace(ipc::first_frame_updated{}));
        //        dll::interprocess_async_send(first_update.value());
        //        cleanup_.emplace_back(
        //            []() { if (first_update.has_value()) first_update = std::nullopt; });
        //    }
        //    dll::interprocess_async_send(ipc::message{}.emplace(ipc::update_index{ update_index_++ }));
    }

    void graphic::update_textures(media::frame& frame, std::array<ID3D11Texture2D*, 3> alphas) const
    {
        auto context = this->context();
        assert(context != nullptr);
        for (auto index : std::array<int, 3>{ 0, 1, 2 })
        {
            D3D11_TEXTURE2D_DESC desc;
            auto texture = alphas[index];
            auto pixels = frame->data[index];
            texture->GetDesc(&desc);
            context->UpdateSubresource(texture, 0, nullptr, pixels, desc.Width, 0);
        }
    }

    void graphic::clean_up() const
    {
        update_index_ = 0;
        if (cleanup_.empty())
            return;
        for (const auto& func : cleanup_)
            func();
        cleanup_.clear();
    }

    std::unique_ptr<ID3D11DeviceContext, graphic::deleter> graphic::context() const
    {
        ID3D11DeviceContext* ctx = nullptr;
        const_cast<ID3D11Device*&>(device_)->GetImmediateContext(&ctx);
        assert(ctx != nullptr);
        return std::unique_ptr<ID3D11DeviceContext, deleter>{ ctx, deleter{} };
    }

    void graphic::clear()
    {
        update_index_ = 0;
        device_ = nullptr;
        alphas_.fill(nullptr);
    }
}

namespace
{
    float unity_time = 0;
    std::unique_ptr<dll::graphic> dll_graphic = nullptr;
    UnityGfxRenderer unity_device = kUnityGfxRendererNull;
    IUnityInterfaces* unity_interface = nullptr;
    IUnityGraphics* unity_graphics = nullptr;
}

namespace dll::graphic_module
{
    void update_textures(media::frame& frame, std::array<ID3D11Texture2D*, 3> alphas)
    {
        dll_graphic->update_textures(frame, alphas);
    }
}

namespace unity
{
    BOOL _nativeGraphicUpdateTextures(INT64 id, HANDLE textureY, HANDLE textureU, HANDLE textureV)
    {
        if (auto frame = dll::media_module::try_take_decoded_frame(id); !frame.empty())
        {
            dll::graphic_module::update_textures(frame, std::array<ID3D11Texture2D*, 3>{
                static_cast<ID3D11Texture2D*>(textureY), static_cast<ID3D11Texture2D*>(textureU), static_cast<ID3D11Texture2D*>(textureV) });
            return true;
        }
        return false;
    }

    void _nativeGraphicSetTextures(HANDLE textureY, HANDLE textureU, HANDLE textureV)
    {
        assert(textureY != nullptr);
        assert(textureU != nullptr);
        assert(textureV != nullptr);
        dll_graphic->store_textures(textureY, textureU, textureV);
    }

    void _nativeGraphicRelease()
    {
        if (dll_graphic)
            dll_graphic->clean_up();
    }
}

static void __stdcall OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    if (eventType == kUnityGfxDeviceEventInitialize)
    {
        core::verify(unity_graphics->GetRenderer() == kUnityGfxRendererD3D11);
        unity_time = 0;
        dll_graphic = std::make_unique<dll::graphic>();
        unity_device = kUnityGfxRendererD3D11;
    }
    if (dll_graphic != nullptr)
    {
        dll_graphic->process_event(eventType, unity_interface);
    }
    if (eventType == kUnityGfxDeviceEventShutdown)
    {
        unity_device = kUnityGfxRendererNull;
        dll_graphic = nullptr;
    }
}

EXTERN_C void DLLAPI __stdcall UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    unity_interface = unityInterfaces;
    unity_graphics = unity_interface->Get<IUnityGraphics>();
    unity_graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

EXTERN_C void DLLAPI __stdcall UnityPluginUnload()
{
    unity_graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

static void __stdcall OnRenderEvent(int eventID)
{
    //if (auto frame = dll::media_module::try_get_decoded_frame(); frame.has_value())
    if (auto frame = dll::media_module::try_take_decoded_frame(0); !frame.empty())
        dll_graphic->update_textures(frame);
}

UnityRenderingEvent unity::_nativeGraphicGetRenderEventFunc()
{
    return OnRenderEvent;
}

#ifdef GALLERY_USE_LEGACY 

BOOL unity::global_create()
{
#ifdef NDEBUG                       
    try
    {
    #endif
        dll::media_prepare();
        dll::interprocess_create();
        dll::media_create();
        dll::interprocess_async_send(ipc::message{}.emplace(ipc::info_launch{}));
    #ifdef NDEBUG
    }
    catch (...) { return false; }
#endif
    return true;
}
void unity::global_release()
{
    dll::interprocess_send(ipc::message{}.emplace(ipc::info_exit{}));
    std::this_thread::yield();
    dll::media_release();
    dll::interprocess_release();
    dll::graphics_release();
}

void unity::store_time(FLOAT t)
{
    unity_time = t;
}

void unity::store_alpha_texture(HANDLE texY, HANDLE texU, HANDLE texV)
{
    core::verify(texY != nullptr, texU != nullptr, texV != nullptr);
    dll_graphic->store_textures(texY, texU, texV);
    dll::interprocess_async_send(ipc::message{}.emplace(ipc::info_started{}));
}

UINT32 unity::store_vr_frame_timing(HANDLE vr_timing)
{
    ipc::message msg;
    auto msg_body = *static_cast<vr::Compositor_FrameTiming*>(vr_timing);
    dll::interprocess_async_send(std::move(msg.emplace(msg_body)));
    return msg_body.m_nFrameIndex;
}

UINT32 unity::store_vr_cumulative_status(HANDLE vr_status)
{
    ipc::message msg;
    auto msg_body = *static_cast<vr::Compositor_CumulativeStats*>(vr_status);
    dll::interprocess_send(std::move(msg.emplace(msg_body)));
    return msg_body.m_nNumFramePresents;
}


void dll::graphics_release()
{
    if (dll_graphic)
        dll_graphic->clean_up();
}

namespace
{
    std::shared_ptr<ipc::channel> channel = nullptr;
    std::future<void> initial;
}

void dll::interprocess_create()
{
    initial = std::async(std::launch::async, []()
                         {
                             const auto[url, codec] = dll::media_retrieve_format();
                             try
                             {
                                 channel = std::make_shared<ipc::channel>(true);
                                 std::map<std::string, std::string> mformat;
                                 mformat["url"] = url;
                                 mformat["codec_name"] = codec->codec->long_name;
                                 mformat["resolution"] = std::to_string(codec->width) + 'x' + std::to_string(codec->height);
                                 mformat["gop_size"] = std::to_string(codec->gop_size);
                                 mformat["pixel_format"] = av_get_pix_fmt_name(codec->pix_fmt);
                                 mformat["frames_count"] = std::to_string(codec.frame_count());
                                 channel->send(ipc::message{}.emplace(ipc::media_format{ std::move(mformat) }));
                             }
                             catch (...)
                             {
                                 channel = nullptr;
                             }
                         });
}

void dll::interprocess_release()
{
    if (initial.valid())
        initial.get();
    channel = nullptr;
}

void dll::interprocess_async_send(ipc::message message)
{
    static struct
    {
        std::mutex mutex;
        std::vector<ipc::message> container;
    }temp_mvec;
    static thread_local std::vector<ipc::message> local_mvec;
    if (initial.wait_for(0ns) != std::future_status::ready)
    {
        std::lock_guard<std::mutex> exlock{ temp_mvec.mutex };
        return temp_mvec.container.push_back(std::move(message));
    }
    {
        std::lock_guard<std::mutex> exlock{ temp_mvec.mutex };
        if (!channel)
        {
            if (!temp_mvec.container.empty())
                temp_mvec.container.clear();
            return;
        }
        if (!temp_mvec.container.empty())
            std::swap(local_mvec, temp_mvec.container);
    }
    if (!local_mvec.empty())
    {
        for (auto& msg : local_mvec)
            channel->async_send(std::move(msg));
        local_mvec.clear();
    }
    channel->async_send(std::move(message));
}

void dll::interprocess_send(ipc::message message)
{
    if (initial.wait(); channel == nullptr)
        return;
    channel->send(std::move(message));
}

#endif  // GALLERY_USE_LEGACY
