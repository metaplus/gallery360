#include "stdafx.h"
#include "export.h"
#include "network/component.h"
#include "multimedia/component.h"
#include "multimedia/pch.h"
#include "graphic.h"

using net::component::dash_manager;
using net::component::frame_indexed_builder;
using net::component::ordinal;
using media::component::frame_segmentor;
using boost::beast::multi_buffer;

struct texture_context;

inline namespace config
{
    template<typename T>
    class alternate
    {
        T value_ = 0;
        mutable std::optional<T> alternate_;

    public:
        constexpr explicit alternate(T&& value)
            : value_(std::forward<T>(value)) {}

        constexpr T value() const {
            return alternate_.value_or(value_);
        }

        std::optional<T> alter(T&& alter) const {
            return std::exchange(alternate_,
                                 std::make_optional(std::forward<T>(alter)));
        }
    };

    using concurrency = alternate<unsigned>;

    const concurrency asio_concurrency{ std::thread::hardware_concurrency() };
    const concurrency executor_concurrency{ std::thread::hardware_concurrency() };
    constexpr concurrency decoder_concurrency{ 2u };
    constexpr auto frame_capacity = 300ui64;
}

std::shared_ptr<folly::ThreadPoolExecutor> cpu_executor;
std::shared_ptr<folly::ThreadedExecutor> session_executor;
folly::Future<dash_manager> manager = folly::Future<dash_manager>::makeEmpty();

auto unity_time = 0.f;
std::shared_ptr<dll::graphic> dll_graphic;
UnityGfxRenderer unity_device = kUnityGfxRendererNull;
IUnityInterfaces* unity_interface = nullptr;
IUnityGraphics* unity_graphics = nullptr;
std::map<std::pair<int, int>, texture_context> tile_textures;
std::optional<folly::futures::Barrier> session_barrier;

std::pair<int, int> frame_grid;
std::pair<int, int> frame_scale;
auto tile_width = 0;
auto tile_height = 0;

namespace debug
{
    constexpr auto enable_null_texture = true;
    constexpr auto enable_dump = false;
    constexpr auto enable_legacy = false;
}

namespace state
{
    auto tile_col = 0;
    auto tile_row = 0;
    decltype(tile_textures)::iterator texture_iterator;

    auto stream_available = [](std::optional<media::frame*> frame = std::nullopt) {
        static auto end_of_stream = false;
        if (frame.has_value()) {
            if (auto& frame_ptr = frame.value(); frame_ptr != nullptr) {
                end_of_stream = frame.value()->empty();
            } else {
                end_of_stream = false;
            }
        }
        return !end_of_stream;
    };
}

struct texture_context
{
    std::array<ID3D11Texture2D*, 3> texture{};
    media::frame avail_frame{ nullptr };
    folly::MPMCQueue<media::frame> frame_queue{ frame_capacity };
    int col = 0;
    int row = 0;
    bool stop = false;
    int64_t decode_count = 0;

    texture_context() = default;
    texture_context(const texture_context&) = delete;
    texture_context(texture_context&&) = delete;
    texture_context& operator=(const texture_context&) = delete;
    texture_context& operator=(texture_context&&) = delete;
    ~texture_context() = default;
};

auto texture_at = [](int col, int row, bool construct = false) {
    const auto tile_key = std::make_pair(col, row);
    auto tile_iterator = tile_textures.find(tile_key);
    if (tile_iterator == tile_textures.end()) {
        if (construct) {
            auto emplace_success = false;
            std::tie(tile_iterator, emplace_success) = tile_textures.try_emplace(tile_key);
            assert(emplace_success);
        } else {
            assert(!"non-exist texture context");
        }
    }
    return tile_iterator;
};

auto tile_index = [](int x, int y) constexpr {
    return x + y * frame_grid.first + 1;
};

auto tile_coordinate = [](int index) constexpr {
    const auto x = (index - 1) % frame_grid.first;
    const auto y = (index - 1) / frame_grid.first;
    return std::make_pair(x, y);
};

