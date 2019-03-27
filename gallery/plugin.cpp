#include "stdafx.h"
#include "plugin.export.h"
#include "plugin.config.h"
#include "plugin.context.h"
#include "plugin.util.h"
#include "network/dash.manager.h"
#include "multimedia/media.h"
#include "multimedia/io.segmentor.h"
#include "core/core.h"
#include "core/exception.hpp"
#include <folly/Uri.h>
#include <folly/executors/ThreadedExecutor.h>
#include <boost/container/small_vector.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/process/environment.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <folly/Lazy.h>

using boost::beast::multi_buffer;
using boost::logic::tribool;
using boost::logic::indeterminate;
#pragma warning(disable: 4455)
using std::literals::operator ""ms;
using std::literals::operator ""s;

inline namespace resource
{
    auto configs = folly::lazy([] { return config{}; });
    std::shared_ptr<folly::ThreadPoolExecutor> compute_executor;
    std::shared_ptr<folly::ThreadedExecutor> stream_executor;
    std::shared_ptr<folly::ThreadedExecutor> update_executor;
    std::vector<stream_context*> tile_stream_cache;

    struct index_key final { };
    struct coordinate_key final { };
    struct sequence final { };

    boost::multi_index_container<
        stream_context,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<index_key>,
                boost::multi_index::member<stream_base, decltype(stream_base::index), &stream_base::index>>,
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<coordinate_key>,
                boost::multi_index::member<stream_base, decltype(stream_base::coordinate), &stream_base::coordinate>>
        >> tile_stream_table;

    folly::Future<net::dash_manager> manager = folly::Future<net::dash_manager>::makeEmpty();

    namespace description
    {
        core::coordinate frame_grid;
        core::dimension frame_scale;
        core::dimension tile_scale;
        auto tile_count = 0;
    }
}

namespace trace
{
    std::shared_ptr<spdlog::logger> plugin_logger;
    std::shared_ptr<spdlog::logger> update_logger;
    std::shared_ptr<spdlog::logger> render_logger;
}

namespace debug
{
    constexpr auto enable_dump = false;
    constexpr auto enable_legacy = false;
}

namespace state
{
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

auto rate_adaptation_algorithms = folly::lazy([] {
    std::vector<std::function<double(int, int)>> rate_adaptation_list;
    rate_adaptation_list.push_back([=](const int tile_col,
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
        /*    return col_offset == 0 && row_offset <= 1
                || row_offset == 0 && col_offset <= 1;*/
        return 1.;
    });
    return rate_adaptation_list;
});

namespace unity
{
    LPSTR _nativeDashCreate() {
        auto index_url = configs().mpd_uri->str();
        assert(!index_url.empty());
        manager = folly::makeFutureWith([&index_url] {
            net::dash_manager manager{
                index_url, configs().concurrency.network,
                compute_executor
            };
            spdlog::sink_ptr sink;
            if (configs().trace.enable) {
                assert(is_directory(configs().trace.directory));
                auto sink_path = configs().trace.directory / "network.log";
                sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(sink_path.string());
            } else {
                sink = std::make_shared<spdlog::sinks::null_sink_st>();
            }
            manager.trace_by(std::move(sink));
            if (configs().adaptation.enable) {
                manager.predict_by(
                    rate_adaptation_algorithms().at(configs().adaptation.algorithm_index));
            } else {
                const auto qp = configs().adaptation.constant_qp;
                manager.select_by([=](int, int) {
                    return qp;
                });
            }
            return manager.request_stream_index();
        });
        return util::unmanaged_string(index_url);
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
            configs().concurrency.decoder =
                std::max(1u, std::thread::hardware_concurrency() / tile_count);
            assert(configs().concurrency.decoder >= 1);
            assert(configs().concurrency.network >= 2);
            assert(configs().concurrency.executor >= 2);
            return true;
        }
        return false;
    }

