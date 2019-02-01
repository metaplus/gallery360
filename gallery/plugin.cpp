#include "stdafx.h"
#include "network/dash.manager.h"
#include "multimedia/io.segmentor.h"
#include "plugin.export.h"
#include "plugin.context.h"
#include "graphic.h"
#include <absl/strings/str_join.h>
#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/process/environment.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>

using boost::beast::multi_buffer;
using boost::logic::tribool;
using boost::logic::indeterminate;

namespace config
{
    using concurrency = alternate<unsigned>;

    const concurrency asio_concurrency{ std::thread::hardware_concurrency() };
    const concurrency executor_concurrency{ std::thread::hardware_concurrency() };
    const concurrency decoder_concurrency{ 2u };

    std::filesystem::path workset_directory;

    namespace trace
    {
        std::filesystem::path directory;
        bool enable = false;
    }

    nlohmann::json document;
    std::optional<folly::Uri> mpd_uri;
    double predict_degrade_factor = 1;
    int decode_capacity = 30;
}

inline namespace resource
{
    std::shared_ptr<folly::ThreadPoolExecutor> compute_executor;
    std::shared_ptr<folly::ThreadedExecutor> stream_executor;
    std::shared_ptr<folly::ThreadedExecutor> update_executor;
    std::optional<graphic> graphic_entity;
    std::vector<stream_context*> stream_cache;

    struct index_key final {};
    struct coordinate_key final {};
    struct sequence final {};

    boost::multi_index_container<
        stream_context,
        boost::multi_index::indexed_by<
            boost::multi_index::sequenced<
                boost::multi_index::tag<sequence>>,
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<index_key>,
                boost::multi_index::member<stream_context, decltype(stream_context::index), &stream_context::index>>,
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<coordinate_key>,
                boost::multi_index::member<stream_context, decltype(stream_context::coordinate), &stream_context::coordinate>>
        >> tile_stream_table;

    folly::Future<net::dash_manager> manager = folly::Future<net::dash_manager>::makeEmpty();

    UnityGfxRenderer unity_device = kUnityGfxRendererNull;
    IUnityInterfaces* unity_interface = nullptr;
    IUnityGraphics* unity_graphics = nullptr;
    IUnityGraphicsD3D11* unity_graphics_dx11 = nullptr;

    namespace description
    {
        core::coordinate frame_grid;
        core::dimension frame_scale;
        core::dimension tile_scale;
        auto tile_count = 0;
    }

    auto cleanup_callback = [](folly::Func clean_callback = nullptr) {
        static std::vector<folly::Func> callback_list;
        if (clean_callback) {
            callback_list.push_back(std::move(clean_callback));
            return;
        }
        for (auto& callback : callback_list) {
            callback();
        }
        callback_list.clear();
    };
}

namespace trace
{
    std::shared_ptr<spdlog::logger> plugin_logger;
    std::shared_ptr<spdlog::logger> update_logger;
    std::shared_ptr<spdlog::logger> render_logger;
}

namespace debug
{
    constexpr auto enable_null_texture = true;
    constexpr auto enable_dump = false;
    constexpr auto enable_legacy = false;
}

namespace state
{
    auto unity_time = 0.f;
    std::atomic<core::coordinate> field_of_view;

    namespace stream
    {
        std::atomic<bool> running{ false };

