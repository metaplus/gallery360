#include "stdafx.h"
#include "net.h"
#include <boost/container/flat_map.hpp>
#include <fstream>
#include <re2/re2.h>
#include <tinyxml2.h>

namespace net
{
    auto logger = core::console_logger_access("net.asio");
    const auto thread_factory = folly::lazy([] {
        return std::make_unique<folly::NamedThreadFactory>("NetAsio");
    });

#ifdef _WIN32
    namespace protocal
    {
        struct dash::parser::impl final
        {
            boost::container::flat_map<
                core::coordinate, video_adaptation_set> video_adaptation_sets;
            audio_adaptation_set audio_adaptation_set;
            std::string title;
            std::chrono::milliseconds min_buffer_time;
            std::chrono::milliseconds max_segment_duration;
            std::chrono::milliseconds presentation_time;
            core::coordinate grid;
            core::dimension scale;
            std::shared_ptr<folly::ThreadPoolExecutor> executor;

            struct duration_parse_mismatch final : std::runtime_error
            {
                using runtime_error::runtime_error;
                using runtime_error::operator=;
            };

            static std::vector<tinyxml2::XMLElement*> list_next_sibling(const char* name,
                                                                        tinyxml2::XMLElement* first_element) {
                std::vector<tinyxml2::XMLElement*> sibling{ first_element };
                auto* next = first_element->NextSiblingElement();
                while (next != nullptr) {
                    sibling.push_back(next);
                    next = next->NextSiblingElement(name);
                }
                return sibling;
            }

            static std::array<int, 6> split_spatial_description(std::string_view srd) {
                std::array<int, 6> spatial{};
                srd.remove_prefix(srd.find(',') + 1);
                folly::splitTo<int>(',', srd, spatial.begin(), false);
                return spatial;
            }

            void parse_video_adaptation_set(std::vector<tinyxml2::XMLElement*>::iterator element_begin,
                                            std::vector<tinyxml2::XMLElement*>::iterator element_end) {
                auto make_video_set = [](tinyxml2::XMLElement* element) {
                    video_adaptation_set adaptation_set;
                    auto represents = list_next_sibling("Representation", element->FirstChildElement("Representation"));
                    adaptation_set.codecs = represents.front()->Attribute("codecs");
                    adaptation_set.mime_type = represents.front()->Attribute("mimeType");
                    adaptation_set.width = folly::to<int>(element->Attribute("maxWidth"));
                    adaptation_set.height = folly::to<int>(element->Attribute("maxHeight"));
                    auto [x, y, w, h, total_w, total_h] = split_spatial_description(
                        element->FirstChildElement("SupplementalProperty")
                               ->Attribute("value"));
                    adaptation_set.col = x;
                    adaptation_set.row = y;
                    adaptation_set.represents.resize(represents.size());
                    std::transform(
                        std::execution::par,
                        represents.begin(), represents.end(),
                        adaptation_set.represents.begin(),
                        [](tinyxml2::XMLElement* element) {
                            const auto format = [](const char* str) {
                                static const RE2 media_regex{ "\\$Number\\$" };
                                std::string media_url_pattern{ str };
                                auto replace_success = RE2::Replace(&media_url_pattern, media_regex, "{}");
                                assert(replace_success);
                                return media_url_pattern;
                            };
                            represent represent;
                            represent.id = folly::to<int>(element->Attribute("id"));
                            represent.bandwidth = folly::to<int>(element->Attribute("bandwidth"));
                            represent.media = format(element->FirstChildElement("SegmentTemplate")->Attribute("media"));
                            represent.initial = element->FirstChildElement("SegmentTemplate")->Attribute("initialization");
                            return represent;
                        });
                    return adaptation_set;
                };
                video_adaptation_sets = std::reduce(
                    element_begin, element_end,
                    decltype(video_adaptation_sets){},
                    [&make_video_set](decltype(video_adaptation_sets)&& reduce,
                                      tinyxml2::XMLElement* element) {
                        auto video_set = make_video_set(element);
                        auto [iterator, success] = reduce.try_emplace(
                            static_cast<core::coordinate&>(video_set),
                            std::move(video_set));
                        assert(success);
                        return std::move(reduce);
                    }
                );
            }

            void parse_audio_adaptation_set(tinyxml2::XMLElement* element) {
                element = element->FirstChildElement("Representation");
                audio_adaptation_set.codecs = element->Attribute("codecs");
                audio_adaptation_set.mime_type = element->Attribute("mimeType");
                represent represent;
                represent.id = folly::to<int>(element->Attribute("id"));
                represent.bandwidth = folly::to<int>(element->Attribute("bandwidth"));
                represent.media = element->FirstChildElement("SegmentTemplate")
                                         ->Attribute("media");
                represent.initial = element->FirstChildElement("SegmentTemplate")
                                           ->Attribute("initialization");
                audio_adaptation_set.represents.push_back(std::move(represent));
                audio_adaptation_set.sample_rate = folly::to<int>(element->Attribute("audioSamplingRate"));
            }

