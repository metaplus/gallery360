#pragma once
#include "network/session.server.h"
#include "network/acceptor.h"
#include <folly/executors/SerialExecutor.h>
#include <boost/asio/signal_set.hpp>

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
        std::shared_ptr<folly::ThreadedExecutor> schedule_worker_;
        std::shared_ptr<boost::asio::io_context> asio_worker_pool_;
        net::server::acceptor<boost::asio::ip::tcp> acceptor_;
        boost::asio::signal_set signals_;
        folly::Baton<false> acceptor_cancellation_;

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
                             .via(pool_executor.get()).thenValue(
                                 [this](socket_type socket) {
                                     return session_type::create(
                                         std::move(socket), *asio_worker_pool_, directory_);
                                 })
                             .thenMultiWithExecutor(
                                 serial_executor.get(),
                                 [this](session_type::pointer session) {
                                     if (session_map_.empty()) {
                                         logger_().warn("first session encountered");
                                         if (std::get<bool>(load_element(config::bandwidth_limit))) {
                                             if (!schedule_worker_) {
                                                 schedule_worker_ = core::make_threaded_executor("ScheduleWorker");
                                             }
                                             schedule_worker_->add([] {});
                                         }
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
                                 },
                                 [this](std::tuple<folly::Try<folly::Unit>,
                                                   folly::Try<session_iterator>> tuple) {
                                     auto iterator = std::get<folly::Try<session_iterator>>(tuple).value();
                                     logger_().warn("erase {} left {}", iterator->second->identity(), session_map_.size() - 1);
                                     iterator = session_map_.erase(iterator);
                                     if (session_map_.empty()) {
                                         logger_().warn("all session erased");
                                         if (schedule_worker_) {
                                             logger_().warn("join scheduler thread");
                                             schedule_worker_ = nullptr;
                                         }
                                     }
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