namespace unity
{
    void DLLAPI unity::_nativeConfigExecutor() {
        if (!cpu_executor) {
            cpu_executor = core::set_cpu_executor(executor_concurrency.value(), "PluginPool");
        }
        if (!session_executor) {
            session_executor = core::make_threaded_executor("SessionWorker");
        }
        state::stream_available(nullptr);
    }

    static_assert(std::is_move_constructible<dash_manager>::value);

    void DLLAPI unity::_nativeDashCreate(LPCSTR mpd_url) {
        manager = dash_manager::create_parsed(std::string{ mpd_url },
                                              asio_concurrency.value());
    }

    BOOL DLLAPI _nativeDashGraphicInfo(INT& col, INT& row,
                                       INT& width, INT& height) {
        if (manager.wait().hasValue()) {
            frame_grid = manager.value().grid_size();
            frame_scale = manager.value().scale_size();
            std::tie(col, row) = frame_grid;
            std::tie(width, height) = frame_scale;
            tile_width = frame_scale.first / frame_grid.first;
            tile_height = frame_scale.second / frame_grid.second;
            return true;
        }
        return false;
    }

    void DLLAPI _nativeDashSetTexture(INT x, INT y,
                                      HANDLE tex_y, HANDLE tex_u, HANDLE tex_v) {
        assert(manager.isReady());
        assert(manager.hasValue());
        auto& tile_texture = texture_at(x, y, true)->second;
        tile_texture.col = x;
        tile_texture.row = y;
        tile_texture.texture[0] = static_cast<ID3D11Texture2D*>(tex_y);
        tile_texture.texture[1] = static_cast<ID3D11Texture2D*>(tex_u);
        tile_texture.texture[2] = static_cast<ID3D11Texture2D*>(tex_v);
    }

    void _nativeDashPrefetch() {
        assert(std::size(tile_textures) == frame_grid.first*frame_grid.second);
        assert(manager.hasValue());
        assert(state::stream_available());
        auto& dash_manager = manager.value();
        session_barrier.emplace(std::size(tile_textures) + 1);
        for (auto& [coordinate, texture_context] : tile_textures) {
            assert(coordinate == std::make_pair(texture_context.col, texture_context.row));
            session_executor->add(
                [coordinate, &texture_context, &dash_manager] {
                    auto [col, row] = coordinate;
                    auto future_tile = dash_manager.request_tile_context(col, row);
                    try {
                        while (true) {
                            auto tile_context = std::move(future_tile).get();
                            future_tile = dash_manager.request_tile_context(col, row);
                            frame_segmentor frame_segmentor{
                                decoder_concurrency.value(),
                                tile_context.initial,
                                tile_context.data
                            };
                            assert(frame_segmentor.context_valid());
                            while (frame_segmentor.codec_valid()) {
                                for (auto& frame : frame_segmentor.try_consume()) {
                                    texture_context.frame_queue.blockingWrite(std::move(frame));
                                    texture_context.decode_count++;
                                }
                            }
                        }
                    } catch (core::bad_response_error) {
                        texture_context.frame_queue.blockingWrite(media::frame{ nullptr });
                    }
                    catch (core::session_closed_error) {
                        assert(!"session_executor catch unexpected session_closed_error");
                        texture_context.frame_queue.blockingWrite(media::frame{ nullptr });
                    }
                    catch (...) {
                        auto diagnostic = boost::current_exception_diagnostic_information();
                        assert(!"session_executor catch unexpected exception");
                    }
                    session_barrier->wait().wait();
                });
        }
    }

    BOOL DLLAPI _nativeDashAvailable() {
        return state::stream_available();
    }

    BOOL DLLAPI _nativeDashTilePollUpdate(INT col, INT row) {
        auto texture_context = texture_at(col, row);
        assert(col == texture_context->second.col);
        assert(row == texture_context->second.row);
        assert(std::make_pair(col, row) == texture_context->first);
        const auto poll_success = texture_context->second.frame_queue
                                                 .readIfNotEmpty(texture_context->second.avail_frame);
        if (poll_success) {
            state::tile_col = texture_context->second.col;
            state::tile_row = texture_context->second.row;
            state::texture_iterator = texture_context;
        }
        return poll_success;
    }