        auto available = [](std::optional<media::frame*> frame = std::nullopt) {
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
    };
}

auto tile_index = [](int col, int row) constexpr {
    using resource::description::frame_grid;
    return col + row * frame_grid.col + 1;
};

auto tile_coordinate = [](int index) constexpr {
    using resource::description::frame_grid;
    const auto x = (index - 1) % frame_grid.col;
    const auto y = (index - 1) / frame_grid.col;
    return std::make_pair(x, y);
};

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

namespace unity
{
    LPSTR _nativeDashCreate() {
        auto index_url = config::mpd_uri->str();
        assert(!index_url.empty());
        manager = folly::makeFutureWith([&index_url] {
            net::dash_manager manager{ index_url, config::asio_concurrency.value(), compute_executor };
            if (config::trace::enable) {
                assert(is_directory(config::trace::directory));
                auto sink_path = config::trace::directory / "network.log";
                auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(sink_path.string());
                manager.trace_by(std::move(sink));
            }
            manager.predict_by([=](const int tile_col,
                                   const int tile_row) {
                using description::frame_grid;
                const auto field_of_view = std::atomic_load(&state::field_of_view);
                const auto trim_offset = [](const int tile, const int view, const int frame) {
                    const auto offset = std::abs(tile - view);
                    if (offset > frame / 2) {
                        return frame - offset;
                    }
                    return offset;
                };
                const auto col_offset = trim_offset(tile_col, field_of_view.col, frame_grid.col);
                const auto row_offset = trim_offset(tile_col, field_of_view.col, frame_grid.col);
                assert(col_offset >= 0 && row_offset >= 0);
                return col_offset == 0 && row_offset <= 1
                    || row_offset == 0 && col_offset <= 1;
            });
            return manager.request_stream_index();
        });
        return unmanaged_ansi_string(index_url);
    }

    BOOL _nativeDashGraphicInfo(INT& col, INT& row,
                                INT& width, INT& height) {
        using namespace resource::description;
        if (manager.wait().hasValue()) {
            frame_grid = manager.value().grid_size();
            frame_scale = manager.value().frame_size();
            tile_scale = manager.value().tile_size();
            tile_count = manager.value().tile_count();
            std::tie(col, row) = std::tie(frame_grid.col, frame_grid.row);
            std::tie(width, height) = std::tie(frame_scale.width, frame_scale.height);
            config::decoder_concurrency.alter(
                std::max(1u, std::thread::hardware_concurrency() / tile_count));
            assert(config::decoder_concurrency.value() >= 1);
            assert(config::asio_concurrency.value() >= 4);
            assert(config::executor_concurrency.value() >= 4);
            return true;
        }
        return false;
    }

    HANDLE _nativeDashCreateTileStream(INT col, INT row, INT index,
                                       HANDLE tex_y, HANDLE tex_u, HANDLE tex_v) {
        using resource::description::tile_scale;
        using resource::description::tile_count;
        assert(manager.isReady());
        assert(manager.hasValue());
        stream_context tile_stream{ config::decode_capacity };
        tile_stream.index = index;
        tile_stream.coordinate = { col, row };
        tile_stream.offset = { col * tile_scale.width, row * tile_scale.height };
        if (tex_y && tex_u && tex_v) {
            tile_stream.texture_array[0] = static_cast<ID3D11Texture2D*>(tex_y);
            tile_stream.texture_array[1] = static_cast<ID3D11Texture2D*>(tex_u);
            tile_stream.texture_array[2] = static_cast<ID3D11Texture2D*>(tex_v);
        }
        auto [iterator, success] = tile_stream_table.emplace_back(std::move(tile_stream));
        assert(success && "tile stream emplace failure");
        if (stream_cache.empty()) {
            stream_cache.assign(tile_count, nullptr);
        }
        return stream_cache.at(index - 1) = &core::as_mutable(iterator.operator*());
    }

    auto stream_mpeg_dash = [](stream_context& tile_stream) {
        auto& dash_manager = manager.value();
        return [&tile_stream, &dash_manager] {
            auto buffer_streamer = dash_manager.tile_streamer(tile_stream.coordinate);
            auto future_buffer = buffer_streamer();
            try {
                while (true) {
                    auto buffer_sequence = std::move(future_buffer).get();
                    future_buffer = buffer_streamer();
                    media::frame_segmentor frame_segmentor{
                        core::split_buffer_sequence(buffer_sequence.initial, buffer_sequence.data),
                        config::decoder_concurrency.value()
                    };
                    assert(frame_segmentor.context_valid());
                    while (frame_segmentor.codec_valid()) {
                        auto running = false;
                        for (auto& frame : frame_segmentor.try_consume()) {
                            assert(frame->width > 200 && frame->height > 100);
                            do {
                                running = std::atomic_load(&state::stream::running);
                            } while (running && !tile_stream.decode.queue
                                                            ->tryWriteUntil(std::chrono::steady_clock::now() + 80ms,
                                                                            std::move(frame)));
                            if (!running) {
                                throw std::runtime_error{ "abort running" };
                            }
                            tile_stream.decode.count++;
                        }
                    }
                }
            } catch (core::bad_response_error) {
                tile_stream.decode.queue->blockingWrite(media::frame{ nullptr });
            } catch (core::session_closed_error) {
                assert(!"stream_executor catch unexpected session_closed_error");
            } catch (std::runtime_error) {
                assert(std::atomic_load(&state::stream::running) == false);
            } catch (...) {
                assert(!"stream_executor catch unexpected exception");
            }
        };
    };

