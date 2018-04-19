#pragma once

namespace core
{
    //  TODO: experimental
    template<typename Callable>
    struct dereference_callable
    {
        template<typename ...Types>
        decltype(auto) operator()(Types&& ...args) const
        //    ->  std::invoke_result_t<std::decay_t<Callable>, decltype(*std::forward<Types>(args))...>
        {
            return std::decay_t<Callable>{}((*std::forward<Types>(args))...);
        }
    };
}

namespace net
{
    template<typename Protocal>
    class session;

    template<>
    class session<core::as_element_t>
    {
    public:
        struct dereference_hash
        {
            template<typename SessionProtocal>
            size_t operator()(const std::shared_ptr<session<SessionProtocal>>& sess) const
            {
                return std::hash<std::string>{}(endpoint_string(sess->socket()));
            }
        };
        struct dereference_equal
        {
            template<typename SessionProtocal>
            bool operator()(const std::shared_ptr<session<SessionProtocal>>& lsess, 
                const std::shared_ptr<session<SessionProtocal>>& rsess) const
            {
                return *lsess == *rsess;
            }
        };
    protected:
        template<typename SocketProtocal>
        static std::string endpoint_string(const boost::asio::basic_socket<SocketProtocal>& sock)
        {
            std::ostringstream oss;
            oss << sock.local_endpoint() << sock.remote_endpoint();
            return oss.str();
        }
    };

    using session_element = session<core::as_element_t>;

    template<>
    class session<boost::asio::ip::tcp>
        : public std::enable_shared_from_this<session<boost::asio::ip::tcp>>
    {
    public:
        explicit session(boost::asio::ip::tcp::socket socket)
            : socket_(std::move(socket))
            , read_strand_(socket_.get_io_context())
            , write_strand_(socket_.get_io_context())
        {}
        virtual ~session() = default;
        bool operator<(const session& that) const
        {
            return socket_.local_endpoint() < that.socket_.local_endpoint()
                || !(that.socket_.local_endpoint() < socket_.local_endpoint())
                && socket_.remote_endpoint() < that.socket_.remote_endpoint();
        }
        bool operator==(const session& that) const
        {
            return !(*this < that) && !(that < *this);
        }
    protected:
        // virtual void do_write() = 0;
        // virtual void do_read() = 0;
        template<typename Callable>
        boost::asio::executor_binder<std::decay_t<Callable>, boost::asio::io_context>
            make_read_handler(Callable&& handler)
        {
            return boost::asio::bind_executor(read_strand_, std::forward<Callable>(handler));
        }
        template<typename Callable>
        boost::asio::executor_binder<std::decay_t<Callable>, boost::asio::io_context>
            make_write_handler(Callable&& handler)
        {
            return boost::asio::bind_executor(write_strand_, std::forward<Callable>(handler));
        }
        boost::asio::ip::tcp::socket socket_;
    private:
        const boost::asio::ip::tcp::socket& socket() const
        {
            return socket_;
        }
        boost::asio::io_context::strand read_strand_;
        boost::asio::io_context::strand write_strand_;
        friend session_element::dereference_hash;
        friend session_element::dereference_equal;
    };
    
    //  TODO:  template<> class session<boost::asio::ip::udp>;

    //template class session<boost::asio::ip::tcp>;
    //template class session<boost::asio::ip::udp>;

    using tcp_session = session<boost::asio::ip::tcp>;
    using udp_session = session<boost::asio::ip::udp>;

    namespace client
    {
        // class session : public tcp_session
        // {
        // public:
        //     using tcp_session::tcp_session;
        // };
        class session_pool
        {
        public:
            session_pool() = delete;
            explicit session_pool(std::shared_ptr<boost::asio::io_context> context)
                : io_context_ptr_(std::move(context))
                , resolver_(*io_context_ptr_)
                , pool_strand_(*io_context_ptr_)
                , resolver_strand_(*io_context_ptr_)
            {}
            session_pool(const session_pool&) = delete;
            session_pool& operator=(const session_pool&) = delete;
        private:
            struct make_session_callback
            {
                explicit make_session_callback(boost::asio::io_context& ioc)
                    : socket(ioc)
                {}
                make_session_callback(make_session_callback&&) noexcept = default;
                make_session_callback& operator=(make_session_callback&&) noexcept = default;
                std::promise<std::weak_ptr<session<boost::asio::ip::tcp>>> session_promise;
                boost::asio::ip::tcp::socket socket;
            };
        public:
            std::future<std::weak_ptr<session<boost::asio::ip::tcp>>>
                resolve_and_connect(std::string_view host, std::string_view service)
            {
                auto callback = std::make_unique<make_session_callback>(*io_context_ptr_);
                auto make_session_future = callback->session_promise.get_future();
                //  make concurrency access to tcp::resolver thread-safe
                post(resolver_strand_, [=, callback = std::move(callback)]() mutable
                {
                    resolver_.async_resolve(host, service,
                        std::bind(&session_pool::handle_resolve, this, 
                            std::placeholders::_1, std::placeholders::_2, std::move(callback)));
                    //      [this, callback = std::move(callback)](
                    //          const boost::system::error_code& error,
                    //          const boost::asio::ip::tcp::resolver::results_type& endpoints) mutable
                    //  {
                    //     handle_resolve(error, endpoints, std::move(callback));
                    //  });
                });
                return make_session_future;
            }
        private:
            struct callback_collection
            {   //  TODO: currently a dummy placeholder
                std::vector<std::shared_future<std::any>> dummy;
            };
            struct handler
            {
                struct for_resolve {};
                struct for_connect {};
            };
            std::unordered_map<
                std::shared_ptr<session<boost::asio::ip::tcp>>,
                callback_collection,
                session_element::dereference_hash,
                session_element::dereference_equal
            > session_pool_;
            std::shared_ptr<boost::asio::io_context> io_context_ptr_;
            boost::asio::ip::tcp::resolver resolver_;
            boost::asio::io_context::strand pool_strand_;
            boost::asio::io_context::strand resolver_strand_;
            void handle_resolve(
                const boost::system::error_code& error,
                const boost::asio::ip::tcp::resolver::results_type& endpoints,
                std::unique_ptr<make_session_callback>& callback)
            {
                if (!error)
                {
                    async_connect(callback->socket, endpoints, 
                        std::bind(&session_pool::handle_connect, this, std::placeholders::_1, std::move(callback)));
                        //[this, callback = std::move(callback)](const boost::system::error_code& error) mutable {});
                }
                else
                {
                    callback->session_promise.set_exception(
                        std::make_exception_ptr(std::runtime_error{ "endpoint resolvement failure" }));
                    std::cout << "Error: " << error.message() << "\n";
                }
            }
            void handle_connect(
                const boost::system::error_code& error,
                std::unique_ptr<make_session_callback>& callback)
            {
                if (!error)
                {
                    auto session_ptr = std::make_shared<session<boost::asio::ip::tcp>>(std::move(callback->socket));
                    std::weak_ptr<session<boost::asio::ip::tcp>> session_weak_ptr = session_ptr;
                    session_pool_.emplace(std::move(session_ptr), callback_collection{});
                    callback->session_promise.set_value(std::move(session_weak_ptr));
                }
                else
                {
                    callback->session_promise.set_exception(
                        std::make_exception_ptr(std::runtime_error{ "socket connection failure" }));
                    std::cout << "Error: " << error.message() << "\n";
                }
            }
        };
    }
}
