#pragma once

namespace net
{
    template<typename T>
    class promise_base;

    namespace detail
    {
        template<typename T>
        class thread_promise final : public promise_base<T>
        {
            boost::promise<T> promise_;

        public:
            explicit thread_promise(boost::promise<T>&& promise)
                : promise_(std::move(promise)) 
            {}

            thread_promise(thread_promise&&) noexcept = default;
            thread_promise& operator=(thread_promise&&) noexcept = default;
            thread_promise(thread_promise const&) = delete;
            thread_promise& operator=(thread_promise const&) = delete;

            void set_value(T&& value) override
            {
                promise_.set_value(std::move(value));
            }

            void set_exception(std::string_view message) override
            {
                promise_.set_exception(std::runtime_error{ message.data() });
            }
        };

        template<typename T>
        class fiber_promise final : public promise_base<T>
        {
            boost::fibers::promise<T> promise_;

        public:
            explicit fiber_promise(boost::fibers::promise<T>&& promise)
                : promise_(std::move(promise))
            {}

            fiber_promise(fiber_promise&&) noexcept = default;
            fiber_promise& operator=(fiber_promise&&) noexcept = default;
            fiber_promise(fiber_promise const&) = delete;
            fiber_promise& operator=(fiber_promise const&) = delete;

            void set_value(T&& value) override
            {
                promise_.set_value(std::move(value));
            }

            void set_exception(std::string_view message) override
            {
                promise_.set_exception(std::make_exception_ptr(std::runtime_error{ message.data() }));
            }
        };
    }

    template<typename T>
    class promise_base
    {
    public:
        promise_base() = delete;
        promise_base(promise_base&&) noexcept = default;
        promise_base& operator=(promise_base&&) noexcept = default;
        promise_base(promise_base const&) = delete;
        promise_base& operator=(promise_base const&) = delete;

        virtual void set_value(T&& value) = 0;
        virtual void set_exception(std::string_view message) = 0;
        virtual ~promise_base() = default;

        static std::unique_ptr<promise_base<T>> from(boost::promise<T>&& promise)
        {
            return std::make_unique<detail::thread_promise<T>>(std::move(promise));
        }

        static std::unique_ptr<promise_base<T>> from(boost::fibers::promise<T>&& promise)
        {
            return std::make_unique<detail::fiber_promise<T>>(std::move(promise));
        }
    };

    static_assert(std::is_move_constructible<detail::thread_promise<int>>::value);
    static_assert(std::is_move_assignable<detail::thread_promise<int>>::value);
    static_assert(std::is_move_constructible<detail::fiber_promise<int>>::value);
    static_assert(std::is_move_assignable<detail::fiber_promise<int>>::value);
}
