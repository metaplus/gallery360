#pragma once
#include "network/session.server.h"
#include "network/acceptor.h"
#include <folly/executors/SerialExecutor.h>
#ifdef signal_set
#undef signal_set
#pragma message("macro conflict: signal_set")
#endif
#include <boost/asio/signal_set.hpp>
#include <boost/process/search_path.hpp>
#include <boost/process/system.hpp>
#include <boost/thread/thread.hpp>

namespace app
{
    class server final
    {
        using session_type = net::server::session<net::protocal::http>;
        using socket_type = session_type::socket_type;

        std::unordered_map<boost::asio::ip::tcp::endpoint,
                           session_type::pointer> session_map_;
        uint16_t port_ = 0;
        std::string directory_;
        core::logger_access logger_;
        std::shared_ptr<folly::ThreadPoolExecutor> compute_worker_pool_;
        std::shared_ptr<boost::asio::io_context> asio_worker_pool_;
        boost::thread schedule_worker_;
        net::server::acceptor<boost::asio::ip::tcp> acceptor_;
        boost::asio::signal_set signals_;
        folly::Baton<false> acceptor_cancellation_;

        struct bandwidth_limit
        {
            struct min_max_pair
            {
                int min = 0;
                int max = 0;
            };

            bool enable = false;
            int offset = 0;
            int span = 0;
            min_max_pair download;
            min_max_pair upload;
        } bandwidth_limit_;

    public:
        struct server_error : virtual core::exception_base<> {};

        struct server_directory_error : virtual core::exception_base<server_directory_error>,
                                        virtual server_error {};

        struct session_map_error : virtual core::exception_base<session_map_error>,
                                   virtual core::session_error, virtual server_error {};

        server()
            : port_{ net::config_entry<uint16_t>("Net.Server.Port") }
#ifdef _WIN32
            , directory_{ net::config_entry<std::string>("Net.Server.Directories.Root.Win") }
#elif defined __linux__ && _SERVER_WSL
            , directory_{ net::config_entry<std::string>("Net.Server.Directories.Root.WSL") }
#elif defined __linux__ && !_SERVER_WSL
            , directory_{ net::config_entry<std::string>("Net.Server.Directories.Root.Linux") }
#else
#error unrecognized platform
#endif
            , logger_{ core::console_logger_access("server") }
            , asio_worker_pool_{ net::make_asio_pool(std::thread::hardware_concurrency()) }
            , acceptor_{ port_, *asio_worker_pool_ }
            , signals_{ *asio_worker_pool_, SIGINT, SIGTERM } {
            if (std::filesystem::is_directory(directory_)) {
                logger_().info("root directory {}", directory_);
            } else {
                logger_().error("invalid root directory {}", directory_);
                server_directory_error::throw_directly();
            }
            signals_.async_wait([this](boost::system::error_code error,
                                       int signal_count) {
                acceptor_cancellation_.post();
                acceptor_.close();
                asio_worker_pool_->stop();
            });
        }

        server& parse_bandwidth_limit() {
            bandwidth_limit_.enable = std::get<bool>(load_element(config::bandwidth_limit));
            bandwidth_limit_.offset = std::get<int>(load_element(config::bandwidth_limit_period_offset));
            bandwidth_limit_.span = std::get<int>(load_element(config::bandwidth_limit_period_span));
            bandwidth_limit_.download.max = std::get<int>(load_element(config::bandwidth_download_rate_max));
            bandwidth_limit_.download.min = std::get<int>(load_element(config::bandwidth_download_rate_min));
            bandwidth_limit_.upload.max = std::get<int>(load_element(config::bandwidth_upload_rate_max));
            bandwidth_limit_.upload.min = std::get<int>(load_element(config::bandwidth_upload_rate_min));
            return *this;
        }