            void parse_grid_size(tinyxml2::XMLElement* element) {
                auto [x, y, w, h, total_w, total_h] =
                    split_spatial_description(element->FirstChildElement("SupplementalProperty")
                                                     ->Attribute("value"));
                grid = core::coordinate{ total_w, total_h };
            }

            void parse_scale_size() {
                using scale_pair = std::pair<int, int>;
                scale = std::transform_reduce(
                    std::execution::par,
                    video_adaptation_sets.begin(), video_adaptation_sets.end(),
                    core::dimension{ 0, 0 },
                    [](core::dimension sum, core::dimension increment) {
                        return core::dimension{
                            sum.width + increment.width,
                            sum.height + increment.height
                        };
                    },
                    [](decltype(video_adaptation_sets)::reference adaptation_set) {
                        return core::dimension{
                            adaptation_set.second.width,
                            adaptation_set.second.height
                        };
                    });
                scale.width /= grid.row;
                scale.height /= grid.col;
            }
        };

        dash::parser::parser(std::string_view xml_text,
                             std::shared_ptr<folly::ThreadPoolExecutor> executor)
            : impl_(std::make_shared<impl>()) {
            tinyxml2::XMLDocument document;
            auto success = document.Parse(xml_text.data());
            assert(success == tinyxml2::XMLError::XML_SUCCESS);
            auto* xml_root = document.RootElement();
            impl_->presentation_time = parse_duration(xml_root->Attribute("mediaPresentationDuration"));
            impl_->min_buffer_time = parse_duration(xml_root->Attribute("minBufferTime"));
            impl_->max_segment_duration = parse_duration(xml_root->Attribute("maxSegmentDuration"));
            impl_->title = xml_root->FirstChildElement("ProgramInformation")
                                   ->FirstChildElement("Title")
                                   ->GetText();
            impl_->executor = std::move(executor);
            auto adaptation_sets = impl::list_next_sibling("AdaptationSet",
                                                           xml_root->FirstChildElement("Period")
                                                                   ->FirstChildElement("AdaptationSet"));
            auto audio_set_iter = std::remove_if(
                std::execution::par,
                adaptation_sets.begin(), adaptation_sets.end(),
                [](tinyxml2::XMLElement* element) {
                    std::string_view mime = element->FirstChildElement("Representation")
                                                   ->Attribute("mimeType");
                    return mime.find("audio") != std::string_view::npos;
                });
            folly::collectAll(
                    folly::collectAll(
                        folly::via(impl_->executor.get(), [&] {
                            impl_->parse_video_adaptation_set(adaptation_sets.begin(), audio_set_iter);
                        }),
                        folly::via(impl_->executor.get(), [&] {
                            impl_->parse_grid_size(adaptation_sets.front());
                        }))
                    .thenValue([&](auto&&) {
                        impl_->parse_scale_size();
                    }),
                    folly::via(impl_->executor.get(), [&] {
                        if (audio_set_iter != adaptation_sets.end()) {
                            impl_->parse_audio_adaptation_set(*audio_set_iter);
                        }
                    }))
                .wait();
        }

        std::string_view dash::parser::title() const {
            return impl_->title;
        }

        core::coordinate dash::parser::grid() const {
            return impl_->grid;
        }

        core::dimension dash::parser::scale() const {
            return impl_->scale;
        }

        dash::video_adaptation_set&
        dash::parser::video_set(core::coordinate coordinate) const {
            return impl_->video_adaptation_sets.at(coordinate);
        }

        dash::audio_adaptation_set&
        dash::parser::audio_set() const {
            return impl_->audio_adaptation_set;
        }
    }

    std::chrono::milliseconds protocal::dash::parser::parse_duration(std::string_view duration) {
        auto hour = 0, minute = 0;
        double second = 0;
        if (RE2::FullMatch(duration.data(), R"(PT(((\d+)H)?(\d+)M)?(\d+\.\d+)S)",
                           nullptr, nullptr, &hour, &minute, &second)
            || RE2::FullMatch(duration.data(), R"(PT(\d+\.\d+)S)", &second)) {
            return std::chrono::hours{ hour }
                + std::chrono::minutes{ minute }
                + std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>{ second });
        }
        throw impl::duration_parse_mismatch{ __FUNCTION__ };
    }
