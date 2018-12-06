#include "stdafx.h"
#include "export.h"
#include "context.h"
#include "network/component.h"
#include "multimedia/component.h"
#include "graphic.h"
#include "database.h"

using net::component::dash_manager;
using net::component::frame_indexed_builder;
using net::component::ordinal;
using media::component::frame_segmentor;
using boost::beast::multi_buffer;

inline namespace config
{
    using concurrency = alternate<unsigned>;

    const concurrency asio_concurrency{ std::thread::hardware_concurrency() };
    const concurrency executor_concurrency{ std::thread::hardware_concurrency() };
    const concurrency decoder_concurrency{ 2u };
    const std::filesystem::path database_directory{ "F:/Debug/TraceDb" };
}

std::shared_ptr<folly::ThreadPoolExecutor> compute_executor;
std::shared_ptr<folly::ThreadedExecutor> session_executor;
std::shared_ptr<folly::ThreadedExecutor> database_executor;
std::shared_ptr<database> dll_database;
std::optional<graphic> dll_graphic;
std::unordered_map<std::pair<int, int>, stream_context> tile_stream_map;
folly::Future<dash_manager> manager = folly::Future<dash_manager>::makeEmpty();
int64_t initial_count = 0;

UnityGfxRenderer unity_device = kUnityGfxRendererNull;
IUnityInterfaces* unity_interface = nullptr;
IUnityGraphics* unity_graphics = nullptr;
IUnityGraphicsD3D11* unity_graphics_dx11 = nullptr;

std::pair<int, int> frame_grid;
std::pair<int, int> frame_scale;
auto tile_width = 0;
auto tile_height = 0;
auto tile_count = 0;

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
    auto tile_index = 0;
    decltype(tile_stream_map)::iterator texture_iterator;
    auto unity_time = 0.f;

    struct stream final
    {
        static inline std::atomic<bool> running{ false };

        static bool available(std::optional<media::frame*> frame = std::nullopt) {
            static auto end_of_stream = false;
            if (frame.has_value()) {
                if (auto& frame_ptr = frame.value(); frame_ptr != nullptr) {
                    end_of_stream = frame.value()->empty();
                } else {
                    end_of_stream = false;
                }
            }
            return !end_of_stream;
        }
    };

    std::unique_ptr<folly::UMPMCQueue<update_batch::tile_render_context, false>> render_tile_queue;
}

auto stream_at = [](int col, int row, bool construct = false) {
    const auto tile_key = std::make_pair(col, row);
    auto tile_iterator = tile_stream_map.find(tile_key);
    if (tile_iterator == tile_stream_map.end()) {
        if (construct) {
            auto emplace_success = false;
            std::tie(tile_iterator, emplace_success) = tile_stream_map.try_emplace(tile_key);
            assert(emplace_success);
        } else {
            assert(!"non-exist texture context");
        }
    }
    return tile_iterator;
};

auto tile_index = [](int col, int row) constexpr {
    return col + row * frame_grid.first + 1;
};

auto tile_coordinate = [](int index) constexpr {
    const auto x = (index - 1) % frame_grid.first;
    const auto y = (index - 1) / frame_grid.first;
    return std::make_pair(x, y);
};

namespace unity
{
    void _nativeDashCreate(LPCSTR mpd_url) {
        manager = dash_manager::create_parsed(std::string{ mpd_url }, asio_concurrency.value(),
                                              compute_executor, dll_database->produce_callback());
    }

    BOOL _nativeDashGraphicInfo(INT& col, INT& row,
                                INT& width, INT& height) {
        if (manager.wait().hasValue()) {
            frame_grid = manager.value().grid_size();
            frame_scale = manager.value().scale_size();
            std::tie(col, row) = frame_grid;
            std::tie(width, height) = frame_scale;
            tile_width = frame_scale.first / frame_grid.first;
            tile_height = frame_scale.second / frame_grid.second;
            tile_count = frame_grid.first * frame_grid.second;
            decoder_concurrency.alter(
                std::max(1u, std::thread::hardware_concurrency() / tile_count));
            assert(decoder_concurrency.value() >= 1);
            assert(asio_concurrency.value() >= 4);
            assert(executor_concurrency.value() >= 4);
            return true;
        }
        return false;
    }

    void _nativeDashCreateTileStream(INT col, INT row, INT index,
                                     HANDLE tex_y, HANDLE tex_u, HANDLE tex_v) {
        assert(manager.isReady());
        assert(manager.hasValue());
        auto& tile_stream = stream_at(col, row, true)->second;
        tile_stream.col = col;
        tile_stream.row = row;
        if (tex_y && tex_u && tex_v) {
            tile_stream.texture_array[0] = static_cast<ID3D11Texture2D*>(tex_y);
            tile_stream.texture_array[1] = static_cast<ID3D11Texture2D*>(tex_u);
            tile_stream.texture_array[2] = static_cast<ID3D11Texture2D*>(tex_v);
        }
        tile_stream.width_offset = col * tile_width;
        tile_stream.height_offset = row * tile_height;
        tile_stream.index = index;
    }