    HANDLE _nativeDashCreateTileStream(INT col, INT row, INT index,
                                       HANDLE tex_y, HANDLE tex_u, HANDLE tex_v) {
        using description::tile_scale;
        using description::tile_count;
        assert(manager.isReady());
        assert(manager.hasValue());
        auto [iterator, success] = tile_stream_table.emplace(
            stream_options{}.with_index(index)
                            .with_coordinate(col, row)
                            .with_offset(col * tile_scale.width, row * tile_scale.height)
                            .with_capacity(configs().system.decode.capacity, 15));
        assert(success && "tile stream emplace failure during creation");
        if (tile_stream_cache.empty()) {
            tile_stream_cache.assign(tile_count, nullptr);
        }
        return tile_stream_cache.at(index - 1) = &core::as_mutable(iterator.operator*());
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
                        configs().concurrency.decoder
                    };
                    const auto decode_disable = !configs().system.decode.enable;
                    assert(frame_segmentor.context_valid());
                    while (frame_segmentor.codec_available()) {
                        auto running = false;
                        auto frame_list = frame_segmentor.try_consume(decode_disable);
                        for (auto& frame : frame_list) {
                            if (!decode_disable) {
                                assert(frame->width > 200 && frame->height > 100);
                            }
                            do {
                                running = std::atomic_load(&state::stream::running);
                            } while (running && !tile_stream.decode.queue
                                                            .tryWriteUntil(std::chrono::steady_clock::now() + 80ms,
                                                                           std::move(frame)));
                            if (!running) {
                                core::aborted_error::throw_directly();
                            }
                            tile_stream.decode.count++;
                        }
                        if (decode_disable && frame_list.empty()) break;
                    }
                }
            } catch (core::bad_response_error e) {
                tile_stream.decode.queue.blockingWrite(media::frame{ nullptr });
                tile_stream.decode.queue.blockingWrite(std::make_exception_ptr(e));
            } catch (core::session_closed_error) {
                assert(!"stream_executor catch unexpected session_closed_error");
            } catch (core::aborted_error) {
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
        stream_context::decode_frame frame{ nullptr };
        if (tile_stream.decode.queue.read(frame)) {
            tile_stream.update.decode_success++;
            if (std::holds_alternative<std::exception_ptr>(frame)) {
                stream_context::update_frame stop_frame{ nullptr };
                state::stream::available(&stop_frame);
            } else if (configs().system.decode.enable) {
                [[maybe_unused]] const auto enqueue_success =
                    tile_stream.render.queue.enqueue(
                        std::get<stream_context::update_frame>(std::move(frame)));
                assert(enqueue_success && "poll_update_frame enqueue render queue failed");
            }
            return true;
        }
        return false;
    }

    void _nativeDashTileFieldOfView(INT col, INT row) {
        std::atomic_store(&state::field_of_view, { col, row });
    }

    namespace test
    {
        void _nativeTestConcurrencyStore(UINT codec, UINT net) {
            configs().concurrency.decoder = codec;
            configs().concurrency.network = net;
        }

        void _nativeTestConcurrencyLoad(UINT& codec, UINT& net, UINT& executor) {
            codec = configs().concurrency.decoder;
            net = configs().concurrency.network;
            executor = configs().concurrency.executor;
        }

        LPSTR _nativeTestString() {
            return util::unmanaged_string("Hello World Test"s);
        }
    }

    auto config_from_template(const char* mpd_url) {
        const auto valid_url = mpd_url && std::string_view{ mpd_url }.size();
        if (valid_url) {
            configs().trace.enable = true;
            configs().trace.directory = util::make_log_directory(configs().workset_directory, "TraceTest");
            configs().system.decode.capacity = 30;
            configs().system.decode.enable = false;
            configs().mpd_uri = folly::Uri{ mpd_url }; // exception: std::invalid_argument
        }
        return valid_url;
    }

    BOOL _nativeLibraryConfigLoad(LPCSTR mpd_url) {
        const auto workset_environment = boost::this_process::environment()["GWorkSet"];
        if (workset_environment.empty()) return false;
        configs().workset_directory = workset_environment.to_string();
        if (!is_directory(configs().workset_directory)) return false;
        if (config_from_template(mpd_url)) return true;
        configs().document_path = configs().workset_directory / "config.json";
        if (!is_regular_file(configs().document_path)) return false;
        auto config_parse = folly::makeTryWith([] {
            // exception: nlohmann::detail::parse_error
            if (std::ifstream reader{ configs().document_path }; reader >> configs().document) {
                // exception: nlohmann::detail::out_of_range,  nlohmann::detail::type_error
                if (configs().document.get_to(configs()); configs().trace.enable) {
                    configs().trace.directory = util::make_log_directory(
                        configs().workset_directory, configs().trace.prefix);
                }
                return true;
            }
            return false;
        });
        return config_parse.hasValue() && config_parse.value();
    }

    auto reset_resource = [] {
        tile_stream_table.clear();
        tile_stream_cache.clear();
        state::stream::available(nullptr);
        std::atomic_store(&state::field_of_view, { 0, 0 });
    };

    auto make_logger(bool enable, std::string logger_name) {
        if (!enable) {
            return spdlog::null_logger_st(logger_name);
        }
        auto sink_path = configs().trace.directory / absl::StrCat(logger_name, ".log");
        return core::make_async_logger(
            logger_name, std::make_shared<spdlog::sinks::basic_file_sink_mt>(sink_path.string()));
    };

    void _nativeLibraryInitialize() {
        reset_resource();
        state::stream::running = true;
        manager = folly::Future<net::dash_manager>::makeEmpty();
        compute_executor = core::make_pool_executor(
            configs().concurrency.executor, "PluginCompute");
        stream_executor = core::make_threaded_executor("PluginSession");
        trace::plugin_logger = make_logger(configs().trace.enable, "plugin");
        trace::update_logger = make_logger(configs().trace.enable, "plugin.update");
        trace::render_logger = make_logger(configs().trace.enable, "plugin.render");
        trace::plugin_logger->info("event=library.initialize");
    }

    void _nativeLibraryRelease() {
        state::stream::running = false;
        stream_executor = nullptr; // join 1-1
        if (configs().trace.enable) {
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
        util::cleanup_callback();
    }

    BOOL _nativeLibraryTraceEvent(LPCSTR instance, LPCSTR event) {
        if (configs().trace.enable) {
            trace::update_logger->info("instance={}:event={}", instance, event);
        }
        return configs().trace.enable;
    }
}

namespace
{
#ifdef PLUGIN_RENDER_CALLBACK
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
        if (!state::stream::available()) return;
        auto& stream = *tile_stream_cache[stream_index];
        switch (static_cast<UnityRenderingExtEventType>(event_id)) {
        case kUnityRenderingExtEventUpdateTextureBeginV2: {
            if (stream.update.texture_state.none()) {
                assert(planar_index == 0);
                assert(stream.render.frame == nullptr);
                stream.render.frame = stream.render.queue.peek();
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
                stream.render.queue.pop();
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

namespace unity
{
#ifdef PLUGIN_RENDER_CALLBACK
    UnityRenderingEvent _nativeGraphicGetRenderCallback() {
        return on_render_event;
    }
#endif

    UnityRenderingEventAndData _nativeGraphicGetUpdateCallback() {
        return on_texture_update_event;
    }
}
