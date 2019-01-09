#pragma once

namespace net::client
{
    template <typename Protocal>
    class session;

    template <typename Protocal>
    using session_ptr = std::unique_ptr<session<Protocal>>;

    template <>
    class session<protocal::http> final :
        detail::session_base<boost::asio::ip::tcp::socket, multi_buffer>,
        protocal::protocal_base<protocal::http>
    {
        using request_list = std::list<
            std::pair<request<empty_body>,
                      folly::Promise<response<dynamic_body>>>>;
        using request_sequence = boost::asio::strand<boost::asio::io_context::executor_type>;
        using response_body_parser = response_parser<dynamic_body>;
        using trace_callback = std::function<void(std::string_view, std::string)>;
        using trace_callback_wrapper = std::function<void(std::string)>;

        const core::logger_access logger_;
        request_list request_list_;
        std::optional<response_body_parser> response_parser_;
        mutable bool active_ = true;
        mutable request_sequence request_sequence_;
        mutable trace_callback_wrapper trace_callback_;

    public:
        using pointer = std::unique_ptr<session>;

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

        static pointer create(socket_type&& socket,
                              boost::asio::io_context& context);

        void trace_by(trace_callback callback);

    private:
        void emplace_response_parser();

        template <typename Exception>
        void fail_request_then_close(Exception&& exception, boost::system::error_code errc,
                                         boost::asio::socket_base::shutdown_type operation) {
            assert(request_sequence_.running_in_this_thread());
            for (auto& [request, response] : request_list_) {
                response.setException(std::forward<Exception>(exception));
            }
            request_list_.clear();
            close_socket(errc, operation);
            active_ = false;
        }

        void send_front_request();

        auto on_send_request(int64_t index);

        auto on_recv_response(int64_t index);

        template <typename ...EventArgs>
        void trace_event(const char* event_format, EventArgs&& ...args) const {
            if (trace_callback_) {
                trace_callback_(fmt::format(event_format, std::forward<EventArgs>(args)...));
            }
        }
    };
}
