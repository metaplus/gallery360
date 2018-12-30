#pragma once

namespace net::client
{
    template <typename Protocal>
    class session;

    template <typename Protocal>
    using session_ptr = std::unique_ptr<session<Protocal>>;
    using http_session = session<protocal::http>;
    using http_session_ptr = std::unique_ptr<session<protocal::http>>;

    template <>
    class session<protocal::http> final : detail::session_base<boost::asio::ip::tcp::socket, multi_buffer>,
                                          protocal::base<protocal::http>
    {
        using request_param = std::variant<std::monostate,
                                           response<dynamic_body>,
                                           core::bad_request_error,
                                           core::bad_response_error,
                                           core::session_closed_error>;
        using request_list = std::list<folly::Function<void(request_param)>>;
        using trace_callback = std::function<void(std::string_view, std::string)>;
        using trace_callback_wrapper = std::function<void(std::string)>;

        folly::Synchronized<request_list> request_list_;
        std::optional<response_parser<dynamic_body>> response_parser_;
        const int64_t index_ = 0;
        const std::string identity_;
        const std::shared_ptr<spdlog::logger> logger_;
        mutable bool active_ = true;
        mutable trace_callback_wrapper trace_callback_;

    public:
        session(socket_type&& socket,
                boost::asio::io_context& context);

        session() = delete;
        session(const session&) = delete;
        session& operator=(const session&) = delete;

        using session_base::operator<;
        using session_base::local_endpoint;
        using session_base::remote_endpoint;

        folly::SemiFuture<response<dynamic_body>> send_request(request<empty_body>&& request);

        template <typename Target, typename Body>
        folly::SemiFuture<Target> send_request_for(request<Body>&& req) {
            static_assert(!std::is_reference<Target>::value);
            return send_request(std::move(req)).deferValue(
                [](response<dynamic_body>&& response) -> Target {
                    if (response.result() != boost::beast::http::status::ok) {
                        core::throw_bad_request("send_request_for");
                    }
                    if constexpr (std::is_same<multi_buffer, Target>::value) {
                        return std::move(response).body();
                    }
                    core::throw_unreachable("send_request_for");
                });
        }

        static http_session_ptr create(socket_type&& socket,
                                       boost::asio::io_context& context);

        void trace_by(trace_callback callback);

    private:
        void config_response_parser();

        template <typename Exception>
        void shutdown_and_reject_request(Exception&& exception,
                                         boost::system::error_code errc,
                                         boost::asio::socket_base::shutdown_type operation) {
            request_list_.withWLock(
                [this, errc, operation, &exception](request_list& request_list) {
                    for (auto& request : request_list) {
                        request(std::forward<Exception>(exception));
                    }
                    request_list.clear();
                    close_socket(errc, operation);
                    active_ = false;
                });
        }

        auto on_send_request(int64_t index, std::any&& request);

        auto on_recv_response(int64_t index);

        template <typename ...EventArgs>
        void trace_event(const char* event_format, EventArgs&& ...args) const {
            if (trace_callback_) {
                trace_callback_(fmt::format(event_format, std::forward<EventArgs>(args)...));
            }
        }
    };
}
