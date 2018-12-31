#pragma once

namespace app
{
    class server final
    {
        uint16_t port_ = 0;
        std::string directory_;
        folly::ConcurrentHashMap<
            boost::asio::ip::tcp::endpoint,
            net::server::session_ptr<net::protocal::http>
        > session_map_;
        folly::USPSCQueue<
            folly::SemiFuture<boost::asio::ip::tcp::socket>, true
        > socket_queue_;
        std::shared_ptr<boost::asio::io_context> asio_pool_;
        net::server::acceptor<boost::asio::ip::tcp> acceptor_;
        boost::asio::signal_set signals_;
        std::shared_ptr<folly::ThreadedExecutor> worker_executor_;
        std::shared_ptr<spdlog::logger> logger_;

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
            , worker_executor_{ core::make_threaded_executor("ServerWorker") }
            , logger_{ spdlog::stdout_color_mt("server") } {
            logger_->info("root directory {}", directory_);
            signals_.async_wait([&](boost::system::error_code error,
                                    int signal_count) {
                acceptor_.close();
                asio_pool_->stop();
            });
        }

        server& spawn_session_builder() {
            worker_executor_->add(
                [this] {
                    auto socket = folly::SemiFuture<boost::asio::ip::tcp::socket>::makeEmpty();
                    try {
                        do {
                            socket_queue_.dequeue(socket);
                            auto endpoint = socket.value().remote_endpoint();
                            auto session = net::server::session<net::protocal::http>::create(
                                std::move(socket).get(), *asio_pool_, directory_,
                                [this, endpoint] {
                                    const auto erase_count = session_map_.erase(endpoint);
                                    logger_->warn("session with endpoint {} erased {}", endpoint, erase_count);
                                });
                            assert(endpoint == session->remote_endpoint());
                            auto [session_iterator, success] = session_map_.emplace(endpoint, std::move(session));
                            if (!success) {
                                logger_->error("endpoint duplicate");
                                throw session_emplace_error{ "ConcurrentHashMap emplace fail" };
                            }
                            assert(endpoint == session_iter->first);
                            session_iterator->second->wait_request();
                        } while (true);
                    } catch (const std::exception& e) {
                        logger_->error("worker catch exception \n{}", boost::diagnostic_information(e));
                    }
                });
            return *this;
        }

        server& loop_listen() {
            logger_->info("port {} listening", port_);
            socket_queue_.enqueue(acceptor_.accept_socket()
                                           .wait());
            logger_->info("socket accepted");
            return *this;
        }

        static void run() {
            auto logger = core::console_logger_access("app");
            try {
                app::server{}.spawn_session_builder()
                             .loop_listen();
            } catch (...) {
                logger()->error("catch exception \n{}", boost::current_exception_diagnostic_information());
            }
            logger()->info("application quit");
        }
    };
}
