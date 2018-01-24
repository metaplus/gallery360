#pragma once
class render
{
    //std::shared_ptr<ID3D11Device> device_;
    //std::vector<std::shared_ptr<ID3D11Texture2D>> alphas_;
    ID3D11Device* device_;
    std::array<ID3D11Texture2D*, 3> alphas_;
public:
    struct deleter
    {
        template<typename T>
        std::enable_if_t<std::is_base_of_v<IUnknown, T>> operator()(T* p) { if (p) p->Release(); }
    };
    render() = default;
    void process_event(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);
    void store_textures(HANDLE texY, HANDLE texU, HANDLE texV);
    void update_textures(av::frame& frame);
    void clear();
private:
    std::unique_ptr<ID3D11DeviceContext, deleter> context() const;
};

static_assert(std::is_copy_constructible_v<render>);