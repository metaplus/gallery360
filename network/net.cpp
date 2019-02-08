#include "stdafx.h"
#include "net.h"
#include <fmt/ostream.h>
#include <folly/Lazy.h>
#include <tinyxml2.h>
#include <fstream>

namespace net
{
    auto logger = core::console_logger_access("net.asio");
    const auto thread_factory = folly::lazy([] {
        return std::make_unique<folly::NamedThreadFactory>("NetAsio");
    });

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
                try {
                    if (!config_stream.is_open()) {
                        config_error::throw_with_message(
                            "invalid config file stream at {}", config_path());
                    }
                    config_stream >> config_json;
                } catch (nlohmann::detail::parse_error e) {
                    config_error::throw_with_message(
                        "invalid json config at {} with detail {}", config_path(), e.what());
                }
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
