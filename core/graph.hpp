#pragma once

namespace util {
    struct compress_policy
    {
        struct edge_vertex_both {};
        struct edge_only {};         // TODO: template specialization
        struct vertex_only {};       // TODO: template specialization
    };

    struct layout_policy {};                  // TODO: considering necessity

    // Directed Graph
    template<typename VertexValue, typename EdgeValue,
        typename CompressPolicy = compress_policy::edge_vertex_both>
        class graph
    {
    private:
        class indexable_base
        {
        public:
            struct hash
            {
                template<typename Indexable>
                bool operator()(const Indexable& var) const noexcept;
            };
            indexable_base() = default;
            explicit indexable_base(uint64_t index);
            uint64_t index() const noexcept;
        protected:
            uint64_t index_ = std::numeric_limits<uint64_t>::max();
        };
    public:
        class vertex : public indexable_base
        {
        public:
            using value_type = VertexValue;
            struct equal
            {
                bool operator()(const vertex& l, const vertex& r) const noexcept;
            };
            vertex() = default;
            explicit vertex(uint64_t index);
            explicit vertex(std::in_place_t, const VertexValue& data);
            bool has_index() const noexcept;
            bool has_value() const noexcept;
            const VertexValue& value() const;
            //bool operator<(const vertex& other) const noexcept;
        private:
            vertex(uint64_t index, const VertexValue& data);
            //vertex(uint64_t index, VertexValue&& data);
            std::optional<VertexValue> data_ = std::nullopt;
            friend graph;
        };
        class edge : public indexable_base
        {
        public:
            using value_type = EdgeValue;
            struct equal
            {
                bool operator()(const edge& l, const edge& r) const noexcept;
            };
            edge() = default;
            explicit edge(uint64_t index);
            bool has_value() const noexcept;
            const EdgeValue& value() const;
            const vertex& from() const noexcept;
            const vertex& to() const noexcept;
            //bool operator<(const vertex& other) const noexcept;
        private:
            edge(uint64_t index, const vertex& from, const vertex& to, const EdgeValue& data);
            //edge(uint64_t index, EdgeValue&& data);
            std::optional<EdgeValue> data_ = std::nullopt;
            core::reference<const vertex> from_{};
            core::reference<const vertex> to_{};
            friend graph;
        };
    private:
        static_assert(std::is_object_v<VertexValue> && std::is_object_v<EdgeValue>);
    public:
        using vertex_container = std::vector<core::reference<const vertex>>;
        using vertex_edge_container = std::vector<std::pair<const vertex&, const edge&>>;
        using edge_containter = std::vector<core::reference<const edge>>;
        class path_tree
        {
        public:
            using edge_container = typename graph::edge_containter;
            using distance_container = std::vector<typename edge::value_type>;
            path_tree() = delete;
            const vertex& source() const noexcept;
            const edge_container& edge_to() const noexcept;
            const distance_container& dist_to() const noexcept;
            const typename edge::value_type& dist_to(const vertex& vert) const noexcept;
        private:
            path_tree(const vertex& source, edge_container&& econt, distance_container&& dcont) noexcept;
            const vertex& source_;
            const edge_container edge_to_;
            const distance_container dist_to_;
            friend graph;
        };
        const vertex& find_vertex(uint64_t index) const;
        std::optional<vertex> try_find_vertex(uint64_t index) const noexcept;
        vertex_edge_container find_vertex_sink(const vertex& vert) const;
        vertex_edge_container find_vertex_source(const vertex& vert) const;
        uint64_t find_vertex_index(const vertex& vertex) const;
        vertex_container topological_sort() const;
        template<typename Compare = std::less<EdgeValue>>
        path_tree shortest_path_for_acyclic(Compare compare = Compare{}) const;
        template<typename Compare = std::less<EdgeValue>>
        path_tree shortest_path(const vertex& source, Compare compare = Compare{}) const;
        const vertex& add_vertex(const VertexValue& vertex_data);
        //const vertex& add_vertex(VertexValue&& vertex_data);
        const edge& add_edge(const EdgeValue& edge_data, const vertex& from, const vertex& to);
        //const edge& add_edge(const vertex& from, const vertex& to, EdgeValue&& edge_data);
        uint64_t count_vertex() const noexcept;
        uint64_t count_edge() const noexcept;
    private:
        mutable uint64_t next_vertex_index_ = 0;
        std::unordered_map<vertex,
            std::vector<std::pair<const vertex&, const edge&>>,
            typename vertex::hash, typename vertex::equal> adjancency_;
        std::unordered_set<edge, typename edge::hash, typename edge::equal> edges_;
    private:
        using adjacency_type = decltype(adjancency_);
        using adjacency_iterator = typename adjacency_type::const_iterator;
        using edges_type = decltype(edges_);
        uint64_t hash_edge_by_vertex(uint64_t vert0, uint64_t vert1) const noexcept;
        uint64_t generate_vertex_index() const noexcept;
        void depth_first_search(
            adjacency_iterator iter, std::vector<bool>& marked, bool is_preorder,
            std::vector<core::reference<const vertex>>& trace) const;
        void depth_first_search_by_iterator(
            adjacency_iterator iter, uint64_t& index, bool is_preorder,
            std::vector<std::pair<uint64_t, adjacency_iterator>>& marked_iterator) const noexcept;
        std::vector<adjacency_iterator> topological_sort_with_iterator() const;
    };