    void _nativeDashPrefetch() {
        using resource::description::frame_grid;
        assert(std::size(tile_stream_table) == frame_grid.col*frame_grid.row);
        assert(manager.hasValue());
        assert(state::stream::available());
        for (auto& tile_stream : tile_stream_table.get<coordinate_key>()) {
            stream_executor->add(stream_mpeg_dash(core::as_mutable(tile_stream)));
        }
    }

    BOOL _nativeDashAvailable() {
        return state::stream::available();
    }

    static_assert(std::is_move_constructible<update_batch::tile_render_context>::value);

    BOOL _nativeDashTilePollUpdate(const INT col, const INT row,
                                   const INT64 frame_index, const INT64 batch_index) {
        auto decode_success = false;
        auto& tile_stream_index = tile_stream_table.get<coordinate_key>();
        const auto modify_success = tile_stream_index.modify(
            tile_stream_index.find(core::coordinate{ col, row }),
            [=, &decode_success](stream_context& tile_stream) {
                assert(tile_stream.coordinate.col == col);
                assert(tile_stream.coordinate.row == row);
                decode_success = _nativeDashTilePtrPollUpdate(&tile_stream, frame_index, batch_index);
            });
        assert(modify_success && "tile_stream_table modify fail");
        return decode_success;
    }

    BOOL _nativeDashTilePtrPollUpdate(HANDLE instance, INT64 frame_index, INT64 batch_index) {
        auto& tile_stream = *reinterpret_cast<stream_context*>(instance);
        tile_stream.update.decode_try++;
        media::frame frame{ nullptr };
        if (tile_stream.decode.queue->read(frame)) {
            tile_stream.update.decode_success++;
            [[maybe_unused]] const auto enqueue_success = tile_stream.render.queue->enqueue(std::move(frame));
            assert(enqueue_success && "poll_update_frame enqueue render queue failed");
            return true;
        }
        return false;
    }

    void _nativeDashTileFieldOfView(INT col, INT row) {
        std::atomic_store(&state::field_of_view, { col, row });
    }

    void _nativeGraphicSetTextures(HANDLE tex_y, HANDLE tex_u, HANDLE tex_v, BOOL temp) {
        if constexpr (!debug::enable_null_texture) {
            assert(tex_y != nullptr);
            assert(tex_u != nullptr);
            assert(tex_v != nullptr);
        }
        if (temp) {
            graphic_entity->store_temp_textures(tex_y, tex_u, tex_v);
        } else {
            graphic_entity->store_textures(tex_y, tex_u, tex_v);
        }
    }

    HANDLE _nativeGraphicCreateTextures(INT width, INT height, CHAR value) {
        return graphic_entity->make_shader_resource(width, height, value, true)
                             .shader;
    }

    namespace test
    {
        void _nativeMockGraphic() {
            graphic_entity.emplace();
            assert(graphic_entity);
        }

        void _nativeConfigConcurrency(UINT codec, UINT net) {
            config::decoder_concurrency.alter(UINT{ codec });
            config::asio_concurrency.alter(UINT{ net });
        }

        void _nativeConcurrencyValue(UINT& codec, UINT& net, UINT& executor) {
            codec = config::decoder_concurrency.value();
            net = config::asio_concurrency.value();
            executor = config::executor_concurrency.value();
        }

        LPSTR _nativeTestString() {
            return unmanaged_ansi_string("Hello World Test"s);
        }
    }

    auto trace_log_directory = [](const std::filesystem::path& workset,
                                  const std::string& name_prefix) {
        const auto directory = absl::StrJoin({ name_prefix, core::local_date_time("%Y%m%d.%H%M%S") }, ".");
        return workset / directory;
    };

