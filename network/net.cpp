#include "stdafx.h"
#include "net.hpp"
#include <folly/executors/thread_factory/NamedThreadFactory.h>
#include <tinyxml2.h>
#include <boost/multi_array.hpp>
#include <re2/re2.h>

namespace net
{
    const auto logger = spdlog::stdout_color_mt("net.asio");
    const auto thread_factory = folly::lazy([] {
        return std::make_unique<folly::NamedThreadFactory>("NetAsio");
    });

    namespace protocal
    {
        struct dash::parser::impl
        {
            std::vector<video_adaptation_set> video_adaptation_sets;
            audio_adaptation_set audio_adaptation_set;
            std::string title;
            std::chrono::milliseconds min_buffer_time;
            std::chrono::milliseconds max_segment_duration;
            std::chrono::milliseconds presentation_time;
            int grid_width = 0;
            int grid_height = 0;
            int scale_width = 0;
            int scale_height = 0;

            struct duration_parse_mismatch : std::runtime_error
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
                video_adaptation_sets.resize(std::distance(element_begin, element_end));
                std::transform(
                    std::execution::par,
                    element_begin, element_end,
                    video_adaptation_sets.begin(),
                    [](tinyxml2::XMLElement* element) {
                        video_adaptation_set adaptation_set;
                        auto represents = list_next_sibling("Representation", element->FirstChildElement("Representation"));
                        adaptation_set.codecs = represents.front()->Attribute("codecs");
                        adaptation_set.mime_type = represents.front()->Attribute("mimeType");
                        adaptation_set.width = folly::to<int>(element->Attribute("maxWidth"));
                        adaptation_set.height = folly::to<int>(element->Attribute("maxHeight"));
                        auto [x, y, w, h, total_w, total_h] = split_spatial_description(element->FirstChildElement("SupplementalProperty")
                                                                                               ->Attribute("value"));
                        adaptation_set.x = x;
                        adaptation_set.y = y;
                        adaptation_set.represents.resize(represents.size());
                        std::transform(
                            std::execution::par, represents.begin(), represents.end(), adaptation_set.represents.begin(),
                            [](tinyxml2::XMLElement* element) {
                                const auto format = [](const char* str) {
                                    static const std::regex media_regex{ "\\$Number\\$" };
                                    return std::regex_replace(str, media_regex, "{}");
                                };
                                represent represent;
                                represent.id = folly::to<int>(element->Attribute("id"));
                                represent.bandwidth = folly::to<int>(element->Attribute("bandwidth"));
                                represent.media = format(element->FirstChildElement("SegmentTemplate")->Attribute("media"));
                                represent.initial = element->FirstChildElement("SegmentTemplate")->Attribute("initialization");
                                return represent;
                            });
                        return adaptation_set;
                    });
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
                audio_adaptation_set.sample_rate = std::stoi(element->Attribute("audioSamplingRate"));
            }

            void parse_grid_size(tinyxml2::XMLElement* element) {
                auto [x, y, w, h, total_w, total_h] = split_spatial_description(element->FirstChildElement("SupplementalProperty")
                                                                                       ->Attribute("value"));
                grid_width = total_w;
                grid_height = total_h;
            }

            void parse_scale_size() {
                using scale_pair = std::pair<int, int>;
                std::tie(scale_width, scale_height) = std::transform_reduce(
                    std::execution::par,
                    video_adaptation_sets.begin(), video_adaptation_sets.end(),
                    scale_pair{ 0, 0 },
                    [](scale_pair sum, scale_pair increment) {
                        return scale_pair{
                            sum.first + increment.first,
                            sum.second + increment.second
                        };
                    },
                    [](video_adaptation_set& adaptation_set) {
                        return scale_pair{
                            adaptation_set.width,
                            adaptation_set.height
                        };
                    });
                scale_width /= grid_height;
                scale_height /= grid_width;
            }
        };

        dash::parser::parser(std::string_view xml_text)
            : impl_(std::make_shared<impl>()) {
            tinyxml2::XMLDocument document;
            core::check[tinyxml2::XMLError::XML_SUCCESS] << document.Parse(xml_text.data());
            auto* xml_root = document.RootElement();
            impl_->presentation_time = parse_duration(xml_root->Attribute("mediaPresentationDuration"));
            impl_->min_buffer_time = parse_duration(xml_root->Attribute("minBufferTime"));
            impl_->max_segment_duration = parse_duration(xml_root->Attribute("maxSegmentDuration"));
            impl_->title = xml_root->FirstChildElement("ProgramInformation")
                                   ->FirstChildElement("Title")
                                   ->GetText();
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
            auto executor = folly::getCPUExecutor();
            folly::collectAll(
                    folly::collectAll(
                        folly::via(executor.get(), [&] {
                            impl_->parse_video_adaptation_set(adaptation_sets.begin(), audio_set_iter);
                        }),
                        folly::via(executor.get(), [&] {
                            impl_->parse_grid_size(adaptation_sets.front());
                        }))
                    .thenValue([&](auto&&) {
                        impl_->parse_scale_size();
                    }),
                    folly::via(executor.get(), [&] {
                        if (audio_set_iter != adaptation_sets.end()) {
                            impl_->parse_audio_adaptation_set(*audio_set_iter);
                        }
                    }))
                .wait();
        }

