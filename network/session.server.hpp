#pragma once

namespace net::server
{
    template <typename Protocal>
    class session;

    template <typename Protocal>
    using session_ptr = std::unique_ptr<session<Protocal>>;

    template <>
    class session<protocal::http> final : private detail::session_base<boost::asio::ip::tcp::socket, flat_buffer>,
                                          public protocal::base<protocal::http>
    {
        const core::logger_access logger_;
        std::filesystem::path root_path_;
        folly::Promise<folly::Unit> completion_;

    public:
        using pointer = std::unique_ptr<session>;

        session(boost::asio::ip::tcp::socket&& socket,
                boost::asio::io_context& context);

        session& root_directory(std::filesystem::path root);

        using session_base::local_endpoint;
        using session_base::remote_endpoint;
        using session_base::index;
        using session_base::identity;

        folly::SemiFuture<folly::Unit> process_requests();

        static pointer create(socket_type&& socket,
                              boost::asio::io_context& context,
                              std::filesystem::path root);

    private:
        void receive_request();

        auto on_recv_request(request_ptr<dynamic_body> request);

        template <typename Body>
        auto on_send_response(response_ptr<Body> response) {
            return [this, response = std::move(response)](boost::system::error_code errc,
                                                          std::size_t transfer_size) mutable {
                logger_().info("on_send_response errc {} last {} transfer {}", errc, response->need_eof(), transfer_size);
                if (errc || response->need_eof()) {
                    return close_socket_then_complete(errc, boost::asio::socket_base::shutdown_send);
                }
                receive_request();
            };
        }

        std::filesystem::path concat_target_path(boost::beast::string_view request_target) const;

        void close_socket_then_complete(boost::system::error_code errc,
                                        boost::asio::socket_base::shutdown_type shutdown_type);

        static file_body::value_type file_response_body(std::filesystem::path& target);
    };
}
