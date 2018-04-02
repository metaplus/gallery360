#pragma once

namespace net
{
    using boost::asio::ip::tcp;
    template<typename Protocal = boost::asio::ip::tcp>
    class session : public std::enable_shared_from_this<session<Protocal>>
    {
    public:
        explicit session(typename Protocal::socket socket) : socket_(std::move(socket)) {};
        virtual ~session() = default;
        virtual size_t hash_code() const;
        struct dereference_hash
        {
            size_t operator()(const std::shared_ptr<session>& s) const
            {
                return s->hash_code();
            }
        };
    protected:
        //virtual void do_write() = 0;
        //virtual void do_read() = 0;
        typename Protocal::socket socket_;
        //tcp::socket& sock_alias = socket_;
    };

    template <typename Protocal>
    size_t session<Protocal>::hash_code() const
    {
        std::ostringstream oss;
        oss << socket_.local_endpoint() << socket_.remote_endpoint();
        return std::hash<std::string>{}(oss.str());
    }

    template class session<boost::asio::ip::tcp>;
    template class session<boost::asio::ip::udp>;

    using tcp_session = session<boost::asio::ip::tcp>;
    using udp_session = session<boost::asio::ip::udp>;
    
    class client_session_pool
    {
    public:
        class client_session : public tcp_session
        {
        public:
            using tcp_session::tcp_session;
        };

        client_session_pool() = delete;
        explicit client_session_pool(std::shared_ptr<boost::asio::io_context> context)
            : session_pool_()
            , io_context_(std::move(context))
            , pool_strand_(*io_context_)
            , resolver_(*io_context_)
        {}

        client_session_pool(const client_session_pool&) = delete;
        client_session_pool& operator=(const client_session_pool&) = delete;


    private:
        struct callback
        {   // currently a dummy placeholder
            std::vector<std::shared_future<std::any>> dummy;
        };
        std::unordered_map<std::shared_ptr<client_session>, callback,
            client_session::dereference_hash> session_pool_;
        std::shared_ptr<boost::asio::io_context> io_context_;
        boost::asio::io_context::strand pool_strand_;
        boost::asio::ip::tcp::resolver resolver_;
    };
}