    BOOL _nativeLoadEnvConfig() {
        if (const auto workset_env = boost::this_process::environment()["GWorkSet"]; !workset_env.empty()) {
            config::workset_directory = workset_env.to_string();
            const auto detail_path = config::workset_directory / "config.json";
            if (is_directory(config::workset_directory) && is_regular_file(detail_path)) {
                config::document.clear();
                try {
                    if (std::ifstream reader{ detail_path }; reader >> config::document) {
                        const int mpd_uri_index = config::document["Dash"]["UriIndex"];
                        const std::string mpd_uri = config::document["Dash"]["Uri"][mpd_uri_index];
                        const std::string trace_prefix = config::document["TraceEvent"]["NamePrefix"];
                        if (config::document["TraceEvent"]["Enable"].get_to(config::trace::enable)) {
                            config::trace::directory = trace_log_directory(config::workset_directory, trace_prefix);
                            const auto result = create_directories(config::trace::directory);
                            assert(result && "trace create storage");
                        }
                        config::mpd_uri = folly::Uri{ mpd_uri };
                        config::predict_degrade_factor = config::document["System"]["PredictFactor"];
                        config::decode_capacity = config::document["System"]["DecodeCapacity"];
                        return true;
                    }
                } catch (nlohmann::detail::type_error) {
                    assert(!"_nativeLoadEnvConfig json catch type_error");
                } catch (nlohmann::detail::parse_error) {
                    assert(!"_nativeLoadEnvConfig json catch parse_error");
                } catch (...) {
                    assert(!"_nativeLoadEnvConfig catch unexpected exception");
                }
            }
        }
        return false;
    }

    auto reset_resource = [] {
        tile_stream_table.clear();
        stream_cache.clear();
        state::stream::available(nullptr);
        std::atomic_store(&state::field_of_view, { 0, 0 });
    };

    void _nativeLibraryInitialize() {
        reset_resource();
        state::stream::running = true;
        manager = folly::Future<net::dash_manager>::makeEmpty();
        compute_executor = core::make_pool_executor(config::executor_concurrency.value(), "PluginCompute");
        stream_executor = core::make_threaded_executor("PluginSession");
        if (config::trace::enable) {
            const auto trace_sink = [](const std::string_view file_name) {
                auto sink_path = config::trace::directory / file_name;
                return std::make_shared<spdlog::sinks::basic_file_sink_mt>(sink_path.string());
            };
            trace::plugin_logger = core::make_async_logger("plugin", trace_sink("plugin.log"));
            trace::update_logger = core::make_async_logger("plugin.update", trace_sink("plugin.update.log"));
            trace::render_logger = core::make_async_logger("plugin.render", trace_sink("plugin.render.log"));
        } else {
            trace::plugin_logger = spdlog::null_logger_st("plugin");
            trace::update_logger = spdlog::null_logger_st("plugin.update");
            trace::render_logger = spdlog::null_logger_st("plugin.render");
        }
        trace::plugin_logger->info("event=library.initialize");
    }

    void _nativeLibraryRelease() {
        state::stream::running = false;
        stream_executor = nullptr; // join 1-1
        if (config::trace::enable) {
            trace::plugin_logger->info("event=library.release");
            spdlog::drop_all();
            trace::plugin_logger = nullptr;
            trace::update_logger = nullptr;
            trace::render_logger = nullptr;
        }
        manager = folly::Future<net::dash_manager>::makeEmpty(); // join 2
        if (compute_executor) {
            compute_executor->join(); // join 3
            assert(compute_executor.use_count() == 1);
        }
        compute_executor = nullptr;
        reset_resource();
        config::trace::enable = false;
        cleanup_callback();
    }