        boost::thread make_scheduled_worker() const {
            if (auto wondershaper = boost::process::search_path("wondershaper"); wondershaper.empty()) {
                logger_().warn("wondershaper not found");
                return boost::thread{};
            }
            return boost::thread{
                [=] {
                    using namespace boost::chrono;
                    auto bandwidth_limit = fmt::format("wondershaper eth0 {} {}",
                                                       bandwidth_limit_.download.min,
                                                       bandwidth_limit_.upload.min);
                    auto bandwidth_reset = fmt::format("wondershaper eth0 {} {}",
                                                       bandwidth_limit_.download.max,
                                                       bandwidth_limit_.upload.max);
                    if (!bandwidth_limit_.enable) {
                        boost::process::system(bandwidth_reset);
                        return;
                    }
                    try {
                        boost::this_thread::sleep_for(seconds{ bandwidth_limit_.offset });
                        while (!boost::this_thread::interruption_requested()) {
                            boost::process::system(bandwidth_limit);
                            boost::this_thread::sleep_for(seconds{ bandwidth_limit_.span });
                            boost::process::system(bandwidth_reset);
                            boost::this_thread::sleep_for(seconds{ bandwidth_limit_.span });
                        }
                    } catch (boost::thread_interrupted e) {
                        logger_().warn("ScheduleWorker interrupted, exception: {}", boost::diagnostic_information(e));
                    }
                    boost::process::system(bandwidth_reset);
                }
            };
        }

        void establish_sessions(std::shared_ptr<folly::ThreadPoolExecutor> pool_executor) {
            assert(!compute_worker_pool_ && "remain threads not joined");
            compute_worker_pool_ = pool_executor;
            using session_iterator = decltype(session_map_)::iterator;
            auto serial_executor = folly::SerialExecutor::create(folly::getKeepAliveToken(pool_executor.get()));
            std::vector<folly::Future<folly::Unit>> procedure_list;
            while (!acceptor_cancellation_.ready()) {
                logger_().info("port {} listening", port_);
                auto session_procedure =
                    acceptor_.accept_socket().wait()
                             .via(pool_executor.get())
                             .thenValue(
                                 [this](socket_type socket) {
                                     return session_type::create(
                                         std::move(socket), *asio_worker_pool_, directory_);
                                 })
                             .via(serial_executor.get())
                             .thenValue(
                                 [this](session_type::pointer session) {
                                     if (session_map_.empty()) {
                                         logger_().warn("first session encountered");
                                         assert(!schedule_worker_.joinable());
                                         schedule_worker_ = make_scheduled_worker();
                                     }
                                     auto endpoint = session->remote_endpoint();
                                     auto [iterator, success] = session_map_.emplace(endpoint, std::move(session));
                                     if (!success) {
                                         logger_().error("endpoint duplicate");
                                         acceptor_cancellation_.post();
                                         session_map_error::throw_in_function("map emplacement");
                                     }
                                     return folly::collectAllSemiFuture(
                                         iterator->second->process_requests(),
                                         folly::makeSemiFuture(iterator));
                                 })
                             .thenValue(
                                 [this](std::tuple<folly::Try<folly::Unit>,
                                                   folly::Try<session_iterator>> tuple) {
                                     auto iterator = std::get<folly::Try<session_iterator>>(tuple).value();
                                     logger_().warn("erase {} left {}", iterator->second->identity(),
                                                    session_map_.size() - 1);
                                     iterator = session_map_.erase(iterator);
                                     if (!session_map_.empty()) return;
                                     logger_().warn("all session erased");
                                     if (!schedule_worker_.joinable()) return;
                                     logger_().warn("join scheduler thread");
                                     schedule_worker_.interrupt();
                                     schedule_worker_.join();
                                 });
                procedure_list.push_back(std::move(session_procedure));
            }
            const auto result_list = folly::collectAll(procedure_list).get();
            const auto success = std::count_if(result_list.begin(), result_list.end(),
                                               std::mem_fn(&folly::Try<folly::Unit>::hasValue));
            logger_().info("exit success {} of {}", success, result_list.size());
        }
    };
}
