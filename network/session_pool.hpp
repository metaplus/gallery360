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
            {   //  TODO: currently unused placeholder, std::list assure iterator stability
                std::list<std::shared_future<std::any>> dummy;
            };

            using element_index = typename element::session_index;
            using session_index = typename session::index;
            using session_key = KeyHandle<session>;
            using session_container = UnorderedAssociativeContainer<session_key, callback_container,
                typename element::dereference_hash, typename element::dereference_equal>;
            using session_iterator = typename session_container::iterator;
            using session_const_iterator = typename session_container::const_iterator;
            using session_reference = session&;
            using session_const_reference = const session&;

            session_pool() = delete;

            explicit session_pool(std::shared_ptr<boost::asio::io_context> context)
                : io_context_ptr_(std::move(context))
                , session_pool_strand_(*io_context_ptr_)
            {}

            session_pool(const session_pool&) = delete;

            session_pool(session_pool&& that) noexcept(std::is_nothrow_move_constructible<boost::asio::io_context::strand>::value
                && std::is_nothrow_move_constructible<session_container>::value)
                : session_pool_(std::move(that.session_pool_))
                , io_context_ptr_(std::move(that.io_context_ptr_))
                , session_pool_strand_(std::move(that.session_pool_strand_))
            {
                assert(std::empty(that.session_pool_));
                assert(that.io_context_ptr_ == nullptr);
            }

            session_pool& operator=(const session_pool&) = delete;

            session_pool& operator=(session_pool&& that) noexcept = delete;

            struct stage {
                struct during_make_session : boost::noncopyable
                {
                    explicit during_make_session(boost::asio::io_context& context)
                        : session_socket(context)
                    {}

                    std::promise<std::weak_ptr<session>> session_promise;
                    socket session_socket;
                };

                struct during_wait_session : boost::noncopyable
                {
                    std::promise<std::shared_ptr<session>> session_promise;
                };
                //  TODO: class during_*, level-triggered event class abstraction
                //  TODO: class on_*, edge-triggered event class abstraction
            };

            bool is_pool_valid() const noexcept
            {
                return io_context_ptr_ != nullptr;
            }

            session_iterator find(const element_index& index)
            {
                return std::find_if(session_pool_.begin(), session_pool_.end(),
                    [&index](typename session_container::const_reference pair) { return pair.first->hash_index() == index; });
            }

            session_const_iterator find(const element_index& index) const
            {
                return std::find_if(session_pool_.cbegin(), session_pool_.cend(),
                    [&index](typename session_container::const_reference pair) { return pair.first->hash_index() == index; });
            }

            void remove_session(const element_index& index) 
            {
                if (const auto session_iter = find(index); session_iter != session_pool_.end())
                    session_pool_.erase(session_iter);
                else throw std::out_of_range{ "session not contained" };
            }

            void remove_session(const session_key& session_ptr)
            {
                remove_session(session_index{ session_ptr->hash_index() });
            }

            session_container session_pool_;
            //  TODO: second hash table for fast deducing session location, STL UnorderedAssociativeContainer assures 
            //        stable reference, not iterator, since occasional rehashing operation breaks the invariance.
            // std::unordered_map<element_index, std::weak_ptr<session>> session_index_cache_;
            std::shared_ptr<boost::asio::io_context> io_context_ptr_ = nullptr;
            boost::asio::io_context::strand session_pool_strand_;
        };
        template class session_pool<>;
    }
    using default_session_pool = base::session_pool<>;
}