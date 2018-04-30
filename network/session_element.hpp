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
            bool operator()(const std::shared_ptr<Session>& lsess, const std::shared_ptr<Session>& rsess) const
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

        class session_index
        {
        public:
            session_index() = default;

            explicit session_index(size_t hash_code)
                : hash_code_(hash_code)
            {}

            template<typename SessionIndex>
            explicit session_index(const SessionIndex& index)
                : hash_code_(index.hash_code_)
            {}

            session_index(const session_index&) = default;

            session_index(session_index&& that) noexcept
                : hash_code_(std::exchange(that.hash_code_, default_hash_code()))
            {}

            session_index& operator=(const session_index&) = default;

            session_index& operator=(session_index&& that) noexcept
            {
                hash_code_ = std::exchange(that.hash_code_, default_hash_code());
                return *this;
            }

            bool valid() const noexcept
            {
                return hash_code_ != default_hash_code();
            }

            explicit operator bool() const noexcept
            {
                return valid();
            }

            bool operator<(const session_index& that) const noexcept
            {
                assert(valid() && that.valid());
                return hash_code_ < that.hash_code_;
            }

            bool operator==(const session_index& that) const noexcept
            {
                assert(valid() && that.valid());
                return hash_code_ == that.hash_code_;
            }

        protected:
            // size_t& hash_code() noexcept
            // {
            //     return hash_code_;
            // }
            // size_t hash_code() const noexcept
            // {
            //     return hash_code_;
            // }

            size_t hash_code_ = default_hash_code();

            struct default_hash
            {
                size_t operator()(const session_index& index) const noexcept
                {
                    return std::hash<size_t>{}(index.hash_code_);
                }
            };
            friend default_hash;

        private:
            static size_t default_hash_code() noexcept
            { 
                return std::numeric_limits<size_t>::infinity();
            }
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