    using graph_element = graph<void, void>;

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    template <typename Indexable>
    bool graph<VertexValue, EdgeValue, CompressPolicy>::indexable_base::
        hash::operator()(const Indexable& var) const noexcept
    {
        return std::hash<uint64_t>{}(var.index());
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    graph<VertexValue, EdgeValue, CompressPolicy>::
        indexable_base::indexable_base(uint64_t index)
        : index_(index)
    {
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    uint64_t graph<VertexValue, EdgeValue, CompressPolicy>::
        indexable_base::index() const noexcept
    {
        return index_;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    bool graph<VertexValue, EdgeValue, CompressPolicy>::
        vertex::equal::operator()(const vertex& l, const vertex& r) const noexcept
    {
        return l.value() == r.value();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    graph<VertexValue, EdgeValue, CompressPolicy>::
        vertex::vertex(uint64_t index)
        : indexable_base(index)
    {
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    graph<VertexValue, EdgeValue, CompressPolicy>::
        vertex::vertex(std::in_place_t, const VertexValue& data)
        : data_(data)
    {
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    bool graph<VertexValue, EdgeValue, CompressPolicy>::
        vertex::has_index() const noexcept
    {
        return index_ != std::numeric_limits<decltype(index_)>::max()();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    bool graph<VertexValue, EdgeValue, CompressPolicy>::
        vertex::has_value() const noexcept
    {
        return data_.has_value();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    const VertexValue& graph<VertexValue, EdgeValue, CompressPolicy>::
        vertex::value() const
    {
        return data_.value();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    graph<VertexValue, EdgeValue, CompressPolicy>::
        vertex::vertex(uint64_t index, const VertexValue& data)
        : indexable_base(index)
        , data_(data)
    {
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    bool graph<VertexValue, EdgeValue, CompressPolicy>::
        edge::equal::operator()(const edge& l, const edge& r) const noexcept
    {
        return l.index() == r.index();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    graph<VertexValue, EdgeValue, CompressPolicy>::
        edge::edge(uint64_t index)
        : indexable_base(index)
    {
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    bool graph<VertexValue, EdgeValue, CompressPolicy>::
        edge::has_value() const noexcept
    {
        return data_.has_value();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    const EdgeValue& graph<VertexValue, EdgeValue, CompressPolicy>::
        edge::value() const
    {
        return data_.value();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>::
        edge::from() const noexcept -> const vertex&
    {
        return from_.get();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>::
        edge::to() const noexcept -> const vertex&
    {
        return to_.get();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    graph<VertexValue, EdgeValue, CompressPolicy>::
        edge::edge(uint64_t index, const vertex& from, const vertex& to, const EdgeValue& data)
        : indexable_base(index)
        , data_(data)
        , from_(from)
        , to_(to)
    {
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::path_tree::source() const noexcept -> const vertex&
    {
        return source_;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>::
        path_tree::edge_to() const noexcept -> const edge_container&
    {
        return edge_to_;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>::
        path_tree::dist_to() const noexcept -> const distance_container&
    {
        return dist_to_;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>::path_tree::
        dist_to(const vertex& vert) const noexcept -> const typename edge::value_type&
    { 
        const auto equal = vertex::equal{};
        return equal(vert, source_) ? *std::find(dist_to_.cbegin(), dist_to_.cend(), 0) :
            dist_to_.at(std::distance(edge_to_.cbegin(),
                std::find_if(edge_to_.cbegin(), edge_to_.cend(), [&](const edge& edge) { return equal(vert, edge.to()); })));
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    graph<VertexValue, EdgeValue, CompressPolicy>::
        path_tree::path_tree(const vertex& source, edge_container&& econt, distance_container&& dcont) noexcept
        : source_(source)
        , edge_to_(std::move(econt))
        , dist_to_(std::move(dcont))
    {
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::find_vertex_sink(const vertex& vert) const -> vertex_edge_container
    {
        const auto iter = adjancency_.find(vert);
        core::verify(iter != adjancency_.cend());
        //const auto& vert_edge_pair_vec = iter->second;
        return iter->second;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::find_vertex_source(const vertex& vert) const -> vertex_edge_container
    {
        vertex_edge_container source;
        const typename vertex::equal equal;
        for (const auto& vert_adj_vec_pair : adjancency_)
        {
            if (equal(vert_adj_vec_pair.first, vert))
                continue;
            for (const auto& adj_vert_edge_pair : vert_adj_vec_pair.second)
            {
                if (!equal(adj_vert_edge_pair.first, vert))
                    continue;
                source.push_back(std::make_pair(vert_adj_vec_pair.first, adj_vert_edge_pair.second));
            }
        }
        return source;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    uint64_t graph<VertexValue, EdgeValue, CompressPolicy>
        ::find_vertex_index(const vertex& vertex) const
    {
        return std::distance(adjancency_.cbegin(), adjancency_.find(vertex));
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::topological_sort() const -> vertex_container
    {
        std::vector<bool> marked(count_vertex(), false);
        std::vector<core::reference<const vertex>> trace;
        trace.reserve(count_vertex());
        for (auto iter = adjancency_.cbegin(); iter != adjancency_.cend(); ++iter)
            depth_first_search(iter, marked, false, trace);
        std::reverse(trace.begin(), trace.end());
        return trace;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::add_vertex(const VertexValue& vertex_data) -> const vertex&
    {
        const auto vertex_index = generate_vertex_index();
        const auto[iterator, success] = adjancency_.emplace(
            vertex{ vertex_index, vertex_data }, decltype(adjancency_)::mapped_type{});
        core::verify(success);
        return iterator->first;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::add_edge(const EdgeValue& edge_data, const vertex& from, const vertex& to) -> const edge&
    {
        const auto to_iter = adjancency_.find(to);
        core::verify(to_iter != adjancency_.cend() || to.has_value());
        const auto& to_vertex = to_iter != adjancency_.cend() ? to_iter->first : add_vertex(to.value());
        auto from_iter = adjancency_.find(from);
        core::verify(to_vertex.index() != std::numeric_limits<uint64_t>::max());
        core::verify(from_iter != adjancency_.end() || from.has_value());
        if (from_iter == adjancency_.cend())
            from_iter = adjancency_.emplace(
                vertex{ generate_vertex_index(),from.value() }, decltype(adjancency_)::mapped_type{}).first;
        const auto& from_vertex = from_iter->first;
        core::verify(from_vertex.index() != std::numeric_limits<uint64_t>::max());
        const auto edge_index = hash_edge_by_vertex(from_vertex.index(), to_vertex.index());
        const auto[edge_iter, insert_success] = edges_.emplace(
            edge{ edge_index, from_vertex, to_vertex, edge_data });
        core::verify(insert_success);
        from_iter->second.push_back(std::make_pair(std::cref(to_vertex), std::cref(*edge_iter)));
        return *edge_iter;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    uint64_t graph<VertexValue, EdgeValue, CompressPolicy>
        ::count_vertex() const noexcept
    {
        return adjancency_.size();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    uint64_t graph<VertexValue, EdgeValue, CompressPolicy>
        ::count_edge() const noexcept
    {
        return edges_.size();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    uint64_t graph<VertexValue, EdgeValue, CompressPolicy>
        ::hash_edge_by_vertex(const uint64_t vert0, const uint64_t vert1) const noexcept
    {
//        const auto& minmax_pair = std::minmax(vert0, vert1);
//        const std::array<uint64_t, 2> array{ minmax_pair.first,minmax_pair.second };
        const std::array<uint64_t, 2> array{ vert0,vert1 };
        static_assert(sizeof array == 16);
        return std::hash<std::string_view>{}(
            std::string_view{ reinterpret_cast<const char*>(array.data()),sizeof array });
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    uint64_t graph<VertexValue, EdgeValue, CompressPolicy>
        ::generate_vertex_index() const noexcept
    {
        return next_vertex_index_++;
    }

    // TODO: find cycle
    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    void graph<VertexValue, EdgeValue, CompressPolicy>
        ::depth_first_search(
            adjacency_iterator iter, std::vector<bool>& marked, const bool is_preorder,
            std::vector<core::reference<const vertex>>& trace) const
    {
        const auto iter_offset = std::distance(adjancency_.cbegin(), iter);
        if (marked.at(iter_offset))  // std::vector<bool> specialization can't be applied with std::exchange
            return;
        marked.at(iter_offset) = true;
        if (is_preorder)
            trace.push_back(core::reference<const vertex>{iter->first});
        for (const auto& vertex_edge_pair : iter->second)
        {
            const auto next_iter = adjancency_.find(vertex_edge_pair.first);
            depth_first_search(next_iter, marked, is_preorder, trace);
        }
        //marked.at(std::distance(adjancency_.cbegin(), iter)) = true;
        if (!is_preorder)
            trace.push_back(core::reference<const vertex>{iter->first});
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    void graph<VertexValue, EdgeValue, CompressPolicy>
        ::depth_first_search_by_iterator(
            adjacency_iterator iter, uint64_t& index, bool is_preorder,
            std::vector<std::pair<uint64_t, adjacency_iterator>>& marked_iterator) const noexcept
    {
        const auto iter_offset = std::distance(adjancency_.cbegin(), iter);
        if (marked_iterator.at(iter_offset).first != std::numeric_limits<uint64_t>::max())
            return;
        if (is_preorder)
            marked_iterator.at(iter_offset) = std::make_pair(index++, iter);
        else    // adapt to recursion predicate
            marked_iterator.at(iter_offset).first = std::numeric_limits<uint64_t>::min();
        for (const auto& vertex_edge_pair : iter->second)
        {
            const auto next_iter = adjancency_.find(vertex_edge_pair.first);
            depth_first_search_by_iterator(next_iter, index, is_preorder, marked_iterator);
        }
        if (!is_preorder)
            marked_iterator.at(iter_offset) = std::make_pair(index++, iter);
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::topological_sort_with_iterator() const -> std::vector<adjacency_iterator>
    {
        std::vector<std::pair<uint64_t, adjacency_iterator>>
            indexed_iterators(adjancency_.size(), std::make_pair(std::numeric_limits<uint64_t>::max(), adjacency_iterator{}));
        uint64_t index = 0;
        for (auto iter = adjancency_.cbegin(); iter != adjancency_.cend(); ++iter)
            depth_first_search_by_iterator(iter, index, false, indexed_iterators);
        std::sort(indexed_iterators.begin(), indexed_iterators.end(),     // reverse order
            [](const auto& lpair, const auto& rpair) { return lpair.first > rpair.first; });
        std::vector<adjacency_iterator> iterators;
        iterators.reserve(indexed_iterators.size());
        std::transform(indexed_iterators.begin(), indexed_iterators.end(),
            std::back_inserter(iterators), [](auto& pair) { return std::move(pair.second); });
        return iterators;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    template <typename Compare>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::shortest_path_for_acyclic(const Compare compare) const -> path_tree
    {
        //const auto topological_order2 = topological_sort();
        const std::vector<adjacency_iterator> topological_order = topological_sort_with_iterator();
        core::verify(topological_order.size() == count_vertex());
        //const auto ref_less = [](const auto& lref, const auto& rref) { return *lref < *rref; };
        std::vector<core::reference<const edge>> edge_to(count_vertex());
        std::vector<EdgeValue> dist_to(count_vertex(), std::numeric_limits<EdgeValue>::max());
        //dist_to.front() = 0;
        const vertex& front_vertex = topological_order.front()->first;
        const auto front_vert_index = std::distance(adjancency_.cbegin(), topological_order.front());
        dist_to.at(front_vert_index) = 0;
        //auto iterate_count = 0;
        for (const adjacency_iterator& adj_iter : topological_order)
        {
            const vertex& vert = adj_iter->first;
            const auto vert_index = std::distance(adjancency_.cbegin(), adj_iter);
            //  relax routine
            for (const std::pair<const vertex&, const edge&>& pair : adj_iter->second)
            {
                const vertex& adj_vert = pair.first;
                const edge& adj_edge = pair.second;
                const auto adj_vert_iter = adjancency_.find(adj_vert);
                const auto adj_vert_index = std::distance(adjancency_.cbegin(), adj_vert_iter);
                //if (dist_to.at(vert_index) + adj_edge.value() < dist_to.at(adj_vert_index))
                if (compare(dist_to.at(vert_index) + adj_edge.value(), dist_to.at(adj_vert_index)))
                {
                    edge_to.at(adj_vert_index) = core::reference<const edge>{ adj_edge };
                    dist_to.at(adj_vert_index) = dist_to.at(vert_index) + adj_edge.value();
                    core::verify(dist_to.at(adj_vert_index) >= 0);
                    //++iterate_count;
                }
            }
        }
        //edge_containter result(count_vertex());
        return path_tree{ front_vertex,std::move(edge_to),std::move(dist_to) };
    }

    //  TODO: negative cycle finder
    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    template <typename Compare>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::shortest_path(const vertex& source, Compare compare) const -> path_tree
    {
        const auto vertex_count = count_vertex();
        std::vector<bool> on_queue(vertex_count, false);
        std::vector<core::reference<const edge>> edge_to(vertex_count, core::reference<const edge>{});
        std::vector<EdgeValue> dist_to(vertex_count, std::numeric_limits<EdgeValue>::max());
        std::deque<core::reference<const vertex>> vertex_queue{ source };
        const auto source_index = find_vertex_index(source);
        on_queue.at(source_index) = true;
        dist_to.at(source_index) = 0;
        uint64_t iterate_count = 0;
        //uint64_t cost = 0;
        while (!vertex_queue.empty())
        {
            const auto vertex_front = std::move(vertex_queue.front());
            vertex_queue.pop_front();
            const auto vert_iter = adjancency_.find(vertex_front);
            const auto vert_index = std::distance(adjancency_.cbegin(), vert_iter);
            on_queue.at(vert_index) = false;
            //  relax routine
            for (const std::pair<const vertex&, const edge&>& pair : vert_iter->second)
            {
                const vertex& adj_vert = pair.first;
                const edge& adj_edge = pair.second;
                //const auto adj_vert_iter = adjancency_.find(adj_vert);
                const auto adj_vert_index = find_vertex_index(adj_vert);
                //if (dist_to.at(vert_index) + adj_edge.value() < dist_to.at(adj_vert_index))
                if (compare(dist_to.at(vert_index) + adj_edge.value(), dist_to.at(adj_vert_index)))
                {
                    edge_to.at(adj_vert_index) = core::reference<const edge>{ adj_edge };
                    dist_to.at(adj_vert_index) = dist_to.at(vert_index) + adj_edge.value();
                    ++iterate_count;
                    core::verify(dist_to.at(adj_vert_index) >= 0);
                    if (!on_queue.at(adj_vert_index))
                    {
                        vertex_queue.push_back(adj_vert);
                        on_queue.at(adj_vert_index) = true;
                    }
                }
                //if (cost++ % vertex_count == 0)
                //if (cost++ % vertex_count == 0 && cost != 1)
                //    throw std::logic_error{ "negative cycle found" };
            }
        }
        auto max_dist_to = *std::max_element(dist_to.cbegin(), dist_to.cend());
        auto edge_to_copy = edge_to;
        std::sort(edge_to_copy.begin(), edge_to_copy.end(),
            [](const auto& lref, const auto& rref) { return lref.get().value() < rref.get().value(); });
        return path_tree{ source,std::move(edge_to),std::move(dist_to) };
    }
}
