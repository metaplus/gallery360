#include "stdafx.h"
#include "net.hpp"
#include <boost/property_tree/xml_parser.hpp>
#include <folly/executors/thread_factory/NamedThreadFactory.h>
#include <tinyxml2.h>
#include <boost/multi_array.hpp>

namespace net
{
    const auto logger = spdlog::stdout_color_mt("net.asio");

    boost::thread_group net_threads;

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
                    std::execution::par, element_begin, element_end, video_adaptation_sets.begin(),
                    [](tinyxml2::XMLElement* element) {
                        video_adaptation_set adaptation_set;
                        auto represents = list_next_sibling("Representation", element->FirstChildElement("Representation"));
                        adaptation_set.codecs = represents.front()->Attribute("codecs");
                        adaptation_set.mime_type = represents.front()->Attribute("mimeType");
                        adaptation_set.width = std::stoi(element->Attribute("maxWidth"));
                        adaptation_set.height = std::stoi(element->Attribute("maxHeight"));
                        auto[x, y, w, h, total_w, total_h] = split_spatial_description(
                            element->FirstChildElement("SupplementalProperty")->Attribute("value"));
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
                                represent.id = std::stoi(element->Attribute("id"));
                                represent.bandwidth = std::stoi(element->Attribute("bandwidth"));
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
                represent.id = std::stoi(element->Attribute("id"));
                represent.bandwidth = std::stoi(element->Attribute("bandwidth"));
                represent.media = element->FirstChildElement("SegmentTemplate")->Attribute("media");
                represent.initial = element->FirstChildElement("SegmentTemplate")->Attribute("initialization");
                audio_adaptation_set.represents.push_back(std::move(represent));
                audio_adaptation_set.sample_rate = std::stoi(element->Attribute("audioSamplingRate"));
            }

            void parse_grid_size(tinyxml2::XMLElement* element) {
                auto[x, y, w, h, total_w, total_h] = split_spatial_description(
                    element->FirstChildElement("SupplementalProperty")->Attribute("value"));
                grid_width = total_w;
                grid_height = total_h;
            }