    auto stream_mpeg_dash = [](std::pair<int, int> coordinate,
                               stream_context& texture_context) {
        auto& dash_manager = manager.value();
        return [coordinate, &texture_context, &dash_manager] {
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
                        auto running = false;
                        for (auto& frame : frame_segmentor.try_consume()) {
                            assert(frame->width > 400 && frame->height > 200);
                            do {
                                running = std::atomic_load(&state::stream::running);
                            }
                            while (running && !texture_context.decode_queue
                                                              .tryWriteUntil(std::chrono::steady_clock::now() + 80ms,
                                                                             std::move(frame)));
                            if (!running) {
                                throw std::runtime_error{ "abort running" };
                            }
                            texture_context.decode_count++;
                        }
                    }
                }
            } catch (core::bad_response_error) {
                texture_context.decode_queue.blockingWrite(media::frame{ nullptr });
            } catch (core::session_closed_error) {
                assert(!"session_executor catch unexpected session_closed_error");
            } catch (std::runtime_error) {
                assert(std::atomic_load(&state::stream::running) == false);
            } catch (...) {
                //auto diagnostic = boost::current_exception_diagnostic_information();
                assert(!"session_executor catch unexpected exception");
            }
            assert("session worker trap");
        };
    };

    void _nativeDashPrefetch() {
        assert(std::size(tile_stream_map) == frame_grid.first*frame_grid.second);
        assert(manager.hasValue());
        assert(state::stream::available());
        for (auto& [coordinate, texture_context] : tile_stream_map) {
            assert(coordinate == std::make_pair(texture_context.col, texture_context.row));
            session_executor->add(stream_mpeg_dash(coordinate, texture_context));
        }
        database_executor->add(dll_database->consume_task());
    }

    BOOL _nativeDashAvailable() {
        return state::stream::available();
    }

    static_assert(std::is_move_constructible<update_batch::tile_render_context>::value);

    BOOL _nativeDashTilePollUpdate(const INT col, const INT row,
                                   const INT64 frame_index, const INT64 batch_index) {
        auto& stream_context = stream_at(col, row)->second;
        assert(stream_context.col == col && stream_context.row == row);
        stream_context.update.decode_try++;
        if (stream_context.render_queue.isFull()) {
            return false;
        }
        media::frame frame{ nullptr };
        const auto decode_success = stream_context.decode_queue.read(frame);
        if (decode_success) {
            stream_context.update.decode_success++;
            auto loop_count = 0;
            while (!stream_context.render_queue.write(std::move(frame))) {
                if (loop_count++ > 30) {
                    assert(!"pool_update > 30 times");
                }
            }
        }
        return decode_success;
    }

    void _nativeGraphicSetTextures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v, BOOL temp) {
        if constexpr (!debug::enable_null_texture) {
            assert(tex_y != nullptr);
            assert(tex_u != nullptr);
            assert(tex_v != nullptr);
        }
        if (temp) {
            dll_graphic->store_temp_textures(tex_y, tex_u, tex_v);
        } else {
            dll_graphic->store_textures(tex_y, tex_u, tex_v);
        }
    }

    HANDLE _nativeGraphicCreateTextures(INT width, INT height, CHAR value) {
        return dll_graphic->make_shader_resource(width, height, value, true)
                          .shader;
    }

    namespace test
    {
        void _nativeMockGraphic() {
            dll_graphic.emplace();
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
        }

        auto unmanaged_ansi_string = [](std::string_view string) {
            assert(!string.empty());
            const auto allocate = reinterpret_cast<LPSTR>(CoTaskMemAlloc(string.size() + 1));
            *std::copy(string.begin(), string.end(), allocate) = '\0';
            return allocate;
        };

        auto unmanaged_unicode_string = [](std::string_view string) {
            return SysAllocStringByteLen(string.data(),
                                         folly::to<UINT>(string.length()));
        };

        LPSTR _nativeTestString() {
            return unmanaged_ansi_string("Hello World Test"s);
        }
    }

    auto reset_resource = [] {
        tile_stream_map.clear();
        state::stream::available(nullptr);
        state::render_tile_queue = std::make_unique<decltype(state::render_tile_queue)::element_type>();
    };

    void _nativeLibraryInitialize() {
        reset_resource();
        state::stream::running = true;
        manager = folly::Future<dash_manager>::makeEmpty();
        dll_database = database::make_ptr(database_directory.string());
        compute_executor = core::make_pool_executor(executor_concurrency.value(), "PluginCompute");
        session_executor = core::make_threaded_executor("PluginSession");
        database_executor = core::make_threaded_executor("PluginDatabase");
        initial_count++;
    }

    void _nativeLibraryRelease() {
        state::stream::running = false;
        session_executor = nullptr; // join 1-1
        if (dll_database) {
            dll_database->stop_consume();
            dll_database = nullptr;
        }
        database_executor = nullptr;                        // join 1-2
        manager = folly::Future<dash_manager>::makeEmpty(); // join 2
        if (compute_executor) {
            compute_executor->join(); // join 3
            assert(compute_executor.use_count() == 1);
        }
        compute_executor = nullptr;
        reset_resource();
    }
}

