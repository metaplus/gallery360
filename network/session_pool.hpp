#pragma once

namespace net
{
    namespace base
    {
        template<
            typename Protocal = boost::asio::ip::tcp,
            template<typename SocketProtocal>
                typename Socket = boost::asio::basic_stream_socket,
            template<typename Key, typename Mapped, typename Hash, typename Equal, 
                     typename Allocator = std::allocator<std::pair<const Key, Mapped>>>
                typename UnorderedAssociativeContainer = std::unordered_map,
            template<typename Element>
                typename KeyHandle = std::shared_ptr
        > class session_pool
        {
        protected:
            using session = session<Protocal, Socket>;
            using element = typename session::element;
            using protocal = typename session::protocal;
            using socket = typename session::socket;

            struct callback_container
            {   //  TODO: currently a dummy placeholder, std::list assure iterator stability
                std::list<std::shared_future<std::any>> dummy;
            };

            using session_key = KeyHandle<session>;
            using session_container = UnorderedAssociativeContainer<session_key, callback_container,
                typename element::dereference_hash, typename element::dereference_equal>;

            session_pool() = default;

            explicit session_pool(std::shared_ptr<boost::asio::io_context> context)
                : io_context_ptr_(std::move(context))
                , session_pool_strand_(*io_context_ptr_)
            {}

            session_pool(const session_pool&) = delete;

            session_pool(session_pool&& that) noexcept(std::is_nothrow_move_constructible<boost::asio::io_context::strand>::value
                && std::is_nothrow_move_constructible<session_container>::value)
                : session_pool_(std::move(session_pool_))
                , io_context_ptr_(std::move(that.io_context_ptr_))
                , session_pool_strand_(std::move(that.session_pool_strand_))
            {
                assert(std::empty(that.session_pool_));
                assert(that.io_context_ptr_ == nullptr);
                assert(!that.session_pool_strand_.has_value());
            }

            session_pool& operator=(const session_pool&) = delete;

            session_pool& operator=(session_pool&& that) noexcept = delete;

            struct stage {
                struct during_make_session
                {
                    during_make_session() = delete;

                    explicit during_make_session(boost::asio::io_context& context)
                        : session_socket(context)
                    {}

                    during_make_session(during_make_session&&) noexcept = default;

                    during_make_session& operator=(during_make_session&&) noexcept = default;

                    std::promise<std::weak_ptr<session>> session_promise;
                    socket session_socket;
                };
                //  TODO: class during_*, level-triggered event class abstraction
                //  TODO: class on_*, edge-triggered event class abstraction
            };

            bool is_pool_valid() const
            {
                return io_context_ptr_ != nullptr && session_pool_strand_.has_value();
            }

            session_container session_pool_;
            std::shared_ptr<boost::asio::io_context> io_context_ptr_ = nullptr;
            std::optional<boost::asio::io_context::strand> session_pool_strand_ = std::nullopt;
        };

        template class session_pool<>;

        using default_session_pool = session_pool<>;
    }
}