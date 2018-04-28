#pragma once

namespace net
{
    class session_element
    {
    public:
        struct dereference_hash
        {
            template<typename Socket>
            size_t operator()(const Socket& sock) const
            {
                return std::hash<std::string>{}(endpoint_string<Socket>(sock));
            }

            template<typename Session>
            size_t operator()(const std::shared_ptr<Session>& sess) const
            {
                return operator()<typename std::remove_cv_t<Session>::socket>(sess->socket_);
            }
        };

        struct dereference_equal
        {
            template<typename Session>
            bool operator()(const std::shared_ptr<Session>& lsess,
                const std::shared_ptr<Session>& rsess) const
            {
                return *lsess == *rsess;
            }
        };

        template<
            typename Value = std::vector<char>,
            template<typename U, typename = std::allocator<U>> 
                typename Container = std::deque
        > class sequence
        {
        public:
            using value_type = std::decay_t<Value>;
            using container = Container<value_type>;
            using iterator = typename container::iterator;
            using const_iterator = typename container::const_iterator;

            sequence() = delete;

            explicit sequence(boost::asio::io_context& context)
                : strand(context)
                , timer(context)
            {}

            explicit sequence(boost::asio::io_context::executor_type executor)
                : sequence(executor.context())
            {}

            bool is_empty() const noexcept
            {
                return std::empty(queue);
            }

            bool is_disposing() const noexcept
            {
                return is_disposing_;
            }

            void is_disposing(bool reverse_status) noexcept
            {
                assert(reverse_status ^ is_disposing_);
                is_disposing_ = reverse_status;
            }

            //Container<std::decay_t<Value>> queue;
            container queue;

            mutable boost::asio::io_context::strand strand;
            mutable boost::asio::steady_timer timer;

        private:
            bool is_disposing_ = false;
        };

    protected:
        template<typename Socket>
        static std::string endpoint_string(const Socket& sock)
        {
            std::ostringstream oss;
            oss << sock.local_endpoint() << sock.remote_endpoint();
            return oss.str();
        }
    };
}