    BOOL _nativeDashTileWaitUpdate(INT x, INT y) {
        core::throw_unimplemented();
    }

    void _nativeGraphicSetTextures(HANDLE tex_y,
                                   HANDLE tex_u,
                                   HANDLE tex_v) {
        if constexpr (!debug::enable_null_texture) {
            assert(tex_y != nullptr);
            assert(tex_u != nullptr);
            assert(tex_v != nullptr);
        }
        core::access(dll_graphic)->store_textures(tex_y, tex_u, tex_v);
    }

    void _nativeGraphicRelease() {
        if (dll_graphic) {
            dll_graphic->clean_up();
        }
    }

    namespace test
    {
        void _nativeMockGraphic() {
            core::access(dll_graphic);
            assert(dll_graphic);
        }

        void _nativeConfigConcurrency(UINT codec, UINT net) {
            config::decoder_concurrency.alter(UINT{ codec });
            config::asio_concurrency.alter(UINT{ net });
        }

        void _nativeConcurrencyValue(UINT& codec, UINT& net, UINT& executor) {
            codec = decoder_concurrency.value();
            net = asio_concurrency.value();
            executor = executor_concurrency.value();
        }

        void _nativeCoordinateState(INT& col, INT& row) {
            col = state::tile_col;
            row = state::tile_row;
            assert(state::texture_iterator->first == std::make_pair(state::tile_col, state::tile_row));
        }
    }

    void DLLAPI _nativeLibraryRelease() {
        session_barrier->wait().wait();
        session_barrier = std::nullopt;
        manager = folly::Future<dash_manager>::makeEmpty();
        tile_textures.clear();
        state::stream_available(nullptr);
        session_executor = nullptr;
        cpu_executor->stop();
        cpu_executor->join();
        cpu_executor = nullptr;
    }
}

static void __stdcall OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    if (eventType == kUnityGfxDeviceEventInitialize) {
        core::verify(unity_graphics->GetRenderer() == kUnityGfxRendererD3D11);
        unity_time = 0;
        dll_graphic = std::make_shared<dll::graphic>();
        unity_device = kUnityGfxRendererD3D11;
    }
    if (dll_graphic != nullptr) {
        dll_graphic->process_event(eventType, unity_interface);
    }
    if (eventType == kUnityGfxDeviceEventShutdown) {
        unity_device = kUnityGfxRendererNull;
        dll_graphic = nullptr;
    }
}

EXTERN_C void DLLAPI __stdcall UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    unity_interface = unityInterfaces;
    unity_graphics = unity_interface->Get<IUnityGraphics>();
    unity_graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

EXTERN_C void DLLAPI __stdcall UnityPluginUnload() {
    unity_graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

static void __stdcall OnRenderEvent(int eventID) {
    auto& texture_context = state::texture_iterator->second;
    if (state::stream_available(&texture_context.avail_frame)) {
        dll_graphic->update_textures(texture_context.avail_frame,
                                     texture_context.texture);
        if constexpr (debug::enable_dump) {
            const auto file = []()-> decltype(auto) {
                static std::map<std::pair<int, int>, std::ofstream> file_map;
                const auto file_key = std::make_pair(state::tile_col, state::tile_row);
                auto file_iter = file_map.find(file_key);
                if (file_iter == file_map.end()) {
                    auto success = false;
                    std::tie(file_iter, success) = file_map.try_emplace(
                        file_key,
                        std::ofstream{
                            fmt::format("F:/Debug/ny_c{}_r{}.yuv", state::tile_col, state::tile_row),
                            std::ios::trunc | std::ios::binary
                        });
                }
                assert(file_iter->second.good());
                return (file_iter->second);
            };
            file().write(reinterpret_cast<const char*>(texture_context.avail_frame->data[0]), tile_width * tile_height);
            file().write(reinterpret_cast<const char*>(texture_context.avail_frame->data[1]), tile_width * tile_height / 4);
            file().write(reinterpret_cast<const char*>(texture_context.avail_frame->data[2]), tile_width * tile_height / 4);
        }
    }

}

UnityRenderingEvent unity::_nativeGraphicGetRenderEventFunc() {
    return OnRenderEvent;
}
