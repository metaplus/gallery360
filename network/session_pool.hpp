#pragma once

namespace net
{
    namespace base
    {
        template<
            typename Protocal = boost::asio::ip::tcp,
            template<typename SocketProtocal>
                typename Socket = boost::asio::basic_stream_socket,
            template<typename Key, typename Mapped, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>,
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
            {   //  TODO: currently unused placeholder, std::list assures iterator stability, std::any fulfills type-erasure
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

            bool is_pool_valid() const noexcept
            {
                return io_context_ptr_ != nullptr;
            }

            session_iterator find(const element_index& index)
            {
                if (const auto key_cache = find_session_key_cache(index); key_cache.has_value())
                    return session_pool_.find(key_cache.value());
                return std::find_if(session_pool_.begin(), session_pool_.end(),
                    [&index](typename session_container::const_reference pair) { return pair.first->hash_index() == index; });
            }

            session_const_iterator find(const element_index& index) const
            {
                if (const auto key_cache = find_session_key_cache(index); key_cache.has_value())
                    return session_pool_.find(key_cache.value());
                return std::find_if(session_pool_.cbegin(), session_pool_.cend(),
                    [&index](typename session_container::const_reference pair) { return pair.first->hash_index() == index; });
            }

            session_const_reference at(const element_index& index) const
            {
                if (const auto session_iter = find(index); session_iter != session_pool_.end())
                    return *(session_iter->first);
                throw std::out_of_range{ "session not contained" };
            }

            session_reference at(const element_index& index)
            {
                return const_cast<session&>(std::as_const(*this).at(index));
            }

            session_const_reference operator[](const element_index& index) const = delete;

            session_iterator add_session(session_key session_ptr)
            {
                const auto session_index = session_ptr->hash_index();
                const std::weak_ptr<session> session_weak_ptr = session_ptr;
                const auto[iterator, success] = session_pool_.emplace(std::move(session_ptr), callback_container{});
                if (!success) throw core::already_exist_error{ "duplicate session_key" };
                index_cache_.emplace(session_index, session_weak_ptr);
                return iterator;
            }

            void remove_session(const element_index& index) 
            {
                if (const auto session_iter = find(index); session_iter != session_pool_.end())
                {
                    index_cache_.erase(session_iter->first->hash_index());
                    session_pool_.erase(session_iter);
                }
                else throw std::out_of_range{ "session not contained" };
            }

            void remove_session(const session_key& session_ptr)
            {
                remove_session(session_index{ session_ptr->hash_index() });
            }

            session_container session_pool_;
            std::shared_ptr<boost::asio::io_context> io_context_ptr_ = nullptr;
            boost::asio::io_context::strand session_pool_strand_;

        private:
            std::unordered_map<session_index, std::weak_ptr<session>, typename session_index::hash> index_cache_;

            std::optional<session_key> find_session_key_cache(const element_index& index) const
            {
                if (index_cache_.empty()) return std::nullopt;
                const auto index_cache_iter = index_cache_.find(session_index{ index });
                if (index_cache_iter != index_cache_.end() && !index_cache_iter->second.expired())
                    return std::make_optional(index_cache_iter->second.lock());
                return std::nullopt;
            }
        };
        template class session_pool<>;
    }
    using default_session_pool = base::session_pool<>;
}