        std::string_view dash::parser::title() const {
            return impl_->title;
        }

        std::pair<int, int> dash::parser::grid_size() const {
            return std::make_pair(impl_->grid_width,
                                  impl_->grid_height);
        }

        std::pair<int, int> dash::parser::scale_size() const {
            return std::make_pair(impl_->scale_width,
                                  impl_->scale_height);
        }

        std::vector<dash::video_adaptation_set>& dash::parser::video_set() const {
            return impl_->video_adaptation_sets;
        }

        dash::video_adaptation_set& dash::parser::video_set(int column, int row) const {
            const auto index = column + row * grid_size().first;
            return impl_->video_adaptation_sets.at(index);
        }

        dash::audio_adaptation_set& dash::parser::audio_set() const {
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

    std::filesystem::path config_path() noexcept {
        const auto config_path = std::filesystem::path{ _NET_CONFIG_DIR } / "config.xml";
        core::verify(std::filesystem::is_regular_file(config_path));
        return config_path;
    }

    std::string config_entry(std::initializer_list<std::string_view> entry_path) {
        static std::optional<tinyxml2::XMLDocument> config_document;
        if (!config_document) {
            const auto load_success = config_document.emplace()
                                                     .LoadFile(config_path().string().data());
            assert(load_success == tinyxml2::XML_SUCCESS);
        }
        tinyxml2::XMLNode* config_node = &config_document.value();
        for (auto& node_name : entry_path) {
            assert(config_node);
            config_node = config_node->FirstChildElement(std::data(node_name));
        }
        return config_node->ToElement()
                          ->GetText();
    }

    using boost::asio::io_context;
    using boost::asio::executor_work_guard;

    struct asio_deleter : std::default_delete<boost::asio::io_context>
    {
        std::unique_ptr<
            executor_work_guard<
                io_context::executor_type>
        > guard;
        std::vector<std::thread> threads;

        asio_deleter() = default;
        asio_deleter(asio_deleter const&) = delete;
        asio_deleter(asio_deleter&&) noexcept = default;
        asio_deleter& operator=(asio_deleter const&) = delete;
        asio_deleter& operator=(asio_deleter&&) noexcept = default;
        ~asio_deleter() = default;

        asio_deleter(io_context* io_context, unsigned concurrency)
            : guard{ std::make_unique<decltype(guard)::element_type>(make_work_guard(*io_context)) }
            , threads{ make_asio_threads(*io_context, concurrency) } { }

        void operator()(io_context* io_context) {
            guard = nullptr;
            const auto join_count = std::count_if(
                threads.begin(), threads.end(),
                [](std::thread& thread) {
                    if (thread.joinable()) {
                        thread.join();
                        return true;
                    }
                    return false;
                }
            );
            logger->info("threads join {} of {}", join_count, std::size(threads));
            static_cast<default_delete&>(*this)(io_context);
            logger->info("io_context destruct");
        }
    };

    static_assert(std::is_move_constructible<asio_deleter>::value);

    std::vector<std::thread> make_asio_threads(boost::asio::io_context& io_context,
                                               unsigned concurrency) {
        std::vector<std::thread> threads(concurrency);
        std::generate(threads.begin(), threads.end(),
                      [&io_context] {
                          return thread_factory()->newThread([&io_context] {
                              const auto thread_id = boost::this_thread::get_id();
                              try {
                                  logger->info("thread@{} start", thread_id);
                                  io_context.run();
                                  logger->info("thread@{} finish", thread_id);
                              } catch (...) {
                                  const auto message = boost::current_exception_diagnostic_information();
                                  logger->error("thread@{} error {}", thread_id, message);
                              }
                              logger->info("thread@{} exit", thread_id);
                          });
                      });
        return threads;
    }

    std::shared_ptr<io_context> make_asio_pool(unsigned concurrency) {
        logger->info("make_asio_pool");
        auto* io_context_ptr = new io_context{};
        return std::shared_ptr<io_context>{
            io_context_ptr, asio_deleter{ io_context_ptr, concurrency }
        };
    }
}