#endif

    auto workset_paths = folly::lazy([] {
        return std::vector<std::filesystem::path>{ std::filesystem::current_path(), _NET_CONFIG_DIR };
    });

    void add_config_path(std::filesystem::path&& path) {
        if (std::filesystem::is_directory(path)) {
            workset_paths().push_back(std::move(path));
        }
    }

    const std::filesystem::path& config_path(bool json) noexcept {
        static const auto config_path = folly::lazy(
            [] {
                std::array<std::filesystem::path, 2> config_path_array;
                const auto config_dir = std::find_if(
                    workset_paths().rbegin(), workset_paths().rend(),
                    [](std::filesystem::path& dir) {
                        return std::filesystem::is_directory(dir);
                    });
                assert(config_dir != workset_paths().rend());
                if (config_dir == std::prev(workset_paths().rend())) {
                    logger().warn("config directory uses default work path");
                }
                logger().info("config directory {}", config_dir->generic_string());
                config_path_array[false] = *config_dir / "config.xml";
                config_path_array[true] = *config_dir / "config.json";
                assert(std::filesystem::is_regular_file(config_path_array[true]));
                return config_path_array;
            });
        return config_path()[folly::to<size_t>(json)];
    }

    std::string config_xml_entry(std::vector<std::string> entry_path) {
        static const auto config_document = folly::lazy(
            [] {
                auto config_document = std::make_unique<tinyxml2::XMLDocument>();
                [[maybe_unused]] const auto load_success = config_document->LoadFile(config_path(false).string().data());
                assert(load_success == tinyxml2::XML_SUCCESS);
                return config_document;
            });
        tinyxml2::XMLNode* config_node = config_document().get();
        for (auto& node_name : entry_path) {
            assert(config_node);
            config_node = config_node->FirstChildElement(node_name.data());
        }
        return config_node->ToElement()
                          ->GetText();
    }

    nlohmann::json::reference config_json_entry(std::vector<std::string> entry_path) {
        static auto config_json = folly::lazy(
            [] {
                nlohmann::json config_json;
                std::ifstream config_stream{ config_path().string().data() };
                assert(config_stream.good());
                config_stream >> config_json;
                return config_json;
            });
        auto& entry = config_json()[entry_path.front()];
        if (entry_path.size() > 1) {
            return
#ifdef __linux__
                *std::accumulate(
#else
                *std::reduce(
#endif
                    std::next(entry_path.begin(), 1), entry_path.end(), &entry,
                    [](decltype(&entry) entry_ptr, std::string& name) {
                        assert(entry_ptr != nullptr);
                        return &(*entry_ptr)[name];
                    });
        }
        return entry;
    }

    using boost::asio::io_context;
    using boost::asio::executor_work_guard;
    using boost::container::small_vector;

    class asio_deleter final : std::default_delete<io_context>
    {
        executor_work_guard<io_context::executor_type> guard_;
        small_vector<std::thread, 8> threads_;

    public:
        asio_deleter(io_context* io_context, unsigned concurrency)
            : guard_{ make_work_guard(*io_context) }
            , threads_{ make_asio_threads(*io_context, concurrency) } { }

        asio_deleter() = default;
        asio_deleter(asio_deleter const&) = delete;

        asio_deleter(asio_deleter&& that) noexcept
            : guard_{ std::move(that.guard_) }
            , threads_{ std::move(that.threads_) } { }

        asio_deleter& operator=(asio_deleter const&) = delete;
        asio_deleter& operator=(asio_deleter&&) noexcept = delete;
        ~asio_deleter() = default;

        void operator()(io_context* io_context) {
            guard_.reset();
            io_context->stop();
            logger().info("threads join start");
            const auto join_count = std::count_if(
                threads_.begin(), threads_.end(),
                [](std::thread& thread) {
                    if (thread.joinable()) {
                        thread.join();
                        return true;
                    }
                    return false;
                }
            );
            logger().info("threads join {} of {}", join_count, std::size(threads_));
            static_cast<default_delete&>(*this)(io_context);
            logger().info("io_context destruct");
        }
    };

    static_assert(std::is_move_constructible<executor_work_guard<io_context::executor_type>>::value);
    static_assert(std::is_move_constructible<small_vector<std::thread, 8>>::value);
    static_assert(std::is_move_constructible<asio_deleter>::value);

    small_vector<std::thread, 8>
    make_asio_threads(io_context& io_context, unsigned concurrency) {
        small_vector<std::thread, 8> threads{ concurrency };
        std::generate(threads.begin(), threads.end(),
                      [&io_context] {
                          return thread_factory()->newThread([&io_context] {
                              const auto thread_id = std::this_thread::get_id();
                              try {
                                  logger().info("thread@{} start", thread_id);
                                  io_context.run();
                                  logger().info("thread@{} finish", thread_id);
                              } catch (...) {
                                  const auto message = boost::current_exception_diagnostic_information();
                                  logger().error("thread@{} error {}", thread_id, message);
                              }
                              logger().info("thread@{} exit", thread_id);
                          });
                      });
        return threads;
    }

    std::shared_ptr<io_context> make_asio_pool(unsigned concurrency) {
        logger().info("make_asio_pool");
        auto* io_context_ptr = new io_context{};
        return std::shared_ptr<io_context>{
            io_context_ptr, asio_deleter{ io_context_ptr, concurrency }
        };
    }
}
