#pragma once

class graphic
{
public:
    struct deleter
    {
        template<typename T>
        std::enable_if_t<std::is_base_of_v<IUnknown, T>>
            operator()(T* p) { if (p != nullptr) p->Release(); }
    };
    void process_event(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);
    void store_textures(HANDLE texY, HANDLE texU, HANDLE texV);
    void update_textures(av::frame& frame);
    void clean_up();
    graphic() = default;
private:
    void clear();
    std::unique_ptr<ID3D11DeviceContext, deleter> context() const;
    //std::shared_ptr<ID3D11Device> device_;
    //std::vector<std::shared_ptr<ID3D11Texture2D>> alphas_;
private:
    ID3D11Device* device_ = nullptr;
    std::array<ID3D11Texture2D*, 3> alphas_;
    std::vector<std::function<void()>> cleanup_;
    std::uint64_t update_index_ = 0;
};