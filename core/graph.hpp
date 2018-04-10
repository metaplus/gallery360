#pragma once

namespace util
{
    struct compress_policy
    {
        struct edge_vertex_both {};
        struct edge_only {};         // TODO: template specialization
        struct vertex_only {};       // TODO: template specialization
    };

    struct layout_policy {};         // TODO: considering necessity

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
        using vertex_view_container = std::vector<core::reference<const vertex>>;
        using vertex_edge_view_container = std::vector<std::pair<const vertex&, const edge&>>;
        using edge_view_containter = std::vector<core::reference<const edge>>;
        const vertex& find_vertex(uint64_t index) const;
        std::optional<vertex> try_find_vertex(uint64_t index) const noexcept;
        vertex_edge_view_container find_adjacent_vertex(const vertex& vert) const;
        uint64_t find_vertex_index(const vertex& vertex);
        vertex_view_container topological_sort() const;
        template<typename Compare = std::less<EdgeValue>>
        edge_view_containter shortest_path(Compare cmp = Compare{}) const;
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
    const typename graph<VertexValue, EdgeValue, CompressPolicy>::vertex&
        graph<VertexValue, EdgeValue, CompressPolicy>::
        edge::from() const noexcept
    {
        return from_.get();
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    const typename graph<VertexValue, EdgeValue, CompressPolicy>::vertex&
        graph<VertexValue, EdgeValue, CompressPolicy>::
        edge::to() const noexcept
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
        ::find_adjacent_vertex(const vertex& vert) const -> vertex_edge_view_container
    {
        const auto iter = adjancency_.find(vert);
        core::verify(iter != adjancency_.cend());
        //const auto& vert_edge_pair_vec = iter->second;
        return iter->second;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::topological_sort() const -> vertex_view_container
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
    template <typename Compare>
    auto graph<VertexValue, EdgeValue, CompressPolicy>
        ::shortest_path(const Compare cmp) const -> edge_view_containter
    {
        //const auto topological_order = topological_sort();
        const std::vector<adjacency_iterator> topological_order = topological_sort_with_iterator();
        core::verify(topological_order.size() == count_vertex());
        const auto ref_less = [](const auto& lref, const auto& rref) { return *lref < *rref; };
        std::vector<core::reference<const edge>> edge_to(count_vertex());
        std::vector<EdgeValue> dist_to(count_vertex(), std::numeric_limits<EdgeValue>::max());
        //dist_to.front() = 0;
        dist_to.at(std::distance(adjancency_.cbegin(), topological_order.front())) = 0;
        for (const adjacency_iterator& adj_iter : topological_order)
        {
            const vertex& vert = adj_iter->first;
            const auto vert_index = std::distance(adjancency_.cbegin(), adj_iter);
            // relax routine
            for (const std::pair<const vertex&, const edge&>& pair : adj_iter->second)
            {
                const vertex& adj_vert = pair.first;
                const edge& adj_edge = pair.second;
                const auto adj_vert_iter = adjancency_.find(adj_vert);
                const auto adj_vert_index = std::distance(adjancency_.cbegin(), adj_vert_iter);
                if (dist_to.at(vert_index) + adj_edge.value() < dist_to.at(adj_vert_index))
                {
                    edge_to.at(adj_vert_index) = core::reference<const edge>{ adj_edge };
                    dist_to.at(adj_vert_index) = dist_to.at(vert_index) + adj_edge.value();
                }
            }
        }
        //edge_view_containter result(count_vertex());
        return edge_to;
    }

    template <typename VertexValue, typename EdgeValue, typename CompressPolicy>
    //const typename graph<VertexValue, EdgeValue, CompressPolicy>::vertex& 
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
    //const typename graph<VertexValue, EdgeValue, CompressPolicy>::edge& 
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
        ::hash_edge_by_vertex(uint64_t vert0, uint64_t vert1) const noexcept
    {
        const auto& minmax_pair = std::minmax(vert0, vert1);
        const std::array<uint64_t, 2> array{ minmax_pair.first,minmax_pair.second };
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
    //std::vector<typename graph<VertexValue, EdgeValue, CompressPolicy>::adjacency_iterator>
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
}