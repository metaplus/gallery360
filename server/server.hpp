#pragma once

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
        std::shared_ptr<boost::asio::io_context> asio_pool_;
        net::server::acceptor<boost::asio::ip::tcp> acceptor_;
        boost::asio::signal_set signals_;
        folly::Baton<false> cancel_accept_;
        core::logger_access logger_;

    public:
        struct session_emplace_error final : std::runtime_error
        {
            using runtime_error::runtime_error;
            using runtime_error::operator=;
        };

        server()
            : port_{ net::config_entry<uint16_t>("Net.Server.Port") }
#ifdef _WIN32
            , directory_{ net::config_entry<std::string>("Net.Server.Directories.Root.Win") }
#elif defined __linux__ && defined _SERVER_WSL
            , directory_{ net::config_entry<std::string>("Net.Server.Directories.Root.WSL") }
#elif defined __linux__ && !defined _SERVER_WSL
            , directory_{ net::config_entry<std::string>("Net.Server.Directories.Root.Linux") }
#else
#error unrecognized platform
#endif
            , asio_pool_{ net::make_asio_pool(std::thread::hardware_concurrency()) }
            , acceptor_{ port_, *asio_pool_ }
            , signals_{ *asio_pool_, SIGINT, SIGTERM }
            , logger_{ core::console_logger_access("server") } {
            logger_().info("root directory {}", directory_);
            signals_.async_wait([this](boost::system::error_code error,
                                       int signal_count) {
                cancel_accept_.post();
                acceptor_.close();
                asio_pool_->stop();
            });
        }

        void establish_sessions(std::shared_ptr<folly::ThreadPoolExecutor> pool_executor) {
            using session_iterator = decltype(session_map_)::iterator;
            auto serial_executor = folly::SerialExecutor::create(folly::getKeepAliveToken(pool_executor.get()));
            std::vector<folly::Future<folly::Unit>> procedure_list;
            while (!cancel_accept_.ready()) {
                logger_().info("port {} listening", port_);
                auto session_procedure =
                    acceptor_.accept_socket().wait()
                             .via(pool_executor.get()).thenValue(
                                 [this](socket_type socket) {
                                     return session_type::create(
                                         std::move(socket), *asio_pool_, directory_);
                                 })
                             .thenMultiWithExecutor(
                                 serial_executor.get(),
                                 [this](session_type::pointer session) {
                                     auto endpoint = session->remote_endpoint();
                                     auto [iterator, success] = session_map_.emplace(endpoint, std::move(session));
                                     if (!success) {
                                         logger_().error("endpoint duplicate");
                                         cancel_accept_.post();
                                         throw session_emplace_error{ "session_map_ emplace fail" };
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