            void parse_scale_size() {
                using scale_pair = std::pair<int, int>;
                std::tie(scale_width, scale_height) = std::transform_reduce(
                    std::execution::par, video_adaptation_sets.begin(), video_adaptation_sets.end(), scale_pair{ 0,0 },
                    [](scale_pair sum, scale_pair increment) {
                        return scale_pair{ sum.first + increment.first,sum.second + increment.second };
                    },
                    [](video_adaptation_set& adaptation_set) {
                        return scale_pair{ adaptation_set.width,adaptation_set.height };
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
            impl_->title = xml_root->FirstChildElement("ProgramInformation")->FirstChildElement("Title")->GetText();
            auto adaptation_sets = impl::list_next_sibling("AdaptationSet",
                                                           xml_root->FirstChildElement("Period")->FirstChildElement("AdaptationSet"));
            auto audio_set_iter = std::remove_if(
                std::execution::par, adaptation_sets.begin(), adaptation_sets.end(),
                [](tinyxml2::XMLElement* element) {
                    std::string_view mime = element->FirstChildElement("Representation")->Attribute("mimeType");
                    return mime.find("audio") != std::string_view::npos;
                });
            auto futures = {
                boost::when_all(
                    boost::async([&] { impl_->parse_video_adaptation_set(adaptation_sets.begin(), audio_set_iter); }),
                    boost::async([&] { impl_->parse_grid_size(adaptation_sets.front()); })
                ).then([&](auto&&) { impl_->parse_scale_size(); }),
                boost::async([&] { impl_->parse_audio_adaptation_set(*audio_set_iter); })
            };
            boost::wait_for_all(futures.begin(), futures.end());
        }

        std::string_view dash::parser::title() const {
            return impl_->title;
        }
        std::pair<int, int> dash::parser::grid_size() const {
            return std::make_pair(impl_->grid_width, impl_->grid_height);
        }

        std::pair<int, int> dash::parser::scale_size() const {
            return std::make_pair(impl_->scale_width, impl_->scale_height);
        }

        std::vector<dash::video_adaptation_set>& dash::parser::video_set() const {
            return impl_->video_adaptation_sets;
        }

        dash::video_adaptation_set& dash::parser::video_set(int column, int row) const {
            boost::multi_array_ref<video_adaptation_set, 2>
                matrix_view{ impl_->video_adaptation_sets.data(), boost::extents[impl_->grid_width][impl_->grid_height] };
            using matrix_index = decltype(matrix_view)::index;
            return matrix_view(boost::array<matrix_index, 2>{ column, row });		//!dimension order
        }

        dash::audio_adaptation_set& dash::parser::audio_set() const {
            return impl_->audio_adaptation_set;
        }
    }

    std::chrono::milliseconds protocal::dash::parser::parse_duration(std::string_view duration) {
        static const std::regex duration_pattern{ R"(PT((\d+)H)?((\d+)M)?(\d+\.\d+)S)" };
        std::cmatch matches;
        if (std::regex_match(duration.data(), matches, duration_pattern)) {
            using namespace std::chrono;
            const hours t1{ matches[2].matched ? std::stoi(matches[2].str()) : 0 };
            const minutes t2{ matches[4].matched ? std::stoi(matches[4].str()) : 0 };
            const milliseconds t3{ boost::numeric_cast<int64_t>(
               matches[5].matched ? 1000 * std::stod(matches[5].str()) : 0) };
            return t1 + t2 + t3;
        }
        throw impl::duration_parse_mismatch{ __FUNCSIG__ };
    }

    std::vector<boost::thread*> create_asio_threads(boost::asio::io_context& context, boost::thread_group& thread_group, uint32_t num) {
        std::vector<boost::thread*> threads(num);
        std::generate(threads.begin(), threads.end(),
                      [&thread_group, &context] {
                          return thread_group.create_thread(
                              [&context] {
                                  try {
                                      context.run();
                                  } catch (...) {
                                      logger->error("Exception@{}: {}\n", boost::this_thread::get_id(),
                                                    boost::diagnostic_information(boost::current_exception()));
                                      return EXIT_FAILURE;
                                  }
                                  return EXIT_SUCCESS;
                              });
                      });
        return threads;
    }

    std::vector<std::thread> create_asio_named_threads(boost::asio::io_context& context, uint32_t num) {
        std::vector<std::thread> threads(num);
        auto thread_factory = std::make_unique<folly::NamedThreadFactory>("NetAsio");
        std::generate(threads.begin(), threads.end(),
                      [&thread_factory, &context] {
                          return thread_factory->newThread([&context] { context.run(); });
                      });
        return threads;
    }

    std::filesystem::path config_path() noexcept {
        const auto config_path = std::filesystem::path{ _NET_DIR } / "config.xml";
        core::verify(std::filesystem::is_regular_file(config_path));
        return config_path;
    }

    boost::property_tree::ptree const& load_config() {
        static std::once_flag flag;
        static boost::property_tree::ptree config_ptree;
        std::call_once(flag,
                       [] {
                           const auto config_path = net::config_path().generic_string();
                           boost::property_tree::read_xml(config_path, config_ptree);
                       });
        return config_ptree;
    }

    std::string config_entry(std::string_view entry_name) {
        return load_config().get<std::string>(entry_name.data());
    }

    struct asio_deleter : std::default_delete<boost::asio::io_context>
    {
        using resource = boost::asio::io_context;
        using guardian = boost::asio::executor_work_guard<resource::executor_type>;
        std::unique_ptr<guardian> guard;
        std::vector<boost::thread*> threads;

        asio_deleter() = default;
        asio_deleter(asio_deleter const&) = delete;
        asio_deleter(asio_deleter&&) noexcept = default;
        asio_deleter& operator=(asio_deleter const&) = delete;
        asio_deleter& operator=(asio_deleter&&) noexcept = default;
        ~asio_deleter() = default;

        asio_deleter(resource* io_context, unsigned concurrency)
            : guard(std::make_unique<guardian>(boost::asio::make_work_guard(*io_context))) {
            std::generate_n(
                std::back_inserter(threads),
                concurrency,
                [this, io_context] {
                    return net_threads.create_thread(
                        [this, io_context] {
                            const auto thread_id = boost::this_thread::get_id();
                            try {
                                logger->info("thread@{} start", thread_id);
                                io_context->run();
                                logger->info("thread@{} finish", thread_id);
                            } catch (...) {
                                const auto message = boost::current_exception_diagnostic_information();
                                logger->error("thread@{} error {}", thread_id, message);
                            }
                            logger->info("thread@{} exit", thread_id);
                        });
                });
        }

        void operator()(resource* io_context) {
            guard->reset();
            auto join_count = 0;
            for (auto* const thread : threads) {
                if (thread->joinable()) {
                    thread->join();
                    ++join_count;
                }
            }
            logger->info("thread_group join count {}", join_count);
            static_cast<default_delete&>(*this)(io_context);
            logger->info("io_context destruct");
        }
    };

    std::shared_ptr<boost::asio::io_context> create_running_asio_pool(unsigned concurrency) {
        logger->info("create_running_asio_pool");
        auto* io_context = new boost::asio::io_context();
        return std::shared_ptr<boost::asio::io_context>{
            io_context, asio_deleter{ io_context,concurrency }
        };
    }

    namespace error
    {
        struct bad_request : std::runtime_error
        {
            using runtime_error::runtime_error;
            using runtime_error::operator=;
        };
    }

    void throw_bad_request(std::string message) {
        core::throw_with_stacktrace(error::bad_request{ message });
    }
}