auto dequeue_render_context = [](int count) {
    std::vector<update_batch::tile_render_context> tile_render_contexts;
    tile_render_contexts.reserve(count);
    std::generate_n(
        std::back_inserter(tile_render_contexts), count,
        [] {
            update_batch::tile_render_context context;
            state::render_tile_queue->dequeue(context);
            return context;
        });
    return tile_render_contexts;
};

namespace
{
    void __stdcall on_graphics_device_event(UnityGfxDeviceEventType eventType) {
        if (eventType == kUnityGfxDeviceEventInitialize) {
            assert(unity_graphics->GetRenderer() == kUnityGfxRendererD3D11);
            state::unity_time = 0;
            dll_graphic.emplace();
            unity_device = kUnityGfxRendererD3D11;
        }
        if (dll_graphic) {
            dll_graphic->process_event(eventType, unity_interface);
        }
        if (eventType == kUnityGfxDeviceEventShutdown) {
            unity_device = kUnityGfxRendererNull;
            dll_graphic.reset();
        }
    }

    void __stdcall on_render_event(const int event_id) {
        auto graphic_context = folly::lazy([] {
            return dll_graphic->update_context();
        });
        if (const auto update_tile_count = std::abs(event_id); update_tile_count > 0) {
            for (auto& tile_render_context : dequeue_render_context(update_tile_count)) {
                if (!tile_render_context.frame.empty()) {
                    dll_graphic->update_frame_texture(
                        *graphic_context(),
                        *tile_render_context.texture_array,
                        tile_render_context.frame);
                    dll_graphic->copy_temp_texture_region(
                        *graphic_context(),
                        *tile_render_context.texture_array,
                        tile_render_context.width_offset,
                        tile_render_context.height_offset);
                }
            }
        }
        if (const auto update_main_texture = event_id <= 0; update_main_texture) {
            dll_graphic->overwrite_main_texture();
        }
    }

    //char \x10 € €
    void __stdcall on_texture_update_event(int event_id, void* data) {
        const auto params = reinterpret_cast<UnityRenderingExtTextureUpdateParams*>(data);
        const auto stream_index = params->userData / 3 + 1;
        const auto planar_index = params->userData % 3;
        if (state::stream::available()) {
            const auto stream_iterator = std::find_if(
                tile_stream_map.begin(), tile_stream_map.end(),
                [stream_index](decltype(tile_stream_map)::reference stream) {
                    return stream.second.index == stream_index;
                });
            assert(stream_iterator != tile_stream_map.end());
            auto& stream = stream_iterator->second;
            auto*& avail_frame = stream.avail_frame;
            switch (static_cast<UnityRenderingExtEventType>(event_id)) {
                case kUnityRenderingExtEventUpdateTextureBegin: {
                    if (stream.update.texture_state.none()) {
                        assert(planar_index == 0);
                        assert(avail_frame == nullptr);
                        avail_frame = stream.render_queue.frontPtr();
                        if (!state::stream::available(avail_frame)) {
                            return;
                        }
                    }
                    assert(avail_frame != nullptr);
                    stream.update.event.begin++;
                    assert(!stream.update.texture_state.test(planar_index));
                    assert(params->format == UnityRenderingExtTextureFormat::kUnityRenderingExtFormatA8_UNorm);
                    stream.update.texture_state.set(planar_index);
                    params->texData = (*avail_frame)->data[planar_index];

                    break;
                }
                case kUnityRenderingExtEventUpdateTextureEnd: {
                    assert(avail_frame != nullptr);
                    stream.update.event.end++;
                    assert(stream.update.event.end <= stream.update.event.begin);
                    assert(stream.update.texture_state.test(planar_index));
                    if (stream.update.texture_state.all()) {
                        stream.update.render_finish++;
                        stream.render_queue.popFront();
                        stream.update.texture_state.reset();
                        stream.avail_frame = nullptr;
                        assert(planar_index == 2);
                    }
                    break;
                }
                default:
                    assert(!"on_texture_update_event unexpected UnityRenderingExtEventType");
            }
        }
    }
}

EXTERN_C void DLLAPI __stdcall UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    unity_interface = unityInterfaces;
    unity_graphics = unity_interface->Get<IUnityGraphics>();
    unity_graphics_dx11 = unity_interface->Get<IUnityGraphicsD3D11>();
    assert(unity_graphics);
    assert(unity_graphics_dx11);
    unity_graphics->RegisterDeviceEventCallback(on_graphics_device_event);
    on_graphics_device_event(kUnityGfxDeviceEventInitialize);
}

EXTERN_C void DLLAPI __stdcall UnityPluginUnload() {
    unity_graphics->UnregisterDeviceEventCallback(on_graphics_device_event);
}

namespace unity
{
    UnityRenderingEvent _nativeGraphicGetRenderEventFunc() {
        return on_render_event;
    }

    UnityRenderingEventAndData _nativeGraphicGetUpdateCallback() {
        return on_texture_update_event;
    }
}