    BOOL _nativeTraceEvent(LPSTR instance, LPSTR event) {
        if (!config::trace::enable) return false;
        trace::update_logger->info("instance={}:event={}", instance, event);
        return true;
    }
}

namespace
{
    void __stdcall on_graphics_device_event(UnityGfxDeviceEventType eventType) {
        if (eventType == kUnityGfxDeviceEventInitialize) {
            assert(unity_graphics->GetRenderer() == kUnityGfxRendererD3D11);
            state::unity_time = 0;
            graphic_entity.emplace();
            unity_device = kUnityGfxRendererD3D11;
        }
        if (graphic_entity) {
            graphic_entity->process_event(eventType, unity_interface);
        }
        if (eventType == kUnityGfxDeviceEventShutdown) {
            unity_device = kUnityGfxRendererNull;
            graphic_entity.reset();
        }
    }

#ifdef PLUGIN_RENDER_CALLBACK_APPROACH
    void __stdcall on_render_event(const int event_id) {
        auto graphic_context = folly::lazy([] {
            return graphic_entity->update_context();
        });
        if (const auto update_tile_count = std::abs(event_id); update_tile_count > 0) {
            for (auto& tile_render_context : dequeue_render_context(update_tile_count)) {
                if (!tile_render_context.frame.empty()) {
                    graphic_entity->update_frame_texture(
                        *graphic_context(),
                        *tile_render_context.texture_array,
                        tile_render_context.frame);
                    graphic_entity->copy_temp_texture_region(
                        *graphic_context(),
                        *tile_render_context.texture_array,
                        tile_render_context.width_offset,
                        tile_render_context.height_offset);
                }
            }
        }
        if (const auto update_main_texture = event_id <= 0; update_main_texture) {
            graphic_entity->overwrite_main_texture();
        }
    }
#endif

    void __stdcall on_texture_update_event(int event_id, void* data) {
        const auto params = reinterpret_cast<UnityRenderingExtTextureUpdateParamsV2*>(data);
        const auto stream_index = params->userData / 3;
        const auto planar_index = params->userData % 3;
        if (state::stream::available()) {
            auto& stream = *stream_cache[stream_index];
            switch (static_cast<UnityRenderingExtEventType>(event_id)) {
                case kUnityRenderingExtEventUpdateTextureBeginV2: {
                    if (stream.update.texture_state.none()) {
                        assert(planar_index == 0);
                        assert(stream.render.frame == nullptr);
                        stream.render.frame = stream.render.queue->peek();
                        if (!state::stream::available(stream.render.frame)) {
                            return;
                        }
                    }
                    assert(stream.render.frame != nullptr);
                    stream.render.begin++;
                    assert(!stream.update.texture_state.test(planar_index));
                    assert(params->format == UnityRenderingExtTextureFormat::kUnityRenderingExtFormatA8_UNorm);
                    stream.update.texture_state.set(planar_index);
                    params->texData = (*stream.render.frame)->data[planar_index];
                    break;
                }
                case kUnityRenderingExtEventUpdateTextureEndV2: {
                    assert(stream.render.frame != nullptr);
                    stream.render.end++;
                    assert(stream.render.end <= stream.render.begin);
                    assert(stream.update.texture_state.test(planar_index));
                    if (stream.update.texture_state.all()) {
                        stream.update.render_finish++;
                        stream.render.queue->pop();
                        stream.update.texture_state.reset();
                        stream.render.frame = nullptr;
                        assert(planar_index == 2);
                    }
                    break;
                }
                default:
                    assert(!"on_texture_update_event switch unexpected UnityRenderingExtEventType");
            }
        }
    }
}

namespace unity
{
    void UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
        unity_interface = unityInterfaces;
        unity_graphics = unity_interface->Get<IUnityGraphics>();
        unity_graphics_dx11 = unity_interface->Get<IUnityGraphicsD3D11>();
        assert(unity_graphics);
        assert(unity_graphics_dx11);
        unity_graphics->RegisterDeviceEventCallback(on_graphics_device_event);
        on_graphics_device_event(kUnityGfxDeviceEventInitialize);
    }

    void UnityPluginUnload() {
        unity_graphics->UnregisterDeviceEventCallback(on_graphics_device_event);
    }

#ifdef PLUGIN_RENDER_CALLBACK_APPROACH
    UnityRenderingEvent _nativeGraphicGetRenderCallback() {
        return on_render_event;
    }
#endif

    UnityRenderingEventAndData _nativeGraphicGetUpdateCallback() {
        return on_texture_update_event;
    }
}
